/*
 * plugin_loader.hpp - Dynamic Plugin Loader for Server Extensions
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_PLUGIN_LOADER_HPP
#define LITHIUM_SERVER_PLUGIN_LOADER_HPP

#include <expected>
#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "components/core/loader.hpp"
#include "plugin_interface.hpp"

namespace lithium::server::plugin {

/**
 * @brief Plugin loading error types
 */
enum class PluginLoadError {
    FileNotFound,
    InvalidPlugin,
    ApiVersionMismatch,
    DependencyMissing,
    InitializationFailed,
    AlreadyLoaded,
    LoadFailed,
    SymbolNotFound
};

/**
 * @brief Convert error to string
 */
[[nodiscard]] auto pluginLoadErrorToString(PluginLoadError error)
    -> std::string;

/**
 * @brief Result type for plugin operations
 */
template <typename T>
using PluginResult = std::expected<T, PluginLoadError>;

/**
 * @brief Loaded plugin information
 */
struct LoadedPluginInfo {
    std::string name;                                ///< Plugin name
    std::string path;                                ///< File path
    std::shared_ptr<IPlugin> instance;               ///< Plugin instance
    PluginState state;                               ///< Current state
    std::chrono::system_clock::time_point loadTime;  ///< Load timestamp
    json config;                                     ///< Plugin configuration
    std::string group;                               ///< Plugin group
    PluginStatistics statistics;                     ///< Runtime statistics

    enum class Type {
        Command,
        Controller,
        Full,
        Unknown
    } type;  ///< Plugin type

    /**
     * @brief Get as command plugin (may return nullptr)
     */
    [[nodiscard]] auto asCommandPlugin() const
        -> std::shared_ptr<ICommandPlugin>;

    /**
     * @brief Get as controller plugin (may return nullptr)
     */
    [[nodiscard]] auto asControllerPlugin() const
        -> std::shared_ptr<IControllerPlugin>;

    /**
     * @brief Get as full plugin (may return nullptr)
     */
    [[nodiscard]] auto asFullPlugin() const -> std::shared_ptr<IFullPlugin>;

    /**
     * @brief Convert to JSON
     */
    [[nodiscard]] auto toJson() const -> json;
};

/**
 * @brief Plugin loader configuration
 */
struct PluginLoaderConfig {
    std::filesystem::path pluginDirectory{"plugins/server"};
    std::vector<std::string> searchPaths;
    bool autoLoadOnStartup{true};
    bool enableHotReload{true};
    int apiVersion{PLUGIN_API_VERSION};
    size_t threadPoolSize{4};
};

/**
 * @brief Dynamic plugin loader for server extensions
 *
 * Wraps the component ModuleLoader to provide plugin-specific functionality
 * for loading command and controller plugins at runtime.
 */
class PluginLoader {
public:
    /**
     * @brief Construct plugin loader with configuration
     */
    explicit PluginLoader(const PluginLoaderConfig& config = {});

    /**
     * @brief Destructor - unloads all plugins
     */
    ~PluginLoader();

    // Non-copyable, non-movable
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    PluginLoader(PluginLoader&&) = delete;
    PluginLoader& operator=(PluginLoader&&) = delete;

    /**
     * @brief Create shared instance
     */
    static auto createShared(const PluginLoaderConfig& config = {})
        -> std::shared_ptr<PluginLoader>;

    /**
     * @brief Load a plugin from file path
     * @param path Path to the plugin library
     * @param config Optional plugin configuration
     * @return Loaded plugin info or error
     */
    auto loadPlugin(const std::filesystem::path& path, const json& config = {})
        -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Load a plugin by name (searches in plugin directories)
     * @param name Plugin name
     * @param config Optional plugin configuration
     * @return Loaded plugin info or error
     */
    auto loadPluginByName(std::string_view name, const json& config = {})
        -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Unload a plugin by name
     * @param name Plugin name
     * @return Success or error
     */
    auto unloadPlugin(std::string_view name) -> PluginResult<bool>;

