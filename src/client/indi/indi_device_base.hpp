/*
 * indi_device_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Device Base Class - Common functionality for all INDI devices

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_DEVICE_BASE_HPP
#define LITHIUM_CLIENT_INDI_DEVICE_BASE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::client::indi {

using json = nlohmann::json;

// Forward declarations
class INDIDeviceBase;

/**
 * @brief Property state enumeration
 */
enum class PropertyState : uint8_t { Idle, Ok, Busy, Alert, Unknown };

/**
 * @brief Property type enumeration
 */
enum class PropertyType : uint8_t { Number, Text, Switch, Light, Blob, Unknown };

/**
 * @brief Device connection state
 */
enum class ConnectionState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief Convert property state to string
 */
[[nodiscard]] constexpr auto propertyStateToString(PropertyState state) noexcept
    -> std::string_view {
    switch (state) {
        case PropertyState::Idle:
            return "Idle";
        case PropertyState::Ok:
            return "Ok";
        case PropertyState::Busy:
            return "Busy";
        case PropertyState::Alert:
            return "Alert";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert string to property state
 */
[[nodiscard]] constexpr auto propertyStateFromString(
    std::string_view state) noexcept -> PropertyState {
    if (state == "Idle")
        return PropertyState::Idle;
    if (state == "Ok")
        return PropertyState::Ok;
    if (state == "Busy")
        return PropertyState::Busy;
    if (state == "Alert")
        return PropertyState::Alert;
    return PropertyState::Unknown;
}

/**
 * @brief Number property element
 */
struct NumberElement {
    std::string name;
    std::string label;
    double value{0.0};
    double min{0.0};
    double max{0.0};
    double step{1.0};
    std::string format{"%g"};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name},     {"label", label}, {"value", value},
                {"min", min},       {"max", max},     {"step", step},
                {"format", format}};
    }
};

/**
 * @brief Text property element
 */
struct TextElement {
    std::string name;
    std::string label;
    std::string value;

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name}, {"label", label}, {"value", value}};
    }
};

/**
 * @brief Switch property element
 */
struct SwitchElement {
    std::string name;
    std::string label;
    bool on{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name}, {"label", label}, {"on", on}};
    }
};

/**
 * @brief Light property element
 */
struct LightElement {
    std::string name;
    std::string label;
    PropertyState state{PropertyState::Idle};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name},
                {"label", label},
                {"state", std::string(propertyStateToString(state))}};
    }
};

/**
 * @brief BLOB property element
 */
struct BlobElement {
    std::string name;
    std::string label;
    std::string format;
    std::vector<uint8_t> data;
    size_t size{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name},
                {"label", label},
                {"format", format},
                {"size", size}};
    }
};

/**
 * @brief Generic property container
 */
struct INDIProperty {
    std::string device;
    std::string name;
    std::string label;
    std::string group;
    PropertyType type{PropertyType::Unknown};
    PropertyState state{PropertyState::Idle};
    std::string permission{"ro"};
    std::string timestamp;

    // Elements storage
    std::vector<NumberElement> numbers;
    std::vector<TextElement> texts;
    std::vector<SwitchElement> switches;
    std::vector<LightElement> lights;
    std::vector<BlobElement> blobs;

    [[nodiscard]] auto isWritable() const noexcept -> bool {
        return permission.find('w') != std::string::npos;
    }

    [[nodiscard]] auto isReadable() const noexcept -> bool {
        return permission.find('r') != std::string::npos;
    }

