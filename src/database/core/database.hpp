#ifndef LITHIUM_DATABASE_CORE_DATABASE_HPP
#define LITHIUM_DATABASE_CORE_DATABASE_HPP

#include <sqlite3.h>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>

#include "types.hpp"

namespace lithium::database::core {

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

}  // namespace lithium::database::core

#endif  // LITHIUM_DATABASE_CORE_DATABASE_HPP
