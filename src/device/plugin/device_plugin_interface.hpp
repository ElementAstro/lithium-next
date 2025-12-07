/*
 * device_plugin_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device plugin interface for extending device support

**************************************************/

#ifndef LITHIUM_DEVICE_PLUGIN_INTERFACE_HPP
#define LITHIUM_DEVICE_PLUGIN_INTERFACE_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"
#include "device/service/device_type_registry.hpp"
#include "device/template/device.hpp"
#include "server/plugin/plugin_interface.hpp"

namespace lithium::device {

// Forward declarations
class DeviceBackend;
class DeviceFactory;

/**
 * @brief Device plugin metadata extending server plugin metadata
 */
struct DevicePluginMetadata : public server::plugin::PluginMetadata {
    std::string backendName;          ///< Backend identifier (e.g., "INDI", "ASCOM")
    std::string backendVersion;       ///< Backend version
    bool supportsHotPlug{true};       ///< Supports hot-plugging
    bool supportsAutoDiscovery{true}; ///< Supports device auto-discovery
    bool requiresServer{false};       ///< Requires external server (e.g., INDI server)
    std::vector<std::string> supportedDeviceCategories;  ///< Device categories

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DevicePluginMetadata;
};

/**
 * @brief Device plugin state extending server plugin state
 */
enum class DevicePluginState {
    Unloaded,       ///< Plugin not loaded
    Loading,        ///< Plugin currently loading
    Loaded,         ///< Plugin loaded but not initialized
    Initializing,   ///< Plugin initializing
    Ready,          ///< Plugin ready, types registered
    Running,        ///< Backend running, devices available
    Paused,         ///< Plugin paused
    Stopping,       ///< Plugin shutting down
    Error,          ///< Plugin in error state
    Disabled        ///< Plugin disabled by user
};

/**
 * @brief Convert device plugin state to string
 */
[[nodiscard]] auto devicePluginStateToString(DevicePluginState state)
    -> std::string;

/**
 * @brief Device plugin event types
 */
enum class DevicePluginEventType {
    TypeRegistered,        ///< New device type registered
    TypeUnregistered,      ///< Device type unregistered
    BackendConnected,      ///< Backend connected to server
    BackendDisconnected,   ///< Backend disconnected from server
    DeviceDiscovered,      ///< New device discovered
    DeviceLost,            ///< Device lost/disconnected
    HotPlugStarted,        ///< Hot-plug reload started
    HotPlugCompleted,      ///< Hot-plug reload completed
    Error                  ///< Error occurred
};

/**
 * @brief Device plugin event
 */
struct DevicePluginEvent {
    DevicePluginEventType type;
    std::string pluginName;
    std::string typeName;           ///< For type events
    std::string deviceId;           ///< For device events
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json data;            ///< Additional event data

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Device plugin event callback
 */
using DevicePluginEventCallback = std::function<void(const DevicePluginEvent&)>;

/**
 * @brief Device migration context for hot-plugging
 *
 * Contains state information for migrating devices during plugin reload.
 */
struct DeviceMigrationContext {
    std::string deviceId;
    std::string deviceName;
    std::string deviceType;
    bool wasConnected{false};
    nlohmann::json deviceState;     ///< Saved device state
    nlohmann::json connectionParams; ///< Connection parameters
    std::chrono::system_clock::time_point migratedAt;
};

/**
 * @brief Base interface for device plugins
 *
 * Device plugins extend server plugins with device-specific functionality:
 * - Device type registration
 * - Device factory registration
 * - Backend management
 * - Hot-plug support
 */
class IDevicePlugin : public server::plugin::IPlugin {
public:
    ~IDevicePlugin() override = default;

    // ==================== Device Type Registration ====================

    /**
     * @brief Get device types provided by this plugin
     * @return Vector of device type information
     */
    [[nodiscard]] virtual auto getDeviceTypes() const
        -> std::vector<DeviceTypeInfo> = 0;

    /**
     * @brief Register device types with the type registry
     * @param registry Device type registry
     * @return Success or error
     */
    virtual auto registerDeviceTypes(DeviceTypeRegistry& registry)
        -> DeviceResult<size_t> = 0;

    /**
     * @brief Unregister device types from the type registry
     * @param registry Device type registry
     * @return Number of types unregistered
     */
    virtual auto unregisterDeviceTypes(DeviceTypeRegistry& registry)
        -> size_t = 0;

    // ==================== Device Factory Registration ====================

    /**
     * @brief Register device creators with the factory
     * @param factory Device factory
     */
    virtual void registerDeviceCreators(DeviceFactory& factory) = 0;

