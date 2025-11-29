#include "statement.hpp"

#include <spdlog/spdlog.h>

#include "database.hpp"

namespace lithium::database::core {

//------------------------------------------------------------------------------
// Statement Implementation
//------------------------------------------------------------------------------

Statement::Statement(Database& db, const std::string& sql)
    : db(db), stmt(nullptr, sqlite3_finalize), sql(sql) {
    sqlite3_stmt* raw_stmt = nullptr;
    int result =
        sqlite3_prepare_v2(db.get(), sql.c_str(), -1, &raw_stmt, nullptr);
    stmt.reset(raw_stmt);

    if (result != SQLITE_OK) {
        std::string error = "Failed to prepare SQL statement: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }

    spdlog::info("Prepared statement: {}", sql);
}

Statement::~Statement() {
    // sqlite3_finalize is called by the custom deleter in stmt's destructor
}

Statement& Statement::bind(int index, int value) {
    validateIndex(index, true);
    int result = sqlite3_bind_int(stmt.get(), index, value);
    if (result != SQLITE_OK) {
        std::string error = "Failed to bind int parameter: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

Statement& Statement::bind(int index, int64_t value) {
    validateIndex(index, true);
    int result = sqlite3_bind_int64(stmt.get(), index, value);
    if (result != SQLITE_OK) {
        std::string error = "Failed to bind int64 parameter: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

Statement& Statement::bind(int index, double value) {
    validateIndex(index, true);
    int result = sqlite3_bind_double(stmt.get(), index, value);
    if (result != SQLITE_OK) {
        std::string error = "Failed to bind double parameter: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

Statement& Statement::bind(int index, const std::string& value) {
    validateIndex(index, true);
    // SQLITE_TRANSIENT makes SQLite copy the data
    int result = sqlite3_bind_text(stmt.get(), index, value.c_str(), -1,
                                   SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        std::string error = "Failed to bind text parameter: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

Statement& Statement::bindNull(int index) {
    validateIndex(index, true);
    int result = sqlite3_bind_null(stmt.get(), index);
    if (result != SQLITE_OK) {
        std::string error = "Failed to bind NULL parameter: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

bool Statement::execute() {
    int result = sqlite3_step(stmt.get());
    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        std::string error = "Failed to execute statement: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_SQL_EXECUTION_ERROR(error);
    }
    return true;
}

bool Statement::step() {
    int result = sqlite3_step(stmt.get());
    if (result == SQLITE_ROW) {
        return true;
    }

    if (result != SQLITE_DONE) {
        std::string error = "Failed to step statement: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_SQL_EXECUTION_ERROR(error);
    }

    return false;
}

Statement& Statement::reset() {
    if (sqlite3_reset(stmt.get()) != SQLITE_OK) {
        std::string error = "Failed to reset statement: ";
        error += sqlite3_errmsg(db.get());
        spdlog::error("{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

int Statement::getInt(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_INTEGER)) {
        spdlog::warn(
            "Column type mismatch: expected INTEGER, converting anyway");
    }
    return sqlite3_column_int(stmt.get(), index);
}

int64_t Statement::getInt64(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_INTEGER)) {
        spdlog::warn(
            "Column type mismatch: expected INTEGER, converting anyway");
    }
    return sqlite3_column_int64(stmt.get(), index);
}

double Statement::getDouble(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_FLOAT)) {
        spdlog::warn("Column type mismatch: expected FLOAT, converting anyway");
    }
    return sqlite3_column_double(stmt.get(), index);
}

std::string Statement::getText(int index) const {
    validateIndex(index, false);
    if (isNull(index)) {
        return "";
    }

    if (!checkColumnType(index, SQLITE_TEXT)) {
        spdlog::warn("Column type mismatch: expected TEXT, converting anyway");
    }

    const unsigned char* text = sqlite3_column_text(stmt.get(), index);
    if (!text) {
        return "";
    }
    return reinterpret_cast<const char*>(text);
}

std::vector<uint8_t> Statement::getBlob(int index) const {
    validateIndex(index, false);
    if (isNull(index)) {
        return {};
    }

    if (!checkColumnType(index, SQLITE_BLOB)) {
        spdlog::warn("Column type mismatch: expected BLOB, converting anyway");
    }

    const void* blob = sqlite3_column_blob(stmt.get(), index);
    int size = sqlite3_column_bytes(stmt.get(), index);

    if (!blob || size <= 0) {
        return {};
    }

    const uint8_t* data = static_cast<const uint8_t*>(blob);
    return std::vector<uint8_t>(data, data + size);
}

bool Statement::isNull(int index) const {
    validateIndex(index, false);
    return sqlite3_column_type(stmt.get(), index) == SQLITE_NULL;
}

int Statement::getColumnCount() const {
    return sqlite3_column_count(stmt.get());
}

std::string Statement::getColumnName(int index) const {
    validateIndex(index, false);
    const char* name = sqlite3_column_name(stmt.get(), index);
    return name ? name : "";
}

sqlite3_stmt* Statement::get() const { return stmt.get(); }

std::string Statement::getSql() const { return sql; }

void Statement::validateIndex(int index, bool isParam) const {
    if (isParam) {
        // Parameter indices are 1-based
        if (index <= 0 || index > sqlite3_bind_parameter_count(stmt.get())) {
            THROW_VALIDATION_ERROR("Parameter index out of bounds: " +
                                   std::to_string(index));
        }
    } else {
        // Column indices are 0-based
        if (index < 0 || index >= sqlite3_column_count(stmt.get())) {
            THROW_VALIDATION_ERROR("Column index out of bounds: " +
                                   std::to_string(index));
        }
    }
}

bool Statement::checkColumnType(int index, int expectedType) const {
    return sqlite3_column_type(stmt.get(), index) == expectedType;
}

}  // namespace lithium::database::core
