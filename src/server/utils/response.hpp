#ifndef LITHIUM_SERVER_UTILS_RESPONSE_HPP
#define LITHIUM_SERVER_UTILS_RESPONSE_HPP

#include <crow.h>
#include <string>
#include "atom/type/json.hpp"

namespace lithium::server::utils {

/**
 * @brief Utility class for creating standardized API responses
 */
class ResponseBuilder {
public:
    /**
     * @brief Create a successful JSON response
     */
    static crow::response success(const nlohmann::json& data, int code = 200) {
        nlohmann::json body = {
            {"status", "success"},
            {"data", data}
        };
        return makeJsonResponse(body, code);
    }

    /**
     * @brief Create a successful JSON response with custom message
     */
    static crow::response successWithMessage(const std::string& message,
                                             const nlohmann::json& data = nullptr,
                                             int code = 200) {
        nlohmann::json body = {
            {"status", "success"},
            {"message", message}
        };
        if (!data.is_null()) {
            body["data"] = data;
        }
        return makeJsonResponse(body, code);
    }

    /**
     * @brief Create an accepted response (202)
     */
    static crow::response accepted(const std::string& message, const nlohmann::json& data = nullptr) {
        nlohmann::json body = {
            {"status", "success"},
            {"message", message}
        };
        if (!data.is_null()) {
            body["data"] = data;
        }
        return makeJsonResponse(body, 202);
    }

    /**
     * @brief Create an error response
     */
    static crow::response error(const std::string& code,
                               const std::string& message,
                               int httpCode = 400,
                               const nlohmann::json& details = nullptr) {
        nlohmann::json errorObj = {
            {"code", code},
            {"message", message}
        };
        if (!details.is_null()) {
            errorObj["details"] = details;
        }
        
        nlohmann::json body = {
            {"status", "error"},
            {"error", errorObj}
        };
        return makeJsonResponse(body, httpCode);
    }

    /**
     * @brief Device not found error (404)
     */
    static crow::response deviceNotFound(const std::string& deviceId,
                                        const std::string& deviceType = "device") {
        nlohmann::json details = {
            {"deviceId", deviceId},
            {"deviceType", deviceType}
        };
        return error("device_not_found",
                    "The " + deviceType + " with ID '" + deviceId + "' does not exist.",
                    404,
                    details);
    }

    /**
     * @brief Device not connected error (503)
     */
    static crow::response deviceNotConnected(const std::string& deviceId,
                                            const std::string& deviceType = "device") {
        return error("device_not_connected",
                    "The " + deviceType + " is not connected.",
                    503,
                    {{"deviceId", deviceId}});
    }

    /**
     * @brief Device busy error (409)
     */
    static crow::response deviceBusy(const std::string& deviceId,
                                    const std::string& currentOperation,
                                    const nlohmann::json& additionalInfo = nullptr) {
        nlohmann::json details = {
            {"deviceId", deviceId},
            {"currentOperation", currentOperation}
        };
        if (!additionalInfo.is_null()) {
            details.merge_patch(additionalInfo);
        }
        return error("device_busy",
                    "Device is currently " + currentOperation + ". Wait for completion or abort the current operation.",
                    409,
                    details);
    }

    /**
     * @brief Missing required field error (400)
     */
    static crow::response missingField(const std::string& fieldName,
                                      const std::string& requirement = "") {
        nlohmann::json details = {{"field", fieldName}};
        if (!requirement.empty()) {
            details["requirement"] = requirement;
        }
        return error("missing_required_field",
                    "Required field '" + fieldName + "' is missing.",
                    400,
                    details);
    }

    /**
     * @brief Invalid field value error (400)
     */
    static crow::response invalidFieldValue(const std::string& fieldName,
                                           const std::string& constraint = "") {
        nlohmann::json details = {{"field", fieldName}};
        if (!constraint.empty()) {
            details["constraint"] = constraint;
        }
        return error("invalid_field_value",
                    "Field '" + fieldName + "' has an invalid value.",
                    400,
                    details);
    }

    /**
     * @brief Invalid JSON error (400)
     */
    static crow::response invalidJson(const std::string& parseError) {
        return error("invalid_json",
                    "The request body is not valid JSON: " + parseError,
                    400);
    }

    /**
     * @brief Safety interlock active error (403)
     */
    static crow::response safetyInterlock(const std::vector<std::string>& reasons) {
        nlohmann::json details = {{"safetyReasons", reasons}};
        return error("safety_interlock_active",
                    "Safety interlock prevents this operation.",
                    403,
                    details);
    }

    /**
     * @brief Rate limit exceeded error (429)
     */
    static crow::response rateLimitExceeded(int limit, const std::string& window, int retryAfter) {
        nlohmann::json details = {
            {"limit", limit},
            {"window", window},
            {"retryAfter", retryAfter}
        };
        return error("rate_limit_exceeded",
                    "Rate limit exceeded. Maximum " + std::to_string(limit) + " requests per " + window + " allowed.",
                    429,
                    details);
    }

    /**
     * @brief Internal server error (500)
     */
    static crow::response internalError(const std::string& message = "An unexpected error occurred") {
        return error("internal_error", message, 500);
    }

    /**
     * @brief Not found error (404)
     */
    static crow::response notFound(const std::string& resource, const std::string& identifier = "") {
        std::string message = resource + " not found";
        if (!identifier.empty()) {
            message += ": " + identifier;
        }
        return error(resource + "_not_found", message, 404);
    }

private:
    static crow::response makeJsonResponse(const nlohmann::json& body, int code) {
        crow::response res(code);
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        return res;
    }
};

}  // namespace lithium::server::utils

#endif  // LITHIUM_SERVER_UTILS_RESPONSE_HPP
