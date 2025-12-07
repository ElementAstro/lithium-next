/*
 * device_component_bridge.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Bridge between device manager and component manager

**************************************************/

#ifndef LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_BRIDGE_HPP
#define LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_BRIDGE_HPP

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"
#include "device_component_adapter.hpp"

namespace lithium {
class ComponentManager;
}

namespace lithium::device {

// Forward declarations
class DevicePluginLoader;

/**
 * @brief Bridge event types
 */
enum class BridgeEventType {
    DeviceRegistered,      ///< Device registered as component
    DeviceUnregistered,    ///< Device unregistered from components
    DeviceStateChanged,    ///< Device/component state changed
    ComponentStateChanged, ///< Component state changed
    SyncStarted,           ///< Synchronization started
    SyncCompleted,         ///< Synchronization completed
    Error                  ///< Error occurred
};

/**
 * @brief Bridge event
 */
struct BridgeEvent {
    BridgeEventType type;
    std::string deviceName;
    std::string componentName;
    DeviceComponentState deviceState;
    ComponentState componentState;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json data;

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Bridge event callback
 */
using BridgeEventCallback = std::function<void(const BridgeEvent&)>;

/**
 * @brief Bridge configuration
 */
struct BridgeConfig {
    bool autoRegister{true};        ///< Auto-register devices as components
    bool autoSync{true};            ///< Auto-sync states
    bool bidirectional{true};       ///< Bidirectional state sync
    std::string deviceGroup{"devices"}; ///< Component group for devices
    int syncInterval{0};            ///< Sync interval in ms (0 = immediate)

    // Auto-grouping configuration
    bool autoGroupByType{true};           ///< Auto-create groups by device type
    bool autoGroupByState{true};          ///< Auto-create groups by device state
    std::string cameraGroup{"cameras"};   ///< Group name for cameras
    std::string mountGroup{"mounts"};     ///< Group name for mounts
    std::string focuserGroup{"focusers"}; ///< Group name for focusers
    std::string filterWheelGroup{"filter_wheels"}; ///< Group name for filter
                                                   ///< wheels
    std::string domeGroup{"domes"};       ///< Group name for domes
    std::string guiderGroup{"guiders"};   ///< Group name for guiders
    std::string rotatorGroup{"rotators"}; ///< Group name for rotators

    // State group names
    std::string connectedGroup{"connected_devices"};
    std::string errorGroup{"error_devices"};
    std::string idleGroup{"idle_devices"};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> BridgeConfig;
};

/**
 * @brief Bridge between DeviceManager and ComponentManager
 *
 * This bridge provides:
 * - Automatic registration of devices as components
 * - State synchronization between device and component systems
 * - Unified lifecycle management
 * - Event forwarding between systems
 *
 * Usage:
 * 1. Create bridge with references to both managers
 * 2. Call initialize() to set up event handlers
 * 3. Use registerDevice() to add devices as components
 * 4. States will be automatically synchronized
 */
class DeviceComponentBridge {
public:
    /**
     * @brief Construct bridge with managers
     * @param componentManager Reference to component manager
     */
    explicit DeviceComponentBridge(
        std::shared_ptr<lithium::ComponentManager> componentManager);

    ~DeviceComponentBridge();

    // Disable copy
    DeviceComponentBridge(const DeviceComponentBridge&) = delete;
    DeviceComponentBridge& operator=(const DeviceComponentBridge&) = delete;

    // ==================== Initialization ====================

    /**
     * @brief Initialize the bridge
     * @param config Bridge configuration
     * @return Success or error
     */
    auto initialize(const BridgeConfig& config = {}) -> DeviceResult<bool>;

    /**
     * @brief Shutdown the bridge
     */
    void shutdown();

    /**
     * @brief Check if bridge is initialized
     */
    [[nodiscard]] auto isInitialized() const -> bool;

    // ==================== Device Registration ====================

