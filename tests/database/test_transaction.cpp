// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * test_transaction.cpp
 *
 * Tests for the Transaction class
 * - Commit saves changes
 * - Rollback discards changes
 * - Auto-rollback in destructor if not committed
 * - Double commit error handling
 * - Commit after rollback error handling
 * - Transaction isolation
 */

#include <gtest/gtest.h>
#include <memory>

#include "database/core/database.hpp"
#include "database/core/statement.hpp"
#include "database/core/transaction.hpp"
#include "database/core/types.hpp"

using namespace lithium::database::core;

// ==================== Transaction Tests ====================

class TransactionTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<Database>(":memory:");

        // Create test table
        db->execute(
            "CREATE TABLE accounts ("
            "  id INTEGER PRIMARY KEY,"
            "  name TEXT,"
            "  balance INTEGER"
            ")");

        // Insert test data
        db->execute("INSERT INTO accounts (name, balance) VALUES ('Alice', 1000)");
        db->execute("INSERT INTO accounts (name, balance) VALUES ('Bob', 500)");
    }

    void TearDown() override {
        db.reset();
    }

    std::unique_ptr<Database> db;
};

TEST_F(TransactionTest, CommitSavesChanges) {
    // Begin transaction
    auto txn = db->beginTransaction();
    EXPECT_NE(txn, nullptr);

    // Make changes
    db->execute("UPDATE accounts SET balance = 900 WHERE name = 'Alice'");

    // Commit
    txn->commit();

    // Verify changes are persisted
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 900);
}

TEST_F(TransactionTest, RollbackDiscardsChanges) {
    // Verify initial state
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 1000);
    }

    // Begin transaction
    auto txn = db->beginTransaction();

    // Make changes
    db->execute("UPDATE accounts SET balance = 500 WHERE name = 'Alice'");

    // Rollback
    txn->rollback();

    // Verify changes were discarded
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1000);
}

TEST_F(TransactionTest, AutoRollbackOnDestruction) {
    // Verify initial state
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 500);
    }

    // Begin transaction in scope
    {
        auto txn = db->beginTransaction();

        // Make changes
        db->execute("UPDATE accounts SET balance = 1000 WHERE name = 'Bob'");

        // Transaction goes out of scope without commit
        // Should auto-rollback
    }

    // Verify changes were rolled back
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 500);
}

TEST_F(TransactionTest, DoubleCommitThrows) {
    auto txn = db->beginTransaction();

    db->execute("UPDATE accounts SET balance = 800 WHERE name = 'Alice'");

    // First commit should succeed
    EXPECT_NO_THROW(txn->commit());

    // Second commit should throw
    EXPECT_THROW(txn->commit(), TransactionError);
}

TEST_F(TransactionTest, CommitAfterRollbackThrows) {
    auto txn = db->beginTransaction();

    db->execute("UPDATE accounts SET balance = 800 WHERE name = 'Alice'");

    // Rollback
    EXPECT_NO_THROW(txn->rollback());

    // Commit after rollback should throw
    EXPECT_THROW(txn->commit(), TransactionError);
}

TEST_F(TransactionTest, RollbackAfterCommitThrows) {
    auto txn = db->beginTransaction();

    db->execute("UPDATE accounts SET balance = 800 WHERE name = 'Alice'");

    // Commit
    EXPECT_NO_THROW(txn->commit());

    // Rollback after commit should throw
    EXPECT_THROW(txn->rollback(), TransactionError);
}

TEST_F(TransactionTest, DoubleRollbackThrows) {
    auto txn = db->beginTransaction();

    db->execute("UPDATE accounts SET balance = 800 WHERE name = 'Alice'");

    // First rollback should succeed
    EXPECT_NO_THROW(txn->rollback());

    // Second rollback should throw
    EXPECT_THROW(txn->rollback(), TransactionError);
}

TEST_F(TransactionTest, TransactionWithMultipleOperations) {
    // Simulate transfer: Alice gives 100 to Bob
    {
        auto txn = db->beginTransaction();

        db->execute("UPDATE accounts SET balance = balance - 100 WHERE name = 'Alice'");
        db->execute("UPDATE accounts SET balance = balance + 100 WHERE name = 'Bob'");

        txn->commit();
    }

    // Verify both changes were applied
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 900);
    }
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 600);
    }
}

