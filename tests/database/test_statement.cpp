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
 * test_statement.cpp
 *
 * Tests for the Statement class
 * - Parameter binding (int, int64, double, string, null)
 * - Execute and step operations
 * - Column value retrieval
 * - Column metadata
 * - Statement reset
 * - Type checking
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "database/core/database.hpp"
#include "database/core/statement.hpp"
#include "database/core/types.hpp"

using namespace lithium::database::core;

// ==================== Statement Tests ====================

class StatementTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<Database>(":memory:");

        // Create test table
        db->execute(
            "CREATE TABLE test_table ("
            "  id INTEGER PRIMARY KEY,"
            "  int_val INTEGER,"
            "  int64_val INTEGER,"
            "  double_val REAL,"
            "  text_val TEXT,"
            "  null_val TEXT"
            ")");
    }

    void TearDown() override {
        db.reset();
    }

    std::unique_ptr<Database> db;
};

TEST_F(StatementTest, BindInt) {
    // Prepare insert statement
    auto stmt = db->prepare("INSERT INTO test_table (int_val) VALUES (?)");
    EXPECT_NE(stmt, nullptr);

    // Bind integer value
    stmt->bind(1, 42);

    // Execute and verify
    EXPECT_TRUE(stmt->execute());

    // Verify insertion
    auto select = db->prepare("SELECT int_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_EQ(select->getInt(0), 42);
}

TEST_F(StatementTest, BindInt64) {
    // Prepare insert statement
    auto stmt = db->prepare("INSERT INTO test_table (int64_val) VALUES (?)");

    // Bind 64-bit integer value
    int64_t largeValue = 9223372036854775807LL;
    stmt->bind(1, largeValue);

    // Execute and verify
    EXPECT_TRUE(stmt->execute());

    // Verify insertion
    auto select = db->prepare("SELECT int64_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_EQ(select->getInt64(0), largeValue);
}

TEST_F(StatementTest, BindDouble) {
    // Prepare insert statement
    auto stmt = db->prepare("INSERT INTO test_table (double_val) VALUES (?)");

    // Bind double value
    double doubleValue = 3.14159265;
    stmt->bind(1, doubleValue);

    // Execute and verify
    EXPECT_TRUE(stmt->execute());

    // Verify insertion
    auto select = db->prepare("SELECT double_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_DOUBLE_EQ(select->getDouble(0), doubleValue);
}

TEST_F(StatementTest, BindString) {
    // Prepare insert statement
    auto stmt = db->prepare("INSERT INTO test_table (text_val) VALUES (?)");

    // Bind string value
    std::string stringValue = "Hello, World!";
    stmt->bind(1, stringValue);

    // Execute and verify
    EXPECT_TRUE(stmt->execute());

    // Verify insertion
    auto select = db->prepare("SELECT text_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_EQ(select->getText(0), stringValue);
}

TEST_F(StatementTest, BindNull) {
    // Prepare insert statement
    auto stmt = db->prepare("INSERT INTO test_table (null_val) VALUES (?)");

    // Bind NULL value
    stmt->bindNull(1);

    // Execute and verify
    EXPECT_TRUE(stmt->execute());

    // Verify NULL was inserted
    auto select = db->prepare("SELECT null_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_TRUE(select->isNull(0));
}

TEST_F(StatementTest, BindChaining) {
    // Test method chaining for bind operations
    auto stmt = db->prepare(
        "INSERT INTO test_table (id, int_val, double_val, text_val) "
        "VALUES (?, ?, ?, ?)");

    stmt->bind(1, 1)
        .bind(2, 100)
        .bind(3, 2.71828)
        .bind(4, std::string("test"));

    EXPECT_TRUE(stmt->execute());

    // Verify all values
    auto select = db->prepare(
        "SELECT id, int_val, double_val, text_val FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_EQ(select->getInt(0), 1);
    EXPECT_EQ(select->getInt(1), 100);
    EXPECT_DOUBLE_EQ(select->getDouble(2), 2.71828);
    EXPECT_EQ(select->getText(3), "test");
}

TEST_F(StatementTest, ExecuteWithoutParameters) {
    // Prepare a statement without parameters
    auto stmt = db->prepare(
        "INSERT INTO test_table (id, int_val) VALUES (1, 42)");

    EXPECT_TRUE(stmt->execute());

    // Verify insertion
    auto select = db->prepare("SELECT COUNT(*) FROM test_table");
    EXPECT_TRUE(select->step());
    EXPECT_EQ(select->getInt(0), 1);
}

TEST_F(StatementTest, Step) {
    // Insert test data
    db->execute("INSERT INTO test_table (int_val) VALUES (1)");
    db->execute("INSERT INTO test_table (int_val) VALUES (2)");
    db->execute("INSERT INTO test_table (int_val) VALUES (3)");

    // Prepare select statement
    auto stmt = db->prepare("SELECT int_val FROM test_table ORDER BY int_val");

    // Step through results
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 1);

    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 2);

    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 3);

    // No more rows
    EXPECT_FALSE(stmt->step());
}

TEST_F(StatementTest, GetInt) {
    auto stmt = db->prepare("INSERT INTO test_table (int_val) VALUES (?)");
    stmt->bind(1, 42);
    stmt->execute();

    auto select = db->prepare("SELECT int_val FROM test_table");
    select->step();
    EXPECT_EQ(select->getInt(0), 42);
}

TEST_F(StatementTest, GetInt64) {
    auto stmt = db->prepare("INSERT INTO test_table (int64_val) VALUES (?)");
    int64_t value = 1234567890123456LL;
    stmt->bind(1, value);
    stmt->execute();

    auto select = db->prepare("SELECT int64_val FROM test_table");
    select->step();
    EXPECT_EQ(select->getInt64(0), value);
}

TEST_F(StatementTest, GetDouble) {
    auto stmt = db->prepare("INSERT INTO test_table (double_val) VALUES (?)");
    double value = 1.23456789;
    stmt->bind(1, value);
    stmt->execute();

    auto select = db->prepare("SELECT double_val FROM test_table");
    select->step();
    EXPECT_DOUBLE_EQ(select->getDouble(0), value);
}

TEST_F(StatementTest, GetText) {
    auto stmt = db->prepare("INSERT INTO test_table (text_val) VALUES (?)");
    std::string value = "Test String";
    stmt->bind(1, value);
    stmt->execute();

    auto select = db->prepare("SELECT text_val FROM test_table");
    select->step();
    EXPECT_EQ(select->getText(0), value);
}

TEST_F(StatementTest, GetBlob) {
    // Create table with BLOB column
    db->execute("CREATE TABLE blob_test (id INTEGER PRIMARY KEY, data BLOB)");

    // Insert binary data
    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto stmt = db->prepare("INSERT INTO blob_test (data) VALUES (?)");
    stmt->bind(1, std::string("test"));
    stmt->execute();

    // Retrieve and verify
    auto select = db->prepare("SELECT data FROM blob_test");
    select->step();
    // Just verify we can call getBlob without throwing
    auto retrieved = select->getBlob(0);
    EXPECT_FALSE(retrieved.empty());
}

TEST_F(StatementTest, IsNull) {
    // Insert NULL value
    auto stmt = db->prepare("INSERT INTO test_table (null_val) VALUES (NULL)");
    stmt->execute();

    // Insert non-NULL value
    auto stmt2 = db->prepare("INSERT INTO test_table (text_val) VALUES (?)");
    stmt2->bind(1, std::string("value"));
    stmt2->execute();

    // Check first row (NULL)
    auto select1 = db->prepare("SELECT null_val FROM test_table WHERE id = 1");
    select1->step();
    EXPECT_TRUE(select1->isNull(0));

    // Check second row (NOT NULL)
    auto select2 = db->prepare("SELECT text_val FROM test_table WHERE id = 2");
    select2->step();
    EXPECT_FALSE(select2->isNull(0));
}

TEST_F(StatementTest, GetColumnCount) {
    auto stmt = db->prepare("SELECT id, int_val, double_val FROM test_table");
    EXPECT_EQ(stmt->getColumnCount(), 3);
}

TEST_F(StatementTest, GetColumnName) {
    auto stmt = db->prepare("SELECT id, int_val, text_val FROM test_table");

    EXPECT_EQ(stmt->getColumnName(0), "id");
    EXPECT_EQ(stmt->getColumnName(1), "int_val");
    EXPECT_EQ(stmt->getColumnName(2), "text_val");
}

TEST_F(StatementTest, Reset) {
    // Insert test data
    db->execute("INSERT INTO test_table (int_val) VALUES (1)");
    db->execute("INSERT INTO test_table (int_val) VALUES (2)");

    auto stmt = db->prepare("SELECT int_val FROM test_table ORDER BY int_val");

    // First iteration
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 1);
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 2);
    EXPECT_FALSE(stmt->step());

    // Reset and iterate again
    stmt->reset();
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 1);
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 2);
    EXPECT_FALSE(stmt->step());
}

