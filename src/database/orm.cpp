#include "orm.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>

namespace lithium::database {

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
        LOG_F(ERROR, "{}", error_msg);
        THROW_FAILED_TO_OPEN_DATABASE(error_msg);
    }

    // Configure for better performance and reliability
    try {
        execute("PRAGMA foreign_keys = ON;");
        execute("PRAGMA journal_mode = WAL;");
        execute("PRAGMA synchronous = NORMAL;");
        valid.store(true);
        LOG_F(INFO, "Database opened successfully: {}", db_name);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to configure database: {}", e.what());
        throw;
    }
}

Database::~Database() {
    try {
        if (valid.load()) {
            execute("PRAGMA optimize;");
        }
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Error during database cleanup: {}", e.what());
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
        LOG_F(ERROR, "{}", error);
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
            LOG_F(INFO, "Set PRAGMA {}: {}", name, value);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to set PRAGMA {}: {}", name, e.what());
            throw;
        }
    }
}

//------------------------------------------------------------------------------
// Transaction Implementation
//------------------------------------------------------------------------------

Transaction::Transaction(Database& db)
    : db(db), committed(false), rolledBack(false) {
    try {
        db.execute("BEGIN TRANSACTION;");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to begin transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to begin transaction: " +
                                std::string(e.what()));
    }
}

Transaction::~Transaction() {
    // Auto-rollback if not committed or rolled back explicitly
    if (!committed && !rolledBack) {
        try {
            rollback();
        } catch (const std::exception& e) {
            LOG_F(ERROR,
                  "Failed to auto-rollback transaction in destructor: {}",
                  e.what());
        }
    }
}

void Transaction::commit() {
    if (committed || rolledBack) {
        THROW_TRANSACTION_ERROR("Transaction already committed or rolled back");
    }

    try {
        db.execute("COMMIT;");
        committed = true;
        LOG_F(INFO, "Transaction committed successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to commit transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to commit transaction: " +
                                std::string(e.what()));
    }
}

void Transaction::rollback() {
    if (committed || rolledBack) {
        THROW_TRANSACTION_ERROR("Transaction already committed or rolled back");
    }

    try {
        db.execute("ROLLBACK;");
        rolledBack = true;
        LOG_F(INFO, "Transaction rolled back successfully");
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to rollback transaction: {}", e.what());
        THROW_TRANSACTION_ERROR("Failed to rollback transaction: " +
                                std::string(e.what()));
    }
}

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
        LOG_F(ERROR, "{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }

    LOG_F(INFO, "Prepared statement: {}", sql);
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
        LOG_F(ERROR, "{}", error);
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
        LOG_F(ERROR, "{}", error);
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
        LOG_F(ERROR, "{}", error);
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
        LOG_F(ERROR, "{}", error);
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
        LOG_F(ERROR, "{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

bool Statement::execute() {
    int result = sqlite3_step(stmt.get());
    if (result != SQLITE_DONE && result != SQLITE_ROW) {
        std::string error = "Failed to execute statement: ";
        error += sqlite3_errmsg(db.get());
        LOG_F(ERROR, "{}", error);
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
        LOG_F(ERROR, "{}", error);
        THROW_SQL_EXECUTION_ERROR(error);
    }

    return false;
}

Statement& Statement::reset() {
    if (sqlite3_reset(stmt.get()) != SQLITE_OK) {
        std::string error = "Failed to reset statement: ";
        error += sqlite3_errmsg(db.get());
        LOG_F(ERROR, "{}", error);
        THROW_PREPARE_STATEMENT_ERROR(error);
    }
    return *this;
}

int Statement::getInt(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_INTEGER)) {
        LOG_F(WARNING,
              "Column type mismatch: expected INTEGER, converting anyway");
    }
    return sqlite3_column_int(stmt.get(), index);
}

int64_t Statement::getInt64(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_INTEGER)) {
        LOG_F(WARNING,
              "Column type mismatch: expected INTEGER, converting anyway");
    }
    return sqlite3_column_int64(stmt.get(), index);
}

