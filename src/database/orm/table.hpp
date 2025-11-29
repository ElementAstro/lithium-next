/*
 * This file is part of Lithium-Next.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * See LICENSE file for more details.
 *
 * Lithium-Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Lithium-Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Lithium-Next. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LITHIUM_DATABASE_ORM_TABLE_HPP
#define LITHIUM_DATABASE_ORM_TABLE_HPP

#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "column.hpp"
#include "../core/database.hpp"
#include "../core/statement.hpp"
#include "../core/transaction.hpp"

namespace lithium::database::orm {

template <typename T>
class Table {
public:
    /**
     * @brief Constructs a Table instance.
     *
     * @param db A reference to the Database instance.
     */
    explicit Table(lithium::database::Database& db);

    /**
     * @brief Creates the table in the database.
     *
     * @param ifNotExists Set to true to add "IF NOT EXISTS" clause (default:
     * true)
     * @throws SQLExecutionError if table creation fails
     */
    void createTable(bool ifNotExists = true);

    /**
     * @brief Inserts a model into the table.
     *
     * @param model The model to insert.
     * @throws SQLExecutionError if insertion fails
     * @throws ValidationError if model validation fails
     */
    void insert(const T& model);

    /**
     * @brief Updates a model in the table.
     *
     * @param model The model to update.
     * @param condition The condition to match for the update.
     * @throws SQLExecutionError if update fails
     * @throws ValidationError if model validation fails
     */
    void update(const T& model, const std::string& condition);

    /**
     * @brief Removes models from the table.
     *
     * @param condition The condition to match for the removal.
     * @throws SQLExecutionError if deletion fails
     * @throws ValidationError if condition is empty
     */
    void remove(const std::string& condition);

    /**
     * @brief Queries models from the table.
     *
     * @param condition The condition to match for the query (optional).
     * @param limit The maximum number of models to return (optional).
     * @param offset The number of models to skip (optional).
     * @return A vector of models matching the query.
     * @throws SQLExecutionError if query fails
     */
    std::vector<T> query(const std::string& condition = "", int limit = -1,
                         int offset = 0);

    /**
     * @brief Asynchronously queries models from the table.
     *
     * @param condition The condition to match for the query (optional).
     * @param limit The maximum number of models to return (optional).
     * @param offset The number of models to skip (optional).
     * @return A future containing a vector of models matching the query.
     */
    std::future<std::vector<T>> queryAsync(const std::string& condition = "",
                                           int limit = -1, int offset = 0);

    /**
     * @brief Inserts multiple models into the table in a batch.
     *
     * @param models A vector of models to insert.
     * @param chunkSize The maximum number of inserts per transaction.
     * @throws SQLExecutionError if batch insertion fails
     */
    void batchInsert(const std::vector<T>& models, size_t chunkSize = 100);

    /**
     * @brief Updates multiple models in the table in a batch.
     *
     * @param models A vector of models to update.
     * @param conditionBuilder A function that builds the condition for each
     * model.
     * @param chunkSize The maximum number of updates per transaction.
     * @throws SQLExecutionError if batch update fails
     */
    void batchUpdate(
        const std::vector<T>& models,
        std::function<std::string(const T&)> conditionBuilder,
        size_t chunkSize = 100);

    /**
     * @brief Creates an index on the table.
     *
     * @param indexName The name of the index.
     * @param columns A vector of column names to include in the index.
     * @param unique Set to true for unique index (default: false).
     * @param ifNotExists Set to true to add "IF NOT EXISTS" clause (default:
     * true).
     * @throws SQLExecutionError if index creation fails
     * @throws ValidationError if columns vector is empty
     */
    void createIndex(const std::string& indexName,
                     const std::vector<std::string>& columns,
                     bool unique = false, bool ifNotExists = true);

    /**
     * @brief Counts the number of records matching a condition.
     *
     * @param condition The condition to match (optional).
     * @return The number of matching records.
     * @throws SQLExecutionError if counting fails
     */
    int64_t count(const std::string& condition = "");

    /**
     * @brief Checks if any records match a condition.
     *
     * @param condition The condition to match.
     * @return True if any records match, false otherwise.
     * @throws SQLExecutionError if checking fails
     */
    bool exists(const std::string& condition);

    /**
     * @brief Gets the table name.
     *
     * @return The name of the table.
     */
    static std::string tableName();