    /**
     * @brief Unregister device creators from the factory
     * @param factory Device factory
     */
    virtual void unregisterDeviceCreators(DeviceFactory& factory) = 0;

    // ==================== Backend Management ====================

    /**
     * @brief Check if plugin provides a device backend
     * @return true if plugin provides a backend
     */
    [[nodiscard]] virtual auto hasBackend() const -> bool = 0;

    /**
     * @brief Create backend instance
     * @return Backend instance or nullptr
     */
    [[nodiscard]] virtual auto createBackend()
        -> std::shared_ptr<DeviceBackend> = 0;

    /**
     * @brief Get the current backend instance (if running)
     */
    [[nodiscard]] virtual auto getBackend() const
        -> std::shared_ptr<DeviceBackend> = 0;

    /**
     * @brief Start the backend
     * @param config Backend configuration
     * @return Success or error
     */
    virtual auto startBackend(const nlohmann::json& config)
        -> DeviceResult<bool> = 0;

    /**
     * @brief Stop the backend
     * @return Success or error
     */
    virtual auto stopBackend() -> DeviceResult<bool> = 0;

    /**
     * @brief Check if backend is running
     */
    [[nodiscard]] virtual auto isBackendRunning() const -> bool = 0;

    // ==================== Hot-Plug Support ====================

    /**
     * @brief Check if plugin supports hot-plug reload
     */
    [[nodiscard]] virtual auto supportsHotPlug() const -> bool = 0;

    /**
     * @brief Prepare for hot-plug (save device states)
     * @return Migration contexts for active devices
     */
    virtual auto prepareHotPlug()
        -> DeviceResult<std::vector<DeviceMigrationContext>> = 0;

    /**
     * @brief Complete hot-plug (restore device states)
     * @param contexts Migration contexts from prepareHotPlug
     * @return Success or error
     */
    virtual auto completeHotPlug(
        const std::vector<DeviceMigrationContext>& contexts)
        -> DeviceResult<bool> = 0;

    /**
     * @brief Abort hot-plug operation
     * @param contexts Migration contexts
     */
    virtual void abortHotPlug(
        const std::vector<DeviceMigrationContext>& contexts) = 0;

    // ==================== Device Metadata ====================

    /**
     * @brief Get device plugin specific metadata
     */
    [[nodiscard]] virtual auto getDeviceMetadata() const
        -> DevicePluginMetadata = 0;

    /**
     * @brief Get current device plugin state
     */
    [[nodiscard]] virtual auto getDevicePluginState() const
        -> DevicePluginState = 0;

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to plugin events
     * @param callback Event callback
     * @return Subscription ID
     */
    virtual auto subscribeEvents(DevicePluginEventCallback callback)
        -> uint64_t = 0;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    virtual void unsubscribeEvents(uint64_t subscriptionId) = 0;

    // ==================== Device Operations ====================

    /**
     * @brief Get list of discovered devices from backend
     * @return Vector of discovered device info
     */
    [[nodiscard]] virtual auto getDiscoveredDevices() const
        -> std::vector<DiscoveredDevice> = 0;

    /**
     * @brief Refresh device discovery
     * @return Updated list of discovered devices
     */
    virtual auto refreshDiscovery()
        -> DeviceResult<std::vector<DiscoveredDevice>> = 0;

    /**
     * @brief Create a device instance
     * @param deviceId Device ID from discovery
     * @return Created device or error
     */
    virtual auto createDevice(const std::string& deviceId)
        -> DeviceResult<std::shared_ptr<AtomDriver>> = 0;
};

/**
 * @brief Base implementation of IDevicePlugin with common functionality
 *
 * Provides default implementations for common device plugin operations.
 * Derived classes should override device-specific methods.
 */
class DevicePluginBase : public IDevicePlugin {
public:
    explicit DevicePluginBase(DevicePluginMetadata metadata);
    ~DevicePluginBase() override = default;

    // ==================== IPlugin Interface ====================

    [[nodiscard]] auto getMetadata() const
        -> const server::plugin::PluginMetadata& override;

    auto initialize(const nlohmann::json& config) -> bool override;
    void shutdown() override;

    [[nodiscard]] auto getState() const -> server::plugin::PluginState override;
    [[nodiscard]] auto getLastError() const -> std::string override;
    [[nodiscard]] auto isHealthy() const -> bool override;

    auto pause() -> bool override;
    auto resume() -> bool override;

    [[nodiscard]] auto getStatistics() const
        -> server::plugin::PluginStatistics override;

    auto updateConfig(const nlohmann::json& config) -> bool override;
    [[nodiscard]] auto getConfig() const -> nlohmann::json override;

