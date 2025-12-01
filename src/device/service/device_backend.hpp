/*
 * device_backend.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device backend abstraction for unified device discovery and management

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_DEVICE_BACKEND_HPP
#define LITHIUM_DEVICE_SERVICE_DEVICE_BACKEND_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::device {

using json = nlohmann::json;

/**
 * @brief Device discovery result from backend
 */
struct DiscoveredDevice {
    std::string deviceId;           // Unique device identifier
    std::string displayName;        // Human-readable name
    std::string deviceType;         // Device type (Camera, Telescope, etc.)
    std::string driverName;         // Driver/backend name
    std::string driverVersion;      // Driver version
    std::string connectionString;   // Connection parameters (host:port, etc.)
    int priority{0};                // Device priority
    bool isConnected{false};        // Current connection state
    json customProperties;          // Backend-specific properties

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["deviceId"] = deviceId;
        j["displayName"] = displayName;
        j["deviceType"] = deviceType;
        j["driverName"] = driverName;
        j["driverVersion"] = driverVersion;
        j["connectionString"] = connectionString;
        j["priority"] = priority;
        j["isConnected"] = isConnected;
        j["customProperties"] = customProperties;
        return j;
    }

    static auto fromJson(const json& j) -> DiscoveredDevice {
        DiscoveredDevice dev;
        dev.deviceId = j.value("deviceId", "");
        dev.displayName = j.value("displayName", "");
        dev.deviceType = j.value("deviceType", "");
        dev.driverName = j.value("driverName", "");
        dev.driverVersion = j.value("driverVersion", "");
        dev.connectionString = j.value("connectionString", "");
        dev.priority = j.value("priority", 0);
        dev.isConnected = j.value("isConnected", false);
        if (j.contains("customProperties")) {
            dev.customProperties = j["customProperties"];
        }
        return dev;
    }
};

/**
 * @brief Backend connection configuration
 */
struct BackendConfig {
    std::string host{"localhost"};
    int port{0};
    int timeout{5000};
    std::unordered_map<std::string, std::string> options;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["host"] = host;
        j["port"] = port;
        j["timeout"] = timeout;
        j["options"] = options;
        return j;
    }

    static auto fromJson(const json& j) -> BackendConfig {
        BackendConfig cfg;
        cfg.host = j.value("host", "localhost");
        cfg.port = j.value("port", 0);
        cfg.timeout = j.value("timeout", 5000);
        if (j.contains("options")) {
            cfg.options = j["options"].get<std::unordered_map<std::string, std::string>>();
        }
        return cfg;
    }
};

/**
 * @brief Backend event types
 */
enum class BackendEventType {
    SERVER_CONNECTED,
    SERVER_DISCONNECTED,
    DEVICE_ADDED,
    DEVICE_REMOVED,
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    DEVICE_UPDATED,
    ERROR
};

/**
 * @brief Backend event data
 */
struct BackendEvent {
    BackendEventType type;
    std::string backendName;
    std::string deviceId;
    std::string message;
    json data;
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["type"] = static_cast<int>(type);
        j["backendName"] = backendName;
        j["deviceId"] = deviceId;
        j["message"] = message;
        j["data"] = data;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        return j;
    }
};

/**
 * @brief Backend event callback type
 */
using BackendEventCallback = std::function<void(const BackendEvent&)>;

/**
 * @brief Abstract device backend interface
 *
 * Provides unified interface for device discovery and management
 * across different backends (INDI, ASCOM, etc.)
 */
class DeviceBackend {
public:
    DeviceBackend() = default;
    virtual ~DeviceBackend() = default;

    // Disable copy
    DeviceBackend(const DeviceBackend&) = delete;
    DeviceBackend& operator=(const DeviceBackend&) = delete;

    // ==================== Backend Identity ====================

    /**
     * @brief Get backend name
     * @return Backend name (e.g., "INDI", "ASCOM")
     */
    [[nodiscard]] virtual auto getBackendName() const -> std::string = 0;

    /**
     * @brief Get backend version
     * @return Version string
     */
    [[nodiscard]] virtual auto getBackendVersion() const -> std::string {
        return "1.0.0";
    }

    // ==================== Server Connection ====================

