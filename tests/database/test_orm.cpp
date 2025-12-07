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
 * test_orm.cpp
 *
 * Tests for the ORM (Object-Relational Mapping) components
 * - Column type detection and conversion
 * - ColumnValue SQL value conversion
 * - ColumnBase interface
 * - Table CRUD operations
 * - Batch operations
 * - Index creation
 * - Count and exists queries
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "database/core/database.hpp"
#include "database/core/statement.hpp"
#include "database/core/types.hpp"
#include "database/orm/column.hpp"
#include "database/orm/column_base.hpp"
#include "database/orm/column_value.hpp"
#include "database/orm/table.hpp"

using namespace lithium::database::core;
using namespace lithium::database::orm;

// ==================== Test Model ====================

// Test model class for ORM testing
class TestModel {
public:
    int id = 0;
    std::string name;
    bool active = false;
    double score = 0.0;

    static std::string tableName() { return "test_models"; }

    static std::vector<std::shared_ptr<ColumnBase>> columns() {
        static std::vector<std::shared_ptr<ColumnBase>> cols = {
            std::make_shared<Column<int, TestModel>>(
                "id", &TestModel::id, "INTEGER", "PRIMARY KEY"),
            std::make_shared<Column<std::string, TestModel>>(
                "name", &TestModel::name, "TEXT", "NOT NULL"),
            std::make_shared<Column<bool, TestModel>>(
                "active", &TestModel::active, "BOOLEAN", ""),
            std::make_shared<Column<double, TestModel>>(
                "score", &TestModel::score, "REAL", "")};
        return cols;
    }
};

// ==================== Column Tests ====================

class ColumnTest : public ::testing::Test {
protected:
    TestModel model;
};

TEST_F(ColumnTest, ColumnGetName) {
    Column<int, TestModel> col("test_column", &TestModel::id);
    EXPECT_EQ(col.getName(), "test_column");
}

TEST_F(ColumnTest, ColumnGetTypeInteger) {
    Column<int, TestModel> col("id", &TestModel::id);
    EXPECT_EQ(col.getType(), "INTEGER");
}

TEST_F(ColumnTest, ColumnGetTypeText) {
    Column<std::string, TestModel> col("name", &TestModel::name);
    EXPECT_EQ(col.getType(), "TEXT");
}

TEST_F(ColumnTest, ColumnGetTypeBoolean) {
    Column<bool, TestModel> col("active", &TestModel::active);
    EXPECT_EQ(col.getType(), "BOOLEAN");
}

TEST_F(ColumnTest, ColumnGetTypeReal) {
    Column<double, TestModel> col("score", &TestModel::score);
    EXPECT_EQ(col.getType(), "REAL");
}

TEST_F(ColumnTest, ColumnGetTypeFloat) {
    // Float should also return REAL
    struct FloatModel {
        float value;
    };
    Column<float, FloatModel> col("value", &FloatModel::value);
    EXPECT_EQ(col.getType(), "REAL");
}

TEST_F(ColumnTest, ColumnCustomType) {
    Column<int, TestModel> col("id", &TestModel::id, "BIGINT");
    EXPECT_EQ(col.getType(), "BIGINT");
}

TEST_F(ColumnTest, ColumnGetConstraints) {
    Column<int, TestModel> col("id", &TestModel::id, "INTEGER", "PRIMARY KEY");
    EXPECT_EQ(col.getConstraints(), "PRIMARY KEY");
}

TEST_F(ColumnTest, ColumnToSQLValueInt) {
    model.id = 42;
    Column<int, TestModel> col("id", &TestModel::id);
    EXPECT_EQ(col.toSQLValue(&model), "42");
}

TEST_F(ColumnTest, ColumnToSQLValueString) {
    model.name = "test_name";
    Column<std::string, TestModel> col("name", &TestModel::name);
    EXPECT_EQ(col.toSQLValue(&model), "'test_name'");
}

TEST_F(ColumnTest, ColumnToSQLValueStringWithQuote) {
    model.name = "test's name";
    Column<std::string, TestModel> col("name", &TestModel::name);
    // Single quotes should be escaped
    EXPECT_EQ(col.toSQLValue(&model), "'test''s name'");
}

