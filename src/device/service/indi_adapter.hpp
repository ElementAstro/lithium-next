/*
 * indi_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024

Description: INDI protocol adapter for device services

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_INDI_ADAPTER_HPP
#define LITHIUM_DEVICE_SERVICE_INDI_ADAPTER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

// Forward declarations in global scope
namespace lithium::client {
class INDIClient;
struct DeviceInfo;
struct PropertyValue;
}  // namespace lithium::client

namespace lithium::device {

using json = nlohmann::json;

/**
 * @brief INDI property types
 */
enum class INDIPropertyType {
    NUMBER,
    SWITCH,
    TEXT,
    LIGHT,
    BLOB,
    UNKNOWN
};

/**
 * @brief INDI property state
 */
enum class INDIPropertyState { IDLE, OK, BUSY, ALERT, UNKNOWN };

/**
 * @brief Convert INDI property state to string
 */
inline auto indiStateToString(INDIPropertyState state) -> std::string {
    switch (state) {
        case INDIPropertyState::IDLE:
            return "Idle";
        case INDIPropertyState::OK:
            return "Ok";
        case INDIPropertyState::BUSY:
            return "Busy";
        case INDIPropertyState::ALERT:
            return "Alert";
        default:
            return "Unknown";
    }
}

/**
 * @brief INDI property value container
 */
struct INDIPropertyValue {
    std::string name;
    INDIPropertyType type = INDIPropertyType::UNKNOWN;
    INDIPropertyState state = INDIPropertyState::UNKNOWN;

    // Value storage (use appropriate field based on type)
    double numberValue = 0.0;
    double numberMin = 0.0;
    double numberMax = 0.0;
    double numberStep = 0.0;
    std::string textValue;
    bool switchValue = false;
    std::vector<uint8_t> blobValue;

    std::string label;
    std::string format;

    auto toJson() const -> json {
        json j;
        j["name"] = name;
        j["state"] = indiStateToString(state);
        j["label"] = label;

        switch (type) {
            case INDIPropertyType::NUMBER:
                j["type"] = "number";
                j["value"] = numberValue;
                j["min"] = numberMin;
                j["max"] = numberMax;
                j["step"] = numberStep;
                j["format"] = format;
                break;
            case INDIPropertyType::SWITCH:
                j["type"] = "switch";
                j["value"] = switchValue;
                break;
            case INDIPropertyType::TEXT:
                j["type"] = "text";
                j["value"] = textValue;
                break;
            case INDIPropertyType::LIGHT:
                j["type"] = "light";
                j["state"] = indiStateToString(state);
                break;
            case INDIPropertyType::BLOB:
                j["type"] = "blob";
                j["size"] = blobValue.size();
                break;
            default:
                j["type"] = "unknown";
                break;
        }
        return j;
    }
};

/**
 * @brief INDI device info
 */
struct INDIDeviceInfo {
    std::string name;
    std::string driverName;
    std::string driverVersion;
    std::string driverInterface;
    bool isConnected = false;
    std::chrono::system_clock::time_point lastUpdate;

    std::unordered_map<std::string, INDIPropertyValue> properties;

    auto toJson() const -> json {
        json j;
        j["name"] = name;
        j["driver"] = driverName;
        j["version"] = driverVersion;
        j["interface"] = driverInterface;
        j["connected"] = isConnected;

        json props = json::object();
        for (const auto& [propName, propValue] : properties) {
            props[propName] = propValue.toJson();
        }
        j["properties"] = props;

        return j;
    }
};

/**
 * @brief INDI event types
 */
enum class INDIEventType {
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    PROPERTY_DEFINED,
    PROPERTY_UPDATED,
    PROPERTY_DELETED,
    MESSAGE_RECEIVED,
    BLOB_RECEIVED,
    SERVER_CONNECTED,
    SERVER_DISCONNECTED,
    ERROR
};

/**
 * @brief INDI event data
 */
struct INDIEvent {
    INDIEventType type;
    std::string deviceName;
    std::string propertyName;
    std::string message;
    json data;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief INDI event callback type
 */
using INDIEventCallback = std::function<void(const INDIEvent&)>;

/**
 * @brief INDI adapter interface for device services
 *
 * Provides a unified interface for INDI protocol operations,
 * abstracting the low-level INDI client details.
 */
class INDIAdapter {
public:
    INDIAdapter() = default;
    virtual ~INDIAdapter() = default;

    // Disable copy
    INDIAdapter(const INDIAdapter&) = delete;
    INDIAdapter& operator=(const INDIAdapter&) = delete;

    /**
     * @brief Connect to INDI server
     */
    virtual auto connectServer(const std::string& host = "localhost",
                               int port = 7624) -> bool = 0;

    /**
     * @brief Disconnect from INDI server
     */
    virtual auto disconnectServer() -> bool = 0;

    /**
     * @brief Check if connected to server
     */
    [[nodiscard]] virtual auto isServerConnected() const -> bool = 0;

    /**
     * @brief Get list of available devices
     */
    virtual auto getDevices() -> std::vector<INDIDeviceInfo> = 0;

    /**
     * @brief Get device info by name
     */
    virtual auto getDevice(const std::string& deviceName)
        -> std::optional<INDIDeviceInfo> = 0;

    /**
     * @brief Connect to a specific device
     */
    virtual auto connectDevice(const std::string& deviceName) -> bool = 0;

    /**
     * @brief Disconnect from a specific device
     */
    virtual auto disconnectDevice(const std::string& deviceName) -> bool = 0;

    /**
     * @brief Get property value
     */
    virtual auto getProperty(const std::string& deviceName,
                             const std::string& propertyName)
        -> std::optional<INDIPropertyValue> = 0;

    /**
     * @brief Set number property
     */
    virtual auto setNumberProperty(const std::string& deviceName,
                                   const std::string& propertyName,
                                   const std::string& elementName,
                                   double value) -> bool = 0;

    /**
     * @brief Set switch property
     */
    virtual auto setSwitchProperty(const std::string& deviceName,
                                   const std::string& propertyName,
                                   const std::string& elementName,
                                   bool value) -> bool = 0;

    /**
     * @brief Set text property
     */
    virtual auto setTextProperty(const std::string& deviceName,
                                 const std::string& propertyName,
                                 const std::string& elementName,
                                 const std::string& value) -> bool = 0;

    /**
     * @brief Register event callback
     */
    virtual void registerEventCallback(INDIEventCallback callback) = 0;

    /**
     * @brief Unregister event callback
     */
    virtual void unregisterEventCallback() = 0;

    /**
     * @brief Wait for property state change
     */
    virtual auto waitForPropertyState(
        const std::string& deviceName, const std::string& propertyName,
        INDIPropertyState targetState,
        std::chrono::milliseconds timeout = std::chrono::seconds(30)) -> bool = 0;

    /**
     * @brief Get server info
     */
    virtual auto getServerInfo() -> json = 0;
};

/**
 * @brief Real INDI adapter implementation using INDIClient
 *
 * This adapter bridges the device service layer to the INDI client
 * in src/client/indi/, providing a unified interface for INDI operations.
 */
class INDIClientAdapter : public INDIAdapter {
public:
    /**
     * @brief Construct adapter with existing client
     * @param client Shared pointer to INDIClient
     */
    explicit INDIClientAdapter(std::shared_ptr<lithium::client::INDIClient> client);
    
    /**
     * @brief Construct adapter (creates internal client)
     */
    INDIClientAdapter();
    
    ~INDIClientAdapter() override;

    auto connectServer(const std::string& host, int port) -> bool override;
    auto disconnectServer() -> bool override;
    [[nodiscard]] auto isServerConnected() const -> bool override;

    auto getDevices() -> std::vector<INDIDeviceInfo> override;
    auto getDevice(const std::string& deviceName)
        -> std::optional<INDIDeviceInfo> override;

    auto connectDevice(const std::string& deviceName) -> bool override;
    auto disconnectDevice(const std::string& deviceName) -> bool override;

    auto getProperty(const std::string& deviceName,
                     const std::string& propertyName)
        -> std::optional<INDIPropertyValue> override;

    auto setNumberProperty(const std::string& deviceName,
                           const std::string& propertyName,
                           const std::string& elementName,
                           double value) -> bool override;

    auto setSwitchProperty(const std::string& deviceName,
                           const std::string& propertyName,
                           const std::string& elementName,
                           bool value) -> bool override;

    auto setTextProperty(const std::string& deviceName,
                         const std::string& propertyName,
                         const std::string& elementName,
                         const std::string& value) -> bool override;

    void registerEventCallback(INDIEventCallback callback) override;
    void unregisterEventCallback() override;

    auto waitForPropertyState(const std::string& deviceName,
                              const std::string& propertyName,
                              INDIPropertyState targetState,
                              std::chrono::milliseconds timeout)
        -> bool override;

    auto getServerInfo() -> json override;

    /**
     * @brief Get the underlying INDI client
     */
    auto getClient() const -> std::shared_ptr<lithium::client::INDIClient> {
        return client_;
    }

private:
    // Convert client DeviceInfo to adapter INDIDeviceInfo
    INDIDeviceInfo convertDeviceInfo(const lithium::client::DeviceInfo& info) const;
    
    // Convert client PropertyValue to adapter INDIPropertyValue
    INDIPropertyValue convertPropertyValue(
        const lithium::client::PropertyValue& prop) const;