    /**
     * @brief Register a device as a component
     * @param adapter Device adapter to register
     * @return Success or error
     */
    auto registerDevice(std::shared_ptr<DeviceComponentAdapter> adapter)
        -> DeviceResult<bool>;

    /**
     * @brief Register a device with configuration
     * @param device Device to wrap and register
     * @param config Adapter configuration
     * @param name Component name
     * @return Success or error
     */
    auto registerDevice(std::shared_ptr<AtomDriver> device,
                        const DeviceAdapterConfig& config = {},
                        const std::string& name = {})
        -> DeviceResult<bool>;

    /**
     * @brief Unregister a device from components
     * @param name Device/component name
     * @return Success or error
     */
    auto unregisterDevice(const std::string& name) -> DeviceResult<bool>;

    /**
     * @brief Unregister all devices
     * @return Number of devices unregistered
     */
    auto unregisterAllDevices() -> size_t;

    // ==================== Device Query ====================

    /**
     * @brief Check if a device is registered
     * @param name Device/component name
     */
    [[nodiscard]] auto isDeviceRegistered(const std::string& name) const
        -> bool;

    /**
     * @brief Get a registered device adapter
     * @param name Device/component name
     * @return Adapter or nullptr
     */
    [[nodiscard]] auto getDeviceAdapter(const std::string& name) const
        -> std::shared_ptr<DeviceComponentAdapter>;

    /**
     * @brief Get all registered device names
     */
    [[nodiscard]] auto getRegisteredDevices() const
        -> std::vector<std::string>;

    /**
     * @brief Get devices by type
     * @param type Device type
     */
    [[nodiscard]] auto getDevicesByType(const std::string& type) const
        -> std::vector<std::shared_ptr<DeviceComponentAdapter>>;

    /**
     * @brief Get devices by state
     * @param state Device state
     */
    [[nodiscard]] auto getDevicesByState(DeviceComponentState state) const
        -> std::vector<std::shared_ptr<DeviceComponentAdapter>>;

    // ==================== State Synchronization ====================

    /**
     * @brief Sync device state to component
     * @param name Device/component name
     */
    void syncDeviceToComponent(const std::string& name);

    /**
     * @brief Sync component state to device
     * @param name Device/component name
     */
    void syncComponentToDevice(const std::string& name);

    /**
     * @brief Sync all device states
     */
    void syncAll();

    // ==================== Batch Operations ====================

    /**
     * @brief Start all registered devices
     * @return Number of devices started
     */
    auto startAllDevices() -> size_t;

    /**
     * @brief Stop all registered devices
     * @return Number of devices stopped
     */
    auto stopAllDevices() -> size_t;

    /**
     * @brief Connect all devices
     * @return Number of devices connected
     */
    auto connectAllDevices() -> size_t;

    /**
     * @brief Disconnect all devices
     * @return Number of devices disconnected
     */
    auto disconnectAllDevices() -> size_t;

    // ==================== Group Operations ====================

    /**
     * @brief Start all devices in a specific group
     * @param group Group name
     * @return Number of devices started
     */
    auto startGroup(const std::string& group) -> size_t;

    /**
     * @brief Stop all devices in a specific group
     * @param group Group name
     * @return Number of devices stopped
     */
    auto stopGroup(const std::string& group) -> size_t;

    /**
     * @brief Connect all devices in a specific group
     * @param group Group name
     * @return Number of devices connected
     */
    auto connectGroup(const std::string& group) -> size_t;

    /**
     * @brief Disconnect all devices in a specific group
     * @param group Group name
     * @return Number of devices disconnected
     */
    auto disconnectGroup(const std::string& group) -> size_t;

    /**
     * @brief Get all available device groups
     * @return Vector of group names
     */
    [[nodiscard]] auto getDeviceGroups() const -> std::vector<std::string>;