TEST_F(TransactionTest, FailedTransferRollsBack) {
    // Try to transfer, but rollback on some condition
    {
        auto txn = db->beginTransaction();

        // Start transfer
        db->execute("UPDATE accounts SET balance = balance - 500 WHERE name = 'Alice'");

        // Check if the operation succeeded
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        int newBalance = stmt->getInt(0);

        if (newBalance < 0) {
            // Insufficient funds, rollback
            txn->rollback();
        } else {
            // Complete transfer
            db->execute(
                "UPDATE accounts SET balance = balance + 500 WHERE name = 'Bob'");
            txn->commit();
        }
    }

    // Verify rollback occurred for insufficient funds
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 1000);  // Original value
    }
}

TEST_F(TransactionTest, NestedTransactionScopes) {
    // SQLite doesn't support nested transactions, but we can create
    // multiple sequential transactions

    // First transaction
    {
        auto txn = db->beginTransaction();
        db->execute("UPDATE accounts SET balance = 1100 WHERE name = 'Alice'");
        txn->commit();
    }

    // Verify first transaction
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 1100);
    }

    // Second transaction
    {
        auto txn = db->beginTransaction();
        db->execute("UPDATE accounts SET balance = 400 WHERE name = 'Bob'");
        txn->commit();
    }

    // Verify both transactions
    {
        auto stmt1 = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt1->step();
        EXPECT_EQ(stmt1->getInt(0), 1100);

        auto stmt2 = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt2->step();
        EXPECT_EQ(stmt2->getInt(0), 400);
    }
}

TEST_F(TransactionTest, TransactionWithPreparedStatements) {
    auto txn = db->beginTransaction();

    // Use prepared statement within transaction
    auto updateStmt = db->prepare("UPDATE accounts SET balance = ? WHERE name = ?");
    updateStmt->bind(1, 750);
    updateStmt->bind(2, std::string("Alice"));
    updateStmt->execute();

    txn->commit();

    // Verify
    auto selectStmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
    selectStmt->step();
    EXPECT_EQ(selectStmt->getInt(0), 750);
}

TEST_F(TransactionTest, PartialRollbackLogic) {
    // Test a transaction where we only perform the commit if all checks pass
    bool shouldCommit = true;

    auto txn = db->beginTransaction();

    // Operation 1
    db->execute("UPDATE accounts SET balance = 800 WHERE name = 'Alice'");

    // Check operation 1
    {
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt->step();
        if (stmt->getInt(0) < 0) {
            shouldCommit = false;
        }
    }

    if (shouldCommit) {
        // Operation 2
        db->execute("UPDATE accounts SET balance = 700 WHERE name = 'Bob'");

        // Check operation 2
        auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt->step();
        if (stmt->getInt(0) < 0) {
            shouldCommit = false;
        }
    }

    if (shouldCommit) {
        txn->commit();
    } else {
        txn->rollback();
    }

    // Verify
    {
        auto stmt1 = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt1->step();
        EXPECT_EQ(stmt1->getInt(0), 800);

        auto stmt2 = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt2->step();
        EXPECT_EQ(stmt2->getInt(0), 700);
    }
}

TEST_F(TransactionTest, TransactionStateTracking) {
    auto txn = db->beginTransaction();

    // Can make updates
    db->execute("UPDATE accounts SET balance = 750 WHERE name = 'Alice'");

    // After commit, state changes
    txn->commit();

    // Rollback after commit should throw
    EXPECT_THROW(txn->rollback(), TransactionError);

    // Commit after commit should also throw
    EXPECT_THROW(txn->commit(), TransactionError);
}

TEST_F(TransactionTest, MultipleRowsInTransaction) {
    auto txn = db->beginTransaction();

    // Update multiple rows
    db->execute("UPDATE accounts SET balance = balance * 2");

    txn->commit();

    // Verify all rows were updated
    {
        auto stmt1 = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
        stmt1->step();
        EXPECT_EQ(stmt1->getInt(0), 2000);

        auto stmt2 = db->prepare("SELECT balance FROM accounts WHERE name = 'Bob'");
        stmt2->step();
        EXPECT_EQ(stmt2->getInt(0), 1000);
    }
}

TEST_F(TransactionTest, InsertInTransaction) {
    auto txn = db->beginTransaction();

    // Insert new record
    db->execute("INSERT INTO accounts (name, balance) VALUES ('Charlie', 750)");

    txn->commit();

    // Verify insertion
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Charlie'");
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 750);
}

