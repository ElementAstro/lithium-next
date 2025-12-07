/*
 * indigo_device_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Device Base Class - Common functionality for all INDIGO devices

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_DEVICE_BASE_HPP
#define LITHIUM_CLIENT_INDIGO_DEVICE_BASE_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "indigo_client.hpp"

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"
#include "device/template/device.hpp"

namespace lithium::client::indigo {

using json = nlohmann::json;
using namespace lithium::device;

/**
 * @brief Connection information for INDIGO devices
 */
struct INDIGOConnectionInfo {
    std::string host{"localhost"};
    int port{7624};
    std::string deviceName;
    bool autoReconnect{false};
    std::chrono::seconds connectTimeout{30};
    std::chrono::seconds commandTimeout{60};
};

/**
 * @brief INDIGO device status
 */
struct INDIGODeviceStatus {
    bool serverConnected{false};
    bool deviceConnected{false};
    PropertyState state{PropertyState::Idle};
    std::string message;
    std::chrono::system_clock::time_point lastUpdate;
};

/**
 * @brief Base class for all INDIGO device implementations
 *
 * This class provides common functionality for INDIGO devices:
 * - Connection management (to INDIGO server and device)
 * - Property caching and synchronization
 * - Event callback handling
 * - Standard INDI/INDIGO property mappings
 *
 * Device-specific implementations (Camera, Focuser, etc.) should inherit
 * from this class and implement device-specific operations.
 */
class INDIGODeviceBase : public AtomDriver {
public:
    /**
     * @brief Constructor
     * @param deviceName The INDIGO device name
     * @param deviceType Device type string (e.g., "Camera", "Focuser")
     */
    explicit INDIGODeviceBase(const std::string& deviceName,
                              const std::string& deviceType = "INDIGO Device");

    /**
     * @brief Virtual destructor
     */
    ~INDIGODeviceBase() override;

    // Non-copyable
    INDIGODeviceBase(const INDIGODeviceBase&) = delete;
    INDIGODeviceBase& operator=(const INDIGODeviceBase&) = delete;

    // Movable
    INDIGODeviceBase(INDIGODeviceBase&&) noexcept;
    INDIGODeviceBase& operator=(INDIGODeviceBase&&) noexcept;

    // ==================== AtomDriver Interface ====================

    /**
     * @brief Connect to the device
     * @param params Connection parameters (host, port in JSON)
     * @return Success or error
     */
    auto connect(const json& params) -> DeviceResult<bool> override;

    /**
     * @brief Disconnect from the device
     * @return Success or error
     */
    auto disconnect() -> DeviceResult<bool> override;

    /**
     * @brief Check if connected
     */
    [[nodiscard]] auto isConnected() const -> bool override;

    /**
     * @brief Reconnect to the device
     * @return Success or error
     */
    auto reconnect() -> DeviceResult<bool> override;

    // ==================== INDIGO Specific Methods ====================

    /**
     * @brief Set connection info
     * @param info Connection information
     */
    void setConnectionInfo(const INDIGOConnectionInfo& info);

    /**
     * @brief Get connection info
     */
    [[nodiscard]] auto getConnectionInfo() const -> const INDIGOConnectionInfo&;

    /**
     * @brief Get device status
     */
    [[nodiscard]] auto getDeviceStatus() const -> INDIGODeviceStatus;

    /**
     * @brief Get the INDIGO client instance
     */
    [[nodiscard]] auto getClient() const -> std::shared_ptr<INDIGOClient>;

    /**
     * @brief Set the shared INDIGO client
     * @param client Shared client instance (for connection pooling)
     */
    void setClient(std::shared_ptr<INDIGOClient> client);

    // ==================== Property Access ====================

    /**
     * @brief Get a property value
     * @param propertyName Property name
     * @return Property or error
     */
    auto getProperty(const std::string& propertyName) -> DeviceResult<Property>;

    /**
     * @brief Get all device properties
     */
    auto getAllProperties() -> DeviceResult<std::vector<Property>>;

    /**
     * @brief Set a text property
     */
    auto setTextProperty(const std::string& propertyName,
                         const std::string& elementName,
                         const std::string& value) -> DeviceResult<bool>;

    /**
     * @brief Set a number property
     */
    auto setNumberProperty(const std::string& propertyName,
                           const std::string& elementName,
                           double value) -> DeviceResult<bool>;

    /**
     * @brief Set a switch property (single element)
     */
    auto setSwitchProperty(const std::string& propertyName,
                           const std::string& elementName,
                           bool value) -> DeviceResult<bool>;

    /**
     * @brief Set multiple switch elements
     */
    auto setSwitchProperty(
        const std::string& propertyName,
        const std::vector<std::pair<std::string, bool>>& elements)
        -> DeviceResult<bool>;

