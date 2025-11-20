#ifndef LITHIUM_DATABASE_ORM_HPP
#define LITHIUM_DATABASE_ORM_HPP

#include <sqlite3.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace lithium::database {

// Exception classes
class FailedToOpenDatabase : public atom::error::Exception {
    using Exception::Exception;
};

class SQLExecutionError : public atom::error::Exception {
    using Exception::Exception;
};

class PrepareStatementError : public atom::error::Exception {
    using Exception::Exception;
};

class TransactionError : public atom::error::Exception {
    using Exception::Exception;
};

class ValidationError : public atom::error::Exception {
    using Exception::Exception;
};

// Exception macros
#define THROW_FAILED_TO_OPEN_DATABASE(...)         \
    throw lithium::database::FailedToOpenDatabase( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_SQL_EXECUTION_ERROR(...)                                         \
    throw lithium::database::SQLExecutionError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                               ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_PREPARE_STATEMENT_ERROR(...)          \
    throw lithium::database::PrepareStatementError( \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_TRANSACTION_ERROR(...)                                          \
    throw lithium::database::TransactionError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                              ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_VALIDATION_ERROR(...)                                          \
    throw lithium::database::ValidationError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                             ATOM_FUNC_NAME, __VA_ARGS__)

// Forward declarations
class Statement;
class Transaction;

class Database {
public:
    /**
     * @brief Constructs a Database instance and opens the specified SQLite
     * database.
     *
     * @param db_name The name of the database file.
     * @param flags SQLite open flags (default: SQLITE_OPEN_READWRITE |
     * SQLITE_OPEN_CREATE)
     * @throws FailedToOpenDatabase if database cannot be opened
     */
    explicit Database(const std::string& db_name,
                      int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

    /**
     * @brief Destructs the Database instance and closes the database
     * connection.
     */
    ~Database();

    // Prevent copying
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Allow moving
    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    /**
     * @brief Gets the SQLite database handle.
     *
     * @return A pointer to the SQLite database handle.
     */
    sqlite3* get();

    /**
     * @brief Creates a prepared statement from an SQL query.
     *
     * @param sql The SQL query to prepare.
     * @return A unique pointer to the created Statement.
     * @throws PrepareStatementError if statement preparation fails
     */
    std::unique_ptr<Statement> prepare(const std::string& sql);

    /**
     * @brief Begins a database transaction.
     *
     * @return A unique pointer to the created Transaction.
     * @throws TransactionError if transaction cannot be started
     */
    std::unique_ptr<Transaction> beginTransaction();

    /**
     * @brief Executes an SQL statement directly.
     *
     * @param sql The SQL statement to execute.
     * @throws SQLExecutionError if execution fails
     */
    void execute(const std::string& sql);

    /**
     * @brief Check if database connection is valid
     *
     * @return bool True if connection is valid
     */
    bool isValid() const noexcept;

    /**
     * @brief Configure SQLite connection parameters
     *
     * @param pragmas Map of PRAGMA name to value
     */
    void configure(const std::unordered_map<std::string, std::string>& pragmas);

    /**
     * @brief Commits the current transaction.
     * @throws TransactionError if commit fails
     */
    void commit();

    /**
     * @brief Rolls back the current transaction.
     * @throws TransactionError if rollback fails
     */
    void rollback();

private:
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db{nullptr,
                                                          sqlite3_close};
    std::atomic<bool> valid{false};
};

class Transaction {
public:
    /**
     * @brief Constructs a Transaction object.
     *
     * @param db Reference to the database connection.
     * @throws TransactionError if transaction cannot be started
     */
    explicit Transaction(Database& db);

    /**
     * @brief Destructor that rolls back the transaction if not committed.
     */
    ~Transaction();

    // Non-copyable
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    /**
     * @brief Commits the transaction.
     *
     * @throws TransactionError if commit fails
     */
    void commit();

    /**
     * @brief Rolls back the transaction.
     *
     * @throws TransactionError if rollback fails
     */
    void rollback();

private:
    Database& db;
    bool committed = false;
    bool rolledBack = false;
};

class Statement {
public:
    /**
     * @brief Constructs a Statement object.
     *
     * @param db Reference to the database connection.
     * @param sql The SQL statement to prepare.
     * @throws PrepareStatementError if preparation fails
     */
    Statement(Database& db, const std::string& sql);

    /**
     * @brief Destructor that finalizes the statement.
     */
    ~Statement();

    // Non-copyable
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    /**
     * @brief Binds an integer value to a parameter.
     *
     * @param index Parameter index (1-based).
     * @param value Integer value to bind.
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    Statement& bind(int index, int value);

    /**
     * @brief Binds a 64-bit integer value to a parameter.
     *
     * @param index Parameter index (1-based).
     * @param value Integer value to bind.
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    Statement& bind(int index, int64_t value);

    /**
     * @brief Binds a double value to a parameter.
     *
     * @param index Parameter index (1-based).
     * @param value Double value to bind.
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    Statement& bind(int index, double value);

    /**
     * @brief Binds a string value to a parameter.
     *
     * @param index Parameter index (1-based).
     * @param value String value to bind.
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    Statement& bind(int index, const std::string& value);

    /**
     * @brief Binds a null value to a parameter.
     *
     * @param index Parameter index (1-based).
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    Statement& bindNull(int index);

    /**
     * @brief Binds a named parameter.
     *
     * @param name Parameter name (without the : prefix).
     * @param value Value to bind.
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if binding fails
     */
    template <typename T>
    Statement& bindNamed(const std::string& name, const T& value);

    /**
     * @brief Executes the statement without returning results.
     *
     * @return True if execution was successful.
     * @throws SQLExecutionError if execution fails
     */
    bool execute();

    /**
     * @brief Steps through the statement results.
     *
     * @return True if a row was retrieved, false if no more rows.
     * @throws SQLExecutionError if stepping fails
     */
    bool step();

    /**
     * @brief Resets the statement for reuse.
     *
     * @return Reference to this Statement for chaining.
     * @throws PrepareStatementError if reset fails
     */
    Statement& reset();

    /**
     * @brief Gets an integer column value.
     *
     * @param index Column index (0-based).
     * @return Integer value from the column.
     */
    int getInt(int index) const;

    /**
     * @brief Gets a 64-bit integer column value.
     *
     * @param index Column index (0-based).
     * @return 64-bit integer value from the column.
     */
    int64_t getInt64(int index) const;

    /**
     * @brief Gets a double column value.
     *
     * @param index Column index (0-based).
     * @return Double value from the column.
     */
    double getDouble(int index) const;

    /**
     * @brief Gets a string column value.
     *
     * @param index Column index (0-based).
     * @return String value from the column.
     */
    std::string getText(int index) const;

    /**
     * @brief Gets a blob column value.
     *
     * @param index Column index (0-based).
     * @return Vector of bytes from the column.
     */
    std::vector<uint8_t> getBlob(int index) const;

    /**
     * @brief Checks if a column contains NULL.
     *
     * @param index Column index (0-based).
     * @return True if the column contains NULL.
     */
    bool isNull(int index) const;

    /**
     * @brief Gets the number of columns in the result set.
     *
     * @return Number of columns.
     */
    int getColumnCount() const;

    /**
     * @brief Gets the name of a column.
     *
     * @param index Column index (0-based).
     * @return Name of the column.
     */
    std::string getColumnName(int index) const;

    /**
     * @brief Gets the SQLite statement handle.
     *
     * @return A pointer to the SQLite statement handle.
     */
    sqlite3_stmt* get() const;

    /**
     * @brief Gets the SQL string for this statement.
     *
     * @return The SQL string.
     */
    std::string getSql() const;

private:
    Database& db;
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> stmt{
        nullptr, sqlite3_finalize};
    std::string sql;

    /**
     * @brief Validates that an index is within the valid range.
     *
     * @param index The index to validate.
     * @param isParam Whether this is a parameter index (1-based) or a column
     * index (0-based).
     * @throws ValidationError if index is out of range
     */
    void validateIndex(int index, bool isParam) const;

    /**
     * @brief Check if column contains a valid type.
     *
     * @param index Column index (0-based).
     * @param expectedType Expected SQLite type.
     * @return True if type matches.
     */
    bool checkColumnType(int index, int expectedType) const;
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
     * @brief Gets additional column constraints.
     *
     * @return String of constraints (e.g., "PRIMARY KEY", "NOT NULL").
     */
    virtual std::string getConstraints() const = 0;

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

    /**
     * @brief Binds the column value to a statement.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param model A pointer to the model containing the column value.
     */
    virtual void bindToStatement(Statement& stmt, int index,
                                 const void* model) const = 0;

    /**
     * @brief Reads the column value from a statement.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @param model A pointer to the model to set the column value in.
     */
    virtual void readFromStatement(const Statement& stmt, int index,
                                   void* model) const = 0;
};

// Using std::false_type is a modern C++ idiom for static_assert failure
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

    /**
     * @brief Binds a value to a statement parameter.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param value The value to bind.
     */
    static void bindToStatement(Statement& stmt, int index, const T& value);

    /**
     * @brief Reads a value from a statement column.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @return The read value.
     */
    static T readFromStatement(const Statement& stmt, int index);
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
     * @param constraints Additional SQL constraints for the column (optional).
     */
    Column(const std::string& name, MemberPtr ptr, const std::string& type = "",
           const std::string& constraints = "");

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
     * @brief Gets additional column constraints.
     *
     * @return String of constraints (e.g., "PRIMARY KEY", "NOT NULL").
     */
    std::string getConstraints() const override;

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

    /**
     * @brief Binds the column value to a statement.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param model A pointer to the model containing the column value.
     */
    void bindToStatement(Statement& stmt, int index,
                         const void* model) const override;

    /**
     * @brief Reads the column value from a statement.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @param model A pointer to the model to set the column value in.
     */
    void readFromStatement(const Statement& stmt, int index,
                           void* model) const override;

private:
    std::string name;  ///< The name of the column.
    MemberPtr
        ptr;  ///< A pointer to the member variable representing the column.
    std::string customType;   ///< The custom type of the column (optional).
    std::string constraints;  ///< Additional SQL constraints for the column.
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
    void batchUpdate(const std::vector<T>& models,
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
    Database& db;  ///< A reference to the Database instance.

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
    std::unique_ptr<Statement> prepareInsert(const T& model);

    /**
     * @brief Prepares and binds an UPDATE statement.
     *
     * @param model The model with updated values.
     * @param condition The condition for the update.
     * @return Prepared and bound statement ready for execution.
     */
    std::unique_ptr<Statement> prepareUpdate(const T& model,
                                             const std::string& condition);

    /**
     * @brief Fills a model from a statement that has stepped to a row.
     *
     * @param stmt The statement containing row data.
     * @return A model object filled with data from the statement.
     */
    T modelFromStatement(Statement& stmt) const;

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

class CacheManager {
public:
    /**
     * @brief Gets the singleton instance of the CacheManager.
     *
     * @return A reference to the CacheManager instance.
     */
    static CacheManager& getInstance();

    /**
     * @brief Destructor that cleans up resources properly.
     */
    ~CacheManager();

    /**
     * @brief Puts a value in the cache.
     *
     * @param key The key for the value.
     * @param value The value to cache.
     * @param ttlSeconds Time-to-live in seconds (default: 300).
     */
    void put(const std::string& key, const std::string& value,
             int ttlSeconds = 300);

    /**
     * @brief Gets a value from the cache.
     *
     * @param key The key for the value.
     * @return An optional containing the value if found, empty if not found or
     * expired.
     */
    std::optional<std::string> get(const std::string& key);

    /**
     * @brief Removes a value from the cache.
     *
     * @param key The key for the value to remove.
     * @return True if the value was removed, false if not found.
     */
    bool remove(const std::string& key);

    /**
     * @brief Clears the entire cache.
     */
    void clear();

    /**
     * @brief Clears expired entries from the cache.
     *
     * @return Number of entries removed.
     */
    size_t purgeExpired();

    /**
     * @brief Sets the default TTL for cache entries.
     *
     * @param seconds TTL in seconds.
     */
    void setDefaultTTL(int seconds);

    /**
     * @brief Gets the current size of the cache.
     *
     * @return The number of entries in the cache.
     */
    size_t size() const;

private:
    struct CacheEntry {
        std::string value;                             ///< The cached value.
        std::chrono::steady_clock::time_point expiry;  ///< The expiration time.
    };

    CacheManager();  // Private constructor for singleton

    mutable std::shared_mutex
        cacheMutex;  ///< Mutex for synchronizing access to the cache.
    std::unordered_map<std::string, CacheEntry> cache;  ///< The cache storage.
    int defaultTTL = 300;  ///< Default time-to-live in seconds.

    // Automatically purge expired entries periodically
    std::thread purgeThread;
    std::atomic<bool> stopPurgeThread{false};

    void purgePeriodically();
};

// Implementation of template methods

// SQL string escaping helper
namespace impl {
inline std::string escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() +
                   10);  // Pre-allocate to avoid multiple reallocations

    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}
}  // namespace impl

// ColumnValue implementation
template <typename T>
std::string ColumnValue<T>::toSQLValue(const T& value) {
    std::stringstream ss;
    if constexpr (std::is_integral_v<T>) {
        ss << value;
    } else if constexpr (std::is_floating_point_v<T>) {
        ss << value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        ss << "'" << impl::escapeString(value) << "'";
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

template <typename T>
void ColumnValue<T>::bindToStatement(Statement& stmt, int index,
                                     const T& value) {
    if constexpr (std::is_integral_v<T>) {
        stmt.bind(index, static_cast<int64_t>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        stmt.bind(index, static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        stmt.bind(index, value);
    } else if constexpr (std::is_same_v<T, bool>) {
        stmt.bind(index, value ? 1 : 0);
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
}

template <typename T>
T ColumnValue<T>::readFromStatement(const Statement& stmt, int index) {
    if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(stmt.getInt64(index));
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(stmt.getDouble(index));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return stmt.getText(index);
    } else if constexpr (std::is_same_v<T, bool>) {
        return stmt.getInt(index) != 0;
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
}

// Column implementation
template <typename T, typename Model>
Column<T, Model>::Column(const std::string& name, MemberPtr ptr,
                         const std::string& type,
                         const std::string& constraints)
    : name(name), ptr(ptr), customType(type), constraints(constraints) {}

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
std::string Column<T, Model>::getConstraints() const {
    return constraints;
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

template <typename T, typename Model>
void Column<T, Model>::bindToStatement(Statement& stmt, int index,
                                       const void* model) const {
    const Model* m = static_cast<const Model*>(model);
    ColumnValue<T>::bindToStatement(stmt, index, m->*ptr);
}

template <typename T, typename Model>
void Column<T, Model>::readFromStatement(const Statement& stmt, int index,
                                         void* model) const {
    Model* m = static_cast<Model*>(model);
    m->*ptr = ColumnValue<T>::readFromStatement(stmt, index);
}

// Table implementation
template <typename T>
Table<T>::Table(Database& db) : db(db) {
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
std::unique_ptr<Statement> Table<T>::prepareInsert(const T& model) {
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
std::unique_ptr<Statement> Table<T>::prepareUpdate(
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
T Table<T>::modelFromStatement(Statement& stmt) const {
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

// QueryBuilder::where implementations
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

}  // namespace lithium::database

#endif  // LITHIUM_DATABASE_ORM_HPP