    /**
     * @brief Reload a plugin (unload then load)
     * @param name Plugin name
     * @return Reloaded plugin info or error
     */
    auto reloadPlugin(std::string_view name) -> PluginResult<LoadedPluginInfo>;

    /**
     * @brief Check if a plugin is loaded
     */
    [[nodiscard]] auto isPluginLoaded(std::string_view name) const -> bool;

    /**
     * @brief Get loaded plugin info
     */
    [[nodiscard]] auto getPlugin(std::string_view name) const
        -> std::optional<LoadedPluginInfo>;

    /**
     * @brief Get all loaded plugins
     */
    [[nodiscard]] auto getAllPlugins() const -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Get all command plugins
     */
    [[nodiscard]] auto getCommandPlugins() const
        -> std::vector<std::shared_ptr<ICommandPlugin>>;

    /**
     * @brief Get all controller plugins
     */
    [[nodiscard]] auto getControllerPlugins() const
        -> std::vector<std::shared_ptr<IControllerPlugin>>;

    /**
     * @brief Discover plugins in configured directories
     * @return List of discovered plugin paths
     */
    [[nodiscard]] auto discoverPlugins() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Load all discovered plugins
     * @return Number of successfully loaded plugins
     */
    auto loadAllDiscovered() -> size_t;

    /**
     * @brief Unload all plugins
     */
    void unloadAll();

    /**
     * @brief Validate plugin dependencies
     * @param name Plugin name
     * @return true if all dependencies are satisfied
     */
    [[nodiscard]] auto validateDependencies(std::string_view name) const
        -> bool;

    /**
     * @brief Get plugin load order based on dependencies
     */
    [[nodiscard]] auto getLoadOrder() const -> std::vector<std::string>;

    /**
     * @brief Set plugin configuration
     */
    void setPluginConfig(std::string_view name, const json& config);

    /**
     * @brief Get plugin configuration
     */
    [[nodiscard]] auto getPluginConfig(std::string_view name) const
        -> std::optional<json>;

    // ========================================================================
    // Extended API - Component-like functionality
    // ========================================================================

    /**
     * @brief Get a function from a plugin
     * @tparam T Function signature type
     * @param pluginName Plugin name
     * @param functionName Function name to retrieve
     * @return Function or error
     */
    template <typename T>
    auto getPluginFunction(std::string_view pluginName,
                           std::string_view functionName)
        -> PluginResult<std::function<T>>;

    /**
     * @brief Execute a plugin action
     * @param pluginName Plugin name
     * @param action Action name
     * @param params Action parameters
     * @return Action result
     */
    auto executePluginAction(std::string_view pluginName,
                             const std::string& action, const json& params)
        -> json;

    /**
     * @brief Execute a command directly on a command plugin
     * @param pluginName Plugin name
     * @param commandId Command ID
     * @param params Command parameters
     * @return Command result
     */
    auto executePluginCommand(std::string_view pluginName,
                              const std::string& commandId, const json& params)
        -> json;

    /**
     * @brief Get plugin statistics
     * @param name Plugin name
     * @return Plugin statistics or nullopt if not found
     */
    [[nodiscard]] auto getPluginStatistics(std::string_view name) const
        -> std::optional<PluginStatistics>;

    /**
     * @brief Get all plugins with a specific capability
     * @param capability Capability to filter by
     * @return List of plugins with the capability
     */
    [[nodiscard]] auto getPluginsByCapability(
        const std::string& capability) const -> std::vector<LoadedPluginInfo>;

    /**
     * @brief Get all plugins with a specific tag
     * @param tag Tag to filter by
     * @return List of plugins with the tag
     */
    [[nodiscard]] auto getPluginsByTag(const std::string& tag) const
        -> std::vector<LoadedPluginInfo>;

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
     * @brief Update plugin configuration at runtime
     * @param name Plugin name
     * @param config New configuration
     * @return true if updated successfully
     */
    auto updatePluginConfig(std::string_view name, const json& config) -> bool;

