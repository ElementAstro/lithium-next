#ifndef LITHIUM_DATABASE_ORM_HPP
#define LITHIUM_DATABASE_ORM_HPP

#include <sqlite3.h>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::database {
class FailedToOpenDatabase : public atom::error::Exception {
    using Exception::Exception;
};

#define THROW_FAILED_TO_OPEN_DATABASE(...)         \
    throw lithium::database::FailedToOpenDatabase( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

class Database {
public:
    /**
     * @brief Constructs a Database instance and opens the specified SQLite
     * database.
     *
     * @param db_name The name of the database file.
     */
    explicit Database(const std::string& db_name);

    /**
     * @brief Destructs the Database instance and closes the database
     * connection.
     */
    ~Database();

    /**
     * @brief Gets the SQLite database handle.
     *
     * @return A pointer to the SQLite database handle.
     */
    sqlite3* get();

    /**
     * @brief Begins a database transaction.
     */
    void beginTransaction();

    /**
     * @brief Commits the current database transaction.
     */
    void commit();

    /**
     * @brief Rolls back the current database transaction.
     */
    void rollback();

private:
    sqlite3* db;  ///< The SQLite database handle.

    /**
     * @brief Executes an SQL statement.
     *
     * @param sql The SQL statement to execute.
     */
    void execute(const std::string& sql);
};

class ColumnBase {
public:
    /**
     * @brief Virtual destructor for ColumnBase.
     */
    virtual ~ColumnBase() = default;

    /**
     * @brief Gets the name of the column.
     *
     * @return The name of the column.
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Gets the type of the column.
     *
     * @return The type of the column.
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Converts the column value to an SQL string.
     *
     * @param model A pointer to the model containing the column value.
     * @return The column value as an SQL string.
     */
    virtual std::string toSQLValue(const void* model) const = 0;

    /**
     * @brief Sets the column value from an SQL string.
     *
     * @param model A pointer to the model to set the column value in.
     * @param value The column value as an SQL string.
     */
    virtual void fromSQLValue(void* model, const std::string& value) const = 0;
};

template <typename T>
struct always_false : std::false_type {};

template <typename T>
struct ColumnValue {
    /**
     * @brief Converts a value to an SQL string.
     *
     * @param value The value to convert.
     * @return The value as an SQL string.
     */
    static std::string toSQLValue(const T& value);

    /**
     * @brief Converts an SQL string to a value.
     *
     * @param value The SQL string to convert.
     * @return The converted value.
     */
    static T fromSQLValue(const std::string& value);
};

template <typename T, typename Model>
class Column : public ColumnBase {
public:
    using MemberPtr = T Model::*;

    /**
     * @brief Constructs a Column instance.
     *
     * @param name The name of the column.
     * @param ptr A pointer to the member variable representing the column.
     * @param type The type of the column (optional).
     */
    Column(const std::string& name, MemberPtr ptr,
           const std::string& type = "");

    /**
     * @brief Gets the name of the column.
     *
     * @return The name of the column.
     */
    std::string getName() const override;

    /**
     * @brief Gets the type of the column.
     *
     * @return The type of the column.
     */
    std::string getType() const override;

    /**
     * @brief Converts the column value to an SQL string.
     *
     * @param model A pointer to the model containing the column value.
     * @return The column value as an SQL string.
     */
    std::string toSQLValue(const void* model) const override;

    /**
     * @brief Sets the column value from an SQL string.
     *
     * @param model A pointer to the model to set the column value in.
     * @param value The column value as an SQL string.
     */
    void fromSQLValue(void* model, const std::string& value) const override;

private:
    std::string name;  ///< The name of the column.
    MemberPtr
        ptr;  ///< A pointer to the member variable representing the column.
    std::string customType;  ///< The custom type of the column (optional).
};

template <typename T>
class Table {
public:
    /**
     * @brief Constructs a Table instance.
     *
     * @param db A reference to the Database instance.
     */
    explicit Table(Database& db);

    /**
     * @brief Creates the table in the database.
     */
    void createTable();

    /**
     * @brief Inserts a model into the table.
     *
     * @param model The model to insert.
     */
    void insert(const T& model);

    /**
     * @brief Updates a model in the table.
     *
     * @param model The model to update.
     * @param condition The condition to match for the update.
     */
    void update(const T& model, const std::string& condition);

    /**
     * @brief Removes models from the table.
     *
     * @param condition The condition to match for the removal.
     */
    void remove(const std::string& condition);

    /**
     * @brief Queries models from the table.
     *
     * @param condition The condition to match for the query (optional).
     * @param limit The maximum number of models to return (optional).
     * @return A vector of models matching the query.
     */
    std::vector<T> query(const std::string& condition = "", int limit = -1);

    /**
     * @brief Inserts multiple models into the table in a batch.
     *
     * @param models A vector of models to insert.
     */
    void batchInsert(const std::vector<T>& models);

    /**
     * @brief Updates multiple models in the table in a batch.
     *
     * @param models A vector of models to update.
     * @param condition The condition to match for the update.
     */
    void batchUpdate(const std::vector<T>& models,
                     const std::string& condition);

    /**
     * @brief Creates an index on the table.
     *
     * @param indexName The name of the index.
     * @param columns A vector of column names to include in the index.
     */
    void createIndex(const std::string& indexName,
                     const std::vector<std::string>& columns);

private:
    Database& db;  ///< A reference to the Database instance.

    /**
     * @brief Gets the name of the table.
     *
     * @return The name of the table.
     */
    std::string tableName() const;

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
     * @brief Gets the list of column values for a model.
     *
     * @param model A pointer to the model.
     * @return The list of column values as a string.
     */
    std::string valuesList(const T* model) const;

    /**
     * @brief Gets the set clause for an update statement.
     *
     * @param model A pointer to the model.
     * @return The set clause as a string.
     */
    std::string setClause(const T* model) const;

    /**
     * @brief Executes an SQL statement.
     *
     * @param sql The SQL statement to execute.
     */
    void execute(const std::string& sql);

    /**
     * @brief Executes an SQL query and returns the results.
     *
     * @param sql The SQL query to execute.
     * @return A vector of models matching the query.
     */
    std::vector<T> executeQuery(const std::string& sql);
};

class QueryBuilder {
public:
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
     * @return A reference to the QueryBuilder instance.
     */
    QueryBuilder& where(const std::string& condition);

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
     * @brief Builds the SQL query string.
     *
     * @return The SQL query string.
     */
    std::string build() const;

private:
    std::string tableName;                   ///< The name of the table.
    std::vector<std::string> selectColumns;  ///< The columns to select.
    std::vector<std::string> conditions;     ///< The conditions for the query.
    std::string orderByClause;               ///< The order by clause.
    int limitValue = -1;                     ///< The limit for the query.
};

class CacheManager {
public:
    /**
     * @brief Gets the singleton instance of the CacheManager.
     *
     * @return A reference to the CacheManager instance.
     */
    static CacheManager& getInstance();

    /**
     * @brief Puts a value in the cache.
     *
     * @param key The key for the value.
     * @param value The value to cache.
     */
    void put(const std::string& key, const std::string& value);

    /**
     * @brief Gets a value from the cache.
     *
     * @param key The key for the value.
     * @param value A reference to a string to store the value.
     * @return True if the value was found in the cache, false otherwise.
     */
    bool get(const std::string& key, std::string& value);

private:
    struct CacheEntry {
        std::string value;  ///< The cached value.
        std::chrono::steady_clock::time_point
            timestamp;  ///< The timestamp when the value was cached.
    };

    std::mutex cacheMutex;  ///< Mutex for synchronizing access to the cache.
    std::unordered_map<std::string, CacheEntry> cache;  ///< The cache storage.
};
}  // namespace lithium::database