private:
    lithium::database::Database&
        db;  ///< A reference to the Database instance.

    /**
     * @brief Validates a model before insertion or update.
     *
     * @param model The model to validate.
     * @throws ValidationError if validation fails
     */
    void validateModel(const T& model) const;

    /**
     * @brief Gets the column definitions for the table.
     *
     * @return The column definitions as a string.
     */
    std::string columnsDefinition() const;

    /**
     * @brief Gets the list of column names for the table.
     *
     * @return The list of column names as a string.
     */
    std::string columnsList() const;

    /**
     * @brief Prepares and binds an INSERT statement.
     *
     * @return Prepared and bound statement ready for execution.
     */
    std::unique_ptr<lithium::database::Statement> prepareInsert(const T& model);

    /**
     * @brief Prepares and binds an UPDATE statement.
     *
     * @param model The model with updated values.
     * @param condition The condition for the update.
     * @return Prepared and bound statement ready for execution.
     */
    std::unique_ptr<lithium::database::Statement> prepareUpdate(
        const T& model, const std::string& condition);

    /**
     * @brief Fills a model from a statement that has stepped to a row.
     *
     * @param stmt The statement containing row data.
     * @return A model object filled with data from the statement.
     */
    T modelFromStatement(lithium::database::Statement& stmt) const;

    /**
     * @brief Executes an SQL statement directly.
     *
     * @param sql The SQL statement to execute.
     * @throws SQLExecutionError if execution fails
     */
    void execute(const std::string& sql);

    /**
     * @brief Executes a query and returns the results as models.
     *
     * @param sql The SQL query to execute.
     * @return A vector of model objects.
     * @throws SQLExecutionError if query execution fails
     */
    std::vector<T> executeQuery(const std::string& sql);
};

// Table implementation
template <typename T>
Table<T>::Table(lithium::database::Database& db) : db(db) {
    spdlog::info("Table instance created for {}", tableName());
}

template <typename T>
void Table<T>::createTable(bool ifNotExists) {
    std::string sql = "CREATE TABLE " +
                      std::string(ifNotExists ? "IF NOT EXISTS " : "") +
                      tableName() + " (" + columnsDefinition() + ");";
    spdlog::info("Creating table with SQL: {}", sql);
    execute(sql);
    spdlog::info("Table {} created successfully", tableName());
}

template <typename T>
void Table<T>::insert(const T& model) {
    validateModel(model);
    auto stmt = prepareInsert(model);
    spdlog::info("Inserting record with SQL: {}", stmt->getSql());
    stmt->execute();
    spdlog::info("Record inserted successfully into {}", tableName());
}

template <typename T>
void Table<T>::update(const T& model, const std::string& condition) {
    validateModel(model);
    auto stmt = prepareUpdate(model, condition);
    spdlog::info("Updating record with SQL: {}", stmt->getSql());
    stmt->execute();
    spdlog::info("Record updated successfully in {}", tableName());
}

template <typename T>
void Table<T>::remove(const std::string& condition) {
    if (condition.empty()) {
        THROW_VALIDATION_ERROR("Condition for removal cannot be empty");
    }
    std::string sql =
        "DELETE FROM " + tableName() + " WHERE " + condition + ";";
    spdlog::info("Deleting record with SQL: {}", sql);
    execute(sql);
    spdlog::info("Record deleted successfully from {}", tableName());
}

template <typename T>
std::vector<T> Table<T>::query(const std::string& condition, int limit,
                               int offset) {
    std::string sql = "SELECT * FROM " + tableName();
    if (!condition.empty()) {
        sql += " WHERE " + condition;
    }
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }
    if (offset > 0) {
        sql += " OFFSET " + std::to_string(offset);
    }
    spdlog::info("Executing query: {}", sql);
    auto results = executeQuery(sql);
    spdlog::info("Query returned {} results", results.size());
    return results;
}

template <typename T>
std::future<std::vector<T>> Table<T>::queryAsync(const std::string& condition,
                                                 int limit, int offset) {
    return std::async(std::launch::async, &Table<T>::query, this, condition,
                      limit, offset);
}

template <typename T>
void Table<T>::batchInsert(const std::vector<T>& models, size_t chunkSize) {
    if (models.empty()) {
        spdlog::warn("Batch insert called with empty models vector");
        return;
    }

    spdlog::info("Starting batch insert of {} records", models.size());
    auto transaction = db.beginTransaction();
    try {
        for (size_t i = 0; i < models.size(); i += chunkSize) {
            auto end = std::min(models.size(), i + chunkSize);
            for (size_t j = i; j < end; ++j) {
                insert(models[j]);
            }
            transaction->commit();
            transaction = db.beginTransaction();
        }
        transaction->commit();
        spdlog::info("Batch insert completed successfully");
    } catch (const std::exception& e) {
        spdlog::error("Batch insert failed: {}", e.what());
        transaction->rollback();
        throw;
    }
}

