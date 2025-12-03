/*
 * device.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Device data models for HTTP/WebSocket responses

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_DEVICE_HPP
#define LITHIUM_SERVER_MODELS_DEVICE_HPP

#include <optional>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::models::device {

using json = nlohmann::json;

/**
 * @brief Device type enumeration
 */
enum class DeviceType {
    Camera,
    Mount,
    Focuser,
    FilterWheel,
    Dome,
    Guider,
    Rotator,
    Switch,
    Weather,
    Unknown
};

/**
 * @brief Convert DeviceType to string
 */
inline auto deviceTypeToString(DeviceType type) -> std::string {
    switch (type) {
        case DeviceType::Camera:
            return "camera";
        case DeviceType::Mount:
            return "mount";
        case DeviceType::Focuser:
            return "focuser";
        case DeviceType::FilterWheel:
            return "filterwheel";
        case DeviceType::Dome:
            return "dome";
        case DeviceType::Guider:
            return "guider";
        case DeviceType::Rotator:
            return "rotator";
        case DeviceType::Switch:
            return "switch";
        case DeviceType::Weather:
            return "weather";
        case DeviceType::Unknown:
            return "unknown";
    }
    return "unknown";
}

/**
 * @brief Parse string to DeviceType
 */
inline auto stringToDeviceType(const std::string& str) -> DeviceType {
    if (str == "camera")
        return DeviceType::Camera;
    if (str == "mount")
        return DeviceType::Mount;
    if (str == "focuser")
        return DeviceType::Focuser;
    if (str == "filterwheel")
        return DeviceType::FilterWheel;
    if (str == "dome")
        return DeviceType::Dome;
    if (str == "guider")
        return DeviceType::Guider;
    if (str == "rotator")
        return DeviceType::Rotator;
    if (str == "switch")
        return DeviceType::Switch;
    if (str == "weather")
        return DeviceType::Weather;
    return DeviceType::Unknown;
}

/**
 * @brief Device connection status
 */
enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief Convert ConnectionStatus to string
 */
inline auto connectionStatusToString(ConnectionStatus status) -> std::string {
    switch (status) {
        case ConnectionStatus::Disconnected:
            return "disconnected";
        case ConnectionStatus::Connecting:
            return "connecting";
        case ConnectionStatus::Connected:
            return "connected";
        case ConnectionStatus::Disconnecting:
            return "disconnecting";
        case ConnectionStatus::Error:
            return "error";
    }
    return "unknown";
}

/**
 * @brief Device summary for list responses
 */
struct DeviceSummary {
    std::string deviceId;
    std::string name;
    DeviceType type;
    ConnectionStatus status;
    std::string driver;
    std::optional<std::string> description;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["deviceId"] = deviceId;
        j["name"] = name;
        j["type"] = deviceTypeToString(type);
        j["status"] = connectionStatusToString(status);
        j["driver"] = driver;

        if (description) {
            j["description"] = *description;
        }

        return j;
    }
};

/**
 * @brief Device health information
 */
struct DeviceHealth {
    std::string deviceId;
    bool isHealthy;
    std::optional<std::string> lastError;
    int64_t lastActivityTime;
    size_t successfulOperations;
    size_t failedOperations;
    double averageResponseTime;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["deviceId"] = deviceId;
        j["isHealthy"] = isHealthy;
        j["lastActivityTime"] = lastActivityTime;
        j["successfulOperations"] = successfulOperations;
        j["failedOperations"] = failedOperations;
        j["averageResponseTime"] = averageResponseTime;

        if (lastError) {
            j["lastError"] = *lastError;
        }

        return j;
    }
};

/**
 * @brief Device event for WebSocket broadcast
 */
struct DeviceEvent {
    std::string eventType;  // connected, disconnected, status_update, error
    std::string deviceId;
    DeviceType deviceType;
    json data;
    int64_t timestamp;

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", "event"},
                {"event", "device." + eventType},
                {"deviceId", deviceId},
                {"deviceType", deviceTypeToString(deviceType)},
                {"data", data},
                {"timestamp", timestamp}};
    }
};

/**
 * @brief Device list response
 */
inline auto makeDeviceListResponse(const std::vector<DeviceSummary>& devices)
    -> json {
    json deviceList = json::array();
    for (const auto& device : devices) {
        deviceList.push_back(device.toJson());
    }

    return {{"devices", deviceList}, {"count", deviceList.size()}};
}

/**
 * @brief Device connection result
 */
inline auto makeConnectionResult(const std::string& deviceId, bool success,
                                 const std::string& message = "") -> json {
    json result;
    result["deviceId"] = deviceId;
    result["success"] = success;

    if (!message.empty()) {
        result["message"] = message;
    }

    return result;
}

}  // namespace lithium::models::device

#endif  // LITHIUM_SERVER_MODELS_DEVICE_HPP