double Statement::getDouble(int index) const {
    validateIndex(index, false);
    if (!checkColumnType(index, SQLITE_FLOAT)) {
        LOG_F(WARNING,
              "Column type mismatch: expected FLOAT, converting anyway");
    }
    return sqlite3_column_double(stmt.get(), index);
}

std::string Statement::getText(int index) const {
    validateIndex(index, false);
    if (isNull(index)) {
        return "";
    }

    if (!checkColumnType(index, SQLITE_TEXT)) {
        LOG_F(WARNING,
              "Column type mismatch: expected TEXT, converting anyway");
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
        LOG_F(WARNING,
              "Column type mismatch: expected BLOB, converting anyway");
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

//------------------------------------------------------------------------------
// CacheManager Implementation
//------------------------------------------------------------------------------

CacheManager::CacheManager() : defaultTTL(300), stopPurgeThread(false) {
    // Start purge thread
    purgeThread = std::thread(&CacheManager::purgePeriodically, this);
}

CacheManager::~CacheManager() {
    // Stop the purge thread
    stopPurgeThread.store(true);
    if (purgeThread.joinable()) {
        purgeThread.join();
    }
}

CacheManager& CacheManager::getInstance() {
    static CacheManager instance;
    return instance;
}

void CacheManager::put(const std::string& key, const std::string& value,
                       int ttlSeconds) {
    if (key.empty()) {
        LOG_F(WARNING, "Attempted to cache with empty key");
        return;
    }

    if (ttlSeconds <= 0) {
        ttlSeconds = defaultTTL;
    }

    auto expiry =
        std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    cache[key] = CacheEntry{value, expiry};
}

std::optional<std::string> CacheManager::get(const std::string& key) {
    if (key.empty()) {
        return std::nullopt;
    }

    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = cache.find(key);
    if (it != cache.end()) {
        // Check if entry has expired
        if (std::chrono::steady_clock::now() < it->second.expiry) {
            return it->second.value;
        }
    }
    return std::nullopt;
}

bool CacheManager::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    return cache.erase(key) > 0;
}

void CacheManager::clear() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    cache.clear();
}

size_t CacheManager::purgeExpired() {
    const auto now = std::chrono::steady_clock::now();
    size_t removedCount = 0;

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    for (auto it = cache.begin(); it != cache.end();) {
        if (now >= it->second.expiry) {
            it = cache.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    return removedCount;
}

void CacheManager::setDefaultTTL(int seconds) {
    if (seconds > 0) {
        defaultTTL = seconds;
    }
}

size_t CacheManager::size() const {
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    return cache.size();
}

void CacheManager::purgePeriodically() {
    using namespace std::chrono_literals;

    try {
        while (!stopPurgeThread.load()) {
            // Sleep for a while
            std::this_thread::sleep_for(60s);  // Check every minute

            size_t removed = purgeExpired();
            if (removed > 0) {
                LOG_F(INFO, "Cache purge: removed {} expired entries", removed);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in cache purge thread: {}", e.what());
    }
}

// Template instantiations to avoid undefined reference errors
// These are commonly used types that need explicit instantiation

// ColumnValue<T>::bindToStatement
template void ColumnValue<int>::bindToStatement(Statement&, int, const int&);
template void ColumnValue<int64_t>::bindToStatement(Statement&, int,
                                                    const int64_t&);
template void ColumnValue<double>::bindToStatement(Statement&, int,
                                                   const double&);
template void ColumnValue<std::string>::bindToStatement(Statement&, int,
                                                        const std::string&);
template void ColumnValue<bool>::bindToStatement(Statement&, int, const bool&);

// ColumnValue<T>::readFromStatement
template int ColumnValue<int>::readFromStatement(const Statement&, int);
template int64_t ColumnValue<int64_t>::readFromStatement(const Statement&, int);
template double ColumnValue<double>::readFromStatement(const Statement&, int);
template std::string ColumnValue<std::string>::readFromStatement(
    const Statement&, int);
template bool ColumnValue<bool>::readFromStatement(const Statement&, int);

// QueryBuilder::where - template specializations
template QueryBuilder& QueryBuilder::where<>(const std::string&);

}  // namespace lithium::database