    // ==================== Common Property Helpers ====================

    /**
     * @brief Get text property value
     */
    auto getTextValue(const std::string& propertyName,
                      const std::string& elementName)
        -> DeviceResult<std::string>;

    /**
     * @brief Get number property value
     */
    auto getNumberValue(const std::string& propertyName,
                        const std::string& elementName) -> DeviceResult<double>;

    /**
     * @brief Get switch property value
     */
    auto getSwitchValue(const std::string& propertyName,
                        const std::string& elementName) -> DeviceResult<bool>;

    /**
     * @brief Get active switch name in a property
     */
    auto getActiveSwitchName(const std::string& propertyName)
        -> DeviceResult<std::string>;

    // ==================== Event Handling ====================

    /**
     * @brief Property update callback type
     */
    using PropertyCallback = std::function<void(const Property&)>;

    /**
     * @brief Register callback for property updates
     * @param propertyName Property name (empty for all properties)
     * @param callback Callback function
     */
    void onPropertyUpdate(const std::string& propertyName,
                          PropertyCallback callback);

    /**
     * @brief Connection state callback type
     */
    using ConnectionStateCallback = std::function<void(bool connected)>;

    /**
     * @brief Register callback for connection state changes
     */
    void onConnectionStateChange(ConnectionStateCallback callback);

    // ==================== Utility Methods ====================

    /**
     * @brief Wait for property state to change
     * @param propertyName Property name
     * @param targetState Target state to wait for
     * @param timeout Timeout duration
     * @return True if state reached, false on timeout
     */
    auto waitForPropertyState(const std::string& propertyName,
                              PropertyState targetState,
                              std::chrono::milliseconds timeout)
        -> DeviceResult<bool>;

    /**
     * @brief Wait for property value to match
     * @param propertyName Property name
     * @param elementName Element name
     * @param expectedValue Expected value (double for numbers)
     * @param tolerance Tolerance for comparison
     * @param timeout Timeout duration
     */
    auto waitForNumberValue(const std::string& propertyName,
                            const std::string& elementName,
                            double expectedValue, double tolerance,
                            std::chrono::milliseconds timeout)
        -> DeviceResult<bool>;

    /**
     * @brief Get device info from INFO property
     */
    [[nodiscard]] auto getDeviceInfo() const -> json;

    /**
     * @brief Get device driver name
     */
    [[nodiscard]] auto getDriverName() const -> std::string;

    /**
     * @brief Get device driver version
     */
    [[nodiscard]] auto getDriverVersion() const -> std::string;

    /**
     * @brief Get device interface flags
     */
    [[nodiscard]] auto getDeviceInterfaces() const -> DeviceInterface;

    // ==================== BLOB Handling ====================

    /**
     * @brief Enable BLOB reception
     * @param enable Enable or disable
     * @param urlMode Use URL mode (efficient transfer)
     */
    auto enableBlob(bool enable, bool urlMode = true) -> DeviceResult<bool>;

    /**
     * @brief Fetch BLOB from URL
     * @param url BLOB URL
     * @return BLOB data
     */
    auto fetchBlob(const std::string& url) -> DeviceResult<std::vector<uint8_t>>;

protected:
    /**
     * @brief Called when device is connected
     * Override in derived classes for device-specific initialization
     */
    virtual void onConnected();

    /**
     * @brief Called when device is disconnected
     * Override in derived classes for device-specific cleanup
     */
    virtual void onDisconnected();

    /**
     * @brief Called when a property is updated
     * Override in derived classes to handle specific properties
     */
    virtual void onPropertyUpdated(const Property& property);

    /**
     * @brief Get the INDIGO device name
     */
    [[nodiscard]] auto getINDIGODeviceName() const -> const std::string&;

    /**
     * @brief Check if server is connected
     */
    [[nodiscard]] auto isServerConnected() const -> bool;

    /**
     * @brief Log a message with device context
     */
    void logDevice(int level, const std::string& message) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Helper macros for property access
// ============================================================================

/**
 * @brief Helper to safely get a number property value with default
 */
#define INDIGO_GET_NUMBER(device, prop, elem, default_val) \
    (device)->getNumberValue(prop, elem).value_or(default_val)

/**
 * @brief Helper to safely get a text property value with default
 */
#define INDIGO_GET_TEXT(device, prop, elem, default_val) \
    (device)->getTextValue(prop, elem).value_or(default_val)

/**
 * @brief Helper to safely get a switch property value with default
 */
#define INDIGO_GET_SWITCH(device, prop, elem, default_val) \
    (device)->getSwitchValue(prop, elem).value_or(default_val)

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_DEVICE_BASE_HPP
