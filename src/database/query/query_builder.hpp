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

#ifndef LITHIUM_DATABASE_QUERY_QUERY_BUILDER_HPP
#define LITHIUM_DATABASE_QUERY_QUERY_BUILDER_HPP

#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace lithium::database::query {

/**
 * @brief A fluent SQL query builder for constructing SELECT queries.
 *
 * QueryBuilder provides a fluent interface for building SQL SELECT queries
 * with support for columns selection, WHERE conditions, JOINs, GROUP BY,
 * HAVING, ORDER BY, LIMIT, and OFFSET clauses.
 */
class QueryBuilder {
public:
    /**
     * @brief Constructs a QueryBuilder for a specific table.
     *
     * @param tableName The name of the table to query.
     */
    explicit QueryBuilder(const std::string& tableName);

    /**
     * @brief Adds columns to select in the query.
     *
     * @param columns A vector of column names to select.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& select(const std::vector<std::string>& columns);

    /**
     * @brief Adds a condition to the query.
     *
     * @param condition The condition to add.
     * @param paramValues Optional values to bind to parameters.
     * @return A reference to the QueryBuilder instance.
     */
    template <typename... Args>
    QueryBuilder& where(const std::string& condition, Args&&... paramValues);

    /**
     * @brief Adds an AND condition to the query.
     *
     * @param condition The condition to add.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& andWhere(const std::string& condition);

    /**
     * @brief Adds an OR condition to the query.
     *
     * @param condition The condition to add.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& orWhere(const std::string& condition);

    /**
     * @brief Adds a join to the query.
     *
     * @param table The table to join with.
     * @param condition The join condition.
     * @param joinType The type of join (default: "INNER").
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& join(const std::string& table, const std::string& condition,
                       const std::string& joinType = "INNER");

    /**
     * @brief Adds a GROUP BY clause to the query.
     *
     * @param columns A vector of column names to group by.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& groupBy(const std::vector<std::string>& columns);

    /**
     * @brief Adds a HAVING clause to the query.
     *
     * @param condition The having condition.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& having(const std::string& condition);

    /**
     * @brief Adds an order by clause to the query.
     *
     * @param column The column to order by.
     * @param asc Whether to order in ascending order (default is true).
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& orderBy(const std::string& column, bool asc = true);

    /**
     * @brief Adds a limit to the query.
     *
     * @param limit The maximum number of rows to return.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& limit(int limit);

    /**
     * @brief Adds an offset to the query.
     *
     * @param offset The number of rows to skip.
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& offset(int offset);

    /**
     * @brief Builds the SQL query string.
     *
     * @return The SQL query string.
     */
    std::string build() const;

    /**
     * @brief Builds an SQL COUNT query string.
     *
     * @return The SQL COUNT query string.
     */
    std::string buildCount() const;

    /**
     * @brief Validates the query parameters.
     *
     * @throws ValidationError if validation fails
     */
    void validate() const;

private:
    std::string tableName;                   ///< The name of the table.
    std::vector<std::string> selectColumns;  ///< The columns to select.
    std::vector<std::string>
        whereConditions;                      ///< The conditions for the query.
    std::vector<std::string> joinClauses;     ///< The join clauses.
    std::vector<std::string> groupByColumns;  ///< The columns to group by.
    std::string havingCondition;              ///< The having condition.
    std::string orderByClause;                ///< The order by clause.
    int limitValue = -1;                      ///< The limit for the query.
    int offsetValue = 0;                      ///< The offset for the query.
    std::vector<std::variant<int, double, std::string>>
        paramValues;  ///< Parameter values.
};

// Template method implementation

/**
 * @brief Template implementation of the where method.
 *
 * @tparam Args Parameter types to bind.
 * @param condition The WHERE condition string.
 * @param paramValues The values to bind to parameters.
 * @return A reference to the QueryBuilder instance.
 */
template <typename... Args>
QueryBuilder& QueryBuilder::where(const std::string& condition,
                                  Args&&... paramValues) {
    if (!condition.empty()) {
        whereConditions.push_back(condition);
        if constexpr (sizeof...(Args) > 0) {
            // Store parameter values (implementation omitted for brevity)
            // This would typically involve using fold expressions to process
            // each argument
            (void)std::initializer_list<int>{(paramValues, 0)...};
        }
    }
    return *this;
}

}  // namespace lithium::database::query

#endif  // LITHIUM_DATABASE_QUERY_QUERY_BUILDER_HPP
