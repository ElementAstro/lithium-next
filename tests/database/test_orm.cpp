#ifndef LITHIUM_DATABASE_TEST_ORM_HPP
#define LITHIUM_DATABASE_TEST_ORM_HPP

#include <gtest/gtest.h>
#include "database/orm.hpp"

namespace lithium::database::test {

// Test model class
class TestModel {
public:
    int id;
    std::string name;
    bool active;
    double score;

    static std::string tableName() { return "test_table"; }

    static std::vector<std::shared_ptr<ColumnBase>> columns() {
        static std::vector<std::shared_ptr<ColumnBase>> cols = {
            std::make_shared<Column<int, TestModel>>("id", &TestModel::id,
                                                     "INTEGER PRIMARY KEY"),
            std::make_shared<Column<std::string, TestModel>>("name",
                                                             &TestModel::name),
            std::make_shared<Column<bool, TestModel>>("active",
                                                      &TestModel::active),
            std::make_shared<Column<double, TestModel>>("score",
                                                        &TestModel::score)};
        return cols;
    }
};

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override { db = std::make_unique<Database>(":memory:"); }

    void TearDown() override { db.reset(); }

    std::unique_ptr<Database> db;
};

TEST_F(DatabaseTest, OpenCloseDatabase) {
    EXPECT_NO_THROW(Database(":memory:"));
    EXPECT_THROW(Database("/invalid/path/db.sqlite"), FailedToOpenDatabase);
}

TEST_F(DatabaseTest, TransactionOperations) {
    EXPECT_NO_THROW(db->beginTransaction());
    EXPECT_NO_THROW(db->commit());
    EXPECT_NO_THROW(db->beginTransaction());
    EXPECT_NO_THROW(db->rollback());
}

class ColumnTest : public ::testing::Test {
protected:
    TestModel model;
};

TEST_F(ColumnTest, ColumnTypeConversion) {
    Column<int, TestModel> intCol("test_int", &TestModel::id);
    Column<std::string, TestModel> strCol("test_str", &TestModel::name);
    Column<bool, TestModel> boolCol("test_bool", &TestModel::active);
    Column<double, TestModel> doubleCol("test_double", &TestModel::score);

    EXPECT_EQ(intCol.getType(), "INTEGER");
    EXPECT_EQ(strCol.getType(), "TEXT");
    EXPECT_EQ(boolCol.getType(), "BOOLEAN");
    EXPECT_EQ(doubleCol.getType(), "REAL");
}

TEST_F(ColumnTest, SQLValueConversion) {
    model.id = 1;
    model.name = "test";
    model.active = true;
    model.score = 3.14;

    Column<int, TestModel> intCol("test_int", &TestModel::id);
    Column<std::string, TestModel> strCol("test_str", &TestModel::name);
    Column<bool, TestModel> boolCol("test_bool", &TestModel::active);
    Column<double, TestModel> doubleCol("test_double", &TestModel::score);

    EXPECT_EQ(intCol.toSQLValue(&model), "1");
    EXPECT_EQ(strCol.toSQLValue(&model), "'test'");
    EXPECT_EQ(boolCol.toSQLValue(&model), "1");
    EXPECT_EQ(doubleCol.toSQLValue(&model), "3.14");
}

class TableTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<Database>(":memory:");
        table = std::make_unique<Table<TestModel>>(*db);
        table->createTable();
    }

    void TearDown() override {
        table.reset();
        db.reset();
    }

    std::unique_ptr<Database> db;
    std::unique_ptr<Table<TestModel>> table;
    TestModel model{1, "test", true, 3.14};
};

TEST_F(TableTest, CreateTable) { EXPECT_NO_THROW(table->createTable()); }

TEST_F(TableTest, InsertRecord) { EXPECT_NO_THROW(table->insert(model)); }

TEST_F(TableTest, UpdateRecord) {
    table->insert(model);
    model.name = "updated";
    EXPECT_NO_THROW(table->update(model, "id = 1"));
}

TEST_F(TableTest, DeleteRecord) {
    table->insert(model);
    EXPECT_NO_THROW(table->remove("id = 1"));
}

TEST_F(TableTest, QueryRecords) {
    table->insert(model);
    auto results = table->query("id = 1");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].name, "test");
}

TEST_F(TableTest, BatchOperations) {
    std::vector<TestModel> models = {{1, "test1", true, 1.1},
                                     {2, "test2", false, 2.2}};
    EXPECT_NO_THROW(table->batchInsert(models));
}

class QueryBuilderTest : public ::testing::Test {
protected:
    QueryBuilder builder;
};

TEST_F(QueryBuilderTest, BuildSimpleQuery) {
    std::string query = builder.select({"name", "score"})
                            .where("active = 1")
                            .orderBy("score", false)
                            .limit(10)
                            .build();
    EXPECT_TRUE(query.find("SELECT name, score") != std::string::npos);
    EXPECT_TRUE(query.find("WHERE active = 1") != std::string::npos);
    EXPECT_TRUE(query.find("ORDER BY score DESC") != std::string::npos);
    EXPECT_TRUE(query.find("LIMIT 10") != std::string::npos);
}

class CacheManagerTest : public ::testing::Test {
protected:
    CacheManager& cache = CacheManager::getInstance();
};

TEST_F(CacheManagerTest, PutAndGet) {
    cache.put("test_key", "test_value");
    std::string value;
    EXPECT_TRUE(cache.get("test_key", value));
    EXPECT_EQ(value, "test_value");
}

TEST_F(CacheManagerTest, CacheExpiration) {
    cache.put("test_key", "test_value");
    std::this_thread::sleep_for(std::chrono::minutes(6));
    std::string value;
    EXPECT_FALSE(cache.get("test_key", value));
}

TEST_F(CacheManagerTest, NonExistentKey) {
    std::string value;
    EXPECT_FALSE(cache.get("non_existent", value));
}

}  // namespace lithium::database::test

#endif  // LITHIUM_DATABASE_TEST_ORM_HPP
