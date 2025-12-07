/*
 * device_error.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device error codes and structures for unified error handling

**************************************************/

#ifndef LITHIUM_DEVICE_COMMON_DEVICE_ERROR_HPP
#define LITHIUM_DEVICE_COMMON_DEVICE_ERROR_HPP

#include <chrono>
#include <optional>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::device {

/**
 * @brief Device error codes for categorizing errors
 */
enum class DeviceErrorCode {
    // General errors (0-99)
    Unknown = 0,
    Success = 1,
    InvalidArgument = 10,
    InvalidState = 11,
    NotSupported = 12,
    NotImplemented = 13,
    AlreadyExists = 14,
    NotFound = 15,

    // Connection errors (100-199)
    ConnectionFailed = 100,
    ConnectionTimeout = 101,
    ConnectionRefused = 102,
    ConnectionLost = 103,
    NotConnected = 104,
    AlreadyConnected = 105,
    AuthenticationFailed = 106,

    // Device operation errors (200-299)
    OperationFailed = 200,
    OperationTimeout = 201,
    OperationAborted = 202,
    OperationBusy = 203,
    OperationNotAllowed = 204,
    DeviceBusy = 205,
    DeviceNotReady = 206,
    DeviceDisabled = 207,

    // Plugin errors (300-399)
    PluginLoadFailed = 300,
    PluginNotFound = 301,
    PluginInvalidInterface = 302,
    PluginVersionMismatch = 303,
    PluginDependencyMissing = 304,
    PluginAlreadyLoaded = 305,
    PluginUnloadFailed = 306,
    PluginInitFailed = 307,

    // Backend errors (400-499)
    BackendNotConnected = 400,
    BackendError = 401,
    BackendTimeout = 402,
    BackendUnavailable = 403,

    // Resource errors (500-599)
    ResourceExhausted = 500,
    ResourceUnavailable = 501,
    MemoryAllocationFailed = 502,

    // Communication errors (600-699)
    CommunicationError = 600,
    ProtocolError = 601,
    DataCorruption = 602,
    MessageTooLarge = 603,

    // Internal errors (900-999)
    InternalError = 900,
    ConfigurationError = 901,
    InitializationFailed = 902
};

/**
 * @brief Convert error code to string
 */
