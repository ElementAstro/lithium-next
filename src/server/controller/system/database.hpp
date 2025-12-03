#ifndef LITHIUM_SERVER_CONTROLLER_DATABASE_HPP
#define LITHIUM_SERVER_CONTROLLER_DATABASE_HPP

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "../controller.hpp"
#include "database/database.hpp"
#include "spdlog/spdlog.h"

#include "../utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief HTTP Controller for database operations
 *
 * Provides REST API endpoints for:
 * - Database connection status
 * - Table management
 * - Query execution (SELECT only)
 * - Write operations (INSERT/UPDATE/DELETE)
 * - Cache management and statistics
 *
 * All routes are prefixed with /api/v1/database
 */
class DatabaseController : public Controller {
private:
    std::weak_ptr<lithium::database::core::Database> mDatabase;

    /**
     * @brief Get or create database connection reference
     */
    std::shared_ptr<lithium::database::core::Database> getDatabase() {
        return mDatabase.lock();
    }

    // Route handlers
    crow::response getStatus(const crow::request& req);
    crow::response getTables(const crow::request& req);
    crow::response executeQuery(const crow::request& req);
    crow::response executeStatement(const crow::request& req);
    crow::response getCacheStats(const crow::request& req);
    crow::response clearCache(const crow::request& req);

public:
    explicit DatabaseController();
    void registerRoutes(ServerApp& app) override;
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_DATABASE_HPP
