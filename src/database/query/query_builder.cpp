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

#include "query_builder.hpp"

#include "../core/types.hpp"

namespace lithium::database::query {

//------------------------------------------------------------------------------
// QueryBuilder Implementation
//------------------------------------------------------------------------------

QueryBuilder::QueryBuilder(const std::string& tableName)
    : tableName(tableName), limitValue(-1), offsetValue(0) {
    // Default to selecting all columns
    selectColumns = {"*"};
}

QueryBuilder& QueryBuilder::select(const std::vector<std::string>& columns) {
    if (!columns.empty()) {
        selectColumns = columns;
    }
    return *this;
}

QueryBuilder& QueryBuilder::andWhere(const std::string& condition) {
    if (!condition.empty()) {
        if (whereConditions.empty()) {
            whereConditions.push_back(condition);
        } else {
            whereConditions.push_back("AND " + condition);
        }
    }
    return *this;
}

QueryBuilder& QueryBuilder::orWhere(const std::string& condition) {
    if (!condition.empty()) {
        if (whereConditions.empty()) {
            whereConditions.push_back(condition);
        } else {
            whereConditions.push_back("OR " + condition);
        }
    }
    return *this;
}

QueryBuilder& QueryBuilder::join(const std::string& table,
                                 const std::string& condition,
                                 const std::string& joinType) {
    std::string joinClause = joinType + " JOIN " + table + " ON " + condition;
    joinClauses.push_back(joinClause);
    return *this;
}

QueryBuilder& QueryBuilder::groupBy(const std::vector<std::string>& columns) {
    groupByColumns = columns;
    return *this;
}

QueryBuilder& QueryBuilder::having(const std::string& condition) {
    havingCondition = condition;
    return *this;
}

QueryBuilder& QueryBuilder::orderBy(const std::string& column, bool asc) {
    orderByClause = column + (asc ? " ASC" : " DESC");
    return *this;
}

QueryBuilder& QueryBuilder::limit(int limit) {
    if (limit >= 0) {
        limitValue = limit;
    }
    return *this;
}

QueryBuilder& QueryBuilder::offset(int offset) {
    if (offset >= 0) {
        offsetValue = offset;
    }
    return *this;
}

std::string QueryBuilder::build() const {
    // Validate query parameters before building
    validate();

    std::stringstream sql;

    // SELECT clause
    sql << "SELECT ";
    if (selectColumns.empty()) {
        sql << "*";
    } else {
        bool first = true;
        for (const auto& column : selectColumns) {
            if (!first)
                sql << ", ";
            sql << column;
            first = false;
        }
    }

    // FROM clause
    sql << " FROM " << tableName;

    // JOIN clauses
    for (const auto& join : joinClauses) {
        sql << " " << join;
    }

    // WHERE clause
    if (!whereConditions.empty()) {
        sql << " WHERE ";
        bool first = true;
        for (const auto& condition : whereConditions) {
            if (!first)
                sql << " ";
            sql << condition;
            first = false;
        }
    }

    // GROUP BY clause
    if (!groupByColumns.empty()) {
        sql << " GROUP BY ";
        bool first = true;
        for (const auto& column : groupByColumns) {
            if (!first)
                sql << ", ";
            sql << column;
            first = false;
        }
    }

    // HAVING clause
    if (!havingCondition.empty()) {
        sql << " HAVING " << havingCondition;
    }

    // ORDER BY clause
    if (!orderByClause.empty()) {
        sql << " ORDER BY " << orderByClause;
    }

    // LIMIT clause
    if (limitValue >= 0) {
        sql << " LIMIT " << limitValue;
    }

    // OFFSET clause
    if (offsetValue > 0) {
        sql << " OFFSET " << offsetValue;
    }

    return sql.str();
}

std::string QueryBuilder::buildCount() const {
    std::stringstream sql;

    // SELECT COUNT(*) clause
    sql << "SELECT COUNT(*) FROM " << tableName;

    // JOIN clauses
    for (const auto& join : joinClauses) {
        sql << " " << join;
    }

    // WHERE clause
    if (!whereConditions.empty()) {
        sql << " WHERE ";
        bool first = true;
        for (const auto& condition : whereConditions) {
            if (!first)
                sql << " ";
            sql << condition;
            first = false;
        }
    }

    // GROUP BY and HAVING are not typically used with COUNT queries
    // But we'll add them for completeness
    if (!groupByColumns.empty()) {
        sql << " GROUP BY ";
        bool first = true;
        for (const auto& column : groupByColumns) {
            if (!first)
                sql << ", ";
            sql << column;
            first = false;
        }
    }

    if (!havingCondition.empty()) {
        sql << " HAVING " << havingCondition;
    }

    return sql.str();
}

void QueryBuilder::validate() const {
    if (tableName.empty()) {
        THROW_VALIDATION_ERROR("Table name cannot be empty");
    }

    // Validate that we don't have both LIMIT and OFFSET without LIMIT
    if (offsetValue > 0 && limitValue < 0) {
        THROW_VALIDATION_ERROR("OFFSET cannot be used without LIMIT");
    }
}

}  // namespace lithium::database::query
