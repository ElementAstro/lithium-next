#ifndef LITHIUM_DATABASE_CORE_STATEMENT_HPP
#define LITHIUM_DATABASE_CORE_STATEMENT_HPP

#include <sqlite3.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::database::core {

// Forward declaration
class Database;

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

}  // namespace lithium::database::core

#endif  // LITHIUM_DATABASE_CORE_STATEMENT_HPP
