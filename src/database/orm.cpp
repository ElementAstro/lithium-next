#include "orm.hpp"

namespace lithium::database {

Database::Database(const std::string& db_name) {
    if (sqlite3_open(db_name.c_str(), &db)) {
        THROW_FAILED_TO_OPEN_DATABASE("Can't open database: " +
                                      std::string(sqlite3_errmsg(db)));
    }
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

sqlite3* Database::get() { return db; }

void Database::beginTransaction() { execute("BEGIN TRANSACTION;"); }

void Database::commit() { execute("COMMIT;"); }

void Database::rollback() { execute("ROLLBACK;"); }

void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        LOG_F(ERROR, "SQL Error: {}", errMsg);
        sqlite3_free(errMsg);
    }
}

CacheManager& CacheManager::getInstance() {
    static CacheManager instance;
    return instance;
}

void CacheManager::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache[key] = CacheEntry{value, std::chrono::steady_clock::now()};
}

bool CacheManager::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto it = cache.find(key);
    if (it != cache.end()) {
        if (std::chrono::steady_clock::now() - it->second.timestamp <
            std::chrono::minutes(5)) {
            value = it->second.value;
            return true;
        }
        cache.erase(it);
    }
    return false;
}
}  // namespace lithium::database