#ifndef LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP
#define LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP

#include <crow.h>
#include <chrono>
#include <string>
#include <unordered_set>

#include "../models/api.hpp"
#include "../rate_limiter.hpp"
#include "atom/log/spdlog_logger.hpp"

namespace lithium::server::middleware {

/**
 * @brief Rate limiting middleware to prevent abuse
 *
 * This middleware uses a token bucket algorithm with IP-based limiting.
 * It runs BEFORE authentication to prevent brute force attacks.
 */
struct RateLimiterMiddleware {
    struct context {
        bool rate_limited = false;
    };

    // Configurable rate limiter: 100 tokens, refill every 1 second
    static inline RateLimiter limiter{100, std::chrono::milliseconds(1000)};

    void before_handle(crow::request& req, crow::response& res, context& ctx) {
        if (!limiter.allow_request_for_ip(req.remote_ip_address)) {
            ctx.rate_limited = true;
            auto request_id = lithium::models::api::generateRequestId();
            auto body = lithium::models::api::makeError(
                "rate_limited", "Too many requests. Please try again later.",
                request_id);
            res.code = 429;
            res.set_header("Content-Type", "application/json");
            res.set_header("X-Request-ID", request_id);
            res.set_header("Retry-After", "60");
            res.write(body.dump());
            res.end();
            LOG_WARN("Request from {} rate limited (request_id: {})",
                     req.remote_ip_address, request_id);
        }
    }

    void after_handle(crow::request& /*req*/, crow::response& /*res*/,
                      context& /*ctx*/) {
        // No action needed
    }
};

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
    static void addApiKey(const std::string& key) { valid_keys.insert(key); }

    /**
     * @brief Remove an API key
     */
    static void revokeApiKey(const std::string& key) { valid_keys.erase(key); }

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
            auto request_id = lithium::models::api::generateRequestId();
            auto body = lithium::models::api::makeError(
                "missing_api_key",
                "No API key provided. Include the X-API-Key header with your "
                "request.",
                request_id);
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.set_header("X-Request-ID", request_id);
            res.write(body.dump());
            res.end();
            LOG_WARN("Request to {} rejected: Missing API key (request_id: {})",
                     req.url, request_id);
            return;
        }

        if (!isValidApiKey(api_key_header)) {
            auto request_id = lithium::models::api::generateRequestId();
            auto body = lithium::models::api::makeError(
                "invalid_api_key",
                "The provided API key is invalid or has been revoked.",
                request_id);
            res.code = 401;
            res.set_header("Content-Type", "application/json");
            res.set_header("X-Request-ID", request_id);
            res.write(body.dump());
            res.end();
            LOG_WARN("Request to {} rejected: Invalid API key (request_id: {})",
                     req.url, request_id);
            return;
        }

        // Authentication successful
        ctx.authenticated = true;
        ctx.api_key = api_key_header;
        LOG_INFO("Request to {} authenticated successfully", req.url);
    }

    void after_handle(crow::request& /*req*/, crow::response& /*res*/,
                      context& /*ctx*/) {
        // No action needed after handling
    }
};

// Initialize static member
inline std::unordered_set<std::string> ApiKeyAuth::valid_keys = {
    "default-api-key-please-change-in-production"};

/**
 * @brief CORS middleware for cross-origin requests
 */
struct CORS {
    struct context {};

    void before_handle(crow::request& /*req*/, crow::response& /*res*/,
                       context& /*ctx*/) {
        // CORS headers are set in after_handle
    }

    void after_handle(crow::request& /*req*/, crow::response& res,
                      context& /*ctx*/) {
        res.add_header("Access-Control-Allow-Origin", "*");
        res.add_header("Access-Control-Allow-Methods",
                       "GET, POST, PUT, DELETE, OPTIONS");
        res.add_header("Access-Control-Allow-Headers",
                       "Content-Type, X-API-Key");
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

    void before_handle(crow::request& req, crow::response& /*res*/,
                       context& ctx) {
        ctx.start_time = std::chrono::steady_clock::now();
        LOG_INFO("Incoming request: {} {}", req.method_str(), req.url);
    }

    void after_handle(crow::request& req, crow::response& res, context& ctx) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ctx.start_time);
        LOG_INFO("Request completed: {} {} - Status: {} - Duration: {}ms",
                 req.method_str(), req.url, res.code, duration.count());
    }
};

}  // namespace lithium::server::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_AUTH_HPP
