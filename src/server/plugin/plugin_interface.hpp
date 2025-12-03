/*
 * plugin_interface.hpp - Dynamic Plugin Interfaces for Server Extensions
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_PLUGIN_INTERFACE_HPP
#define LITHIUM_SERVER_PLUGIN_INTERFACE_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/json_fwd.hpp"

// Forward declarations
namespace lithium::server {
class ServerApp;
}

namespace lithium::app {
class CommandDispatcher;
}

namespace lithium::server::plugin {

using json = nlohmann::json;

/**
 * @brief Plugin metadata structure
 */
struct PluginMetadata {
    std::string name;         ///< Unique plugin identifier
    std::string version;      ///< Semantic version string
    std::string description;  ///< Human-readable description
    std::string author;       ///< Plugin author
    std::string license;      ///< License type
    std::string homepage;     ///< Plugin homepage URL
    std::string repository;   ///< Source repository URL
    int priority{0};          ///< Load priority (higher = earlier)
    std::vector<std::string> dependencies;  ///< Required plugin dependencies
    std::vector<std::string> optionalDeps;  ///< Optional plugin dependencies
    std::vector<std::string> conflicts;     ///< Conflicting plugins
    std::vector<std::string> tags;          ///< Categorization tags
    std::vector<std::string> capabilities;  ///< Plugin capabilities

    /**
     * @brief Convert metadata to JSON
     */
    [[nodiscard]] auto toJson() const -> json;

    /**
     * @brief Parse metadata from JSON
     */
    static auto fromJson(const json& j) -> PluginMetadata;

    /**
     * @brief Check if plugin has a specific capability
     */
    [[nodiscard]] auto hasCapability(const std::string& cap) const -> bool;
};

/**
 * @brief Plugin lifecycle state
 */
enum class PluginState {
    Unloaded,    ///< Plugin not loaded
    Loading,     ///< Plugin currently loading
    Loaded,      ///< Plugin loaded but not initialized
    Initialized, ///< Plugin initialized and ready
    Running,     ///< Plugin actively running
    Paused,      ///< Plugin paused
    Stopping,    ///< Plugin shutting down
    Error,       ///< Plugin in error state
    Disabled     ///< Plugin disabled by user
};

/**
 * @brief Convert plugin state to string
 */
[[nodiscard]] auto pluginStateToString(PluginState state) -> std::string;

/**
 * @brief Base interface for all server plugins
 */
/**
 * @brief Plugin statistics
 */
struct PluginStatistics {
    size_t callCount{0};           ///< Total function calls
    size_t errorCount{0};          ///< Total errors
    double avgResponseTimeMs{0.0}; ///< Average response time
    std::chrono::system_clock::time_point lastAccessTime;  ///< Last access
    std::chrono::system_clock::time_point loadTime;        ///< Load time
    size_t memoryUsageBytes{0};    ///< Estimated memory usage
};

/**
 * @brief Base interface for all server plugins
 */
class IPlugin {
public:
    virtual ~IPlugin() = default;

    /**
     * @brief Get plugin metadata
     */
    [[nodiscard]] virtual auto getMetadata() const -> const PluginMetadata& = 0;

    /**
     * @brief Initialize the plugin
     * @param config Plugin configuration
     * @return true if initialization successful
     */
    virtual auto initialize(const json& config) -> bool = 0;

    /**
     * @brief Shutdown the plugin
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get current plugin state
     */
    [[nodiscard]] virtual auto getState() const -> PluginState = 0;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] virtual auto getLastError() const -> std::string = 0;

    /**
     * @brief Health check
     */
    [[nodiscard]] virtual auto isHealthy() const -> bool = 0;

    /**
     * @brief Pause the plugin
     * @return true if paused successfully
     */
    virtual auto pause() -> bool { return false; }

    /**
     * @brief Resume the plugin
     * @return true if resumed successfully
     */
    virtual auto resume() -> bool { return false; }

    /**
     * @brief Get plugin statistics
     */
    [[nodiscard]] virtual auto getStatistics() const -> PluginStatistics {
        return {};
    }

    /**
     * @brief Update plugin configuration at runtime
     * @param config New configuration
     * @return true if configuration updated successfully
     */
    virtual auto updateConfig(const json& config) -> bool { return false; }

    /**
     * @brief Get current plugin configuration
     */
    [[nodiscard]] virtual auto getConfig() const -> json { return {}; }

    /**
     * @brief Execute a plugin-specific action
     * @param action Action name
     * @param params Action parameters
     * @return Action result
     */
    virtual auto executeAction(const std::string& action, const json& params)
        -> json {
        return {{"error", "Action not supported"}};
    }

    /**
     * @brief Get list of supported actions
     */
    [[nodiscard]] virtual auto getSupportedActions() const
        -> std::vector<std::string> {
        return {};
    }

    /**
     * @brief Validate plugin configuration
     * @param config Configuration to validate
     * @return Validation result with error messages if any
     */
    [[nodiscard]] virtual auto validateConfig(const json& config) const
        -> std::pair<bool, std::string> {
        return {true, ""};
    }
};