template <typename T>
void Table<T>::batchUpdate(
    const std::vector<T>& models,
    std::function<std::string(const T&)> conditionBuilder, size_t chunkSize) {
    if (models.empty()) {
        spdlog::warn("Batch update called with empty models vector");
        return;
    }

    spdlog::info("Starting batch update of {} records", models.size());
    auto transaction = db.beginTransaction();
    try {
        for (size_t i = 0; i < models.size(); i += chunkSize) {
            auto end = std::min(models.size(), i + chunkSize);
            for (size_t j = i; j < end; ++j) {
                update(models[j], conditionBuilder(models[j]));
            }
            transaction->commit();
            transaction = db.beginTransaction();
        }
        transaction->commit();
        spdlog::info("Batch update completed successfully");
    } catch (const std::exception& e) {
        spdlog::error("Batch update failed: {}", e.what());
        transaction->rollback();
        throw;
    }
}

template <typename T>
void Table<T>::createIndex(const std::string& indexName,
                           const std::vector<std::string>& columns, bool unique,
                           bool ifNotExists) {
    if (columns.empty()) {
        THROW_VALIDATION_ERROR("Columns for index cannot be empty");
    }
    std::string sql = "CREATE " + std::string(unique ? "UNIQUE " : "") +
                      "INDEX " +
                      std::string(ifNotExists ? "IF NOT EXISTS " : "") +
                      indexName + " ON " + tableName() + " (";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += ");";
    spdlog::info("Creating index with SQL: {}", sql);
    execute(sql);
    spdlog::info("Index {} created successfully", indexName);
}

template <typename T>
int64_t Table<T>::count(const std::string& condition) {
    std::string sql = "SELECT COUNT(*) FROM " + tableName();
    if (!condition.empty()) {
        sql += " WHERE " + condition;
    }
    spdlog::info("Executing count query: {}", sql);
    auto stmt = db.prepare(sql);
    if (!stmt->step()) {
        THROW_SQL_EXECUTION_ERROR("Failed to execute count query");
    }
    return stmt->getInt64(0);
}

template <typename T>
bool Table<T>::exists(const std::string& condition) {
    std::string sql =
        "SELECT 1 FROM " + tableName() + " WHERE " + condition + " LIMIT 1;";
    spdlog::info("Executing exists query: {}", sql);
    auto stmt = db.prepare(sql);
    return stmt->step();
}

template <typename T>
std::string Table<T>::tableName() {
    return T::tableName();
}

template <typename T>
void Table<T>::validateModel(const T& model) const {
    // Implement model validation logic here
    // This is a placeholder - actual implementation depends on model
    // requirements
}

template <typename T>
std::string Table<T>::columnsDefinition() const {
    std::string result;
    for (const auto& column : T::columns()) {
        if (!result.empty())
            result += ", ";
        result += column->getName() + " " + column->getType() + " " +
                  column->getConstraints();
    }
    return result;
}

template <typename T>
std::string Table<T>::columnsList() const {
    std::string result;
    for (const auto& column : T::columns()) {
        if (!result.empty())
            result += ", ";
        result += column->getName();
    }
    return result;
}

template <typename T>
std::unique_ptr<lithium::database::Statement> Table<T>::prepareInsert(
    const T& model) {
    std::string sql =
        "INSERT INTO " + tableName() + " (" + columnsList() + ") VALUES (";
    for (size_t i = 0; i < T::columns().size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += "?";
    }
    sql += ");";
    auto stmt = db.prepare(sql);
    int index = 1;
    for (const auto& column : T::columns()) {
        column->bindToStatement(*stmt, index++, &model);
    }
    return stmt;
}

template <typename T>
std::unique_ptr<lithium::database::Statement> Table<T>::prepareUpdate(
    const T& model, const std::string& condition) {
    std::string sql = "UPDATE " + tableName() + " SET ";
    for (size_t i = 0; i < T::columns().size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += T::columns()[i]->getName() + " = ?";
    }
    sql += " WHERE " + condition + ";";
    auto stmt = db.prepare(sql);
    int index = 1;
    for (const auto& column : T::columns()) {
        column->bindToStatement(*stmt, index++, &model);
    }
    return stmt;
}

template <typename T>
T Table<T>::modelFromStatement(lithium::database::Statement& stmt) const {
    T model;
    for (size_t i = 0; i < T::columns().size(); ++i) {
        T::columns()[i]->readFromStatement(stmt, static_cast<int>(i), &model);
    }
    return model;
}

template <typename T>
void Table<T>::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &errMsg) !=
        SQLITE_OK) {
        std::string error = "SQL Error: " + std::string(errMsg);
        spdlog::error("{}", error);
        sqlite3_free(errMsg);
        THROW_SQL_EXECUTION_ERROR(error);
    }
}

template <typename T>
std::vector<T> Table<T>::executeQuery(const std::string& sql) {
    auto stmt = db.prepare(sql);
    std::vector<T> results;
    while (stmt->step()) {
        results.push_back(modelFromStatement(*stmt));
    }
    return results;
}

}  // namespace lithium::database::orm

#endif  // LITHIUM_DATABASE_ORM_TABLE_HPP