    [[nodiscard]] auto getNumber(std::string_view elemName) const
        -> std::optional<double> {
        for (const auto& elem : numbers) {
            if (elem.name == elemName) {
                return elem.value;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto getText(std::string_view elemName) const
        -> std::optional<std::string> {
        for (const auto& elem : texts) {
            if (elem.name == elemName) {
                return elem.value;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto getSwitch(std::string_view elemName) const
        -> std::optional<bool> {
        for (const auto& elem : switches) {
            if (elem.name == elemName) {
                return elem.on;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["device"] = device;
        j["name"] = name;
        j["label"] = label;
        j["group"] = group;
        j["state"] = std::string(propertyStateToString(state));
        j["permission"] = permission;

        switch (type) {
            case PropertyType::Number: {
                j["type"] = "number";
                json elems = json::array();
                for (const auto& e : numbers) {
                    elems.push_back(e.toJson());
                }
                j["elements"] = elems;
                break;
            }
            case PropertyType::Text: {
                j["type"] = "text";
                json elems = json::array();
                for (const auto& e : texts) {
                    elems.push_back(e.toJson());
                }
                j["elements"] = elems;
                break;
            }
            case PropertyType::Switch: {
                j["type"] = "switch";
                json elems = json::array();
                for (const auto& e : switches) {
                    elems.push_back(e.toJson());
                }
                j["elements"] = elems;
                break;
            }
            case PropertyType::Light: {
                j["type"] = "light";
                json elems = json::array();
                for (const auto& e : lights) {
                    elems.push_back(e.toJson());
                }
                j["elements"] = elems;
                break;
            }
            case PropertyType::Blob: {
                j["type"] = "blob";
                json elems = json::array();
                for (const auto& e : blobs) {
                    elems.push_back(e.toJson());
                }
                j["elements"] = elems;
                break;
            }
            default:
                j["type"] = "unknown";
                break;
        }
        return j;
    }
};

/**
 * @brief Device event types
 */
enum class DeviceEventType {
    Connected,
    Disconnected,
    PropertyDefined,
    PropertyUpdated,
    PropertyDeleted,
    MessageReceived,
    BlobReceived,
    Error
};

/**
 * @brief Device event data
 */
struct DeviceEvent {
    DeviceEventType type;
    std::string deviceName;
    std::string propertyName;
    std::string message;
    json data;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Device event callback type
 */
using DeviceEventCallback = std::function<void(const DeviceEvent&)>;

/**
 * @brief Property change callback type
 */
using PropertyCallback = std::function<void(const INDIProperty&)>;

/**
 * @brief Driver information
 */
struct DriverInfo {
    std::string name;
    std::string exec;
    std::string version;
    std::string interfaceStr;
};

/**
 * @brief INDI Device Base Class
 *
 * Provides common functionality for all INDI device types including:
 * - Connection management
 * - Property handling
 * - Event system
 * - State management
 */
class INDIDeviceBase {
public:
    /**
     * @brief Construct a new INDI device
     * @param name Device name
     */
    explicit INDIDeviceBase(std::string name);

    /**
     * @brief Virtual destructor
     */
    virtual ~INDIDeviceBase();

    // Disable copy
    INDIDeviceBase(const INDIDeviceBase&) = delete;
    INDIDeviceBase& operator=(const INDIDeviceBase&) = delete;

    // Enable move
    INDIDeviceBase(INDIDeviceBase&&) noexcept = default;
    INDIDeviceBase& operator=(INDIDeviceBase&&) noexcept = default;

    // ==================== Lifecycle ====================

    /**
     * @brief Initialize the device
     * @return true if successful
     */
    virtual auto initialize() -> bool;

    /**
     * @brief Destroy the device
     * @return true if successful
     */
    virtual auto destroy() -> bool;

    /**
     * @brief Connect to the device
     * @param deviceName INDI device name
     * @param timeout Connection timeout in milliseconds
     * @param maxRetry Maximum retry attempts
     * @return true if successful
     */
    virtual auto connect(const std::string& deviceName, int timeout = 5000,
                         int maxRetry = 3) -> bool;

    /**
     * @brief Disconnect from the device
     * @return true if successful
     */
    virtual auto disconnect() -> bool;

    /**
     * @brief Check if device is connected
     * @return true if connected
     */
    [[nodiscard]] virtual auto isConnected() const -> bool;

    /**
     * @brief Get connection state
     * @return Current connection state
     */
    [[nodiscard]] auto getConnectionState() const -> ConnectionState;

    /**
     * @brief Scan for available devices
     * @return List of device names
     */
    virtual auto scan() -> std::vector<std::string>;

    // ==================== Device Information ====================

    /**
     * @brief Get device name
     * @return Device name
     */
    [[nodiscard]] auto getName() const -> const std::string&;

    /**
     * @brief Get INDI device name
     * @return INDI device name
     */
    [[nodiscard]] auto getDeviceName() const -> const std::string&;

    /**
     * @brief Get driver information
     * @return Driver info
     */
    [[nodiscard]] auto getDriverInfo() const -> const DriverInfo&;

    /**
     * @brief Get device type string
     * @return Device type
     */
    [[nodiscard]] virtual auto getDeviceType() const -> std::string = 0;

    /**
     * @brief Get device status as JSON
     * @return Status JSON
     */
    [[nodiscard]] virtual auto getStatus() const -> json;

    // ==================== Property Management ====================

    /**
     * @brief Get all properties
     * @return Map of property name to property
     */
    [[nodiscard]] auto getProperties() const
        -> std::unordered_map<std::string, INDIProperty>;

    /**
     * @brief Get a specific property
     * @param propertyName Property name
     * @return Property or nullopt if not found
     */
    [[nodiscard]] auto getProperty(const std::string& propertyName) const
        -> std::optional<INDIProperty>;

    /**
     * @brief Set number property
     * @param propertyName Property name
     * @param elementName Element name
     * @param value Value to set
     * @return true if successful
     */
    virtual auto setNumberProperty(const std::string& propertyName,
                                   const std::string& elementName, double value)
        -> bool;

    /**
     * @brief Set text property
     * @param propertyName Property name
     * @param elementName Element name
     * @param value Value to set
     * @return true if successful
     */
    virtual auto setTextProperty(const std::string& propertyName,
                                 const std::string& elementName,
                                 const std::string& value) -> bool;

    /**
     * @brief Set switch property
     * @param propertyName Property name
     * @param elementName Element name
     * @param value Value to set
     * @return true if successful
     */
    virtual auto setSwitchProperty(const std::string& propertyName,
                                   const std::string& elementName, bool value)
        -> bool;

    /**
     * @brief Wait for property state
     * @param propertyName Property name
     * @param targetState Target state
     * @param timeout Timeout in milliseconds
     * @return true if state reached
     */
    auto waitForPropertyState(const std::string& propertyName,
                              PropertyState targetState,
                              std::chrono::milliseconds timeout =
                                  std::chrono::seconds(30)) -> bool;

    // ==================== Event System ====================

    /**
     * @brief Register event callback
     * @param callback Callback function
     */
    void registerEventCallback(DeviceEventCallback callback);

    /**
     * @brief Unregister event callback
     */
    void unregisterEventCallback();

    /**
     * @brief Watch a property for changes
     * @param propertyName Property name
     * @param callback Callback function
     */
    void watchProperty(const std::string& propertyName,
                       PropertyCallback callback);

    /**
     * @brief Stop watching a property
     * @param propertyName Property name
     */
    void unwatchProperty(const std::string& propertyName);

protected:
    // ==================== Internal Property Handling ====================

    /**
     * @brief Handle property definition
     * @param property Property data
     */
    virtual void onPropertyDefined(const INDIProperty& property);

    /**
     * @brief Handle property update
     * @param property Property data
     */
    virtual void onPropertyUpdated(const INDIProperty& property);

    /**
     * @brief Handle property deletion
     * @param propertyName Property name
     */
    virtual void onPropertyDeleted(const std::string& propertyName);

    /**
     * @brief Handle device message
     * @param message Message text
     */
    virtual void onMessage(const std::string& message);

    /**
     * @brief Handle BLOB received
     * @param property BLOB property
     */
    virtual void onBlobReceived(const INDIProperty& property);

    /**
     * @brief Emit device event
     * @param event Event data
     */
    void emitEvent(const DeviceEvent& event);

    /**
     * @brief Update internal property cache
     * @param property Property to update
     */
    void updatePropertyCache(const INDIProperty& property);

    /**
     * @brief Set connection state
     * @param state New state
     */
    void setConnectionState(ConnectionState state);

    /**
     * @brief Log message
     * @param level Log level
     * @param message Message
     */
    void log(std::string_view level, std::string_view message) const;

    // ==================== Member Variables ====================

    std::string name_;
    std::string deviceName_;
    DriverInfo driverInfo_;

    std::atomic<ConnectionState> connectionState_{ConnectionState::Disconnected};
    std::atomic<bool> initialized_{false};

    mutable std::mutex propertiesMutex_;
    std::unordered_map<std::string, INDIProperty> properties_;

    mutable std::mutex callbackMutex_;
    DeviceEventCallback eventCallback_;
    std::unordered_map<std::string, PropertyCallback> propertyCallbacks_;

    std::mutex stateMutex_;
    std::condition_variable stateCondition_;

    // Common device properties
    std::atomic<double> pollingPeriod_{1000.0};
    std::atomic<bool> debugEnabled_{false};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_DEVICE_BASE_HPP
