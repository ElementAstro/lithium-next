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
 * test_database_core.cpp
 *
 * Tests for the core Database class functionality
 * - Database construction and validation
 * - Execute simple SQL statements
 * - Prepare statements
 * - Begin transactions
 * - Move semantics
 * - Error handling
 */

#include <gtest/gtest.h>
#include <memory>
#include <sqlite3.h>

#include "database/core/database.hpp"
#include "database/core/types.hpp"

using namespace lithium::database::core;

// ==================== DatabaseCore Tests ====================

class DatabaseCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fresh in-memory database for each test
        db = std::make_unique<Database>(":memory:");
    }

    void TearDown() override {
        db.reset();
    }

    std::unique_ptr<Database> db;
};

TEST_F(DatabaseCoreTest, ConstructorWithMemoryDatabase) {
    // Test that in-memory database can be created successfully
    EXPECT_TRUE(db->isValid());
    EXPECT_NE(db->get(), nullptr);
}

TEST_F(DatabaseCoreTest, IsValid) {
    // Test that a valid database returns true for isValid()
    ASSERT_TRUE(db->isValid());

    // The database should remain valid
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, GetDatabaseHandle) {
    // Test that we can get the sqlite3 handle
    sqlite3* handle = db->get();
    EXPECT_NE(handle, nullptr);

    // Handle should be consistent across multiple calls
    EXPECT_EQ(handle, db->get());
}

TEST_F(DatabaseCoreTest, ExecuteSimpleSQL) {
    // Test executing a simple CREATE TABLE statement
    EXPECT_NO_THROW(db->execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)"));

    // Verify the table was created by inserting data
    EXPECT_NO_THROW(db->execute("INSERT INTO test (name) VALUES ('test_value')"));
}

TEST_F(DatabaseCoreTest, ExecuteMultipleStatements) {
    // Test executing multiple statements sequentially
    EXPECT_NO_THROW(db->execute("CREATE TABLE t1 (id INTEGER PRIMARY KEY)"));
    EXPECT_NO_THROW(db->execute("CREATE TABLE t2 (id INTEGER PRIMARY KEY)"));
    EXPECT_NO_THROW(db->execute("INSERT INTO t1 VALUES (1)"));
    EXPECT_NO_THROW(db->execute("INSERT INTO t2 VALUES (2)"));
}

TEST_F(DatabaseCoreTest, ExecuteInvalidSQL) {
    // Test that invalid SQL throws an exception
    EXPECT_THROW(db->execute("INVALID SQL STATEMENT"),
                 SQLExecutionError);
}

TEST_F(DatabaseCoreTest, PrepareStatement) {
    // Setup table
    db->execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");

    // Prepare a statement
    auto stmt = db->prepare("INSERT INTO users (name) VALUES (?)");
    EXPECT_NE(stmt, nullptr);

    // Statement should be ready to use
    EXPECT_NE(stmt->get(), nullptr);
}

TEST_F(DatabaseCoreTest, PrepareInvalidSQL) {
    // Test that preparing invalid SQL throws an exception
    EXPECT_THROW(db->prepare("INVALID SQL STATEMENT"),
                 PrepareStatementError);
}

TEST_F(DatabaseCoreTest, BeginTransaction) {
    // Test creating a transaction
    auto txn = db->beginTransaction();
    EXPECT_NE(txn, nullptr);
}

TEST_F(DatabaseCoreTest, BeginTransactionMultiple) {
    // Test that multiple transactions can be created and committed
    auto txn1 = db->beginTransaction();
    EXPECT_NE(txn1, nullptr);
    txn1->commit();

    auto txn2 = db->beginTransaction();
    EXPECT_NE(txn2, nullptr);
    txn2->commit();
}

TEST_F(DatabaseCoreTest, MoveConstructor) {
    // Create a database
    Database db1(":memory:");
    EXPECT_TRUE(db1.isValid());
    sqlite3* handle1 = db1.get();

    // Move construct from db1
    Database db2(std::move(db1));

    // db2 should be valid
    EXPECT_TRUE(db2.isValid());
    // db2 should have the same handle
    EXPECT_EQ(db2.get(), handle1);
}

TEST_F(DatabaseCoreTest, MoveAssignment) {
    // Create two databases
    Database db1(":memory:");
    Database db2(":memory:");

    EXPECT_TRUE(db1.isValid());
    EXPECT_TRUE(db2.isValid());

    sqlite3* handle1 = db1.get();

    // Move assign db1 to db2
    db2 = std::move(db1);

    // db2 should now have db1's handle
    EXPECT_EQ(db2.get(), handle1);
    EXPECT_TRUE(db2.isValid());
}