[[nodiscard]] inline auto deviceErrorCodeToString(DeviceErrorCode code)
    -> std::string {
    switch (code) {
        case DeviceErrorCode::Unknown:
            return "Unknown";
        case DeviceErrorCode::Success:
            return "Success";
        case DeviceErrorCode::InvalidArgument:
            return "InvalidArgument";
        case DeviceErrorCode::InvalidState:
            return "InvalidState";
        case DeviceErrorCode::NotSupported:
            return "NotSupported";
        case DeviceErrorCode::NotImplemented:
            return "NotImplemented";
        case DeviceErrorCode::AlreadyExists:
            return "AlreadyExists";
        case DeviceErrorCode::NotFound:
            return "NotFound";
        case DeviceErrorCode::ConnectionFailed:
            return "ConnectionFailed";
        case DeviceErrorCode::ConnectionTimeout:
            return "ConnectionTimeout";
        case DeviceErrorCode::ConnectionRefused:
            return "ConnectionRefused";
        case DeviceErrorCode::ConnectionLost:
            return "ConnectionLost";
        case DeviceErrorCode::NotConnected:
            return "NotConnected";
        case DeviceErrorCode::AlreadyConnected:
            return "AlreadyConnected";
        case DeviceErrorCode::AuthenticationFailed:
            return "AuthenticationFailed";
        case DeviceErrorCode::OperationFailed:
            return "OperationFailed";
        case DeviceErrorCode::OperationTimeout:
            return "OperationTimeout";
        case DeviceErrorCode::OperationAborted:
            return "OperationAborted";
        case DeviceErrorCode::OperationBusy:
            return "OperationBusy";
        case DeviceErrorCode::OperationNotAllowed:
            return "OperationNotAllowed";
        case DeviceErrorCode::DeviceBusy:
            return "DeviceBusy";
        case DeviceErrorCode::DeviceNotReady:
            return "DeviceNotReady";
        case DeviceErrorCode::DeviceDisabled:
            return "DeviceDisabled";
        case DeviceErrorCode::PluginLoadFailed:
            return "PluginLoadFailed";
        case DeviceErrorCode::PluginNotFound:
            return "PluginNotFound";
        case DeviceErrorCode::PluginInvalidInterface:
            return "PluginInvalidInterface";
        case DeviceErrorCode::PluginVersionMismatch:
            return "PluginVersionMismatch";
        case DeviceErrorCode::PluginDependencyMissing:
            return "PluginDependencyMissing";
        case DeviceErrorCode::PluginAlreadyLoaded:
            return "PluginAlreadyLoaded";
        case DeviceErrorCode::PluginUnloadFailed:
            return "PluginUnloadFailed";
        case DeviceErrorCode::PluginInitFailed:
            return "PluginInitFailed";
        case DeviceErrorCode::BackendNotConnected:
            return "BackendNotConnected";
        case DeviceErrorCode::BackendError:
            return "BackendError";
        case DeviceErrorCode::BackendTimeout:
            return "BackendTimeout";
        case DeviceErrorCode::BackendUnavailable:
            return "BackendUnavailable";
        case DeviceErrorCode::ResourceExhausted:
            return "ResourceExhausted";
        case DeviceErrorCode::ResourceUnavailable:
            return "ResourceUnavailable";
        case DeviceErrorCode::MemoryAllocationFailed:
            return "MemoryAllocationFailed";
        case DeviceErrorCode::CommunicationError:
            return "CommunicationError";
        case DeviceErrorCode::ProtocolError:
            return "ProtocolError";
        case DeviceErrorCode::DataCorruption:
            return "DataCorruption";
        case DeviceErrorCode::MessageTooLarge:
            return "MessageTooLarge";
        case DeviceErrorCode::InternalError:
            return "InternalError";
        case DeviceErrorCode::ConfigurationError:
            return "ConfigurationError";
        case DeviceErrorCode::InitializationFailed:
            return "InitializationFailed";
        default:
            return "Unknown(" + std::to_string(static_cast<int>(code)) + ")";
    }
}

/**
 * @brief Check if error code indicates success
 */
[[nodiscard]] inline auto isSuccess(DeviceErrorCode code) -> bool {
    return code == DeviceErrorCode::Success;
}

/**
 * @brief Check if error code is recoverable
 */
[[nodiscard]] inline auto isRecoverable(DeviceErrorCode code) -> bool {
    switch (code) {
        case DeviceErrorCode::ConnectionTimeout:
        case DeviceErrorCode::OperationTimeout:
        case DeviceErrorCode::OperationBusy:
        case DeviceErrorCode::DeviceBusy:
        case DeviceErrorCode::DeviceNotReady:
        case DeviceErrorCode::BackendTimeout:
        case DeviceErrorCode::ResourceExhausted:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Device error structure with detailed information
 */
struct DeviceError {
    DeviceErrorCode code{DeviceErrorCode::Unknown};
    std::string message;
    std::optional<std::string> deviceName;
    std::optional<std::string> operationName;
    std::optional<std::string> details;
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};

    DeviceError() = default;

    explicit DeviceError(DeviceErrorCode errorCode,
                         std::string errorMessage = "")
        : code(errorCode), message(std::move(errorMessage)) {}

    DeviceError(DeviceErrorCode errorCode, std::string errorMessage,
                std::string device)
        : code(errorCode),
          message(std::move(errorMessage)),
          deviceName(std::move(device)) {}

    /**
     * @brief Create error with full context
     */
    static auto create(DeviceErrorCode code, const std::string& message,
                       const std::string& device = "",
                       const std::string& operation = "",
                       const std::string& details = "") -> DeviceError {
        DeviceError err;
        err.code = code;
        err.message = message;
        if (!device.empty()) {
            err.deviceName = device;
        }
        if (!operation.empty()) {
            err.operationName = operation;
        }
        if (!details.empty()) {
            err.details = details;
        }
        return err;
    }

    /**
     * @brief Get formatted error string
     */
    [[nodiscard]] auto toString() const -> std::string {
        std::string result =
            "[" + deviceErrorCodeToString(code) + "] " + message;
        if (deviceName) {
            result += " (device: " + *deviceName + ")";
        }
        if (operationName) {
            result += " (operation: " + *operationName + ")";
        }
        if (details) {
            result += " - " + *details;
        }
        return result;
    }

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["code"] = static_cast<int>(code);
        j["codeName"] = deviceErrorCodeToString(code);
        j["message"] = message;
        if (deviceName) {
            j["deviceName"] = *deviceName;
        }
        if (operationName) {
            j["operationName"] = *operationName;
        }
        if (details) {
            j["details"] = *details;
        }
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             timestamp.time_since_epoch())
                             .count();
        return j;
    }

    /**
     * @brief Create from JSON
     */
    static auto fromJson(const nlohmann::json& j) -> DeviceError {
        DeviceError err;
        err.code = static_cast<DeviceErrorCode>(j.value("code", 0));
        err.message = j.value("message", "");
        if (j.contains("deviceName")) {
            err.deviceName = j["deviceName"].get<std::string>();
        }
        if (j.contains("operationName")) {
            err.operationName = j["operationName"].get<std::string>();
        }
        if (j.contains("details")) {
            err.details = j["details"].get<std::string>();
        }
        return err;
    }

    /**
     * @brief Check if error is recoverable
     */
    [[nodiscard]] auto isRecoverable() const -> bool {
        return lithium::device::isRecoverable(code);
    }

    /**
     * @brief Check if this represents success
     */
    [[nodiscard]] auto isSuccess() const -> bool {
        return lithium::device::isSuccess(code);
    }
};