    // ==================== IDevicePlugin Interface ====================

    [[nodiscard]] auto getDeviceMetadata() const
        -> DevicePluginMetadata override;

    [[nodiscard]] auto getDevicePluginState() const
        -> DevicePluginState override;

    auto subscribeEvents(DevicePluginEventCallback callback)
        -> uint64_t override;
    void unsubscribeEvents(uint64_t subscriptionId) override;

    // Default implementations (can be overridden)
    [[nodiscard]] auto supportsHotPlug() const -> bool override;

    auto prepareHotPlug()
        -> DeviceResult<std::vector<DeviceMigrationContext>> override;

    auto completeHotPlug(const std::vector<DeviceMigrationContext>& contexts)
        -> DeviceResult<bool> override;

    void abortHotPlug(
        const std::vector<DeviceMigrationContext>& contexts) override;

protected:
    /**
     * @brief Set the plugin state
     */
    void setState(DevicePluginState state);

    /**
     * @brief Set the last error message
     */
    void setLastError(const std::string& error);

    /**
     * @brief Emit a plugin event
     */
    void emitEvent(const DevicePluginEvent& event);

    /**
     * @brief Create a plugin event
     */
    [[nodiscard]] auto createEvent(DevicePluginEventType type,
                                    const std::string& message = {}) const
        -> DevicePluginEvent;

    // Protected members
    DevicePluginMetadata metadata_;
    DevicePluginState state_{DevicePluginState::Unloaded};
    server::plugin::PluginState pluginState_{
        server::plugin::PluginState::Unloaded};
    std::string lastError_;
    nlohmann::json config_;
    server::plugin::PluginStatistics statistics_;

    // Event subscribers
    mutable std::shared_mutex eventMutex_;
    std::unordered_map<uint64_t, DevicePluginEventCallback> eventSubscribers_;
    uint64_t nextSubscriberId_{1};

    // Backend
    std::shared_ptr<DeviceBackend> backend_;
    mutable std::shared_mutex backendMutex_;
};

// ============================================================================
// Plugin Entry Points
// ============================================================================

/**
 * @brief Device plugin factory function type
 */
using DevicePluginFactory = std::function<std::shared_ptr<IDevicePlugin>()>;

/**
 * @brief Device plugin entry point function signature
 *
 * Dynamic libraries must export this function to be loadable as device plugins.
 * The function name must be "createDevicePlugin".
 */
extern "C" using CreateDevicePluginFunc = IDevicePlugin* (*)();

/**
 * @brief Device plugin destruction function signature
 *
 * Optional function for custom cleanup. Function name: "destroyDevicePlugin"
 */
extern "C" using DestroyDevicePluginFunc = void (*)(IDevicePlugin*);

/**
 * @brief Get device plugin API version function signature
 *
 * Returns the API version the plugin was built against.
 * Function name: "getDevicePluginApiVersion"
 */
extern "C" using GetDevicePluginApiVersionFunc = int (*)();

/// Current device plugin API version
constexpr int DEVICE_PLUGIN_API_VERSION = 1;

// ============================================================================
// Plugin Capability Constants
// ============================================================================

namespace device_capabilities {
    constexpr const char* HOT_PLUG = "device_hot_plug";
    constexpr const char* AUTO_DISCOVERY = "device_auto_discovery";
    constexpr const char* BACKEND = "device_backend";
    constexpr const char* ASYNC_OPERATIONS = "device_async";
    constexpr const char* EVENT_NOTIFICATIONS = "device_events";
    constexpr const char* PROPERTY_CONTROL = "device_properties";
    constexpr const char* BATCH_OPERATIONS = "device_batch";
    constexpr const char* STATE_MIGRATION = "device_migration";
}  // namespace device_capabilities

// ============================================================================
// Plugin Tag Constants
// ============================================================================

namespace device_tags {
    constexpr const char* DEVICE_PLUGIN = "device";
    constexpr const char* INDI = "indi";
    constexpr const char* ASCOM = "ascom";
    constexpr const char* SIMULATOR = "simulator";
    constexpr const char* NATIVE = "native";
    constexpr const char* CAMERA = "camera";
    constexpr const char* TELESCOPE = "telescope";
    constexpr const char* FOCUSER = "focuser";
    constexpr const char* FILTERWHEEL = "filterwheel";
    constexpr const char* DOME = "dome";
    constexpr const char* GUIDER = "guider";
    constexpr const char* ROTATOR = "rotator";
    constexpr const char* WEATHER = "weather";
}  // namespace device_tags

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_PLUGIN_INTERFACE_HPP