/**
 * @brief Interface for command plugins
 *
 * Command plugins register handlers with the CommandDispatcher
 * to extend WebSocket command capabilities.
 */
class ICommandPlugin : public IPlugin {
public:
    /**
     * @brief Register commands with the dispatcher
     * @param dispatcher Command dispatcher instance
     */
    virtual void registerCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) = 0;

    /**
     * @brief Unregister all commands from the dispatcher
     * @param dispatcher Command dispatcher instance
     */
    virtual void unregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) = 0;

    /**
     * @brief Get list of command IDs registered by this plugin
     */
    [[nodiscard]] virtual auto getCommandIds() const
        -> std::vector<std::string> = 0;

    /**
     * @brief Get command description
     * @param commandId Command ID
     * @return Command description or empty if not found
     */
    [[nodiscard]] virtual auto getCommandDescription(const std::string& commandId) const
        -> std::string {
        return "";
    }

    /**
     * @brief Get command parameter schema
     * @param commandId Command ID
     * @return JSON schema for command parameters
     */
    [[nodiscard]] virtual auto getCommandSchema(const std::string& commandId) const
        -> json {
        return {};
    }

    /**
     * @brief Execute a command directly (bypass dispatcher)
     * @param commandId Command ID
     * @param params Command parameters
     * @return Command result
     */
    virtual auto executeCommand(const std::string& commandId, const json& params)
        -> json {
        return {{"error", "Direct execution not supported"}};
    }
};

/**
 * @brief Interface for controller plugins
 *
 * Controller plugins register HTTP routes with the Crow application
 * to extend REST API capabilities.
 */
/**
 * @brief Route information
 */
struct RouteInfo {
    std::string path;           ///< Route path
    std::string method;         ///< HTTP method (GET, POST, etc.)
    std::string description;    ///< Route description
    json parameterSchema;       ///< Parameter schema
    json responseSchema;        ///< Response schema
    bool requiresAuth{false};   ///< Requires authentication
};

class IControllerPlugin : public IPlugin {
public:
    /**
     * @brief Register HTTP routes with the application
     * @param app Crow application instance
     */
    virtual void registerRoutes(ServerApp& app) = 0;

    /**
     * @brief Get list of route paths registered by this plugin
     */
    [[nodiscard]] virtual auto getRoutePaths() const
        -> std::vector<std::string> = 0;

    /**
     * @brief Get route prefix for this controller
     */
    [[nodiscard]] virtual auto getRoutePrefix() const -> std::string = 0;

    /**
     * @brief Get detailed route information
     */
    [[nodiscard]] virtual auto getRouteInfo() const
        -> std::vector<RouteInfo> {
        return {};
    }

    /**
     * @brief Get OpenAPI specification for routes
     */
    [[nodiscard]] virtual auto getOpenApiSpec() const -> json {
        return {};
    }
};

/**
 * @brief Combined plugin interface for plugins that provide both
 * commands and controllers
 */
class IFullPlugin : public ICommandPlugin, public IControllerPlugin {
public:
    // Inherits all methods from both interfaces
};

/**
 * @brief Plugin factory function type
 */
using PluginFactory = std::function<std::shared_ptr<IPlugin>()>;
using CommandPluginFactory = std::function<std::shared_ptr<ICommandPlugin>()>;
using ControllerPluginFactory = std::function<std::shared_ptr<IControllerPlugin>()>;

/**
 * @brief Plugin entry point function signature
 *
 * Dynamic libraries must export this function to be loadable as plugins.
 * The function name must be "createPlugin".
 */
extern "C" using CreatePluginFunc = IPlugin* (*)();

/**
 * @brief Plugin destruction function signature
 *
 * Optional function for custom cleanup. Function name: "destroyPlugin"
 */
extern "C" using DestroyPluginFunc = void (*)(IPlugin*);

/**
 * @brief Get plugin API version function signature
 *
 * Returns the API version the plugin was built against.
 * Function name: "getPluginApiVersion"
 */
extern "C" using GetPluginApiVersionFunc = int (*)();

/// Current plugin API version
constexpr int PLUGIN_API_VERSION = 1;

/**
 * @brief Plugin capability constants
 */
namespace capabilities {
    constexpr const char* COMMAND = "command";
    constexpr const char* CONTROLLER = "controller";
    constexpr const char* HOT_RELOAD = "hot_reload";
    constexpr const char* PAUSE_RESUME = "pause_resume";
    constexpr const char* RUNTIME_CONFIG = "runtime_config";
    constexpr const char* DIRECT_EXECUTION = "direct_execution";
    constexpr const char* OPENAPI = "openapi";
}  // namespace capabilities

/**
 * @brief Plugin group for batch operations
 */
struct PluginGroup {
    std::string name;
    std::string description;
    std::vector<std::string> plugins;
    bool enabled{true};
};

}  // namespace lithium::server::plugin

#endif  // LITHIUM_SERVER_PLUGIN_INTERFACE_HPP