// Convenient factory functions
namespace error {

inline auto unknown(const std::string& msg = "Unknown error") -> DeviceError {
    return DeviceError(DeviceErrorCode::Unknown, msg);
}

inline auto invalidArgument(const std::string& msg) -> DeviceError {
    return DeviceError(DeviceErrorCode::InvalidArgument, msg);
}

inline auto notFound(const std::string& what) -> DeviceError {
    return DeviceError(DeviceErrorCode::NotFound, what + " not found");
}

inline auto connectionFailed(const std::string& device,
                             const std::string& reason = "") -> DeviceError {
    DeviceError err(DeviceErrorCode::ConnectionFailed,
                    "Connection failed" + (reason.empty() ? "" : ": " + reason),
                    device);
    return err;
}

inline auto connectionTimeout(const std::string& device) -> DeviceError {
    return DeviceError(DeviceErrorCode::ConnectionTimeout, "Connection timeout",
                       device);
}

inline auto operationFailed(const std::string& operation,
                            const std::string& reason = "") -> DeviceError {
    DeviceError err(
        DeviceErrorCode::OperationFailed,
        "Operation failed" + (reason.empty() ? "" : ": " + reason));
    err.operationName = operation;
    return err;
}

inline auto operationTimeout(const std::string& operation) -> DeviceError {
    DeviceError err(DeviceErrorCode::OperationTimeout, "Operation timeout");
    err.operationName = operation;
    return err;
}

inline auto deviceBusy(const std::string& device) -> DeviceError {
    return DeviceError(DeviceErrorCode::DeviceBusy, "Device is busy", device);
}

inline auto notConnected(const std::string& device) -> DeviceError {
    return DeviceError(DeviceErrorCode::NotConnected, "Device not connected",
                       device);
}

inline auto pluginLoadFailed(const std::string& pluginName,
                             const std::string& reason = "") -> DeviceError {
    return DeviceError(
        DeviceErrorCode::PluginLoadFailed,
        "Failed to load plugin: " + pluginName +
            (reason.empty() ? "" : " - " + reason));
}

inline auto pluginNotFound(const std::string& pluginName) -> DeviceError {
    return DeviceError(DeviceErrorCode::PluginNotFound,
                       "Plugin not found: " + pluginName);
}

inline auto internalError(const std::string& msg) -> DeviceError {
    return DeviceError(DeviceErrorCode::InternalError, msg);
}

}  // namespace error

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_COMMON_DEVICE_ERROR_HPP
