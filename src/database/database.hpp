#ifndef LITHIUM_DATABASE_HPP
#define LITHIUM_DATABASE_HPP

/**
 * @file database.hpp
 * @brief Unified facade header for the lithium database module.
 *
 * This header provides access to all database module components:
 * - Core: Database, Transaction, Statement, exception types
 * - ORM: Column, ColumnValue, Table templates
 * - Query: QueryBuilder for fluent SQL construction
 * - Cache: CacheManager for query result caching
 *
 * @par Usage Example:
 * @code
 * #include "database/database.hpp"
 *
 * using namespace lithium::database;
 *
 * // Open a database connection
 * core::Database db("mydata.db");
 *
 * // Execute simple SQL
 * db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)");
 *
 * // Use prepared statements
 * auto stmt = db.prepare("INSERT INTO users (name) VALUES (?)");
 * stmt->bind(1, std::string("Alice"));
 * stmt->execute();
 *
 * // Use transactions
 * auto txn = db.beginTransaction();
 * // ... operations ...
 * txn->commit();
 *
 * // Use query builder
 * query::QueryBuilder qb("users");
 * auto sql = qb.select({"id", "name"})
 *              .where("id > ?", 0)
 *              .orderBy("name")
 *              .limit(10)
 *              .build();
 * @endcode
 */

#include <memory>
#include <string>
#include <unordered_map>

// Core components
#include "core/database.hpp"
#include "core/statement.hpp"
#include "core/transaction.hpp"
#include "core/types.hpp"

// ORM components
#include "orm/column.hpp"
#include "orm/column_base.hpp"
#include "orm/column_value.hpp"
#include "orm/table.hpp"

// Query building
#include "query/query_builder.hpp"

// Caching
#include "cache/cache_entry.hpp"
#include "cache/cache_manager.hpp"

namespace lithium::database {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Database module version.
 */
inline constexpr const char* DATABASE_VERSION = "1.1.0";

/**
 * @brief Get database module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDatabaseVersion() noexcept {
    return DATABASE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

// Bring core types into main database namespace for convenience
using core::Database;
using core::Statement;
using core::Transaction;

// Exception types
using core::DatabaseOpenError;
using core::SqlExecutionError;
using core::StatementPrepareError;
using core::TransactionError;
using core::ValidationError;

// Query builder
using query::QueryBuilder;

// Cache
using cache::CacheEntry;
using cache::CacheManager;

/// Unique pointer to Database
using DatabasePtr = std::unique_ptr<Database>;

/// Unique pointer to Statement
using StatementPtr = std::unique_ptr<Statement>;

/// Unique pointer to Transaction
using TransactionPtr = std::unique_ptr<Transaction>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new Database instance.
 * @param dbName The name of the database file.
 * @return Unique pointer to the new Database.
 */
[[nodiscard]] inline DatabasePtr createDatabase(const std::string& dbName) {
    return std::make_unique<Database>(dbName);
}

/**
 * @brief Create a new Database instance with custom flags.
 * @param dbName The name of the database file.
 * @param flags SQLite open flags.
 * @return Unique pointer to the new Database.
 */
[[nodiscard]] inline DatabasePtr createDatabase(const std::string& dbName,
                                                int flags) {
    return std::make_unique<Database>(dbName, flags);
}

/**
 * @brief Create a new QueryBuilder instance.
 * @param tableName The name of the table to query.
 * @return QueryBuilder instance.
 */
[[nodiscard]] inline QueryBuilder createQueryBuilder(
    const std::string& tableName) {
    return QueryBuilder(tableName);
}

/**
 * @brief Get the CacheManager singleton instance.
 * @return Reference to the CacheManager.
 */
[[nodiscard]] inline CacheManager& getCacheManager() {
    return CacheManager::getInstance();
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Execute a simple SQL statement on a database.
 * @param db The database connection.
 * @param sql The SQL statement to execute.
 */
inline void executeSQL(Database& db, const std::string& sql) {
    db.execute(sql);
}

/**
 * @brief Begin a transaction on a database.
 * @param db The database connection.
 * @return Unique pointer to the Transaction.
 */
[[nodiscard]] inline TransactionPtr beginTransaction(Database& db) {
    return db.beginTransaction();
}

/**
 * @brief Prepare a statement on a database.
 * @param db The database connection.
 * @param sql The SQL statement to prepare.
 * @return Unique pointer to the Statement.
 */
[[nodiscard]] inline StatementPtr prepareStatement(Database& db,
                                                   const std::string& sql) {
    return db.prepare(sql);
}

/**
 * @brief Check if a database connection is valid.
 * @param db The database connection.
 * @return True if the connection is valid.
 */
[[nodiscard]] inline bool isDatabaseValid(const Database& db) {
    return db.isValid();
}

/**
 * @brief Configure database connection with PRAGMA settings.
 * @param db The database connection.
 * @param pragmas Map of PRAGMA name to value.
 */
inline void configureDatabase(
    Database& db, const std::unordered_map<std::string, std::string>& pragmas) {
    db.configure(pragmas);
}

/**
 * @brief Create default PRAGMA configuration for optimal performance.
 * @return Map of PRAGMA settings.
 */
[[nodiscard]] inline std::unordered_map<std::string, std::string>
createDefaultPragmas() {
    return {{"foreign_keys", "ON"},
            {"journal_mode", "WAL"},
            {"synchronous", "NORMAL"},
            {"cache_size", "-64000"},
            {"temp_store", "MEMORY"}};
}

}  // namespace lithium::database

#endif  // LITHIUM_DATABASE_HPP