TEST_F(StatementTest, ResetChaining) {
    db->execute("INSERT INTO test_table (int_val) VALUES (10)");

    auto stmt = db->prepare("SELECT int_val FROM test_table");

    // First query
    stmt->step();
    EXPECT_EQ(stmt->getInt(0), 10);

    // Reset and query again
    stmt->reset().step();
    EXPECT_EQ(stmt->getInt(0), 10);
}

TEST_F(StatementTest, GetSql) {
    std::string sql = "SELECT * FROM test_table WHERE id = ?";
    auto stmt = db->prepare(sql);

    // The returned SQL should match (or be similar to) the original
    std::string retrieved = stmt->getSql();
    EXPECT_FALSE(retrieved.empty());
    EXPECT_NE(retrieved.find("SELECT"), std::string::npos);
}

TEST_F(StatementTest, GetHandle) {
    auto stmt = db->prepare("SELECT * FROM test_table");
    sqlite3_stmt* handle = stmt->get();
    EXPECT_NE(handle, nullptr);

    // Handle should be consistent
    EXPECT_EQ(handle, stmt->get());
}

TEST_F(StatementTest, MultipleBindsAndExecute) {
    auto stmt = db->prepare("INSERT INTO test_table (int_val) VALUES (?)");

    // Bind and execute multiple times with reset
    for (int i = 1; i <= 5; ++i) {
        stmt->bind(1, i * 10);
        stmt->execute();
        stmt->reset();
    }

    // Verify all values were inserted
    auto select = db->prepare("SELECT COUNT(*) FROM test_table");
    select->step();
    EXPECT_EQ(select->getInt(0), 5);
}

TEST_F(StatementTest, BindInvalidIndex) {
    auto stmt = db->prepare("INSERT INTO test_table (int_val) VALUES (?)");

    // Binding to invalid index should throw
    EXPECT_THROW(stmt->bind(99, 42), ValidationError);
}

TEST_F(StatementTest, GetColumnInvalidIndex) {
    db->execute("INSERT INTO test_table (id, int_val) VALUES (1, 42)");

    auto stmt = db->prepare("SELECT id, int_val FROM test_table");
    stmt->step();

    // Getting invalid column index should throw
    EXPECT_THROW(stmt->getInt(99), ValidationError);
}

TEST_F(StatementTest, EmptyResultSet) {
    auto stmt = db->prepare("SELECT * FROM test_table");

    // No rows inserted, so step should return false
    EXPECT_FALSE(stmt->step());
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