TEST_F(DatabaseCoreTest, CopyConstructorDeleted) {
    // Verify that copy constructor is deleted
    EXPECT_FALSE(std::is_copy_constructible_v<Database>);
}

TEST_F(DatabaseCoreTest, CopyAssignmentDeleted) {
    // Verify that copy assignment is deleted
    EXPECT_FALSE(std::is_copy_assignable_v<Database>);
}

TEST_F(DatabaseCoreTest, ConfigurePragmas) {
    // Test setting various PRAGMA settings
    std::unordered_map<std::string, std::string> pragmas;
    pragmas["synchronous"] = "OFF";
    pragmas["cache_size"] = "10000";

    EXPECT_NO_THROW(db->configure(pragmas));

    // Database should still be valid after configuration
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, CommitTransaction) {
    // Test explicit commit
    auto txn = db->beginTransaction();
    txn->commit();

    // Database should still be valid
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, RollbackTransaction) {
    // Test explicit rollback
    auto txn = db->beginTransaction();
    txn->rollback();

    // Database should still be valid
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, ExecuteWithPreparedStatement) {
    // Create table
    db->execute("CREATE TABLE items (id INTEGER PRIMARY KEY, value TEXT)");

    // Prepare and execute
    auto stmt = db->prepare("INSERT INTO items (value) VALUES (?)");
    stmt->bind(1, std::string("test_value"));
    EXPECT_TRUE(stmt->execute());

    // Verify insertion by selecting
    auto select_stmt = db->prepare("SELECT COUNT(*) FROM items");
    EXPECT_TRUE(select_stmt->step());
    EXPECT_EQ(select_stmt->getInt(0), 1);
}

TEST_F(DatabaseCoreTest, FailedToOpenDatabaseException) {
    // Test exception when trying to open invalid database path
    // On most systems, /invalid/path/db.sqlite will fail to open
    EXPECT_THROW(
        {
            try {
                Database invalid_db("/invalid/path/that/does/not/exist/db.sqlite");
            } catch (const FailedToOpenDatabase& e) {
                EXPECT_NE(std::string(e.what()).length(), 0);
                throw;
            }
        },
        FailedToOpenDatabase);
}

TEST_F(DatabaseCoreTest, DatabaseOperationsAfterMoveConstruct) {
    // Create original database
    Database db1(":memory:");
    db1.execute("CREATE TABLE test (id INTEGER)");
    sqlite3* handle1 = db1.get();

    // Move to new database
    Database db2(std::move(db1));

    // Should be able to use db2 with the original handle
    EXPECT_EQ(db2.get(), handle1);

    // Should be able to execute queries
    EXPECT_NO_THROW(db2.execute("INSERT INTO test VALUES (1)"));
}

TEST_F(DatabaseCoreTest, ConcurrentPreparedStatements) {
    // Create table
    db->execute("CREATE TABLE data (id INTEGER PRIMARY KEY, value INTEGER)");

    // Prepare multiple statements
    auto stmt1 = db->prepare("INSERT INTO data (value) VALUES (?)");
    auto stmt2 = db->prepare("INSERT INTO data (value) VALUES (?)");
    auto stmt3 = db->prepare("SELECT COUNT(*) FROM data");

    // All should be valid
    EXPECT_NE(stmt1, nullptr);
    EXPECT_NE(stmt2, nullptr);
    EXPECT_NE(stmt3, nullptr);

    // Use them sequentially
    stmt1->bind(1, 10);
    stmt1->execute();

    stmt2->bind(1, 20);
    stmt2->execute();

    stmt3->step();
    EXPECT_EQ(stmt3->getInt(0), 2);
}

TEST_F(DatabaseCoreTest, DirectCommitMethod) {
    // Test Database::commit() direct method
    db->execute("CREATE TABLE commit_test (id INTEGER PRIMARY KEY, value TEXT)");

    // Begin transaction manually
    db->execute("BEGIN TRANSACTION;");
    db->execute("INSERT INTO commit_test (value) VALUES ('test')");

    // Use direct commit method
    EXPECT_NO_THROW(db->commit());

    // Verify data was committed
    auto stmt = db->prepare("SELECT COUNT(*) FROM commit_test");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1);
}