TEST_F(ColumnTest, ColumnToSQLValueBoolTrue) {
    model.active = true;
    Column<bool, TestModel> col("active", &TestModel::active);
    EXPECT_EQ(col.toSQLValue(&model), "1");
}

TEST_F(ColumnTest, ColumnToSQLValueBoolFalse) {
    model.active = false;
    Column<bool, TestModel> col("active", &TestModel::active);
    EXPECT_EQ(col.toSQLValue(&model), "0");
}

TEST_F(ColumnTest, ColumnToSQLValueDouble) {
    model.score = 3.14159;
    Column<double, TestModel> col("score", &TestModel::score);
    std::string result = col.toSQLValue(&model);
    EXPECT_NE(result.find("3.14"), std::string::npos);
}

TEST_F(ColumnTest, ColumnFromSQLValueInt) {
    Column<int, TestModel> col("id", &TestModel::id);
    col.fromSQLValue(&model, "123");
    EXPECT_EQ(model.id, 123);
}

TEST_F(ColumnTest, ColumnFromSQLValueString) {
    Column<std::string, TestModel> col("name", &TestModel::name);
    col.fromSQLValue(&model, "hello");
    EXPECT_EQ(model.name, "hello");
}

TEST_F(ColumnTest, ColumnFromSQLValueBool) {
    Column<bool, TestModel> col("active", &TestModel::active);
    col.fromSQLValue(&model, "1");
    EXPECT_TRUE(model.active);
}

TEST_F(ColumnTest, ColumnFromSQLValueDouble) {
    Column<double, TestModel> col("score", &TestModel::score);
    col.fromSQLValue(&model, "2.718");
    EXPECT_DOUBLE_EQ(model.score, 2.718);
}

// ==================== ColumnValue Tests ====================

class ColumnValueTest : public ::testing::Test {};

TEST_F(ColumnValueTest, ToSQLValueInt) {
    EXPECT_EQ(ColumnValue<int>::toSQLValue(42), "42");
}

TEST_F(ColumnValueTest, ToSQLValueNegativeInt) {
    EXPECT_EQ(ColumnValue<int>::toSQLValue(-100), "-100");
}

TEST_F(ColumnValueTest, ToSQLValueDouble) {
    std::string result = ColumnValue<double>::toSQLValue(3.14);
    EXPECT_NE(result.find("3.14"), std::string::npos);
}

TEST_F(ColumnValueTest, ToSQLValueString) {
    EXPECT_EQ(ColumnValue<std::string>::toSQLValue("hello"), "'hello'");
}

TEST_F(ColumnValueTest, ToSQLValueEmptyString) {
    EXPECT_EQ(ColumnValue<std::string>::toSQLValue(""), "''");
}

TEST_F(ColumnValueTest, ToSQLValueBoolTrue) {
    EXPECT_EQ(ColumnValue<bool>::toSQLValue(true), "1");
}

TEST_F(ColumnValueTest, ToSQLValueBoolFalse) {
    EXPECT_EQ(ColumnValue<bool>::toSQLValue(false), "0");
}

TEST_F(ColumnValueTest, FromSQLValueInt) {
    EXPECT_EQ(ColumnValue<int>::fromSQLValue("42"), 42);
}

TEST_F(ColumnValueTest, FromSQLValueDouble) {
    EXPECT_DOUBLE_EQ(ColumnValue<double>::fromSQLValue("3.14"), 3.14);
}

TEST_F(ColumnValueTest, FromSQLValueString) {
    EXPECT_EQ(ColumnValue<std::string>::fromSQLValue("hello"), "hello");
}

// ==================== Table Tests ====================

class TableTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<Database>(":memory:");
        table = std::make_unique<Table<TestModel>>(*db);
    }

    void TearDown() override {
        table.reset();
        db.reset();
    }

    std::unique_ptr<Database> db;
    std::unique_ptr<Table<TestModel>> table;
};