    /**
     * @brief Connect to backend server
     * @param config Connection configuration
     * @return true if connected successfully
     */
    virtual auto connectServer(const BackendConfig& config) -> bool = 0;

    /**
     * @brief Disconnect from backend server
     * @return true if disconnected successfully
     */
    virtual auto disconnectServer() -> bool = 0;

    /**
     * @brief Check if connected to server
     * @return true if connected
     */
    [[nodiscard]] virtual auto isServerConnected() const -> bool = 0;

    /**
     * @brief Get server status
     * @return Server status as JSON
     */
    [[nodiscard]] virtual auto getServerStatus() const -> json {
        json status;
        status["connected"] = isServerConnected();
        status["backend"] = getBackendName();
        return status;
    }

    // ==================== Device Discovery ====================

    /**
     * @brief Discover available devices
     * @param timeout Discovery timeout in milliseconds
     * @return Vector of discovered devices
     */
    virtual auto discoverDevices(int timeout = 5000)
        -> std::vector<DiscoveredDevice> = 0;

    /**
     * @brief Get all known devices
     * @return Vector of known devices
     */
    virtual auto getDevices() -> std::vector<DiscoveredDevice> = 0;

    /**
     * @brief Get device by ID
     * @param deviceId Device identifier
     * @return Device info or nullopt
     */
    virtual auto getDevice(const std::string& deviceId)
        -> std::optional<DiscoveredDevice> = 0;

    /**
     * @brief Refresh device list from server
     * @return Number of devices found
     */
    virtual auto refreshDevices() -> int = 0;

    // ==================== Device Connection ====================

    /**
     * @brief Connect to a specific device
     * @param deviceId Device identifier
     * @return true if connected
     */
    virtual auto connectDevice(const std::string& deviceId) -> bool = 0;

    /**
     * @brief Disconnect from a specific device
     * @param deviceId Device identifier
     * @return true if disconnected
     */
    virtual auto disconnectDevice(const std::string& deviceId) -> bool = 0;

    /**
     * @brief Check if device is connected
     * @param deviceId Device identifier
     * @return true if connected
     */
    [[nodiscard]] virtual auto isDeviceConnected(const std::string& deviceId) const -> bool = 0;

    // ==================== Property Access ====================

    /**
     * @brief Get device property
     * @param deviceId Device identifier
     * @param propertyName Property name
     * @return Property value as JSON or nullopt
     */
    virtual auto getProperty(const std::string& deviceId,
                             const std::string& propertyName)
        -> std::optional<json> = 0;

    /**
     * @brief Set device property
     * @param deviceId Device identifier
     * @param propertyName Property name
     * @param value Property value
     * @return true if set successfully
     */
    virtual auto setProperty(const std::string& deviceId,
                             const std::string& propertyName,
                             const json& value) -> bool = 0;

    /**
     * @brief Get all properties for a device
     * @param deviceId Device identifier
     * @return Map of property name to value
     */
    virtual auto getAllProperties(const std::string& deviceId)
        -> std::unordered_map<std::string, json> {
        return {};
    }

    // ==================== Event System ====================

    /**
     * @brief Register event callback
     * @param callback Event callback function
     */
    virtual void registerEventCallback(BackendEventCallback callback) = 0;

    /**
     * @brief Unregister event callback
     */
    virtual void unregisterEventCallback() = 0;

    // ==================== Utility ====================

    /**
     * @brief Get backend configuration
     * @return Current configuration
     */
    [[nodiscard]] virtual auto getConfig() const -> BackendConfig {
        return config_;
    }

protected:
    /**
     * @brief Emit backend event
     */
    void emitEvent(const BackendEvent& event) {
        if (eventCallback_) {
            eventCallback_(event);
        }
    }

    BackendConfig config_;
    BackendEventCallback eventCallback_;
};

/**
 * @brief Backend factory interface
 */
class DeviceBackendFactory {
public:
    virtual ~DeviceBackendFactory() = default;

    /**
     * @brief Create backend instance
     * @return Backend instance
     */
    virtual auto createBackend() -> std::shared_ptr<DeviceBackend> = 0;

    /**
     * @brief Get backend name
     * @return Backend name
     */
    [[nodiscard]] virtual auto getBackendName() const -> std::string = 0;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_DEVICE_BACKEND_HPP
