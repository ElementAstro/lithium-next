/*
 * ascom_device_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Device Base Class

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_DEVICE_BASE_HPP
#define LITHIUM_CLIENT_ASCOM_DEVICE_BASE_HPP

#include "alpaca_client.hpp"
#include "ascom_types.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace lithium::client::ascom {

/**
 * @brief Device connection state
 */
enum class DeviceState : uint8_t {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

/**
 * @brief Convert device state to string
 */
[[nodiscard]] inline auto deviceStateToString(DeviceState state)
    -> std::string {
    switch (state) {
        case DeviceState::Disconnected:
            return "Disconnected";
        case DeviceState::Connecting:
            return "Connecting";
        case DeviceState::Connected:
            return "Connected";
        case DeviceState::Disconnecting:
            return "Disconnecting";
        case DeviceState::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

/**
 * @brief Device event types
 */
enum class DeviceEventType : uint8_t {
    Connected,
    Disconnected,
    PropertyChanged,
    StateChanged,
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

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", static_cast<int>(type)},
                {"deviceName", deviceName},
                {"propertyName", propertyName},
                {"message", message},
                {"data", data}};
    }
};

/**
 * @brief Device event callback type
 */
using DeviceEventCallback = std::function<void(const DeviceEvent&)>;

/**
 * @brief ASCOM Device Base Class
 *
 * Provides common functionality for all ASCOM device types:
 * - Connection management
 * - Property access via Alpaca REST API
 * - Event callbacks
 * - Status reporting
 */
class ASCOMDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new ASCOM Device
     * @param name Device name
     * @param deviceType Device type
     * @param deviceNumber Device number (0-based)
     */
    ASCOMDeviceBase(std::string name, ASCOMDeviceType deviceType,
                    int deviceNumber = 0);

    /**
     * @brief Virtual destructor
     */
    virtual ~ASCOMDeviceBase();

    // Disable copy
    ASCOMDeviceBase(const ASCOMDeviceBase&) = delete;
    ASCOMDeviceBase& operator=(const ASCOMDeviceBase&) = delete;

    // ==================== Device Type ====================

    /**
     * @brief Get device type string
     * @return Device type name
     */
    [[nodiscard]] virtual auto getDeviceType() const -> std::string {
        return deviceTypeToString(deviceType_);
    }

    /**
     * @brief Get ASCOM device type enum
     */
    [[nodiscard]] auto getASCOMDeviceType() const -> ASCOMDeviceType {
        return deviceType_;
    }

    /**
     * @brief Get device number
     */
    [[nodiscard]] auto getDeviceNumber() const -> int { return deviceNumber_; }

    // ==================== Connection ====================

    /**
     * @brief Set the Alpaca client to use
     * @param client Shared pointer to Alpaca client
     */
    void setClient(std::shared_ptr<AlpacaClient> client);

    /**
     * @brief Get the Alpaca client
     */
    [[nodiscard]] auto getClient() const -> std::shared_ptr<AlpacaClient> {
        return client_;
    }

    /**
     * @brief Connect to the device
     * @param timeout Connection timeout in milliseconds
     * @return true if connection successful
     */
    virtual auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool;

    /**
     * @brief Disconnect from the device
     * @return true if disconnection successful
     */
    virtual auto disconnect() -> bool;

    /**
     * @brief Check if device is connected
     */
    [[nodiscard]] virtual auto isConnected() const -> bool;

    /**
     * @brief Get device connection state
     */
    [[nodiscard]] auto getState() const -> DeviceState { return state_.load(); }

    // ==================== Device Info ====================

    /**
     * @brief Get device name
     */
    [[nodiscard]] auto getName() const -> const std::string& { return name_; }

    /**
     * @brief Get device description
     */
    [[nodiscard]] auto getDescription() const -> std::string;

    /**
     * @brief Get driver info
     */
    [[nodiscard]] auto getDriverInfo() const -> std::string;

    /**
     * @brief Get driver version
     */
    [[nodiscard]] auto getDriverVersion() const -> std::string;

    /**
     * @brief Get interface version
     */
    [[nodiscard]] auto getInterfaceVersion() const -> int;

    /**
     * @brief Get supported actions
     */
    [[nodiscard]] auto getSupportedActions() const -> std::vector<std::string>;

    // ==================== Property Access ====================

    /**
     * @brief Get a property value (GET request)
     * @param property Property name
     * @param params Optional query parameters
     * @return Alpaca response
     */
    auto getProperty(
        const std::string& property,
        const std::unordered_map<std::string, std::string>& params = {})
        -> AlpacaResponse;

    /**
     * @brief Set a property value (PUT request)
     * @param property Property name
     * @param params Form parameters
     * @return Alpaca response
     */
    auto setProperty(const std::string& property,
                     const std::unordered_map<std::string, std::string>& params)
        -> AlpacaResponse;

    /**
     * @brief Execute an action
     * @param action Action name
     * @param parameters Action parameters (JSON string)
     * @return Action result
     */
    auto executeAction(const std::string& action,
                       const std::string& parameters = "") -> std::string;

    // ==================== Convenience Property Getters ====================

    /**
     * @brief Get boolean property
     */
    [[nodiscard]] auto getBoolProperty(const std::string& property) const
        -> std::optional<bool>;

    /**
     * @brief Get integer property
     */
    [[nodiscard]] auto getIntProperty(const std::string& property) const
        -> std::optional<int>;

    /**
     * @brief Get double property
     */
    [[nodiscard]] auto getDoubleProperty(const std::string& property) const
        -> std::optional<double>;

    /**
     * @brief Get string property
     */
    [[nodiscard]] auto getStringProperty(const std::string& property) const
        -> std::optional<std::string>;

    // ==================== Convenience Property Setters ====================

    /**
     * @brief Set boolean property
     */
    auto setBoolProperty(const std::string& property, bool value) -> bool;

    /**
     * @brief Set integer property
     */
    auto setIntProperty(const std::string& property, int value) -> bool;

    /**
     * @brief Set double property
     */
    auto setDoubleProperty(const std::string& property, double value) -> bool;

    /**
     * @brief Set string property
     */
    auto setStringProperty(const std::string& property,
                           const std::string& value) -> bool;

    // ==================== Events ====================

    /**
     * @brief Register event callback
     * @param callback Callback function
     */
    void registerEventCallback(DeviceEventCallback callback);

    /**
     * @brief Unregister event callback
     */
    void unregisterEventCallback();

    // ==================== Status ====================

    /**
     * @brief Get device status as JSON
     * @return Status JSON object
     */
    [[nodiscard]] virtual auto getStatus() const -> json;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] auto getLastError() const -> const std::string& {
        return lastError_;
    }

protected:
    // ==================== Event Emission ====================

    /**
     * @brief Emit device event
     */
    void emitEvent(DeviceEventType type, const std::string& property = "",
                   const std::string& message = "", const json& data = {});

    /**
     * @brief Set error state
     */
    void setError(const std::string& message);

    /**
     * @brief Clear error state
     */
    void clearError();

    // ==================== Member Variables ====================

    std::string name_;
    ASCOMDeviceType deviceType_;
    int deviceNumber_;

    std::shared_ptr<AlpacaClient> client_;
    std::atomic<DeviceState> state_{DeviceState::Disconnected};

    std::string lastError_;
    mutable std::mutex mutex_;

    DeviceEventCallback eventCallback_;
    std::mutex eventMutex_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_DEVICE_BASE_HPP