TEST_F(TableTest, TableName) {
    EXPECT_EQ(Table<TestModel>::tableName(), "test_models");
}

TEST_F(TableTest, CreateTable) {
    EXPECT_NO_THROW(table->createTable());

    // Verify table exists by inserting data
    EXPECT_NO_THROW(db->execute(
        "INSERT INTO test_models (id, name, active, score) VALUES (1, 'test', 1, 1.0)"));
}

TEST_F(TableTest, CreateTableIfNotExists) {
    // Create twice should not throw
    EXPECT_NO_THROW(table->createTable(true));
    EXPECT_NO_THROW(table->createTable(true));
}

TEST_F(TableTest, InsertRecord) {
    table->createTable();

    TestModel model;
    model.id = 1;
    model.name = "Alice";
    model.active = true;
    model.score = 95.5;

    EXPECT_NO_THROW(table->insert(model));

    // Verify insertion
    auto results = table->query("id = 1");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Alice");
    EXPECT_TRUE(results[0].active);
    EXPECT_DOUBLE_EQ(results[0].score, 95.5);
}

TEST_F(TableTest, UpdateRecord) {
    table->createTable();

    TestModel model;
    model.id = 1;
    model.name = "Bob";
    model.active = false;
    model.score = 80.0;

    table->insert(model);

    // Update
    model.name = "Bob Updated";
    model.score = 85.0;
    EXPECT_NO_THROW(table->update(model, "id = 1"));

    // Verify update
    auto results = table->query("id = 1");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "Bob Updated");
    EXPECT_DOUBLE_EQ(results[0].score, 85.0);
}

TEST_F(TableTest, RemoveRecord) {
    table->createTable();

    TestModel model;
    model.id = 1;
    model.name = "Charlie";
    model.active = true;
    model.score = 70.0;

    table->insert(model);

    // Verify exists
    EXPECT_EQ(table->count(), 1);

    // Remove
    EXPECT_NO_THROW(table->remove("id = 1"));

    // Verify removed
    EXPECT_EQ(table->count(), 0);
}

TEST_F(TableTest, RemoveWithEmptyConditionThrows) {
    table->createTable();

    // Empty condition should throw
    EXPECT_THROW(table->remove(""), ValidationError);
}

TEST_F(TableTest, QueryAll) {
    table->createTable();

    // Insert multiple records
    for (int i = 1; i <= 5; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = (i % 2 == 0);
        model.score = i * 10.0;
        table->insert(model);
    }

    // Query all
    auto results = table->query();
    EXPECT_EQ(results.size(), 5);
}

TEST_F(TableTest, QueryWithCondition) {
    table->createTable();

    for (int i = 1; i <= 5; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = (i % 2 == 0);
        model.score = i * 10.0;
        table->insert(model);
    }

    // Query active users only
    auto results = table->query("active = 1");
    EXPECT_EQ(results.size(), 2);  // Users 2 and 4
}

