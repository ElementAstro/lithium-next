/*
 * plugin_manager.hpp - Server Plugin Manager
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_PLUGIN_MANAGER_HPP
#define LITHIUM_SERVER_PLUGIN_MANAGER_HPP

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "plugin_interface.hpp"
#include "plugin_loader.hpp"
#include "atom/type/json.hpp"

// Forward declarations
namespace lithium::server {
class ServerApp;
}

namespace lithium::app {
class CommandDispatcher;
}

namespace lithium::server::plugin {

/**
 * @brief Plugin event types
 */
enum class PluginEvent {
    Loaded,
    Unloaded,
    Reloaded,
    Initialized,
    Shutdown,
    Error,
    StateChanged,
    Enabled,
    Disabled,
    Paused,
    Resumed,
    ConfigUpdated,
    ActionExecuted
};

/**
 * @brief Plugin event callback type
 */
using PluginEventCallback = std::function<void(
    PluginEvent event, const std::string& pluginName, const json& data)>;

/**
 * @brief Plugin manager configuration
 */
struct PluginManagerConfig {
    PluginLoaderConfig loaderConfig;
    bool autoRegisterOnLoad{true};
    bool enableEventNotifications{true};
    std::filesystem::path configFile{"config/plugins.json"};
    bool enablePerformanceMonitoring{false};
    size_t maxConcurrentLoads{4};
    std::chrono::milliseconds healthCheckInterval{30000};
};

/**
 * @brief Central manager for server plugins
 *
 * Manages the complete lifecycle of command and controller plugins:
 * - Loading/unloading plugins
 * - Registering commands and routes
 * - Configuration management
 * - Event notifications
 * - Health monitoring
 */
class PluginManager {
public:
    /**
     * @brief Construct plugin manager
     */
    explicit PluginManager(const PluginManagerConfig& config = {});

    /**
     * @brief Destructor - shuts down all plugins
     */
    ~PluginManager();

    // Non-copyable, non-movable
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    /**
     * @brief Create shared instance
     */
    static auto createShared(const PluginManagerConfig& config = {})
        -> std::shared_ptr<PluginManager>;

    /**
     * @brief Initialize the plugin manager
     * @param app Server application for controller registration
     * @param dispatcher Command dispatcher for command registration
     * @return true if initialization successful
     */
    auto initialize(ServerApp& app,
                    std::shared_ptr<app::CommandDispatcher> dispatcher) -> bool;

    /**
     * @brief Shutdown the plugin manager
     */
    void shutdown();

    /**
     * @brief Load a plugin by name
     * @param name Plugin name
     * @param config Optional configuration
     * @return Success or error
     */
    auto loadPlugin(std::string_view name, const json& config = {})
        -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Load a plugin from file path
     * @param path Plugin file path
     * @param config Optional configuration
     * @return Success or error
     */
    auto loadPluginFromPath(const std::filesystem::path& path,
                            const json& config = {})
        -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Unload a plugin
     * @param name Plugin name
     * @return Success or error
     */
    auto unloadPlugin(std::string_view name) -> PluginResult<bool>;

    /**
     * @brief Reload a plugin
     * @param name Plugin name
     * @return Success or error
     */
    auto reloadPlugin(std::string_view name) -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Enable a plugin (register its commands/routes)
     * @param name Plugin name
     * @return true if successful
     */
    auto enablePlugin(std::string_view name) -> bool;

    /**
     * @brief Disable a plugin (unregister its commands/routes)
     * @param name Plugin name
     * @return true if successful
     */
    auto disablePlugin(std::string_view name) -> bool;

    /**
     * @brief Check if a plugin is loaded
     */
    [[nodiscard]] auto isPluginLoaded(std::string_view name) const -> bool;

    /**
     * @brief Check if a plugin is enabled
     */
    [[nodiscard]] auto isPluginEnabled(std::string_view name) const -> bool;

    /**
     * @brief Get plugin information
     */
    [[nodiscard]] auto getPluginInfo(std::string_view name) const
        -> std::optional<LoadedPluginInfo>;

    /**
     * @brief Get all loaded plugins
     */
    [[nodiscard]] auto getAllPlugins() const -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Get plugins by type
     */
    [[nodiscard]] auto getPluginsByType(LoadedPluginInfo::Type type) const
        -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Discover and load all plugins from configured directories
     * @return Number of successfully loaded plugins
     */
    auto discoverAndLoadAll() -> size_t;

    /**
     * @brief Get list of discovered but not loaded plugins
     */
    [[nodiscard]] auto getAvailablePlugins() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Subscribe to plugin events
     * @param callback Event callback
     * @return Subscription ID for unsubscribing
     */
    auto subscribeToEvents(PluginEventCallback callback) -> int;

    /**
     * @brief Unsubscribe from plugin events
     * @param subscriptionId Subscription ID
     */
    void unsubscribeFromEvents(int subscriptionId);

    /**
     * @brief Get plugin health status
     */
    [[nodiscard]] auto getPluginHealth(std::string_view name) const -> json;

