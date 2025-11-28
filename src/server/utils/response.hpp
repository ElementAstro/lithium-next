#ifndef LITHIUM_SERVER_UTILS_RESPONSE_HPP
#define LITHIUM_SERVER_UTILS_RESPONSE_HPP

#include <crow.h>
#include "atom/type/json.hpp"
#include "../models/api.hpp"

namespace lithium::server::utils {

/**
 * @brief Utility class for building standardized HTTP responses
 *
 * All responses follow the format:
 * - Success: {"success": true, "request_id": "...", "data": {...}}
 * - Error: {"success": false, "request_id": "...", "error": {"code": "...", "message": "..."}}
 *
 * The X-Request-ID header is also set for correlation.
 */
class ResponseBuilder {
public:
    /**
     * @brief Create a 200 OK success response
     */
    static crow::response success(const nlohmann::json& data,
                                  const std::string& message = "") {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeSuccess(data, request_id, message);
        return makeResponse(200, request_id, body);
    }

    /**
     * @brief Create a 201 Created response
     */
    static crow::response created(const nlohmann::json& data,
                                  const std::string& message = "") {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeSuccess(data, request_id, message);
        return makeResponse(201, request_id, body);
    }

    /**
     * @brief Create a 202 Accepted response (for async operations)
     */
    static crow::response accepted(const std::string& message,
                                   const nlohmann::json& data = {}) {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeAccepted(data, request_id, message);
        return makeResponse(202, request_id, body);
    }

    /**
     * @brief Create a 200 OK success response with message
     */
    static crow::response successWithMessage(const std::string& message,
                                             const nlohmann::json& data = {}) {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeSuccess(data, request_id, message);
        return makeResponse(200, request_id, body);
    }

    /**
     * @brief Create a 204 No Content response
     */
    static crow::response noContent() {
        crow::response res(204);
        res.set_header("X-Request-ID", lithium::models::api::generateRequestId());
        return res;
    }

    /**
     * @brief Create a generic error response
     */
    static crow::response error(int status,
                                const std::string& code,
                                const std::string& message,
                                const nlohmann::json& details = {}) {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeError(code, message, request_id, details);
        return makeResponse(status, request_id, body);
    }

    /**
     * @brief Create a 400 Bad Request response
     */
    static crow::response badRequest(const std::string& message,
                                     const nlohmann::json& details = {}) {
        return error(400, "bad_request", message, details);
    }

    /**
     * @brief Create a 401 Unauthorized response
     */
    static crow::response unauthorized(const std::string& message = "Unauthorized") {
        return error(401, "unauthorized", message);
    }

    /**
     * @brief Create a 403 Forbidden response
     */
    static crow::response forbidden(const std::string& message = "Forbidden") {
        return error(403, "forbidden", message);
    }

    /**
     * @brief Create a 404 Not Found response
     */
    static crow::response notFound(const std::string& resource) {
        return error(404, "not_found", resource + " not found");
    }

    /**
     * @brief Create a 404 Not Found response with resource type and name
     */
    static crow::response notFound(const std::string& resourceType, const std::string& resourceName) {
        nlohmann::json details = {
            {"type", resourceType},
            {"name", resourceName}
        };
        return error(404, "not_found", resourceType + " '" + resourceName + "' not found", details);
    }

    /**
     * @brief Create a 404 Device Not Found response
     */
    static crow::response deviceNotFound(const std::string& deviceId,
                                         const std::string& deviceType) {
        nlohmann::json details = {
            {"deviceId", deviceId},
            {"deviceType", deviceType}
        };
        return error(404, "device_not_found", deviceType + " not found", details);
    }

    /**
     * @brief Create a 409 Conflict response
     */
    static crow::response conflict(const std::string& message,
                                   const nlohmann::json& details = {}) {
        return error(409, "conflict", message, details);
    }

    /**
     * @brief Create a 422 Unprocessable Entity response
     */
    static crow::response unprocessable(const std::string& message,
                                        const nlohmann::json& details = {}) {
        return error(422, "unprocessable_entity", message, details);
    }

    /**
     * @brief Create a 429 Rate Limited response
     */
    static crow::response rateLimited(int retryAfterSeconds = 60) {
        auto request_id = lithium::models::api::generateRequestId();
        auto body = lithium::models::api::makeError(
            "rate_limited",
            "Too many requests. Please try again later.",
            request_id);
        crow::response res(429);
        res.set_header("Content-Type", "application/json");
        res.set_header("X-Request-ID", request_id);
        res.set_header("Retry-After", std::to_string(retryAfterSeconds));
        res.write(body.dump());
        return res;
    }

    /**
     * @brief Create a 500 Internal Server Error response
     */
    static crow::response internalError(const std::string& message = "Internal server error") {
        return error(500, "internal_error", message);
    }

    /**
     * @brief Create a 503 Service Unavailable response
     */
    static crow::response serviceUnavailable(const std::string& message = "Service temporarily unavailable") {
        return error(503, "service_unavailable", message);
    }

    /**
     * @brief Create a 400 Missing Field response
     */
    static crow::response missingField(const std::string& fieldName) {
        nlohmann::json details = {
            {"field", fieldName}
        };
        return error(400, "missing_field", "Required field '" + fieldName + "' is missing", details);
    }

    /**
     * @brief Create a 400 Invalid JSON response
     */
    static crow::response invalidJson(const std::string& message) {
        return error(400, "invalid_json", "Invalid JSON: " + message);
    }

private:
    static crow::response makeResponse(int status,
                                       const std::string& request_id,
                                       const nlohmann::json& body) {
        crow::response res(status);
        res.set_header("Content-Type", "application/json");
        res.set_header("X-Request-ID", request_id);
        res.write(body.dump());
        return res;
    }
};

}  // namespace lithium::server::utils

#endif  // LITHIUM_SERVER_UTILS_RESPONSE_HPP
