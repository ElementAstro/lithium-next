/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager Types and Enums

**************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "atom/type/json.hpp"

namespace lithium {

using json = nlohmann::json;

/**
 * @brief Device metadata for enhanced device management
 */
struct DeviceMetadata {
    std::string deviceId;          // Unique device identifier
    std::string displayName;       // Human-readable name
    std::string driverName;        // Driver/backend name (e.g., "INDI", "ASCOM")
    std::string driverVersion;     // Driver version
    std::string connectionString;  // Connection parameters
    int priority = 0;              // Device priority (higher = preferred)
    bool autoConnect = false;      // Auto-connect on startup
    json customProperties;         // Additional device-specific properties

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["deviceId"] = deviceId;
        j["displayName"] = displayName;
        j["driverName"] = driverName;
        j["driverVersion"] = driverVersion;
        j["connectionString"] = connectionString;
        j["priority"] = priority;
        j["autoConnect"] = autoConnect;
        j["customProperties"] = customProperties;
        return j;
    }

    static auto fromJson(const json& j) -> DeviceMetadata {
        DeviceMetadata meta;
        meta.deviceId = j.value("deviceId", "");
        meta.displayName = j.value("displayName", "");
        meta.driverName = j.value("driverName", "");
        meta.driverVersion = j.value("driverVersion", "");
        meta.connectionString = j.value("connectionString", "");
        meta.priority = j.value("priority", 0);
        meta.autoConnect = j.value("autoConnect", false);
        if (j.contains("customProperties")) {
            meta.customProperties = j["customProperties"];
        }
        return meta;
    }
};

/**
 * @brief Device state information
 */
struct DeviceState {
    bool isConnected = false;
    bool isInitialized = false;
    bool isBusy = false;
    std::string lastError;
    float healthScore = 1.0f;  // 0.0 to 1.0
    std::chrono::system_clock::time_point lastActivity;
    int consecutiveErrors = 0;  // Error count for health tracking
    int totalOperations = 0;    // Total operations performed
    int failedOperations = 0;   // Failed operations count

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["isConnected"] = isConnected;
        j["isInitialized"] = isInitialized;
        j["isBusy"] = isBusy;
        j["lastError"] = lastError;
        j["healthScore"] = healthScore;
        j["consecutiveErrors"] = consecutiveErrors;
        j["totalOperations"] = totalOperations;
        j["failedOperations"] = failedOperations;
        auto lastActivityMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                lastActivity.time_since_epoch())
                .count();
        j["lastActivityMs"] = lastActivityMs;
        return j;
    }

    static auto fromJson(const json& j) -> DeviceState {
        DeviceState state;
        state.isConnected = j.value("isConnected", false);
        state.isInitialized = j.value("isInitialized", false);
        state.isBusy = j.value("isBusy", false);
        state.lastError = j.value("lastError", "");
        state.healthScore = j.value("healthScore", 1.0f);
        state.consecutiveErrors = j.value("consecutiveErrors", 0);
        state.totalOperations = j.value("totalOperations", 0);
        state.failedOperations = j.value("failedOperations", 0);
        return state;
    }
};

/**
 * @brief Retry strategy configuration for device operations
 */
struct DeviceRetryConfig {
    enum class Strategy {
        None,        ///< No retry
        Linear,      ///< Fixed delay between retries
        Exponential  ///< Exponential backoff
    };

    Strategy strategy = Strategy::Exponential;
    int maxRetries = 3;
    std::chrono::milliseconds initialDelay{100};
    std::chrono::milliseconds maxDelay{5000};
    float multiplier = 2.0f;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["strategy"] = static_cast<int>(strategy);
        j["maxRetries"] = maxRetries;
        j["initialDelayMs"] = initialDelay.count();
        j["maxDelayMs"] = maxDelay.count();
        j["multiplier"] = multiplier;
        return j;
    }

    static auto fromJson(const json& j) -> DeviceRetryConfig {
        DeviceRetryConfig config;
        config.strategy = static_cast<Strategy>(j.value("strategy", 2));
        config.maxRetries = j.value("maxRetries", 3);
        config.initialDelay =
            std::chrono::milliseconds(j.value("initialDelayMs", 100));
        config.maxDelay =
            std::chrono::milliseconds(j.value("maxDelayMs", 5000));
        config.multiplier = j.value("multiplier", 2.0f);
        return config;
    }
};

/**
 * @brief Device event types
 */
enum class DeviceEventType {
    Connected,
    Disconnected,
    StateChanged,
    PropertyChanged,
    Error,
    HealthChanged,
    OperationStarted,
    OperationCompleted,
    OperationFailed
};

/**
 * @brief Device event data
 */
struct DeviceEvent {
    DeviceEventType type;
    std::string deviceName;
    std::string deviceType;
    std::string message;
    json data;
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["type"] = static_cast<int>(type);
        j["deviceName"] = deviceName;
        j["deviceType"] = deviceType;
        j["message"] = message;
        j["data"] = data;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             timestamp.time_since_epoch())
                             .count();
        return j;
    }
};

/**
 * @brief Event callback type
 */
using DeviceEventCallback = std::function<void(const DeviceEvent&)>;

/**
 * @brief Event callback ID for subscription management
 */
using EventCallbackId = uint64_t;

/**
 * @brief Device operation result
 */
struct DeviceOperationResult {
    bool success = false;
    std::string errorMessage;
    int retryCount = 0;
    std::chrono::milliseconds duration{0};
    json data;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["success"] = success;
        j["errorMessage"] = errorMessage;
        j["retryCount"] = retryCount;
        j["durationMs"] = duration.count();
        j["data"] = data;
        return j;
    }
};

}  // namespace lithium
