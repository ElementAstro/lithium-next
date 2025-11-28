/*
 * ascom_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM protocol adapter for device services

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_ASCOM_ADAPTER_HPP
#define LITHIUM_DEVICE_SERVICE_ASCOM_ADAPTER_HPP

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

// Forward declarations
namespace lithium::client {
class ASCOMClient;
struct DeviceInfo;
struct PropertyValue;
}  // namespace lithium::client

namespace lithium::client::ascom {
struct ASCOMDeviceDescription;
enum class ASCOMDeviceType;
}  // namespace lithium::client::ascom

namespace lithium::device {

using json = nlohmann::json;

/**
 * @brief ASCOM property types
 */
enum class ASCOMPropertyType {
    NUMBER,
    STRING,
    BOOLEAN,
    ARRAY,
    UNKNOWN
};

/**
 * @brief ASCOM property value container
 */
struct ASCOMPropertyValue {
    std::string name;
    ASCOMPropertyType type = ASCOMPropertyType::UNKNOWN;

    // Value storage
    double numberValue = 0.0;
    std::string stringValue;
    bool boolValue = false;
    std::vector<double> arrayValue;

    std::string label;

    auto toJson() const -> json {
        json j;
        j["name"] = name;
        j["label"] = label;

        switch (type) {
            case ASCOMPropertyType::NUMBER:
                j["type"] = "number";
                j["value"] = numberValue;
                break;
            case ASCOMPropertyType::STRING:
                j["type"] = "string";
                j["value"] = stringValue;
                break;
            case ASCOMPropertyType::BOOLEAN:
                j["type"] = "boolean";
                j["value"] = boolValue;
                break;
            case ASCOMPropertyType::ARRAY:
                j["type"] = "array";
                j["value"] = arrayValue;
                break;
            default:
                j["type"] = "unknown";
                break;
        }
        return j;
    }
};

/**
 * @brief ASCOM device info
 */
struct ASCOMDeviceInfo {
    std::string name;
    std::string deviceType;
    int deviceNumber = 0;
    std::string uniqueId;
    std::string driverInfo;
    std::string driverVersion;
    bool isConnected = false;
    std::chrono::system_clock::time_point lastUpdate;

    std::unordered_map<std::string, ASCOMPropertyValue> properties;