    /**
     * @brief Validate plugin configuration
     * @param name Plugin name
     * @param config Configuration to validate
     * @return Validation result
     */
    [[nodiscard]] auto validatePluginConfig(std::string_view name,
                                            const json& config) const
        -> std::pair<bool, std::string>;

    /**
     * @brief Get plugin's supported actions
     * @param name Plugin name
     * @return List of supported actions
     */
    [[nodiscard]] auto getPluginActions(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Get command schema from a command plugin
     * @param pluginName Plugin name
     * @param commandId Command ID
     * @return Command schema
     */
    [[nodiscard]] auto getCommandSchema(std::string_view pluginName,
                                        const std::string& commandId) const
        -> json;

    /**
     * @brief Get route information from a controller plugin
     * @param pluginName Plugin name
     * @return Route information
     */
    [[nodiscard]] auto getRouteInfo(std::string_view pluginName) const
        -> std::vector<RouteInfo>;

    /**
     * @brief Get OpenAPI spec from a controller plugin
     * @param pluginName Plugin name
     * @return OpenAPI specification
     */
    [[nodiscard]] auto getOpenApiSpec(std::string_view pluginName) const
        -> json;

    /**
     * @brief Check if plugin has a specific capability
     * @param name Plugin name
     * @param capability Capability to check
     * @return true if plugin has the capability
     */
    [[nodiscard]] auto hasCapability(std::string_view name,
                                     const std::string& capability) const
        -> bool;

    /**
     * @brief Get plugins that conflict with a given plugin
     * @param name Plugin name
     * @return List of conflicting plugin names
     */
    [[nodiscard]] auto getConflictingPlugins(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Check if loading a plugin would cause conflicts
     * @param name Plugin name to check
     * @return true if there are conflicts
     */
    [[nodiscard]] auto hasConflicts(std::string_view name) const -> bool;

    /**
     * @brief Get the underlying ModuleLoader
     */
    [[nodiscard]] auto getModuleLoader() const
        -> std::shared_ptr<lithium::ModuleLoader>;

private:
    PluginLoaderConfig config_;
    std::shared_ptr<lithium::ModuleLoader> moduleLoader_;
    std::unordered_map<std::string, LoadedPluginInfo> loadedPlugins_;
    std::unordered_map<std::string, json> pluginConfigs_;
    mutable std::shared_mutex mutex_;

    /**
     * @brief Find plugin file in search paths
     */
    [[nodiscard]] auto findPluginFile(std::string_view name) const
        -> std::optional<std::filesystem::path>;

    /**
     * @brief Determine plugin type from instance
     */
    [[nodiscard]] static auto determinePluginType(
        const std::shared_ptr<IPlugin>& plugin) -> LoadedPluginInfo::Type;

    /**
     * @brief Get platform-specific library extension
     */
    [[nodiscard]] static auto getLibraryExtension() -> std::string;

    /**
     * @brief Convert string to std::string safely
     */
    [[nodiscard]] static auto toStdString(std::string_view sv) -> std::string {
        return std::string(sv);
    }

    /**
     * @brief Update plugin statistics
     */
    void updateStatistics(const std::string& name);
};

// Template implementation
template <typename T>
auto PluginLoader::getPluginFunction(std::string_view pluginName,
                                     std::string_view functionName)
    -> PluginResult<std::function<T>> {
    std::shared_lock lock(mutex_);

    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return std::unexpected(PluginLoadError::FileNotFound);
    }

    // Delegate to ModuleLoader
    auto result = moduleLoader_->getFunction<T>(toStdString(pluginName),
                                                toStdString(functionName));

    if (!result) {
        return std::unexpected(PluginLoadError::SymbolNotFound);
    }

    return result.value();
}

}  // namespace lithium::server::plugin

#endif  // LITHIUM_SERVER_PLUGIN_LOADER_HPP
