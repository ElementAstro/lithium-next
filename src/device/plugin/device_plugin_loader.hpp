/*
 * device_plugin_loader.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device plugin loader with hot-plug support

**************************************************/

#ifndef LITHIUM_DEVICE_PLUGIN_LOADER_HPP
#define LITHIUM_DEVICE_PLUGIN_LOADER_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"
#include "device_plugin_interface.hpp"

namespace lithium::device {

// Forward declarations
class DeviceTypeRegistry;
class DeviceFactory;

/**
 * @brief Plugin load event types
 */
enum class PluginLoadEventType {
    Loading,       ///< Plugin loading started
    Loaded,        ///< Plugin loaded successfully
    LoadFailed,    ///< Plugin load failed
    Unloading,     ///< Plugin unloading started
    Unloaded,      ///< Plugin unloaded successfully
    UnloadFailed,  ///< Plugin unload failed
    Reloading,     ///< Plugin reloading (hot-plug)
    Reloaded,      ///< Plugin reloaded successfully
    ReloadFailed   ///< Plugin reload failed
};

/**
 * @brief Plugin load event
 */
struct PluginLoadEvent {
    PluginLoadEventType type;
    std::string pluginName;
    std::filesystem::path pluginPath;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json data;

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Plugin load event callback
 */
using PluginLoadEventCallback = std::function<void(const PluginLoadEvent&)>;

/**
 * @brief Loaded plugin info
 */
struct LoadedPluginInfo {
    std::string name;
    std::filesystem::path path;
    std::shared_ptr<IDevicePlugin> plugin;
    void* handle{nullptr};                    ///< Dynamic library handle
    std::chrono::system_clock::time_point loadedAt;
    size_t reloadCount{0};
    bool isBuiltIn{false};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Plugin discovery result
 */
struct PluginDiscoveryResult {
    std::filesystem::path path;
    std::string name;
    std::string version;
    bool isDevicePlugin{false};
    nlohmann::json metadata;
    std::string error;                        ///< Error if discovery failed
};

/**
 * @brief Device plugin loader
 *
 * Manages loading, unloading, and hot-reloading of device plugins.
 * Supports:
 * - Dynamic library loading (.dll/.so/.dylib)
 * - Plugin discovery from directories
 * - Hot-plug with device state migration
 * - Built-in plugin registration
 */
class DevicePluginLoader {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> DevicePluginLoader&;

    // Disable copy
    DevicePluginLoader(const DevicePluginLoader&) = delete;
    DevicePluginLoader& operator=(const DevicePluginLoader&) = delete;

    // ==================== Initialization ====================

    /**
     * @brief Initialize the loader
     * @param config Loader configuration
     */
    void initialize(const nlohmann::json& config = {});

    /**
     * @brief Shutdown the loader and unload all plugins
     */
    void shutdown();

    /**
     * @brief Set the plugin search paths
     * @param paths List of directory paths to search for plugins
     */
    void setPluginPaths(const std::vector<std::filesystem::path>& paths);

    /**
     * @brief Add a plugin search path
     * @param path Directory path to add
     */
    void addPluginPath(const std::filesystem::path& path);

    // ==================== Plugin Discovery ====================

    /**
     * @brief Discover plugins in configured paths
     * @return List of discovered plugin info
     */
    [[nodiscard]] auto discoverPlugins() const
        -> std::vector<PluginDiscoveryResult>;

    /**
     * @brief Discover plugins in a specific directory
     * @param directory Directory to search
     * @return List of discovered plugin info
     */
    [[nodiscard]] auto discoverPluginsIn(
        const std::filesystem::path& directory) const
        -> std::vector<PluginDiscoveryResult>;

    /**
     * @brief Check if a file is a valid plugin
     * @param path File path to check
     * @return Discovery result with plugin info
     */
    [[nodiscard]] auto probePlugin(const std::filesystem::path& path) const
        -> PluginDiscoveryResult;

    // ==================== Plugin Loading ====================

    /**
     * @brief Load a device plugin from a file
     * @param path Plugin file path
     * @param config Plugin configuration
     * @return Success or error
     */
    auto loadPlugin(const std::filesystem::path& path,
                    const nlohmann::json& config = {}) -> DeviceResult<bool>;

    /**
     * @brief Load a device plugin by name (search in configured paths)
     * @param name Plugin name
     * @param config Plugin configuration
     * @return Success or error
     */
    auto loadPluginByName(const std::string& name,
                          const nlohmann::json& config = {})
        -> DeviceResult<bool>;

    /**
     * @brief Load all discovered plugins
     * @param config Configuration map (plugin name -> config)
     * @return Number of plugins loaded successfully
     */
    auto loadAllPlugins(
        const std::unordered_map<std::string, nlohmann::json>& configs = {})
        -> size_t;

    /**
     * @brief Register a built-in plugin
     * @param plugin Plugin instance
     * @param config Plugin configuration
     * @return Success or error
     */
    auto registerBuiltInPlugin(std::shared_ptr<IDevicePlugin> plugin,
                               const nlohmann::json& config = {})
        -> DeviceResult<bool>;

    // ==================== Plugin Unloading ====================

