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

#include "database/core/types.hpp"
#include "database/query/query_builder.hpp"

using namespace lithium::database::query;
using namespace lithium::database::core;

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

TEST_F(QueryBuilderTest, OffsetWithoutLimitThrows) {
    QueryBuilder builder("users");

    // OFFSET without LIMIT should throw ValidationError
    builder.select({"*"}).offset(5);
    EXPECT_THROW(builder.build(), ValidationError);
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

    // Negative limit is ignored (not set)
    std::string sql = builder.select({"*"}).limit(-1).build();

    // -1 means no limit - LIMIT should NOT appear in SQL
    EXPECT_EQ(sql.find("LIMIT"), std::string::npos);
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

TEST_F(QueryBuilderTest, WhereWithIntParameter) {
    QueryBuilder builder("users");

    builder.select({"*"}).where("id > ?", 10);

    EXPECT_EQ(builder.getParamCount(), 1);
    auto params = builder.getParamValues();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(std::get<int>(params[0]), 10);
}

TEST_F(QueryBuilderTest, WhereWithDoubleParameter) {
    QueryBuilder builder("products");

    builder.select({"*"}).where("price > ?", 99.99);

    EXPECT_EQ(builder.getParamCount(), 1);
    auto params = builder.getParamValues();
    EXPECT_EQ(params.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(params[0]), 99.99);
}

TEST_F(QueryBuilderTest, WhereWithStringParameter) {
    QueryBuilder builder("users");

    builder.select({"*"}).where("name = ?", std::string("Alice"));

    EXPECT_EQ(builder.getParamCount(), 1);
    auto params = builder.getParamValues();
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(std::get<std::string>(params[0]), "Alice");
}

TEST_F(QueryBuilderTest, WhereWithMultipleParameters) {
    QueryBuilder builder("users");

    builder.select({"*"})
        .where("age > ?", 18)
        .where("score > ?", 85.5);

    EXPECT_EQ(builder.getParamCount(), 2);
    auto params = builder.getParamValues();
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(std::get<int>(params[0]), 18);
    EXPECT_DOUBLE_EQ(std::get<double>(params[1]), 85.5);
}

TEST_F(QueryBuilderTest, GetParamCountEmpty) {
    QueryBuilder builder("users");

    builder.select({"*"});

    EXPECT_EQ(builder.getParamCount(), 0);
    EXPECT_TRUE(builder.getParamValues().empty());
}

TEST_F(QueryBuilderTest, AndWhereFirstCondition) {
    QueryBuilder builder("users");

    // andWhere as first condition should work like where
    std::string sql = builder.select({"*"}).andWhere("active = 1").build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("active = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, OrWhereFirstCondition) {
    QueryBuilder builder("users");

    // orWhere as first condition should work like where
    std::string sql = builder.select({"*"}).orWhere("active = 1").build();

    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("active = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, EmptyWhereConditionIgnored) {
    QueryBuilder builder("users");

    // Empty condition should be ignored
    std::string sql = builder.select({"*"}).where("").build();

    EXPECT_EQ(sql.find("WHERE"), std::string::npos);
}

TEST_F(QueryBuilderTest, EmptyAndWhereConditionIgnored) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("id = 1")
                          .andWhere("")
                          .build();

    // Should have WHERE but no extra AND
    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("id = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, EmptyOrWhereConditionIgnored) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("id = 1")
                          .orWhere("")
                          .build();

    // Should have WHERE but no extra OR
    EXPECT_NE(sql.find("WHERE"), std::string::npos);
    EXPECT_NE(sql.find("id = 1"), std::string::npos);
}

TEST_F(QueryBuilderTest, EmptySelectColumns) {
    QueryBuilder builder("users");

    // Empty columns vector should be ignored (keep default *)
    std::string sql = builder.select({}).build();

    EXPECT_NE(sql.find("SELECT *"), std::string::npos);
}

TEST_F(QueryBuilderTest, JoinWithDefaultType) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("orders", "users.id = orders.user_id")
                          .build();

    // Default join type is INNER
    EXPECT_NE(sql.find("INNER JOIN"), std::string::npos);
}

TEST_F(QueryBuilderTest, FullOuterJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("orders", "users.id = orders.user_id", "FULL OUTER")
                          .build();

    EXPECT_NE(sql.find("FULL OUTER JOIN"), std::string::npos);
}

TEST_F(QueryBuilderTest, CrossJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("roles", "1=1", "CROSS")
                          .build();

    EXPECT_NE(sql.find("CROSS JOIN"), std::string::npos);
}

TEST_F(QueryBuilderTest, BuildCountWithJoin) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .join("orders", "users.id = orders.user_id")
                          .where("orders.status = 'active'")
                          .buildCount();

    EXPECT_NE(sql.find("COUNT"), std::string::npos);
    EXPECT_NE(sql.find("JOIN"), std::string::npos);
    EXPECT_NE(sql.find("WHERE"), std::string::npos);
}

TEST_F(QueryBuilderTest, OrderByDefaultAscending) {
    QueryBuilder builder("users");

    // Default is ascending
    std::string sql = builder.select({"*"}).orderBy("name").build();

    EXPECT_NE(sql.find("ORDER BY"), std::string::npos);
    EXPECT_NE(sql.find("name ASC"), std::string::npos);
}

TEST_F(QueryBuilderTest, GroupByEmpty) {
    QueryBuilder builder("users");

    // Empty groupBy should not add GROUP BY clause
    std::string sql = builder.select({"*"}).groupBy({}).build();

    EXPECT_EQ(sql.find("GROUP BY"), std::string::npos);
}

TEST_F(QueryBuilderTest, HavingWithoutGroupBy) {
    QueryBuilder builder("users");

    // Having without GroupBy (unusual but valid SQL)
    std::string sql = builder.select({"COUNT(*)"})
                          .having("COUNT(*) > 5")
                          .build();

    EXPECT_NE(sql.find("HAVING"), std::string::npos);
}

TEST_F(QueryBuilderTest, NegativeOffset) {
    QueryBuilder builder("users");

    // Negative offset should be ignored
    std::string sql = builder.select({"*"}).limit(10).offset(-5).build();

    EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
    EXPECT_EQ(sql.find("OFFSET"), std::string::npos);
}

TEST_F(QueryBuilderTest, ZeroOffset) {
    QueryBuilder builder("users");

    // Zero offset should not add OFFSET clause
    std::string sql = builder.select({"*"}).limit(10).offset(0).build();

    EXPECT_NE(sql.find("LIMIT 10"), std::string::npos);
    EXPECT_EQ(sql.find("OFFSET"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectWithAggregates) {
    QueryBuilder builder("orders");

    std::string sql = builder.select({"user_id", "COUNT(*)", "SUM(total)", "AVG(total)"})
                          .groupBy({"user_id"})
                          .build();

    EXPECT_NE(sql.find("COUNT(*)"), std::string::npos);
    EXPECT_NE(sql.find("SUM(total)"), std::string::npos);
    EXPECT_NE(sql.find("AVG(total)"), std::string::npos);
}

TEST_F(QueryBuilderTest, SelectDistinct) {
    QueryBuilder builder("users");

    // DISTINCT can be part of column selection
    std::string sql = builder.select({"DISTINCT name"}).build();

    EXPECT_NE(sql.find("DISTINCT"), std::string::npos);
}

TEST_F(QueryBuilderTest, SubqueryInWhere) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("id IN (SELECT user_id FROM orders)")
                          .build();

    EXPECT_NE(sql.find("IN (SELECT"), std::string::npos);
}

TEST_F(QueryBuilderTest, LikeCondition) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("name LIKE '%test%'")
                          .build();

    EXPECT_NE(sql.find("LIKE"), std::string::npos);
}

TEST_F(QueryBuilderTest, BetweenCondition) {
    QueryBuilder builder("products");

    std::string sql = builder.select({"*"})
                          .where("price BETWEEN 10 AND 100")
                          .build();

    EXPECT_NE(sql.find("BETWEEN"), std::string::npos);
}

TEST_F(QueryBuilderTest, IsNullCondition) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("deleted_at IS NULL")
                          .build();

    EXPECT_NE(sql.find("IS NULL"), std::string::npos);
}

TEST_F(QueryBuilderTest, IsNotNullCondition) {
    QueryBuilder builder("users");

    std::string sql = builder.select({"*"})
                          .where("email IS NOT NULL")
                          .build();

    EXPECT_NE(sql.find("IS NOT NULL"), std::string::npos);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