TEST_F(DatabaseCoreTest, DirectRollbackMethod) {
    // Test Database::rollback() direct method
    db->execute("CREATE TABLE rollback_test (id INTEGER PRIMARY KEY, value TEXT)");
    db->execute("INSERT INTO rollback_test (value) VALUES ('initial')");

    // Begin transaction manually
    db->execute("BEGIN TRANSACTION;");
    db->execute("INSERT INTO rollback_test (value) VALUES ('should_be_rolled_back')");

    // Use direct rollback method
    EXPECT_NO_THROW(db->rollback());

    // Verify only initial data exists
    auto stmt = db->prepare("SELECT COUNT(*) FROM rollback_test");
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 1);
}

TEST_F(DatabaseCoreTest, DatabaseOpenErrorExceptionType) {
    // Test that DatabaseOpenError is thrown for invalid paths
    EXPECT_THROW(
        Database("/invalid/path/that/does/not/exist/db.sqlite"),
        DatabaseOpenError);
}

TEST_F(DatabaseCoreTest, SqlExecutionErrorExceptionType) {
    // Test that SqlExecutionError is thrown for invalid SQL
    EXPECT_THROW(db->execute("INVALID SQL SYNTAX"), SqlExecutionError);
}

TEST_F(DatabaseCoreTest, StatementPrepareErrorExceptionType) {
    // Test that StatementPrepareError is thrown for invalid prepare
    EXPECT_THROW(db->prepare("INVALID SQL SYNTAX"), StatementPrepareError);
}

TEST_F(DatabaseCoreTest, ValidationErrorOnInvalidDatabase) {
    // Create and move database to make it invalid
    Database db1(":memory:");
    Database db2(std::move(db1));

    // db1 is now invalid, operations should throw ValidationError
    EXPECT_THROW(db1.get(), ValidationError);
    EXPECT_THROW(db1.prepare("SELECT 1"), ValidationError);
    EXPECT_THROW(db1.execute("SELECT 1"), ValidationError);
    EXPECT_THROW(db1.beginTransaction(), ValidationError);
}

TEST_F(DatabaseCoreTest, ConfigureEmptyPragmas) {
    // Test configure with empty pragmas map
    std::unordered_map<std::string, std::string> emptyPragmas;
    EXPECT_NO_THROW(db->configure(emptyPragmas));
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, ConfigureMultiplePragmas) {
    // Test configure with multiple pragmas
    std::unordered_map<std::string, std::string> pragmas = {
        {"synchronous", "OFF"},
        {"cache_size", "10000"},
        {"temp_store", "MEMORY"},
        {"mmap_size", "268435456"}
    };
    EXPECT_NO_THROW(db->configure(pragmas));
    EXPECT_TRUE(db->isValid());
}

TEST_F(DatabaseCoreTest, CustomOpenFlags) {
    // Test opening database with custom flags
    Database readOnlyDb(":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    EXPECT_TRUE(readOnlyDb.isValid());
}

TEST_F(DatabaseCoreTest, TransactionWithPreparedStatementExecution) {
    // Test transaction with prepared statement execution
    db->execute("CREATE TABLE txn_test (id INTEGER PRIMARY KEY, value INTEGER)");

    auto txn = db->beginTransaction();
    auto stmt = db->prepare("INSERT INTO txn_test (value) VALUES (?)");

    for (int i = 1; i <= 5; ++i) {
        stmt->bind(1, i * 100);
        stmt->execute();
        stmt->reset();
    }

    txn->commit();

    // Verify all inserts
    auto countStmt = db->prepare("SELECT COUNT(*) FROM txn_test");
    countStmt->step();
    EXPECT_EQ(countStmt->getInt(0), 5);

    auto sumStmt = db->prepare("SELECT SUM(value) FROM txn_test");
    sumStmt->step();
    EXPECT_EQ(sumStmt->getInt(0), 1500);  // 100+200+300+400+500
}

TEST_F(DatabaseCoreTest, MoveConstructorInvalidatesSource) {
    Database db1(":memory:");
    EXPECT_TRUE(db1.isValid());

    Database db2(std::move(db1));

    // db1 should be invalid after move
    EXPECT_FALSE(db1.isValid());
    // db2 should be valid
    EXPECT_TRUE(db2.isValid());
}

TEST_F(DatabaseCoreTest, MoveAssignmentInvalidatesSource) {
    Database db1(":memory:");
    Database db2(":memory:");

    EXPECT_TRUE(db1.isValid());
    EXPECT_TRUE(db2.isValid());

    db2 = std::move(db1);

    // db1 should be invalid after move
    EXPECT_FALSE(db1.isValid());
    // db2 should be valid
    EXPECT_TRUE(db2.isValid());
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
