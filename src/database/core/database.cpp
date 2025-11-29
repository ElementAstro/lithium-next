#include "database.hpp"

#include <spdlog/spdlog.h>

#include "statement.hpp"
#include "transaction.hpp"

namespace lithium::database::core {

//------------------------------------------------------------------------------
// Database Implementation
//------------------------------------------------------------------------------

Database::Database(const std::string& db_name, int flags)
    : db(nullptr, sqlite3_close) {
    sqlite3* raw_db = nullptr;
    int result = sqlite3_open_v2(db_name.c_str(), &raw_db, flags, nullptr);
    db.reset(raw_db);

    if (result != SQLITE_OK) {
        std::string error_msg = "Can't open database: ";
        if (raw_db) {
            error_msg += sqlite3_errmsg(raw_db);
        } else {
            error_msg += "Unknown error";
        }
        spdlog::error("{}", error_msg);
        THROW_FAILED_TO_OPEN_DATABASE(error_msg);
    }

    // Configure for better performance and reliability
    try {
        execute("PRAGMA foreign_keys = ON;");
        execute("PRAGMA journal_mode = WAL;");
        execute("PRAGMA synchronous = NORMAL;");
        valid.store(true);
        spdlog::info("Database opened successfully: {}", db_name);
    } catch (const std::exception& e) {
        spdlog::error("Failed to configure database: {}", e.what());
        throw;
    }
}

Database::~Database() {
    try {
        if (valid.load()) {
            execute("PRAGMA optimize;");
        }
    } catch (const std::exception& e) {
        spdlog::warn("Error during database cleanup: {}", e.what());
    }
    valid.store(false);
}

Database::Database(Database&& other) noexcept : db(std::move(other.db)) {
    valid.store(other.valid.load());
    other.valid.store(false);
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        db = std::move(other.db);
        valid.store(other.valid.load());
        other.valid.store(false);
    }
    return *this;
}

sqlite3* Database::get() {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to use an invalid database connection");
    }
    return db.get();
}

std::unique_ptr<Statement> Database::prepare(const std::string& sql) {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to prepare statement on invalid database connection");
    }
    return std::make_unique<Statement>(*this, sql);
}

std::unique_ptr<Transaction> Database::beginTransaction() {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to begin transaction on invalid database connection");
    }
    return std::make_unique<Transaction>(*this);
}

void Database::execute(const std::string& sql) {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to execute SQL on invalid database connection");
    }

    char* errMsg = nullptr;
    int result = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &errMsg);

    if (result != SQLITE_OK) {
        std::string error = "SQL Error: ";
        if (errMsg) {
            error += errMsg;
            sqlite3_free(errMsg);
        } else {
            error += "Unknown error";
        }
        spdlog::error("{}", error);
        THROW_SQL_EXECUTION_ERROR(error);
    }
}

bool Database::isValid() const noexcept { return valid.load(); }

void Database::configure(
    const std::unordered_map<std::string, std::string>& pragmas) {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to configure invalid database connection");
    }

    for (const auto& [name, value] : pragmas) {
        try {
            std::string sql = "PRAGMA " + name + " = " + value + ";";
            execute(sql);
            spdlog::info("Set PRAGMA {}: {}", name, value);
        } catch (const std::exception& e) {
            spdlog::error("Failed to set PRAGMA {}: {}", name, e.what());
            throw;
        }
    }
}

void Database::commit() {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to commit on invalid database connection");
    }
    try {
        execute("COMMIT;");
        spdlog::info("Database transaction committed via Database::commit()");
    } catch (const std::exception& e) {
        spdlog::error("Failed to commit transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to commit transaction: " +
                                std::string(e.what()));
    }
}

void Database::rollback() {
    if (!valid.load()) {
        THROW_VALIDATION_ERROR(
            "Attempted to rollback on invalid database connection");
    }
    try {
        execute("ROLLBACK;");
        spdlog::info(
            "Database transaction rolled back via Database::rollback()");
    } catch (const std::exception& e) {
        spdlog::error("Failed to rollback transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to rollback transaction: " +
                                std::string(e.what()));
    }
}

}  // namespace lithium::database::core