    /**
     * @brief Unload a plugin by name
     * @param name Plugin name
     * @return Success or error
     */
    auto unloadPlugin(const std::string& name) -> DeviceResult<bool>;

    /**
     * @brief Unload all plugins
     * @return Number of plugins unloaded
     */
    auto unloadAllPlugins() -> size_t;

    // ==================== Hot-Plug Support ====================

    /**
     * @brief Reload a plugin (hot-plug)
     * @param name Plugin name
     * @param config New configuration (optional)
     * @return Success or error
     */
    auto reloadPlugin(const std::string& name,
                      const nlohmann::json& config = {}) -> DeviceResult<bool>;

    /**
     * @brief Check if hot-plug is in progress
     */
    [[nodiscard]] auto isHotPlugInProgress() const -> bool;

    /**
     * @brief Get current hot-plug status
     */
    [[nodiscard]] auto getHotPlugStatus() const -> nlohmann::json;

    // ==================== Plugin Query ====================

    /**
     * @brief Check if a plugin is loaded
     * @param name Plugin name
     */
    [[nodiscard]] auto isPluginLoaded(const std::string& name) const -> bool;

    /**
     * @brief Get a loaded plugin by name
     * @param name Plugin name
     * @return Plugin instance or nullptr
     */
    [[nodiscard]] auto getPlugin(const std::string& name) const
        -> std::shared_ptr<IDevicePlugin>;

    /**
     * @brief Get all loaded plugins
     * @return Map of plugin name to plugin info
     */
    [[nodiscard]] auto getLoadedPlugins() const
        -> std::unordered_map<std::string, LoadedPluginInfo>;

    /**
     * @brief Get loaded plugin names
     */
    [[nodiscard]] auto getPluginNames() const -> std::vector<std::string>;

    /**
     * @brief Get plugins by tag
     * @param tag Tag to filter by
     */
    [[nodiscard]] auto getPluginsByTag(const std::string& tag) const
        -> std::vector<std::shared_ptr<IDevicePlugin>>;

    /**
     * @brief Get plugins by capability
     * @param capability Capability to filter by
     */
    [[nodiscard]] auto getPluginsByCapability(
        const std::string& capability) const
        -> std::vector<std::shared_ptr<IDevicePlugin>>;

    // ==================== Registry Integration ====================

    /**
     * @brief Set the device type registry
     * @param registry Type registry instance
     */
    void setTypeRegistry(DeviceTypeRegistry* registry);

    /**
     * @brief Set the device factory
     * @param factory Device factory instance
     */
    void setDeviceFactory(DeviceFactory* factory);

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to plugin load events
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribe(PluginLoadEventCallback callback) -> uint64_t;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    void unsubscribe(uint64_t subscriptionId);

    // ==================== Statistics ====================

    /**
     * @brief Get loader statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

private:
    DevicePluginLoader();
    ~DevicePluginLoader();

    /**
     * @brief Load a dynamic library
     */
    auto loadLibrary(const std::filesystem::path& path)
        -> DeviceResult<void*>;

    /**
     * @brief Unload a dynamic library
     */
    void unloadLibrary(void* handle);

    /**
     * @brief Get a function from a loaded library
     */
    template <typename Func>
    auto getFunction(void* handle, const char* name) -> Func;

    /**
     * @brief Register plugin with type registry and factory
     */
    auto registerPluginTypes(IDevicePlugin* plugin) -> DeviceResult<bool>;

    /**
     * @brief Unregister plugin from type registry and factory
     */
    void unregisterPluginTypes(IDevicePlugin* plugin);

    /**
     * @brief Emit a load event
     */
    void emitEvent(const PluginLoadEvent& event);

    /**
     * @brief Create a load event
     */
    [[nodiscard]] auto createEvent(PluginLoadEventType type,
                                    const std::string& pluginName,
                                    const std::filesystem::path& path,
                                    const std::string& message = {}) const
        -> PluginLoadEvent;

    /**
     * @brief Get platform-specific plugin file extension
     */
    [[nodiscard]] static auto getPluginExtension() -> std::string;

    /**
     * @brief Check if a file has a valid plugin extension
     */
    [[nodiscard]] static auto hasValidExtension(
        const std::filesystem::path& path) -> bool;

    // Members
    mutable std::shared_mutex mutex_;
    std::vector<std::filesystem::path> pluginPaths_;
    std::unordered_map<std::string, LoadedPluginInfo> loadedPlugins_;

    // Registry integration
    DeviceTypeRegistry* typeRegistry_{nullptr};
    DeviceFactory* deviceFactory_{nullptr};

    // Event subscribers
    mutable std::shared_mutex eventMutex_;
    std::unordered_map<uint64_t, PluginLoadEventCallback> eventSubscribers_;
    uint64_t nextSubscriberId_{1};

    // Hot-plug state
    std::atomic<bool> hotPlugInProgress_{false};
    std::string hotPlugPluginName_;
    std::vector<DeviceMigrationContext> hotPlugMigrationContexts_;

    // Statistics
    size_t totalLoads_{0};
    size_t totalUnloads_{0};
    size_t totalReloads_{0};
    size_t loadFailures_{0};

    // Configuration
    nlohmann::json config_;
    bool initialized_{false};
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_PLUGIN_LOADER_HPP