TEST_F(TableTest, QueryWithLimit) {
    table->createTable();

    for (int i = 1; i <= 10; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = true;
        model.score = i * 10.0;
        table->insert(model);
    }

    // Query with limit
    auto results = table->query("", 3);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(TableTest, QueryWithLimitAndOffset) {
    table->createTable();

    for (int i = 1; i <= 10; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = true;
        model.score = i * 10.0;
        table->insert(model);
    }

    // Query with limit and offset
    auto results = table->query("", 3, 5);
    EXPECT_EQ(results.size(), 3);
}

TEST_F(TableTest, BatchInsert) {
    table->createTable();

    std::vector<TestModel> models;
    for (int i = 1; i <= 100; ++i) {
        TestModel model;
        model.id = i;
        model.name = "BatchUser" + std::to_string(i);
        model.active = true;
        model.score = i * 1.5;
        models.push_back(model);
    }

    EXPECT_NO_THROW(table->batchInsert(models));

    // Verify all inserted
    EXPECT_EQ(table->count(), 100);
}

TEST_F(TableTest, BatchInsertEmpty) {
    table->createTable();

    std::vector<TestModel> emptyModels;
    EXPECT_NO_THROW(table->batchInsert(emptyModels));

    EXPECT_EQ(table->count(), 0);
}

TEST_F(TableTest, BatchInsertWithChunkSize) {
    table->createTable();

    std::vector<TestModel> models;
    for (int i = 1; i <= 50; ++i) {
        TestModel model;
        model.id = i;
        model.name = "ChunkUser" + std::to_string(i);
        model.active = true;
        model.score = i * 2.0;
        models.push_back(model);
    }

    // Insert with small chunk size
    EXPECT_NO_THROW(table->batchInsert(models, 10));

    EXPECT_EQ(table->count(), 50);
}

TEST_F(TableTest, BatchUpdate) {
    table->createTable();

    // Insert initial data
    for (int i = 1; i <= 5; ++i) {
        TestModel model;
        model.id = i;
        model.name = "Original" + std::to_string(i);
        model.active = false;
        model.score = 0.0;
        table->insert(model);
    }

    // Prepare updated models
    std::vector<TestModel> updatedModels;
    for (int i = 1; i <= 5; ++i) {
        TestModel model;
        model.id = i;
        model.name = "Updated" + std::to_string(i);
        model.active = true;
        model.score = i * 100.0;
        updatedModels.push_back(model);
    }

    // Batch update
    EXPECT_NO_THROW(table->batchUpdate(
        updatedModels,
        [](const TestModel& m) { return "id = " + std::to_string(m.id); }));

    // Verify updates
    auto results = table->query();
    for (const auto& r : results) {
        EXPECT_TRUE(r.name.find("Updated") != std::string::npos);
        EXPECT_TRUE(r.active);
    }
}

TEST_F(TableTest, CreateIndex) {
    table->createTable();

    EXPECT_NO_THROW(table->createIndex("idx_name", {"name"}));
}

TEST_F(TableTest, CreateUniqueIndex) {
    table->createTable();

    EXPECT_NO_THROW(table->createIndex("idx_name_unique", {"name"}, true));
}

TEST_F(TableTest, CreateCompositeIndex) {
    table->createTable();

    EXPECT_NO_THROW(table->createIndex("idx_composite", {"name", "active"}));
}

TEST_F(TableTest, CreateIndexEmptyColumnsThrows) {
    table->createTable();

    EXPECT_THROW(table->createIndex("idx_empty", {}), ValidationError);
}

TEST_F(TableTest, Count) {
    table->createTable();

    EXPECT_EQ(table->count(), 0);

    for (int i = 1; i <= 5; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = true;
        model.score = 0.0;
        table->insert(model);
    }

    EXPECT_EQ(table->count(), 5);
}

TEST_F(TableTest, CountWithCondition) {
    table->createTable();

    for (int i = 1; i <= 10; ++i) {
        TestModel model;
        model.id = i;
        model.name = "User" + std::to_string(i);
        model.active = (i % 2 == 0);
        model.score = 0.0;
        table->insert(model);
    }

    EXPECT_EQ(table->count("active = 1"), 5);
    EXPECT_EQ(table->count("active = 0"), 5);
}

TEST_F(TableTest, Exists) {
    table->createTable();

    TestModel model;
    model.id = 1;
    model.name = "ExistingUser";
    model.active = true;
    model.score = 0.0;
    table->insert(model);

    EXPECT_TRUE(table->exists("id = 1"));
    EXPECT_TRUE(table->exists("name = 'ExistingUser'"));
    EXPECT_FALSE(table->exists("id = 999"));
    EXPECT_FALSE(table->exists("name = 'NonExistent'"));
}

TEST_F(TableTest, QueryAsync) {
    table->createTable();

    for (int i = 1; i <= 10; ++i) {
        TestModel model;
        model.id = i;
        model.name = "AsyncUser" + std::to_string(i);
        model.active = true;
        model.score = i * 5.0;
        table->insert(model);
    }

    // Query asynchronously
    auto future = table->queryAsync("", 5);

    // Get results
    auto results = future.get();
    EXPECT_EQ(results.size(), 5);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