    auto toJson() const -> json {
        json j;
        j["name"] = name;
        j["deviceType"] = deviceType;
        j["deviceNumber"] = deviceNumber;
        j["uniqueId"] = uniqueId;
        j["driverInfo"] = driverInfo;
        j["driverVersion"] = driverVersion;
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
 * @brief ASCOM event types
 */
enum class ASCOMEventType {
    DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    PROPERTY_CHANGED,
    ERROR,
    SERVER_CONNECTED,
    SERVER_DISCONNECTED
};

/**
 * @brief ASCOM event data
 */
struct ASCOMEvent {
    ASCOMEventType type;
    std::string deviceName;
    std::string propertyName;
    std::string message;
    json data;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief ASCOM event callback type
 */
using ASCOMEventCallback = std::function<void(const ASCOMEvent&)>;

/**
 * @brief ASCOM adapter interface for device services
 *
 * Provides a unified interface for ASCOM Alpaca protocol operations,
 * abstracting the low-level ASCOM client details.
 */
class ASCOMAdapter {
public:
    ASCOMAdapter() = default;
    virtual ~ASCOMAdapter() = default;

    // Disable copy
    ASCOMAdapter(const ASCOMAdapter&) = delete;
    ASCOMAdapter& operator=(const ASCOMAdapter&) = delete;

    /**
     * @brief Connect to ASCOM Alpaca server
     */
    virtual auto connectServer(const std::string& host = "localhost",
                               int port = 11111) -> bool = 0;

    /**
     * @brief Disconnect from ASCOM server
     */
    virtual auto disconnectServer() -> bool = 0;

    /**
     * @brief Check if connected to server
     */
    [[nodiscard]] virtual auto isServerConnected() const -> bool = 0;

    /**
     * @brief Get list of available devices
     */
    virtual auto getDevices() -> std::vector<ASCOMDeviceInfo> = 0;

    /**
     * @brief Get device info by name
     */
    virtual auto getDevice(const std::string& deviceName)
        -> std::optional<ASCOMDeviceInfo> = 0;

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
        -> std::optional<ASCOMPropertyValue> = 0;

    /**
     * @brief Set property value
     */
    virtual auto setProperty(const std::string& deviceName,
                             const std::string& propertyName,
                             const json& value) -> bool = 0;

    /**
     * @brief Execute device action
     */
    virtual auto executeAction(const std::string& deviceName,
                               const std::string& action,
                               const std::string& parameters = "") -> std::string = 0;

    /**
     * @brief Get supported actions for device
     */
    virtual auto getSupportedActions(const std::string& deviceName)
        -> std::vector<std::string> = 0;

    /**
     * @brief Register event callback
     */
    virtual void registerEventCallback(ASCOMEventCallback callback) = 0;

    /**
     * @brief Unregister event callback
     */
    virtual void unregisterEventCallback() = 0;

    /**
     * @brief Get server info
     */
    virtual auto getServerInfo() -> json = 0;
};

/**
 * @brief Real ASCOM adapter implementation using ASCOMClient
 */
class ASCOMClientAdapter : public ASCOMAdapter {
public:
    /**
     * @brief Construct adapter with existing client
     */
    explicit ASCOMClientAdapter(std::shared_ptr<lithium::client::ASCOMClient> client);
    
    /**
     * @brief Construct adapter (creates internal client)
     */
    ASCOMClientAdapter();
    
    ~ASCOMClientAdapter() override;

    auto connectServer(const std::string& host, int port) -> bool override;
    auto disconnectServer() -> bool override;
    [[nodiscard]] auto isServerConnected() const -> bool override;

    auto getDevices() -> std::vector<ASCOMDeviceInfo> override;
    auto getDevice(const std::string& deviceName)
        -> std::optional<ASCOMDeviceInfo> override;

    auto connectDevice(const std::string& deviceName) -> bool override;
    auto disconnectDevice(const std::string& deviceName) -> bool override;

    auto getProperty(const std::string& deviceName,
                     const std::string& propertyName)
        -> std::optional<ASCOMPropertyValue> override;

    auto setProperty(const std::string& deviceName,
                     const std::string& propertyName,
                     const json& value) -> bool override;

    auto executeAction(const std::string& deviceName,
                       const std::string& action,
                       const std::string& parameters) -> std::string override;

    auto getSupportedActions(const std::string& deviceName)
        -> std::vector<std::string> override;

    void registerEventCallback(ASCOMEventCallback callback) override;
    void unregisterEventCallback() override;

    auto getServerInfo() -> json override;

    /**
     * @brief Get the underlying ASCOM client
     */
    auto getClient() const -> std::shared_ptr<lithium::client::ASCOMClient> {
        return client_;
    }

private:
    // Convert client DeviceInfo to adapter ASCOMDeviceInfo
    ASCOMDeviceInfo convertDeviceInfo(const lithium::client::DeviceInfo& info) const;

    std::shared_ptr<lithium::client::ASCOMClient> client_;
    mutable std::mutex mutex_;
    ASCOMEventCallback eventCallback_;
    bool ownsClient_{false};
};

/**
 * @brief Default/stub ASCOM adapter implementation for testing
 */
class DefaultASCOMAdapter : public ASCOMAdapter {
public:
    DefaultASCOMAdapter() = default;
    ~DefaultASCOMAdapter() override = default;

    auto connectServer(const std::string& host, int port) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = host;
        port_ = port;
        serverConnected_ = true;
        LOG_INFO( "ASCOMAdapter: Connected to server %s:%d", host.c_str(), port);
        return true;
    }

    auto disconnectServer() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        serverConnected_ = false;
        devices_.clear();
        LOG_INFO( "ASCOMAdapter: Disconnected from server");
        return true;
    }

    [[nodiscard]] auto isServerConnected() const -> bool override {
        return serverConnected_.load();
    }

    auto getDevices() -> std::vector<ASCOMDeviceInfo> override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ASCOMDeviceInfo> result;
        result.reserve(devices_.size());
        for (const auto& [name, info] : devices_) {
            result.push_back(info);
        }
        return result;
    }

    auto getDevice(const std::string& deviceName)
        -> std::optional<ASCOMDeviceInfo> override {
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
        -> std::optional<ASCOMPropertyValue> override {
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

    auto setProperty(const std::string& deviceName,
                     const std::string& propertyName,
                     const json& value) -> bool override {
        (void)deviceName;
        (void)propertyName;
        (void)value;
        return true;
    }

    auto executeAction(const std::string& deviceName,
                       const std::string& action,
                       const std::string& parameters) -> std::string override {
        (void)deviceName;
        (void)action;
        (void)parameters;
        return "";
    }

    auto getSupportedActions(const std::string& deviceName)
        -> std::vector<std::string> override {
        (void)deviceName;
        return {};
    }

    void registerEventCallback(ASCOMEventCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        eventCallback_ = std::move(callback);
    }

    void unregisterEventCallback() override {
        std::lock_guard<std::mutex> lock(mutex_);
        eventCallback_ = nullptr;
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
    void registerDevice(const ASCOMDeviceInfo& device) {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_[device.name] = device;
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> serverConnected_{false};
    std::string host_;
    int port_ = 11111;
    std::unordered_map<std::string, ASCOMDeviceInfo> devices_;
    ASCOMEventCallback eventCallback_;
};

/**
 * @brief ASCOM adapter factory
 */
class ASCOMAdapterFactory {
public:
    /**
     * @brief Create default (stub) adapter for testing
     */
    static auto createDefaultAdapter() -> std::shared_ptr<ASCOMAdapter> {
        return std::make_shared<DefaultASCOMAdapter>();
    }
    
    /**
     * @brief Create real adapter with new ASCOM client
     */
    static auto createAdapter() -> std::shared_ptr<ASCOMAdapter>;
    
    /**
     * @brief Create adapter with existing ASCOM client
     */
    static auto createAdapter(std::shared_ptr<lithium::client::ASCOMClient> client)
        -> std::shared_ptr<ASCOMAdapter>;
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_ASCOM_ADAPTER_HPP