    /**
     * @brief Get overall plugin system status
     */
    [[nodiscard]] auto getSystemStatus() const -> json;

    /**
     * @brief Load plugin configuration from file
     * @return true if successful
     */
    auto loadConfiguration() -> bool;

    /**
     * @brief Save plugin configuration to file
     * @return true if successful
     */
    auto saveConfiguration() const -> bool;

    /**
     * @brief Update plugin configuration
     * @param name Plugin name
     * @param config New configuration
     */
    void updatePluginConfig(std::string_view name, const json& config);

    /**
     * @brief Get plugin configuration
     */
    [[nodiscard]] auto getPluginConfig(std::string_view name) const
        -> std::optional<json>;

    /**
     * @brief Get the underlying plugin loader
     */
    [[nodiscard]] auto getLoader() const -> std::shared_ptr<PluginLoader>;

    // ========================================================================
    // Extended API - Batch Operations
    // ========================================================================

    /**
     * @brief Load multiple plugins in batch
     * @param names Plugin names to load
     * @return Number of successfully loaded plugins
     */
    auto batchLoad(const std::vector<std::string>& names) -> size_t;

    /**
     * @brief Unload multiple plugins in batch
     * @param names Plugin names to unload
     * @return Number of successfully unloaded plugins
     */
    auto batchUnload(const std::vector<std::string>& names) -> size_t;

    /**
     * @brief Enable multiple plugins in batch
     * @param names Plugin names to enable
     * @return Number of successfully enabled plugins
     */
    auto batchEnable(const std::vector<std::string>& names) -> size_t;

    /**
     * @brief Disable multiple plugins in batch
     * @param names Plugin names to disable
     * @return Number of successfully disabled plugins
     */
    auto batchDisable(const std::vector<std::string>& names) -> size_t;

    // ========================================================================
    // Extended API - Group Management
    // ========================================================================

    /**
     * @brief Create a plugin group
     * @param group Group definition
     */
    void createGroup(const PluginGroup& group);

    /**
     * @brief Delete a plugin group
     * @param name Group name
     */
    void deleteGroup(const std::string& name);

    /**
     * @brief Get a plugin group
     * @param name Group name
     * @return Group or nullopt if not found
     */
    [[nodiscard]] auto getGroup(const std::string& name) const
        -> std::optional<PluginGroup>;

    /**
     * @brief Get all plugin groups
     */
    [[nodiscard]] auto getAllGroups() const -> std::vector<PluginGroup>;

    /**
     * @brief Add a plugin to a group
     * @param pluginName Plugin name
     * @param groupName Group name
     */
    void addToGroup(const std::string& pluginName, const std::string& groupName);

    /**
     * @brief Remove a plugin from a group
     * @param pluginName Plugin name
     * @param groupName Group name
     */
    void removeFromGroup(const std::string& pluginName,
                         const std::string& groupName);

    /**
     * @brief Enable all plugins in a group
     * @param groupName Group name
     * @return Number of enabled plugins
     */
    auto enableGroup(const std::string& groupName) -> size_t;

    /**
     * @brief Disable all plugins in a group
     * @param groupName Group name
     * @return Number of disabled plugins
     */
    auto disableGroup(const std::string& groupName) -> size_t;

    // ========================================================================
    // Extended API - Plugin Execution
    // ========================================================================

    /**
     * @brief Execute an action on a plugin
     * @param pluginName Plugin name
     * @param action Action name
     * @param params Action parameters
     * @return Action result
     */
    auto executeAction(std::string_view pluginName, const std::string& action,
                       const json& params) -> json;

    /**
     * @brief Execute a command directly on a command plugin
     * @param pluginName Plugin name
     * @param commandId Command ID
     * @param params Command parameters
     * @return Command result
     */
    auto executeCommand(std::string_view pluginName, const std::string& commandId,
                        const json& params) -> json;

    /**
     * @brief Get supported actions for a plugin
     * @param pluginName Plugin name
     * @return List of supported actions
     */
    [[nodiscard]] auto getPluginActions(std::string_view pluginName) const
        -> std::vector<std::string>;

    // ========================================================================
    // Extended API - Plugin Queries
    // ========================================================================

    /**
     * @brief Get plugins by capability
     * @param capability Capability to filter by
     * @return List of matching plugins
     */
    [[nodiscard]] auto getPluginsByCapability(const std::string& capability) const
        -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Get plugins by tag
     * @param tag Tag to filter by
     * @return List of matching plugins
     */
    [[nodiscard]] auto getPluginsByTag(const std::string& tag) const
        -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Get plugins by group
     * @param groupName Group name
     * @return List of plugins in the group
     */
    [[nodiscard]] auto getPluginsByGroup(const std::string& groupName) const
        -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Search plugins by name pattern
     * @param pattern Search pattern (supports wildcards)
     * @return List of matching plugins
     */
    [[nodiscard]] auto searchPlugins(const std::string& pattern) const
        -> std::vector<LoadedPluginInfo>;