namespace lithium::database {

template <typename T>
std::string ColumnValue<T>::toSQLValue(const T& value) {
    std::stringstream ss;
    if constexpr (std::is_integral_v<T>) {
        ss << value;
    } else if constexpr (std::is_floating_point_v<T>) {
        ss << value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        ss << "'" << value << "'";
    } else if constexpr (std::is_same_v<T, bool>) {
        ss << (value ? 1 : 0);
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
    return ss.str();
}

template <typename T>
T ColumnValue<T>::fromSQLValue(const std::string& value) {
    std::stringstream ss(value);
    T result;
    if constexpr (std::is_same_v<T, std::string>) {
        result = value;
    } else {
        ss >> result;
    }
    return result;
}

template <typename T, typename Model>
Column<T, Model>::Column(const std::string& name, MemberPtr ptr,
                         const std::string& type)
    : name(name), ptr(ptr), customType(type) {}

template <typename T, typename Model>
std::string Column<T, Model>::getName() const {
    return name;
}

template <typename T, typename Model>
std::string Column<T, Model>::getType() const {
    if (!customType.empty()) {
        return customType;
    }
    if constexpr (std::is_same_v<T, int>)
        return "INTEGER";
    if constexpr (std::is_same_v<T, std::string>)
        return "TEXT";
    if constexpr (std::is_same_v<T, bool>)
        return "BOOLEAN";
    if constexpr (std::is_floating_point_v<T>)
        return "REAL";
    return "TEXT";
}

template <typename T, typename Model>
std::string Column<T, Model>::toSQLValue(const void* model) const {
    const Model* m = static_cast<const Model*>(model);
    return ColumnValue<T>::toSQLValue(m->*ptr);
}

template <typename T, typename Model>
void Column<T, Model>::fromSQLValue(void* model,
                                    const std::string& value) const {
    Model* m = static_cast<Model*>(model);
    m->*ptr = ColumnValue<T>::fromSQLValue(value);
}

template <typename T>
Table<T>::Table(Database& db) : db(db) {
    LOG_F(INFO, "Table instance created for {}", tableName());
}

template <typename T>
void Table<T>::createTable() {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName() + " (" +
                      columnsDefinition() + ");";
    LOG_F(INFO, "Creating table with SQL: {}", sql);
    execute(sql);
    LOG_F(INFO, "Table {} created successfully", tableName());
}

template <typename T>
void Table<T>::insert(const T& model) {
    std::string sql = "INSERT INTO " + tableName() + " (" + columnsList() +
                      ") VALUES (" + valuesList(&model) + ");";
    LOG_F(INFO, "Inserting record with SQL: {}", sql);
    execute(sql);
    LOG_F(INFO, "Record inserted successfully into {}", tableName());
}

template <typename T>
void Table<T>::update(const T& model, const std::string& condition) {
    std::string sql = "UPDATE " + tableName() + " SET " + setClause(&model) +
                      " WHERE " + condition + ";";
    LOG_F(INFO, "Updating record with SQL: {}", sql);
    execute(sql);
    LOG_F(INFO, "Record updated successfully in {}", tableName());
}

template <typename T>
void Table<T>::remove(const std::string& condition) {
    std::string sql =
        "DELETE FROM " + tableName() + " WHERE " + condition + ";";
    LOG_F(INFO, "Deleting record with SQL: {}", sql);
    execute(sql);
    LOG_F(INFO, "Record deleted successfully from {}", tableName());
}

template <typename T>
std::vector<T> Table<T>::query(const std::string& condition, int limit) {
    std::string sql = "SELECT * FROM " + tableName();
    if (!condition.empty()) {
        sql += " WHERE " + condition;
    }
    if (limit > 0) {
        sql += " LIMIT " + std::to_string(limit);
    }
    LOG_F(INFO, "Executing query: {}", sql);
    auto results = executeQuery(sql);
    LOG_F(INFO, "Query returned {} results", results.size());
    return results;
}

template <typename T>
void Table<T>::batchInsert(const std::vector<T>& models) {
    if (models.empty()) {
        LOG_F(WARNING, "Batch insert called with empty models vector");
        return;
    }

    LOG_F(INFO, "Starting batch insert of {} records", models.size());
    db.beginTransaction();
    try {
        for (const auto& model : models) {
            insert(model);
        }
        db.commit();
        LOG_F(INFO, "Batch insert completed successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Batch insert failed: {}", e.what());
        db.rollback();
        throw;
    }
}

template <typename T>
void Table<T>::batchUpdate(const std::vector<T>& models,
                           const std::string& condition) {
    if (models.empty()) {
        LOG_F(WARNING, "Batch update called with empty models vector");
        return;
    }

    LOG_F(INFO, "Starting batch update of {} records", models.size());
    db.beginTransaction();
    try {
        for (const auto& model : models) {
            update(model, condition);
        }
        db.commit();
        LOG_F(INFO, "Batch update completed successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Batch update failed: {}", e.what());
        db.rollback();
        throw;
    }
}

template <typename T>
void Table<T>::createIndex(const std::string& indexName,
                           const std::vector<std::string>& columns) {
    std::string sql =
        "CREATE INDEX IF NOT EXISTS " + indexName + " ON " + tableName() + " (";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += ");";
    LOG_F(INFO, "Creating index with SQL: {}", sql);
    execute(sql);
    LOG_F(INFO, "Index {} created successfully", indexName);
}

template <typename T>
std::string Table<T>::tableName() const {
    return T::tableName();
}

template <typename T>
std::string Table<T>::columnsDefinition() const {
    std::string result;
    for (const auto& column : T::columns()) {
        if (!result.empty())
            result += ", ";
        result += column->getName() + " " + column->getType();
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
std::string Table<T>::valuesList(const T* model) const {
    std::string result;
    for (const auto& column : T::columns()) {
        if (!result.empty())
            result += ", ";
        result += column->toSQLValue(model);
    }
    return result;
}

template <typename T>
std::string Table<T>::setClause(const T* model) const {
    std::string result;
    for (const auto& column : T::columns()) {
        if (!result.empty())
            result += ", ";
        result += column->getName() + " = " + column->toSQLValue(model);
    }
    return result;
}

template <typename T>
void Table<T>::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &errMsg) !=
        SQLITE_OK) {
        std::string error = "SQL Error: " + std::string(errMsg);
        LOG_F(ERROR, "{}", error);
        sqlite3_free(errMsg);
        THROW_RUNTIME_ERROR(error);
    }
}

template <typename T>
std::vector<T> Table<T>::executeQuery(const std::string& sql) {
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        std::string error = "Failed to prepare SQL statement: " +
                            std::string(sqlite3_errmsg(db.get()));
        LOG_F(ERROR, "{}", error);
        THROW_RUNTIME_ERROR(error);
    }

    std::vector<T> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        T model;
        int colIndex = 0;
        for (auto& column : T::columns()) {
            const char* text = reinterpret_cast<const char*>(
                sqlite3_column_text(stmt, colIndex++));
            if (text) {
                column->fromSQLValue(&model, text);
            }
        }
        results.push_back(model);
    }

    sqlite3_finalize(stmt);
    return results;
}

}  // namespace lithium::database

#endif  // LITHIUM_DATABASE_ORM_HPP