    /**
     * @brief Get devices in a specific group
     * @param group Group name
     * @return Vector of device names
     */
    [[nodiscard]] auto getDevicesInGroup(const std::string& group) const
        -> std::vector<std::string>;

    // ==================== Configuration Update ====================

    /**
     * @brief Update device configuration at runtime
     * @param name Device name
     * @param config New configuration JSON
     * @return Success or error
     */
    auto updateDeviceConfig(const std::string& name,
                            const nlohmann::json& config)
        -> DeviceResult<bool>;

    /**
     * @brief Get device configuration
     * @param name Device name
     * @return Configuration JSON or empty
     */
    [[nodiscard]] auto getDeviceConfig(const std::string& name) const
        -> nlohmann::json;

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to bridge events
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribe(BridgeEventCallback callback) -> uint64_t;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    void unsubscribe(uint64_t subscriptionId);

    // ==================== Plugin Loader Integration ====================

    /**
     * @brief Set the device plugin loader
     * @param loader Plugin loader instance
     */
    void setPluginLoader(std::shared_ptr<DevicePluginLoader> loader);

    /**
     * @brief Get the plugin loader
     */
    [[nodiscard]] auto getPluginLoader() const
        -> std::shared_ptr<DevicePluginLoader>;

    // ==================== Statistics ====================

    /**
     * @brief Get bridge statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

    /**
     * @brief Get bridge configuration
     */
    [[nodiscard]] auto getConfig() const -> BridgeConfig;

private:
    /**
     * @brief Emit a bridge event
     */
    void emitEvent(const BridgeEvent& event);

    /**
     * @brief Create a bridge event
     */
    [[nodiscard]] auto createEvent(BridgeEventType type,
                                    const std::string& deviceName,
                                    const std::string& message = {}) const
        -> BridgeEvent;

    /**
     * @brief Handle component state change
     */
    void onComponentStateChanged(const std::string& name,
                                 ComponentEvent event,
                                 const nlohmann::json& data);

    /**
     * @brief Handle device state change
     */
    void onDeviceStateChanged(const std::string& name,
                              DeviceComponentState oldState,
                              DeviceComponentState newState);

    // ==================== Group Helper Methods ====================

    /**
     * @brief Map device category to group name
     */
    [[nodiscard]] auto categoryToGroup(const std::string& category) const
        -> std::string;

    /**
     * @brief Map device state to state group name
     */
    [[nodiscard]] auto stateToGroup(DeviceComponentState state) const
        -> std::string;

    /**
     * @brief Add device to type-based group
     */
    void addToTypeGroup(const std::string& name, const std::string& deviceType);

    /**
     * @brief Update device state groups
     */
    void updateStateGroups(const std::string& name,
                           DeviceComponentState oldState,
                           DeviceComponentState newState);

    // Members
    mutable std::shared_mutex mutex_;
    std::shared_ptr<lithium::ComponentManager> componentManager_;
    std::shared_ptr<DevicePluginLoader> pluginLoader_;
    BridgeConfig config_;
    bool initialized_{false};

    // Registered device adapters
    std::unordered_map<std::string, std::shared_ptr<DeviceComponentAdapter>>
        deviceAdapters_;

    // Event subscribers
    mutable std::shared_mutex eventMutex_;
    std::unordered_map<uint64_t, BridgeEventCallback> eventSubscribers_;
    uint64_t nextSubscriberId_{1};

    // Statistics
    size_t registrationCount_{0};
    size_t unregistrationCount_{0};
    size_t syncCount_{0};
    size_t errorCount_{0};
};

/**
 * @brief Factory function to create the bridge
 * @param componentManager Component manager instance
 * @param config Bridge configuration
 * @return Bridge instance
 */
[[nodiscard]] auto createDeviceComponentBridge(
    std::shared_ptr<lithium::ComponentManager> componentManager,
    const BridgeConfig& config = {})
    -> std::shared_ptr<DeviceComponentBridge>;

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_BRIDGE_HPP
