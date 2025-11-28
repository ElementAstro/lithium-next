#ifndef LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP
#define LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP

#include <crow.h>
#include <string>
#include <unordered_set>

#include "atom/log/loguru.hpp"

namespace lithium::server::middleware {

/**
 * @brief API Key authentication middleware for REST API endpoints
 * 
 * This middleware validates the X-API-Key header in all incoming requests.
 * If the key is missing or invalid, it responds with 401 Unauthorized.
 */
struct ApiKeyAuth {
    struct context {
        bool authenticated = false;
        std::string api_key;
    };

    // Valid API keys (in production, these should be stored securely)
    static std::unordered_set<std::string> valid_keys;

    /**
     * @brief Add a valid API key
     */
    static void addApiKey(const std::string& key) {
        valid_keys.insert(key);
    }

    /**
     * @brief Remove an API key
     */
    static void revokeApiKey(const std::string& key) {
        valid_keys.erase(key);
    }

    /**
     * @brief Check if an API key is valid
     */
    static bool isValidApiKey(const std::string& key) {
        return valid_keys.find(key) != valid_keys.end();
    }

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        // Extract API key from X-API-Key header
        auto api_key_header = req.get_header_value("X-API-Key");

        if (api_key_header.empty()) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(R"({
                "status": "error",
                "error": {
                    "code": "missing_api_key",
                    "message": "No API key provided. Include the X-API-Key header with your request."
                }
            })");
            res.end();
            LOG_F(WARNING, "Request to {} rejected: Missing API key", req.url);
            return;
        }

        if (!isValidApiKey(api_key_header)) {
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.write(R"({
                "status": "error",
                "error": {
                    "code": "invalid_api_key",
                    "message": "The provided API key is invalid or has been revoked."
                }
            })");
            res.end();
            LOG_F(WARNING, "Request to {} rejected: Invalid API key", req.url);
            return;
        }

        // Authentication successful
        ctx.authenticated = true;
        ctx.api_key = api_key_header;
        LOG_F(INFO, "Request to {} authenticated successfully", req.url);
    }

    void after_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/) {
        // No action needed after handling
    }
};

// Initialize static member
inline std::unordered_set<std::string> ApiKeyAuth::valid_keys = {
    "default-api-key-please-change-in-production"
};

/**
 * @brief CORS middleware for cross-origin requests
 */
struct CORS {
    struct context {};

    void before_handle(crow::request& /*req*/, crow::response& /*res*/, context& /*ctx*/) {
        // CORS headers are set in after_handle
    }

    void after_handle(crow::request& /*req*/, crow::response& res, context& /*ctx*/) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
        res.add_header("Access-Control-Max-Age", "3600");
    }
};

/**
 * @brief Request logging middleware
 */
struct RequestLogger : crow::ILocalMiddleware {
    struct context {
        std::chrono::steady_clock::time_point start_time;
    };

    void before_handle(crow::request& req, crow::response& /*res*/, context& ctx) {
        ctx.start_time = std::chrono::steady_clock::now();
        LOG_F(INFO, "Incoming request: {} {}", req.method_str(), req.url);
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ctx.start_time
        );
        LOG_F(INFO, "Request completed: {} {} - Status: {} - Duration: {}ms",
              req.method_str(), req.url, res.code, duration.count());
    }
};

}  // namespace lithium::server::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP
