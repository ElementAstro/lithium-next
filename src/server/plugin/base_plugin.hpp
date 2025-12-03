/*
 * base_plugin.hpp - Base Plugin Implementation Helper
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_PLUGIN_BASE_PLUGIN_HPP
#define LITHIUM_SERVER_PLUGIN_BASE_PLUGIN_HPP

#include "plugin_interface.hpp"

#include <atomic>
#include <mutex>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::server::plugin {

/**
 * @brief Base implementation for plugins
 *
 * Provides common functionality for plugin implementations.
 * Derive from this class to create command or controller plugins.
 */
class BasePlugin : public IPlugin {
public:
    explicit BasePlugin(PluginMetadata metadata)
        : metadata_(std::move(metadata)),
          state_(PluginState::Unloaded) {}

    ~BasePlugin() override = default;

    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
        return metadata_;
    }

    auto initialize(const json& config) -> bool override {
        std::lock_guard lock(mutex_);

        if (state_ != PluginState::Unloaded && state_ != PluginState::Error) {
            lastError_ = "Plugin already initialized";
            return false;
        }

        state_ = PluginState::Loading;
        config_ = config;

        try {
            if (!onInitialize(config)) {
                state_ = PluginState::Error;
                return false;
            }

            state_ = PluginState::Initialized;
            return true;
        } catch (const std::exception& e) {
            lastError_ = e.what();
            state_ = PluginState::Error;
            return false;
        }
    }

    void shutdown() override {
        std::lock_guard lock(mutex_);

        if (state_ == PluginState::Unloaded) {
            return;
        }

        state_ = PluginState::Stopping;

        try {
            onShutdown();
        } catch (...) {
            // Ignore shutdown errors
        }

        state_ = PluginState::Unloaded;
    }

    [[nodiscard]] auto getState() const -> PluginState override {
        return state_.load();
    }

    [[nodiscard]] auto getLastError() const -> std::string override {
        std::lock_guard lock(mutex_);
        return lastError_;
    }

    [[nodiscard]] auto isHealthy() const -> bool override {
        auto currentState = state_.load();
        return currentState == PluginState::Initialized ||
               currentState == PluginState::Running;
    }

protected:
    /**
     * @brief Override to perform custom initialization
     * @param config Plugin configuration
     * @return true if initialization successful
     */
    virtual auto onInitialize(const json& config) -> bool { return true; }

    /**
     * @brief Override to perform custom shutdown
     */
    virtual void onShutdown() {}

    /**
     * @brief Set the last error message
     */
    void setError(const std::string& error) {
        std::lock_guard lock(mutex_);
        lastError_ = error;
    }

    /**
     * @brief Set the plugin state
     */
    void setState(PluginState state) {
        state_ = state;
    }

    /**
     * @brief Get the plugin configuration
     */
    [[nodiscard]] auto getConfig() const -> const json& {
        return config_;
    }

private:
    PluginMetadata metadata_;
    std::atomic<PluginState> state_;
    json config_;
    std::string lastError_;
    mutable std::mutex mutex_;
};

/**
 * @brief Base implementation for command plugins
 */
class BaseCommandPlugin : public BasePlugin, public ICommandPlugin {
public:
    using BasePlugin::BasePlugin;

    // IPlugin interface - delegate to BasePlugin
    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
        return BasePlugin::getMetadata();
    }

    auto initialize(const json& config) -> bool override {
        return BasePlugin::initialize(config);
    }

    void shutdown() override {
        BasePlugin::shutdown();
    }

    [[nodiscard]] auto getState() const -> PluginState override {
        return BasePlugin::getState();
    }

    [[nodiscard]] auto getLastError() const -> std::string override {
        return BasePlugin::getLastError();
    }

    [[nodiscard]] auto isHealthy() const -> bool override {
        return BasePlugin::isHealthy();
    }

    // ICommandPlugin interface
    void registerCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        dispatcher_ = dispatcher;
        onRegisterCommands(dispatcher);
    }

    void unregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        onUnregisterCommands(dispatcher);
        dispatcher_.reset();
    }

    [[nodiscard]] auto getCommandIds() const
        -> std::vector<std::string> override {
        return commandIds_;
    }

protected:
    /**
     * @brief Override to register commands
     */
    virtual void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) = 0;

    /**
     * @brief Override to unregister commands
     */
    virtual void onUnregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) {
        for (const auto& id : commandIds_) {
            dispatcher->unregisterCommand(id);
        }
    }

    /**
     * @brief Add a command ID to the list
     */
    void addCommandId(const std::string& id) {
        commandIds_.push_back(id);
    }

    /**
     * @brief Get the command dispatcher
     */
    [[nodiscard]] auto getDispatcher() const
        -> std::shared_ptr<app::CommandDispatcher> {
        return dispatcher_;
    }

private:
    std::weak_ptr<app::CommandDispatcher> dispatcher_;
    std::vector<std::string> commandIds_;
};

/**
 * @brief Base implementation for controller plugins
 */
class BaseControllerPlugin : public BasePlugin, public IControllerPlugin {
public:
    explicit BaseControllerPlugin(PluginMetadata metadata,
                                   std::string routePrefix = "/api/v1/plugins")
        : BasePlugin(std::move(metadata)),
          routePrefix_(std::move(routePrefix)) {}

    // IPlugin interface - delegate to BasePlugin
    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
        return BasePlugin::getMetadata();
    }

    auto initialize(const json& config) -> bool override {
        return BasePlugin::initialize(config);
    }

    void shutdown() override {
        BasePlugin::shutdown();
    }

    [[nodiscard]] auto getState() const -> PluginState override {
        return BasePlugin::getState();
    }

    [[nodiscard]] auto getLastError() const -> std::string override {
        return BasePlugin::getLastError();
    }

    [[nodiscard]] auto isHealthy() const -> bool override {
        return BasePlugin::isHealthy();
    }

    // IControllerPlugin interface
    void registerRoutes(ServerApp& app) override {
        onRegisterRoutes(app);
    }

    [[nodiscard]] auto getRoutePaths() const
        -> std::vector<std::string> override {
        return routePaths_;
    }

    [[nodiscard]] auto getRoutePrefix() const -> std::string override {
        return routePrefix_;
    }

protected:
    /**
     * @brief Override to register routes
     */
    virtual void onRegisterRoutes(ServerApp& app) = 0;

    /**
     * @brief Add a route path to the list
     */
    void addRoutePath(const std::string& path) {
        routePaths_.push_back(path);
    }

    /**
     * @brief Set the route prefix
     */
    void setRoutePrefix(const std::string& prefix) {
        routePrefix_ = prefix;
    }

private:
    std::string routePrefix_;
    std::vector<std::string> routePaths_;
};

/**
 * @brief Base implementation for full plugins (both command and controller)
 */
class BaseFullPlugin : public BasePlugin, public IFullPlugin {
public:
    explicit BaseFullPlugin(PluginMetadata metadata,
                            std::string routePrefix = "/api/v1/plugins")
        : BasePlugin(std::move(metadata)),
          routePrefix_(std::move(routePrefix)) {}

    // IPlugin interface - delegate to BasePlugin
    [[nodiscard]] auto getMetadata() const -> const PluginMetadata& override {
        return BasePlugin::getMetadata();
    }

    auto initialize(const json& config) -> bool override {
        return BasePlugin::initialize(config);
    }

    void shutdown() override {
        BasePlugin::shutdown();
    }

    [[nodiscard]] auto getState() const -> PluginState override {
        return BasePlugin::getState();
    }

    [[nodiscard]] auto getLastError() const -> std::string override {
        return BasePlugin::getLastError();
    }

    [[nodiscard]] auto isHealthy() const -> bool override {
        return BasePlugin::isHealthy();
    }

    // ICommandPlugin interface
    void registerCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        dispatcher_ = dispatcher;
        onRegisterCommands(dispatcher);
    }

    void unregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        onUnregisterCommands(dispatcher);
        dispatcher_.reset();
    }

    [[nodiscard]] auto getCommandIds() const
        -> std::vector<std::string> override {
        return commandIds_;
    }

    // IControllerPlugin interface
    void registerRoutes(ServerApp& app) override {
        onRegisterRoutes(app);
    }

    [[nodiscard]] auto getRoutePaths() const
        -> std::vector<std::string> override {
        return routePaths_;
    }

    [[nodiscard]] auto getRoutePrefix() const -> std::string override {
        return routePrefix_;
    }

protected:
    /**
     * @brief Override to register commands
     */
    virtual void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) = 0;

    /**
     * @brief Override to unregister commands
     */
    virtual void onUnregisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) {
        for (const auto& id : commandIds_) {
            dispatcher->unregisterCommand(id);
        }
    }

    /**
     * @brief Override to register routes
     */
    virtual void onRegisterRoutes(ServerApp& app) = 0;

    /**
     * @brief Add a command ID to the list
     */
    void addCommandId(const std::string& id) {
        commandIds_.push_back(id);
    }

    /**
     * @brief Add a route path to the list
     */
    void addRoutePath(const std::string& path) {
        routePaths_.push_back(path);
    }

    /**
     * @brief Get the command dispatcher
     */
    [[nodiscard]] auto getDispatcher() const
        -> std::shared_ptr<app::CommandDispatcher> {
        return dispatcher_.lock();
    }

private:
    std::weak_ptr<app::CommandDispatcher> dispatcher_;
    std::vector<std::string> commandIds_;
    std::string routePrefix_;
    std::vector<std::string> routePaths_;
};

/**
 * @brief Macro to define plugin entry points
 *
 * Usage:
 *   LITHIUM_DEFINE_PLUGIN(MyPlugin)
 *
 * This will create the required createPlugin, destroyPlugin, and
 * getPluginApiVersion functions.
 */
#define LITHIUM_DEFINE_PLUGIN(PluginClass)                                    \
    extern "C" {                                                              \
    lithium::server::plugin::IPlugin* createPlugin() {                        \
        return new PluginClass();                                             \
    }                                                                         \
    void destroyPlugin(lithium::server::plugin::IPlugin* plugin) {            \
        delete plugin;                                                        \
    }                                                                         \
    int getPluginApiVersion() {                                               \
        return lithium::server::plugin::PLUGIN_API_VERSION;                   \
    }                                                                         \
    }

}  // namespace lithium::server::plugin

#endif  // LITHIUM_SERVER_PLUGIN_BASE_PLUGIN_HPP
