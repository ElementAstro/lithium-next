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
 * test_query_builder.cpp
 *
 * Tests for the QueryBuilder class
 * - Select columns
 * - Where, AndWhere, OrWhere conditions
 * - Join operations with different types
 * - GroupBy and Having clauses
 * - OrderBy with ascending/descending
 * - Limit and Offset
 * - Build SQL generation
 * - BuildCount for COUNT queries
 * - Validation
 */

#include <gtest/gtest.h>

#include "database/query/query_builder.hpp"

using namespace lithium::database::query;

// ==================== QueryBuilder Tests ====================

class QueryBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(QueryBuilderTest, ConstructorWithTableName) {
    QueryBuilder builder("users");

    // Should be able to build even without columns
    std::string sql = builder.build();
    EXPECT_NE(sql.find("users"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectSingleColumn) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"name"}).build();

    EXPECT_NE(sql.find("SELECT"), std::string::npos);
    EXPECT_NE(sql.find("name"), std::string::npos);
    EXPECT_NE(sql.find("users"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectMultipleColumns) {
    QueryBuilder builder("users");

    std::string sql =
        builder.select({"id", "name", "email"}).build();

    EXPECT_NE(sql.find("SELECT"), std::string::npos);
    EXPECT_NE(sql.find("id"), std::string::npos);
    EXPECT_NE(sql.find("name"), std::string::npos);
    EXPECT_NE(sql.find("email"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectAll) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).build();

    EXPECT_NE(sql.find("SELECT *"), std::string::npos);
    EXPECT_NE(sql.find("users"), std::string::npos);
}

TEST_F(QueryBuilderTest, WhereCondition) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).where("id = 1").build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("id = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, MultipleWhereConditions) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("id = 1")
                          .where("active = 1")
                          .build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("id = 1"), std::string::npos);
    // Multiple WHERE conditions should be AND-ed
    EXPECT_NE(sql.find("active = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, AndWhere) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("id = 1")
                          .andWhere("active = 1")
                          .build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("AND"), std::string::npos);
}

TEST_F(QueryBuilderTest, OrWhere) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("status = 'active'")
                          .orWhere("status = 'pending'")
                          .build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("OR"), std::string::npos);
}

TEST_F(QueryBuilderTest, InnerJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("posts", "users.id = posts.user_id", "INNER")
                          .build();

    EXPECT_NE(sql.find("INNER JOIN"), std::string::npos);
    EXPECT_NE(sql.find("posts"), std::string::npos);
}

TEST_F(QueryBuilderTest, LeftJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("posts", "users.id = posts.user_id", "LEFT")
                          .build();

    EXPECT_NE(sql.find("LEFT JOIN"), std::string::npos);
    EXPECT_NE(sql.find("posts"), std::string::npos);
}

TEST_F(QueryBuilderTest, RightJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("posts", "users.id = posts.user_id", "RIGHT")
                          .build();

    EXPECT_NE(sql.find("RIGHT JOIN"), std::string::npos);
    EXPECT_NE(sql.find("posts"), std::string::npos);
}

TEST_F(QueryBuilderTest, DefaultJoinType) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("posts", "users.id = posts.user_id")
                          .build();

    // Default is INNER JOIN
    EXPECT_NE(sql.find("JOIN"), std::string::npos);
}

TEST_F(QueryBuilderTest, MultipleJoins) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("posts", "users.id = posts.user_id")
                          .join("comments", "posts.id = comments.post_id")
                          .build();

    EXPECT_NE(sql.find("JOIN"), std::string::npos);
    // Should have both tables
    EXPECT_NE(sql.find("posts"), std::string::npos);
    EXPECT_NE(sql.find("comments"), std::string::npos);
}

TEST_F(QueryBuilderTest, GroupBySingleColumn) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"user_id", "COUNT(*)"})
                          .groupBy({"user_id"})
                          .build();

    EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(sql.find("user_id"), std::string::npos);
}

TEST_F(QueryBuilderTest, GroupByMultipleColumns) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"user_id", "status", "COUNT(*)"})
                          .groupBy({"user_id", "status"})
                          .build();

    EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(sql.find("user_id"), std::string::npos);
    EXPECT_NE(sql.find("status"), std::string::npos);
}

TEST_F(QueryBuilderTest, Having) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"user_id", "COUNT(*) as count"})
                          .groupBy({"user_id"})
                          .having("COUNT(*) > 5")
                          .build();

    EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(sql.find("HAVING"), std::string::npos);
    EXPECT_NE(sql.find("COUNT(*) > 5"), std::string::npos);
}

TEST_F(QueryBuilderTest, OrderByAscending) {
    QueryBuilder builder("users");

    std::string sql =
        builder.select({"*"}).orderBy("name", true).build();

    EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(sql.find("name"), std::string::npos);
    EXPECT_NE(sql.find("ASC"), std::string::npos);
}

