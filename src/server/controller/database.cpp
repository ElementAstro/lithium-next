/*
 * database.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "database.hpp"

#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>

#include "atom/function/global_ptr.hpp"
#include "constant/constant.hpp"
#include "database/cache/cache_manager.hpp"

namespace lithium::server::controller {

DatabaseController::DatabaseController() {
    spdlog::debug("DatabaseController constructed");
    // Get weak reference to database instance
    mDatabase = atom::function::GetWeakPtr<lithium::database::core::Database>(
        Constants::DATABASE_MANAGER);
}

void DatabaseController::registerRoutes(ServerApp& app) {
    spdlog::info("Registering DatabaseController routes");

    // GET /api/v1/database/status - Connection status
    CROW_ROUTE(app, "/api/v1/database/status")
        .methods("GET"_method)([this](const crow::request& req) {
            return getStatus(req);
        });

    // GET /api/v1/database/tables - List tables
    CROW_ROUTE(app, "/api/v1/database/tables")
        .methods("GET"_method)([this](const crow::request& req) {
            return getTables(req);
        });

    // POST /api/v1/database/query - Execute SELECT query
    CROW_ROUTE(app, "/api/v1/database/query")
        .methods("POST"_method)([this](const crow::request& req) {
            return executeQuery(req);
        });

    // POST /api/v1/database/execute - Execute write statement
    CROW_ROUTE(app, "/api/v1/database/execute")
        .methods("POST"_method)([this](const crow::request& req) {
            return executeStatement(req);
        });

    // GET /api/v1/database/cache/stats - Cache statistics
    CROW_ROUTE(app, "/api/v1/database/cache/stats")
        .methods("GET"_method)([this](const crow::request& req) {
            return getCacheStats(req);
        });

    // POST /api/v1/database/cache/clear - Clear cache
    CROW_ROUTE(app, "/api/v1/database/cache/clear")
        .methods("POST"_method)([this](const crow::request& req) {
            return clearCache(req);
        });

    spdlog::info("DatabaseController routes registered successfully");
}

crow::response DatabaseController::getStatus(const crow::request& req) {
    spdlog::debug("GET /api/v1/database/status");

    try {
        auto db = getDatabase();
        if (!db) {
            spdlog::warn("Database instance is null");
            return ResponseBuilder::serviceUnavailable(
                "Database service is not available");
        }

        if (!db->isValid()) {
            spdlog::warn("Database connection is invalid");
            return ResponseBuilder::serviceUnavailable(
                "Database connection is invalid");
        }

        nlohmann::json data;
        data["status"] = "connected";
        data["valid"] = true;
        data["timestamp"] = std::chrono::system_clock::now().time_since_epoch()
                                .count();

        spdlog::debug("Database status check successful");
        return ResponseBuilder::success(data, "Database connection is active");

    } catch (const std::exception& e) {
        spdlog::error("Exception in getStatus: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error checking database status: ") + e.what());
    }
}

crow::response DatabaseController::getTables(const crow::request& req) {
    spdlog::debug("GET /api/v1/database/tables");

    try {
        auto db = getDatabase();
        if (!db) {
            spdlog::warn("Database instance is null");
            return ResponseBuilder::serviceUnavailable(
                "Database service is not available");
        }

        // Query to get all tables from SQLite
        std::string query =
            "SELECT name FROM sqlite_master WHERE type='table' AND "
            "name NOT LIKE 'sqlite_%' ORDER BY name;";

        auto stmt = db->prepare(query);
        if (!stmt) {
            spdlog::error("Failed to prepare statement for table listing");
            return ResponseBuilder::internalError(
                "Failed to prepare database statement");
        }

        nlohmann::json tables = nlohmann::json::array();
        while (stmt->step()) {
            nlohmann::json tableInfo;
            tableInfo["name"] = stmt->getText(0);
            tables.push_back(tableInfo);
        }

        nlohmann::json data;
        data["tables"] = tables;
        data["count"] = tables.size();

        spdlog::debug("Retrieved {} tables from database", tables.size());
        return ResponseBuilder::success(data);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getTables: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error retrieving tables: ") + e.what());
    }
}

crow::response DatabaseController::executeQuery(const crow::request& req) {
    spdlog::debug("POST /api/v1/database/query");

    try {
        auto db = getDatabase();
        if (!db) {
            spdlog::warn("Database instance is null");
            return ResponseBuilder::serviceUnavailable(
                "Database service is not available");
        }

        // Parse request body
        auto body = nlohmann::json::parse(req.body);

        if (!body.contains("sql") || !body["sql"].is_string()) {
            spdlog::warn("Missing or invalid 'sql' parameter in request");
            return ResponseBuilder::missingField("sql");
        }

        std::string sql = body["sql"].get<std::string>();

        // Validate that it's a SELECT query (case-insensitive)
        std::string upperSql = sql;
        std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(),
                       ::toupper);
        if (upperSql.find("SELECT") != 0) {
            spdlog::warn("Query endpoint only accepts SELECT statements");
            return ResponseBuilder::badRequest(
                "Query endpoint only accepts SELECT statements");
        }

        // Check for write operations in SELECT (subqueries with INSERT/UPDATE/DELETE)
        if (upperSql.find("INSERT") != std::string::npos ||
            upperSql.find("UPDATE") != std::string::npos ||
            upperSql.find("DELETE") != std::string::npos) {
            spdlog::warn("Write operations are not allowed in query endpoint");
            return ResponseBuilder::badRequest(
                "Write operations are not allowed in query endpoint");
        }

        auto stmt = db->prepare(sql);
        if (!stmt) {
            spdlog::error("Failed to prepare query: {}", sql);
            return ResponseBuilder::badRequest("Failed to prepare query");
        }

        nlohmann::json rows = nlohmann::json::array();
        int columnCount = stmt->getColumnCount();
        std::vector<std::string> columnNames;

        // Get column names
        for (int i = 0; i < columnCount; ++i) {
            columnNames.push_back(stmt->getColumnName(i));
        }

        // Fetch rows
        while (stmt->step()) {
            nlohmann::json row;
            for (int i = 0; i < columnCount; ++i) {
                if (stmt->isNull(i)) {
                    row[columnNames[i]] = nullptr;
                } else {
                    // Try to determine type and convert appropriately
                    try {
                        row[columnNames[i]] = stmt->getInt(i);
                    } catch (...) {
                        try {
                            row[columnNames[i]] = stmt->getDouble(i);
                        } catch (...) {
                            row[columnNames[i]] = stmt->getText(i);
                        }
                    }
                }
            }
            rows.push_back(row);
        }

        nlohmann::json data;
        data["rows"] = rows;
        data["count"] = rows.size();
        data["columns"] = columnNames;

        spdlog::debug("Query executed successfully, returned {} rows",
                      rows.size());
        return ResponseBuilder::success(data);

    } catch (const std::exception& e) {
        spdlog::error("Exception in executeQuery: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error executing query: ") + e.what());
    }
}

crow::response DatabaseController::executeStatement(
    const crow::request& req) {
    spdlog::debug("POST /api/v1/database/execute");

    try {
        auto db = getDatabase();
        if (!db) {
            spdlog::warn("Database instance is null");
            return ResponseBuilder::serviceUnavailable(
                "Database service is not available");
        }

        // Parse request body
        auto body = nlohmann::json::parse(req.body);

        if (!body.contains("sql") || !body["sql"].is_string()) {
            spdlog::warn("Missing or invalid 'sql' parameter in request");
            return ResponseBuilder::missingField("sql");
        }

        std::string sql = body["sql"].get<std::string>();

        // Validate that it's an allowed write statement (case-insensitive)
        std::string upperSql = sql;
        std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(),
                       ::toupper);

        bool isInsert = upperSql.find("INSERT") == 0;
        bool isUpdate = upperSql.find("UPDATE") == 0;
        bool isDelete = upperSql.find("DELETE") == 0;

        if (!isInsert && !isUpdate && !isDelete) {
            spdlog::warn(
                "Execute endpoint only accepts INSERT/UPDATE/DELETE statements");
            return ResponseBuilder::badRequest(
                "Execute endpoint only accepts INSERT/UPDATE/DELETE statements");
        }

        auto stmt = db->prepare(sql);
        if (!stmt) {
            spdlog::error("Failed to prepare statement: {}", sql);
            return ResponseBuilder::badRequest("Failed to prepare statement");
        }

        // Execute the statement
        if (!stmt->execute()) {
            spdlog::error("Failed to execute statement: {}", sql);
            return ResponseBuilder::internalError(
                "Failed to execute statement");
        }

        nlohmann::json data;
        data["success"] = true;
        data["statement_type"] = isInsert ? "INSERT" : (isUpdate ? "UPDATE" : "DELETE");

        spdlog::debug("Statement executed successfully");
        return ResponseBuilder::success(data, "Statement executed successfully");

    } catch (const std::exception& e) {
        spdlog::error("Exception in executeStatement: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error executing statement: ") + e.what());
    }
}

crow::response DatabaseController::getCacheStats(const crow::request& req) {
    spdlog::debug("GET /api/v1/database/cache/stats");

    try {
        auto& cacheManager = lithium::database::cache::CacheManager::getInstance();

        nlohmann::json data;
        data["size"] = cacheManager.size();
        data["default_ttl"] = 300;  // Default TTL in seconds
        data["timestamp"] = std::chrono::system_clock::now().time_since_epoch()
                                .count();

        spdlog::debug("Cache stats retrieved: size={}", data["size"]);
        return ResponseBuilder::success(data);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCacheStats: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error retrieving cache stats: ") + e.what());
    }
}

crow::response DatabaseController::clearCache(const crow::request& req) {
    spdlog::debug("POST /api/v1/database/cache/clear");

    try {
        auto& cacheManager = lithium::database::cache::CacheManager::getInstance();

        size_t previousSize = cacheManager.size();
        cacheManager.clear();

        nlohmann::json data;
        data["previous_size"] = previousSize;
        data["new_size"] = cacheManager.size();
        data["cleared"] = true;

        spdlog::info("Cache cleared: {} entries removed", previousSize);
        return ResponseBuilder::success(data, "Cache cleared successfully");

    } catch (const std::exception& e) {
        spdlog::error("Exception in clearCache: {}", e.what());
        return ResponseBuilder::internalError(
            std::string("Error clearing cache: ") + e.what());
    }
}

}  // namespace lithium::server::controller