    // ========================================================================
    // Extended API - Plugin State Control
    // ========================================================================

    /**
     * @brief Pause a plugin
     * @param name Plugin name
     * @return true if paused successfully
     */
    auto pausePlugin(std::string_view name) -> bool;

    /**
     * @brief Resume a plugin
     * @param name Plugin name
     * @return true if resumed successfully
     */
    auto resumePlugin(std::string_view name) -> bool;

    /**
     * @brief Restart a plugin (disable, reload, enable)
     * @param name Plugin name
     * @return true if restarted successfully
     */
    auto restartPlugin(std::string_view name) -> bool;

    // ========================================================================
    // Extended API - Schema and Documentation
    // ========================================================================

    /**
     * @brief Get command schema from a plugin
     * @param pluginName Plugin name
     * @param commandId Command ID
     * @return Command schema
     */
    [[nodiscard]] auto getCommandSchema(std::string_view pluginName,
                                        const std::string& commandId) const
        -> json;

    /**
     * @brief Get all command schemas from a plugin
     * @param pluginName Plugin name
     * @return Map of command ID to schema
     */
    [[nodiscard]] auto getAllCommandSchemas(std::string_view pluginName) const
        -> json;

    /**
     * @brief Get route information from a plugin
     * @param pluginName Plugin name
     * @return Route information
     */
    [[nodiscard]] auto getRouteInfo(std::string_view pluginName) const
        -> std::vector<RouteInfo>;

    /**
     * @brief Get OpenAPI spec from a plugin
     * @param pluginName Plugin name
     * @return OpenAPI specification
     */
    [[nodiscard]] auto getOpenApiSpec(std::string_view pluginName) const -> json;

    /**
     * @brief Get combined OpenAPI spec from all controller plugins
     * @return Combined OpenAPI specification
     */
    [[nodiscard]] auto getCombinedOpenApiSpec() const -> json;

    // ========================================================================
    // Extended API - Statistics and Monitoring
    // ========================================================================

    /**
     * @brief Get plugin statistics
     * @param name Plugin name
     * @return Plugin statistics
     */
    [[nodiscard]] auto getPluginStatistics(std::string_view name) const
        -> std::optional<PluginStatistics>;

    /**
     * @brief Get all plugin statistics
     * @return Map of plugin name to statistics
     */
    [[nodiscard]] auto getAllStatistics() const -> json;

    /**
     * @brief Reset plugin statistics
     * @param name Plugin name (empty for all)
     */
    void resetStatistics(std::string_view name = "");

    /**
     * @brief Enable performance monitoring
     * @param enable Enable or disable
     */
    void enablePerformanceMonitoring(bool enable);

    // ========================================================================
    // Extended API - Dependency Management
    // ========================================================================

    /**
     * @brief Get plugin dependencies
     * @param name Plugin name
     * @return List of dependencies
     */
    [[nodiscard]] auto getPluginDependencies(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Get plugins that depend on a given plugin
     * @param name Plugin name
     * @return List of dependent plugins
     */
    [[nodiscard]] auto getDependentPlugins(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Check if plugin has conflicts
     * @param name Plugin name
     * @return true if there are conflicts
     */
    [[nodiscard]] auto hasConflicts(std::string_view name) const -> bool;

    /**
     * @brief Get conflicting plugins
     * @param name Plugin name
     * @return List of conflicting plugin names
     */
    [[nodiscard]] auto getConflictingPlugins(std::string_view name) const
        -> std::vector<std::string>;

private:
    PluginManagerConfig config_;
    std::shared_ptr<PluginLoader> loader_;
    ServerApp* app_{nullptr};
    std::shared_ptr<app::CommandDispatcher> dispatcher_;

    std::unordered_map<std::string, bool> enabledPlugins_;
    std::unordered_map<int, PluginEventCallback> eventSubscribers_;
    int nextSubscriberId_{0};

    mutable std::shared_mutex mutex_;
    bool initialized_{false};

    /**
     * @brief Register plugin commands
     */
    void registerPluginCommands(const std::string& name,
                                 std::shared_ptr<ICommandPlugin> plugin);

    /**
     * @brief Unregister plugin commands
     */
    void unregisterPluginCommands(const std::string& name,
                                   std::shared_ptr<ICommandPlugin> plugin);

    /**
     * @brief Register plugin routes
     */
    void registerPluginRoutes(const std::string& name,
                               std::shared_ptr<IControllerPlugin> plugin);

    /**
     * @brief Notify event subscribers
     */
    void notifyEvent(PluginEvent event, const std::string& pluginName,
                     const json& data = {});

    /**
     * @brief Convert string_view to string
     */
    [[nodiscard]] static auto toStdString(std::string_view sv) -> std::string {
        return std::string(sv);
    }

    // Plugin groups
    std::unordered_map<std::string, PluginGroup> groups_;

    // Performance monitoring
    bool performanceMonitoringEnabled_{false};
};

}  // namespace lithium::server::plugin

#endif  // LITHIUM_SERVER_PLUGIN_MANAGER_HPP