TEST_F(TransactionTest, DeleteInTransaction) {
    auto txn = db->beginTransaction();

    // Delete record
    db->execute("DELETE FROM accounts WHERE name = 'Bob'");

    txn->commit();

    // Verify deletion
    auto stmt = db->prepare("SELECT COUNT(*) FROM accounts WHERE name = 'Bob'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 0);
}

TEST_F(TransactionTest, RollbackDeleteRestoresData) {
    // Verify Bob exists
    {
        auto stmt = db->prepare("SELECT COUNT(*) FROM accounts WHERE name = 'Bob'");
        stmt->step();
        EXPECT_EQ(stmt->getInt(0), 1);
    }

    // Delete in transaction then rollback
    {
        auto txn = db->beginTransaction();
        db->execute("DELETE FROM accounts WHERE name = 'Bob'");
        txn->rollback();
    }

    // Verify Bob still exists
    auto stmt = db->prepare("SELECT COUNT(*) FROM accounts WHERE name = 'Bob'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1);
}

TEST_F(TransactionTest, RollbackInsertRemovesData) {
    // Insert in transaction then rollback
    {
        auto txn = db->beginTransaction();
        db->execute("INSERT INTO accounts (name, balance) VALUES ('Dave', 999)");
        txn->rollback();
    }

    // Verify Dave doesn't exist
    auto stmt = db->prepare("SELECT COUNT(*) FROM accounts WHERE name = 'Dave'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 0);
}

TEST_F(TransactionTest, TransactionWithCreateTable) {
    auto txn = db->beginTransaction();

    // Create new table in transaction
    db->execute("CREATE TABLE temp_table (id INTEGER PRIMARY KEY, data TEXT)");
    db->execute("INSERT INTO temp_table (data) VALUES ('test')");

    txn->commit();

    // Verify table exists and has data
    auto stmt = db->prepare("SELECT data FROM temp_table");
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getText(0), "test");
}

TEST_F(TransactionTest, TransactionWithDropTable) {
    // Create a table first
    db->execute("CREATE TABLE to_drop (id INTEGER)");

    auto txn = db->beginTransaction();
    db->execute("DROP TABLE to_drop");
    txn->commit();

    // Verify table is dropped (this should throw when trying to select from it)
    EXPECT_THROW(db->execute("SELECT * FROM to_drop"), SqlExecutionError);
}

TEST_F(TransactionTest, LargeTransactionWithManyInserts) {
    auto txn = db->beginTransaction();

    // Insert many records
    for (int i = 0; i < 100; ++i) {
        std::string sql = "INSERT INTO accounts (name, balance) VALUES ('User" +
                          std::to_string(i) + "', " + std::to_string(i * 10) + ")";
        db->execute(sql);
    }

    txn->commit();

    // Verify all inserts
    auto stmt = db->prepare("SELECT COUNT(*) FROM accounts");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 102);  // 2 original + 100 new
}

TEST_F(TransactionTest, TransactionExceptionSafety) {
    // Start transaction
    auto txn = db->beginTransaction();

    db->execute("UPDATE accounts SET balance = 500 WHERE name = 'Alice'");

    // Simulate error by trying invalid SQL
    try {
        db->execute("INVALID SQL");
    } catch (const SqlExecutionError&) {
        // Expected exception
    }

    // Transaction should still be usable
    txn->rollback();

    // Verify original value is preserved
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1000);
}

TEST_F(TransactionTest, NonCopyable) {
    // Verify Transaction is not copyable
    EXPECT_FALSE(std::is_copy_constructible_v<Transaction>);
    EXPECT_FALSE(std::is_copy_assignable_v<Transaction>);
}

TEST_F(TransactionTest, TransactionWithConstraintViolation) {
    // Create table with unique constraint
    db->execute("CREATE TABLE unique_test (id INTEGER PRIMARY KEY, value TEXT UNIQUE)");
    db->execute("INSERT INTO unique_test (value) VALUES ('unique_value')");

    auto txn = db->beginTransaction();

    // Try to insert duplicate
    EXPECT_THROW(
        db->execute("INSERT INTO unique_test (value) VALUES ('unique_value')"),
        SqlExecutionError);

    // Transaction should still be rollbackable
    EXPECT_NO_THROW(txn->rollback());
}

TEST_F(TransactionTest, SequentialTransactions) {
    // Multiple sequential transactions
    for (int i = 0; i < 5; ++i) {
        auto txn = db->beginTransaction();
        std::string sql = "UPDATE accounts SET balance = balance + 10 WHERE name = 'Alice'";
        db->execute(sql);
        txn->commit();
    }

    // Verify cumulative effect
    auto stmt = db->prepare("SELECT balance FROM accounts WHERE name = 'Alice'");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1050);  // 1000 + 5*10
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
