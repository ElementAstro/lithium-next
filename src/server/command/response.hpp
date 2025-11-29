/*
 * response.hpp - Command Response Builder
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_APP_COMMAND_RESPONSE_HPP
#define LITHIUM_APP_COMMAND_RESPONSE_HPP

#include <nlohmann/json.hpp>
#include <string>

namespace lithium::app::command {

/**
 * @brief Unified command response builder for WebSocket command handlers
 *
 * Provides standardized response formats consistent with the REST API
 * ResponseBuilder pattern. All command handlers should use these methods
 * to ensure consistent API responses.
 */
struct CommandResponse {
    /**
     * @brief Creates a success response with optional data
     *
     * @param data Response payload data
     * @return JSON response with status "success"
     */
    static auto success(const nlohmann::json& data = nlohmann::json::object())
        -> nlohmann::json {
        return {{"status", "success"}, {"data", data}};
    }

    /**
     * @brief Creates an error response with code, message, and optional details
     *
     * @param code Error code identifier (e.g., "device_not_found")
     * @param message Human-readable error description
     * @param details Additional error context
     * @return JSON response with status "error"
     */
    static auto error(const std::string& code, const std::string& message,
                      const nlohmann::json& details = nlohmann::json::object())
        -> nlohmann::json {
        nlohmann::json err = {
            {"status", "error"},
            {"error", {{"code", code}, {"message", message}}}};
        if (!details.empty()) {
            err["error"]["details"] = details;
        }
        return err;
    }

    /**
     * @brief Creates device not found error response
     *
     * @param deviceId The device ID that was not found
     * @param deviceType Type of device (camera, mount, etc.)
     * @return JSON error response
     */
    static auto deviceNotFound(const std::string& deviceId,
                               const std::string& deviceType)
        -> nlohmann::json {
        return error("device_not_found", deviceType + " not found: " + deviceId,
                     {{"deviceId", deviceId}, {"deviceType", deviceType}});
    }

    /**
     * @brief Creates missing parameter error response
     *
     * @param param Name of the missing required parameter
     * @return JSON error response
     */
    static auto missingParameter(const std::string& param) -> nlohmann::json {
        return error("missing_parameter",
                     "Required parameter missing: " + param,
                     {{"param", param}});
    }

    /**
     * @brief Creates invalid parameter error response
     *
     * @param param Name of the invalid parameter
     * @param reason Description of why the parameter is invalid
     * @return JSON error response
     */
    static auto invalidParameter(const std::string& param,
                                 const std::string& reason) -> nlohmann::json {
        return error("invalid_parameter",
                     "Invalid parameter '" + param + "': " + reason,
                     {{"param", param}, {"reason", reason}});
    }

    /**
     * @brief Creates service unavailable error response
     *
     * @param serviceName Name of the unavailable service
     * @return JSON error response
     */
    static auto serviceUnavailable(const std::string& serviceName)
        -> nlohmann::json {
        return error("service_unavailable",
                     "Service is not available: " + serviceName,
                     {{"service", serviceName}});
    }

    /**
     * @brief Creates operation failed error response
     *
     * @param operation Name of the failed operation
     * @param reason Description of the failure
     * @return JSON error response
     */
    static auto operationFailed(const std::string& operation,
                                const std::string& reason) -> nlohmann::json {
        return error("operation_failed", operation + " failed: " + reason,
                     {{"operation", operation}, {"reason", reason}});
    }

    /**
     * @brief Creates timeout error response
     *
     * @param operation Name of the timed out operation
     * @return JSON error response
     */
    static auto timeout(const std::string& operation) -> nlohmann::json {
        return error("timeout", "Operation timed out: " + operation,
                     {{"operation", operation}});
    }

    /**
     * @brief Creates device busy error response
     *
     * @param deviceId The busy device ID
     * @param currentOperation Current operation blocking the device
     * @return JSON error response
     */
    static auto deviceBusy(const std::string& deviceId,
                           const std::string& currentOperation = "")
        -> nlohmann::json {
        nlohmann::json details = {{"deviceId", deviceId}};
        if (!currentOperation.empty()) {
            details["currentOperation"] = currentOperation;
        }
        return error("device_busy", "Device is busy: " + deviceId, details);
    }

    /**
     * @brief Creates not connected error response
     *
     * @param deviceId The disconnected device ID
     * @return JSON error response
     */
    static auto notConnected(const std::string& deviceId) -> nlohmann::json {
        return error("not_connected", "Device is not connected: " + deviceId,
                     {{"deviceId", deviceId}});
    }
};

}  // namespace lithium::app::command

#endif  // LITHIUM_APP_COMMAND_RESPONSE_HPP