    std::shared_ptr<lithium::client::INDIClient> client_;
    mutable std::mutex mutex_;
    INDIEventCallback eventCallback_;
    bool ownsClient_{false};
};

/**
 * @brief Default/stub INDI adapter implementation for testing
 *
 * This is a stub implementation that can be used for testing
 * or when no real INDI server is available.
 */
class DefaultINDIAdapter : public INDIAdapter {
public:
    DefaultINDIAdapter() = default;
    ~DefaultINDIAdapter() override = default;

    auto connectServer(const std::string& host, int port) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = host;
        port_ = port;
        serverConnected_ = true;
        LOG_INFO( "INDIAdapter: Connected to server %s:%d", host.c_str(),
              port);
        return true;
    }

    auto disconnectServer() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        serverConnected_ = false;
        devices_.clear();
        LOG_INFO( "INDIAdapter: Disconnected from server");
        return true;
    }

    [[nodiscard]] auto isServerConnected() const -> bool override {
        return serverConnected_.load();
    }

    auto getDevices() -> std::vector<INDIDeviceInfo> override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<INDIDeviceInfo> result;
        result.reserve(devices_.size());
        for (const auto& [name, info] : devices_) {
            result.push_back(info);
        }
        return result;
    }

    auto getDevice(const std::string& deviceName)
        -> std::optional<INDIDeviceInfo> override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(deviceName);
        if (it != devices_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    auto connectDevice(const std::string& deviceName) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(deviceName);
        if (it != devices_.end()) {
            it->second.isConnected = true;
            return true;
        }
        return false;
    }

    auto disconnectDevice(const std::string& deviceName) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(deviceName);
        if (it != devices_.end()) {
            it->second.isConnected = false;
            return true;
        }
        return false;
    }

    auto getProperty(const std::string& deviceName,
                     const std::string& propertyName)
        -> std::optional<INDIPropertyValue> override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = devices_.find(deviceName);
        if (it != devices_.end()) {
            auto propIt = it->second.properties.find(propertyName);
            if (propIt != it->second.properties.end()) {
                return propIt->second;
            }
        }
        return std::nullopt;
    }

    auto setNumberProperty(const std::string& deviceName,
                           const std::string& propertyName,
                           const std::string& elementName,
                           double value) -> bool override {
        (void)deviceName;
        (void)propertyName;
        (void)elementName;
        (void)value;
        return true;
    }

    auto setSwitchProperty(const std::string& deviceName,
                           const std::string& propertyName,
                           const std::string& elementName,
                           bool value) -> bool override {
        (void)deviceName;
        (void)propertyName;
        (void)elementName;
        (void)value;
        return true;
    }

    auto setTextProperty(const std::string& deviceName,
                         const std::string& propertyName,
                         const std::string& elementName,
                         const std::string& value) -> bool override {
        (void)deviceName;
        (void)propertyName;
        (void)elementName;
        (void)value;
        return true;
    }

    void registerEventCallback(INDIEventCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        eventCallback_ = std::move(callback);
    }

    void unregisterEventCallback() override {
        std::lock_guard<std::mutex> lock(mutex_);
        eventCallback_ = nullptr;
    }

    auto waitForPropertyState(const std::string& deviceName,
                              const std::string& propertyName,
                              INDIPropertyState targetState,
                              std::chrono::milliseconds timeout)
        -> bool override {
        (void)deviceName;
        (void)propertyName;
        (void)targetState;
        (void)timeout;
        return true;
    }

    auto getServerInfo() -> json override {
        json info;
        info["host"] = host_;
        info["port"] = port_;
        info["connected"] = serverConnected_.load();
        info["deviceCount"] = devices_.size();
        return info;
    }

    /**
     * @brief Register a device (for testing/simulation)
     */
    void registerDevice(const INDIDeviceInfo& device) {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_[device.name] = device;
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> serverConnected_{false};
    std::string host_;
    int port_ = 7624;
    std::unordered_map<std::string, INDIDeviceInfo> devices_;
    INDIEventCallback eventCallback_;
};

/**
 * @brief INDI adapter factory
 */
class INDIAdapterFactory {
public:
    /**
     * @brief Create default (stub) adapter for testing
     */
    static auto createDefaultAdapter() -> std::shared_ptr<INDIAdapter> {
        return std::make_shared<DefaultINDIAdapter>();
    }
    
    /**
     * @brief Create real adapter with new INDI client
     */
    static auto createAdapter() -> std::shared_ptr<INDIAdapter>;
    
    /**
     * @brief Create adapter with existing INDI client
     */
    static auto createAdapter(std::shared_ptr<lithium::client::INDIClient> client)
        -> std::shared_ptr<INDIAdapter>;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_INDI_ADAPTER_HPP