TEST_F(QueryBuilderTest, OrderByDescending) {
    QueryBuilder builder("users");

    std::string sql =
        builder.select({"*"}).orderBy("created_at", false).build();

    EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(sql.find("created_at"), std::string::npos);
    EXPECT_NE(sql.find("DESC"), std::string::npos);
}

TEST_F(QueryBuilderTest, MultipleOrderBy) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .orderBy("status", true)
                          .orderBy("created_at", false)
                          .build();

    EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
}

TEST_F(QueryBuilderTest, Limit) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).limit(10).build();

    EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
}

TEST_F(QueryBuilderTest, LimitAndOffset) {
    QueryBuilder builder("users");

    std::string sql =
        builder.select({"*"}).limit(10).offset(20).build();

    EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
    EXPECT_NE(sql.find("OFFSET 20"), std::string::npos);
}

TEST_F(QueryBuilderTest, Offset) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).offset(5).build();

    EXPECT_NE(sql.find("OFFSET 5"), std::string::npos);
}

TEST_F(QueryBuilderTest, ComplexQuery) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"users.name", "orders.id", "SUM(orders.total)"})
                          .join("users", "orders.user_id = users.id")
                          .where("orders.status = 'completed'")
                          .groupBy({"users.id", "users.name"})
                          .having("SUM(orders.total) > 1000")
                          .orderBy("SUM(orders.total)", false)
                          .limit(50)
                          .offset(0)
                          .build();

    // Verify all components are in the SQL
    EXPECT_NE(sql.find("SELECT"), std::string::npos);
    EXPECT_NE(sql.find("FROM"), std::string::npos);
    EXPECT_NE(sql.find("JOIN"), std::string::npos);
    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("GROUP BY"), std::string::npos);
    EXPECT_NE(sql.find("HAVING"), std::string::npos);
    EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(sql.find("LIMIT"), std::string::npos);
}

TEST_F(QueryBuilderTest, BuildCount) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).where("active = 1").buildCount();

    EXPECT_NE(sql.find("COUNT"), std::string::npos);
    EXPECT_NE(sql.find("users"), std::string::npos);
    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("active = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, BuildCountWithoutWhere) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).buildCount();

    EXPECT_NE(sql.find("COUNT"), std::string::npos);
    EXPECT_NE(sql.find("users"), std::string::npos);
}

TEST_F(QueryBuilderTest, BuildCountWithGroupBy) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"user_id"})
                          .groupBy({"user_id"})
                          .buildCount();

    EXPECT_NE(sql.find("COUNT"), std::string::npos);
}

TEST_F(QueryBuilderTest, Validate) {
    QueryBuilder builder("users");

    // Valid query should not throw
    EXPECT_NO_THROW(builder.validate());

    // Empty table name might throw on validate
    // (depends on implementation details)
}

TEST_F(QueryBuilderTest, ValidateEmptyTableName) {
    // Creating with empty table name might fail
    // This depends on implementation
    QueryBuilder builder("");

    EXPECT_THROW(builder.validate(), ValidationError);
}

TEST_F(QueryBuilderTest, FluentInterface) {
    QueryBuilder builder("users");

    std::string sql = builder
                          .select({"id", "name"})
                          .where("status = 'active'")
                          .orderBy("created_at", false)
                          .limit(100)
                          .build();

    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("SELECT"), std::string::npos);
}

TEST_F(QueryBuilderTest, BuildWithoutSelect) {
    QueryBuilder builder("users");

    // Should still build, but might not have explicit column selection
    std::string sql = builder.build();

    EXPECT_NE(sql.find("users"), std::string::npos);
}

TEST_F(QueryBuilderTest, WhereWithSpecialCharacters) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("email LIKE '%@example.com'")
                          .build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("LIKE"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectWithAlias) {
    QueryBuilder builder("users");

    std::string sql = builder
                          .select({"id", "name as user_name", "email"})
                          .build();

    EXPECT_NE(sql.find("user_name"), std::string::npos);
}

TEST_F(QueryBuilderTest, LimitZero) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"}).limit(0).build();

    // Limit 0 should still be included
    EXPECT_NE(sql.find("LIMIT"), std::string::npos);
}

TEST_F(QueryBuilderTest, NegativeLimit) {
    QueryBuilder builder("users");

    // Negative limit might be handled differently
    std::string sql = builder.select({"*"}).limit(-1).build();

    // -1 typically means no limit in SQL
    EXPECT_NE(sql.find("LIMIT"), std::string::npos);
}

TEST_F(QueryBuilderTest, ComplexWhereConditions) {
    QueryBuilder builder("products");

    std::string sql = builder.select({"*"})
                          .where("price > 100")
                          .andWhere("category = 'electronics'")
                          .andWhere("in_stock = 1")
                          .build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("AND"), std::string::npos);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
