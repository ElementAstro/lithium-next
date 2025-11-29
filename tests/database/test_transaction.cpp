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

    // After rollback, state changes
    txn->rollback();

    // The second rollback should fail due to state
    EXPECT_THROW(txn->rollback(), TransactionError);
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

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
