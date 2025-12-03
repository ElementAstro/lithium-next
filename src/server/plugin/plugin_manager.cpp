/*
 * plugin_manager.cpp - Server Plugin Manager Implementation
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "plugin_manager.hpp"

#include <fstream>

#include "atom/log/spdlog_logger.hpp"

namespace lithium::server::plugin {

PluginManager::PluginManager(const PluginManagerConfig& config)
    : config_(config),
      loader_(PluginLoader::createShared(config.loaderConfig)) {
    LOG_INFO("PluginManager created");
}

PluginManager::~PluginManager() {
    shutdown();
}

auto PluginManager::createShared(const PluginManagerConfig& config)
    -> std::shared_ptr<PluginManager> {
    return std::make_shared<PluginManager>(config);
}

auto PluginManager::initialize(ServerApp& app,
                                std::shared_ptr<app::CommandDispatcher> dispatcher)
    -> bool {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        LOG_WARN("PluginManager already initialized");
        return true;
    }

    app_ = &app;
    dispatcher_ = dispatcher;

    LOG_INFO("Initializing PluginManager...");

    // Load configuration if exists
    if (std::filesystem::exists(config_.configFile)) {
        lock.unlock();
        loadConfiguration();
        lock.lock();
    }

    initialized_ = true;
    LOG_INFO("PluginManager initialized successfully");

    return true;
}

void PluginManager::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    LOG_INFO("Shutting down PluginManager...");

    // Disable all plugins first
    std::vector<std::string> pluginNames;
    for (const auto& [name, _] : enabledPlugins_) {
        pluginNames.push_back(name);
    }

    lock.unlock();

    for (const auto& name : pluginNames) {
        disablePlugin(name);
    }

    // Unload all plugins
    loader_->unloadAll();

    lock.lock();
    initialized_ = false;
    app_ = nullptr;
    dispatcher_.reset();

    LOG_INFO("PluginManager shutdown complete");
}

auto PluginManager::loadPlugin(std::string_view name, const json& config)
    -> PluginResult<LoadedPluginInfo> {
    LOG_INFO("Loading plugin: {}", name);

    auto result = loader_->loadPluginByName(name, config);
    if (!result) {
        LOG_ERROR("Failed to load plugin {}: {}", name,
                  pluginLoadErrorToString(result.error()));
        notifyEvent(PluginEvent::Error, toStdString(name),
                    {{"error", pluginLoadErrorToString(result.error())}});
        return result;
    }

    notifyEvent(PluginEvent::Loaded, toStdString(name),
                result->instance->getMetadata().toJson());

    // Auto-register if configured
    if (config_.autoRegisterOnLoad) {
        enablePlugin(name);
    }

    return result;
}

auto PluginManager::loadPluginFromPath(const std::filesystem::path& path,
                                        const json& config)
    -> PluginResult<LoadedPluginInfo> {
    LOG_INFO("Loading plugin from path: {}", path.string());

    auto result = loader_->loadPlugin(path, config);
    if (!result) {
        LOG_ERROR("Failed to load plugin from {}: {}", path.string(),
                  pluginLoadErrorToString(result.error()));
        notifyEvent(PluginEvent::Error, path.stem().string(),
                    {{"error", pluginLoadErrorToString(result.error())}});
        return result;
    }

    notifyEvent(PluginEvent::Loaded, result->name,
                result->instance->getMetadata().toJson());

    // Auto-register if configured
    if (config_.autoRegisterOnLoad) {
        enablePlugin(result->name);
    }

    return result;
}

auto PluginManager::unloadPlugin(std::string_view name) -> PluginResult<bool> {
    LOG_INFO("Unloading plugin: {}", name);

    // Disable first if enabled
    if (isPluginEnabled(name)) {
        disablePlugin(name);
    }

    auto result = loader_->unloadPlugin(name);
    if (!result) {
        LOG_ERROR("Failed to unload plugin {}: {}", name,
                  pluginLoadErrorToString(result.error()));
        return result;
    }

    notifyEvent(PluginEvent::Unloaded, toStdString(name), {});

    return result;
}

auto PluginManager::reloadPlugin(std::string_view name)
    -> PluginResult<LoadedPluginInfo> {
    LOG_INFO("Reloading plugin: {}", name);

    bool wasEnabled = isPluginEnabled(name);

    // Disable if enabled
    if (wasEnabled) {
        disablePlugin(name);
    }

    auto result = loader_->reloadPlugin(name);
    if (!result) {
        LOG_ERROR("Failed to reload plugin {}: {}", name,
                  pluginLoadErrorToString(result.error()));
        notifyEvent(PluginEvent::Error, toStdString(name),
                    {{"error", pluginLoadErrorToString(result.error())}});
        return result;
    }

    notifyEvent(PluginEvent::Reloaded, toStdString(name),
                result->instance->getMetadata().toJson());

    // Re-enable if was enabled
    if (wasEnabled) {
        enablePlugin(name);
    }

    return result;
}

auto PluginManager::enablePlugin(std::string_view name) -> bool {
    std::string pluginName = toStdString(name);

    LOG_INFO("Enabling plugin: {}", pluginName);

    auto pluginInfo = loader_->getPlugin(name);
    if (!pluginInfo) {
        LOG_ERROR("Plugin not loaded: {}", pluginName);
        return false;
    }

    {
        std::shared_lock lock(mutex_);
        if (enabledPlugins_.contains(pluginName) && enabledPlugins_[pluginName]) {
            LOG_WARN("Plugin already enabled: {}", pluginName);
            return true;
        }
    }

    // Register commands if it's a command plugin
    if (auto cmdPlugin = pluginInfo->asCommandPlugin()) {
        if (dispatcher_) {
            registerPluginCommands(pluginName, cmdPlugin);
        }
    }

    // Register routes if it's a controller plugin
    if (auto ctrlPlugin = pluginInfo->asControllerPlugin()) {
        if (app_) {
            registerPluginRoutes(pluginName, ctrlPlugin);
        }
    }

    {
        std::unique_lock lock(mutex_);
        enabledPlugins_[pluginName] = true;
    }

    notifyEvent(PluginEvent::Initialized, pluginName, {});
    LOG_INFO("Plugin enabled: {}", pluginName);

    return true;
}

auto PluginManager::disablePlugin(std::string_view name) -> bool {
    std::string pluginName = toStdString(name);

    LOG_INFO("Disabling plugin: {}", pluginName);

    auto pluginInfo = loader_->getPlugin(name);
    if (!pluginInfo) {
        LOG_ERROR("Plugin not loaded: {}", pluginName);
        return false;
    }

    {
        std::shared_lock lock(mutex_);
        if (!enabledPlugins_.contains(pluginName) || !enabledPlugins_[pluginName]) {
            LOG_WARN("Plugin not enabled: {}", pluginName);
            return true;
        }
    }

    // Unregister commands if it's a command plugin
    if (auto cmdPlugin = pluginInfo->asCommandPlugin()) {
        if (dispatcher_) {
            unregisterPluginCommands(pluginName, cmdPlugin);
        }
    }

    // Note: Crow doesn't support dynamic route removal, so controller routes
    // remain registered but the plugin instance will be unavailable

    {
        std::unique_lock lock(mutex_);
        enabledPlugins_[pluginName] = false;
    }

    notifyEvent(PluginEvent::Shutdown, pluginName, {});
    LOG_INFO("Plugin disabled: {}", pluginName);

    return true;
}

auto PluginManager::isPluginLoaded(std::string_view name) const -> bool {
    return loader_->isPluginLoaded(name);
}

auto PluginManager::isPluginEnabled(std::string_view name) const -> bool {
    std::shared_lock lock(mutex_);
    auto it = enabledPlugins_.find(toStdString(name));
    return it != enabledPlugins_.end() && it->second;
}

auto PluginManager::getPluginInfo(std::string_view name) const
    -> std::optional<LoadedPluginInfo> {
    return loader_->getPlugin(name);
}

auto PluginManager::getAllPlugins() const -> std::vector<LoadedPluginInfo> {
    return loader_->getAllPlugins();
}

auto PluginManager::getPluginsByType(LoadedPluginInfo::Type type) const
    -> std::vector<LoadedPluginInfo> {
    auto allPlugins = loader_->getAllPlugins();
    std::vector<LoadedPluginInfo> filtered;

    for (const auto& plugin : allPlugins) {
        if (plugin.type == type) {
            filtered.push_back(plugin);
        }
    }

    return filtered;
}

auto PluginManager::discoverAndLoadAll() -> size_t {
    LOG_INFO("Discovering and loading all plugins...");

    size_t loaded = loader_->loadAllDiscovered();

    // Enable all loaded plugins if auto-register is on
    if (config_.autoRegisterOnLoad) {
        for (const auto& plugin : loader_->getAllPlugins()) {
            enablePlugin(plugin.name);
        }
    }

    LOG_INFO("Loaded {} plugins", loaded);
    return loaded;
}

auto PluginManager::getAvailablePlugins() const
    -> std::vector<std::filesystem::path> {
    return loader_->discoverPlugins();
}

auto PluginManager::subscribeToEvents(PluginEventCallback callback) -> int {
    std::unique_lock lock(mutex_);
    int id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void PluginManager::unsubscribeFromEvents(int subscriptionId) {
    std::unique_lock lock(mutex_);
    eventSubscribers_.erase(subscriptionId);
}

auto PluginManager::getPluginHealth(std::string_view name) const -> json {
    auto pluginInfo = loader_->getPlugin(name);
    if (!pluginInfo) {
        return {{"error", "Plugin not found"}};
    }

    json health;
    health["name"] = pluginInfo->name;
    health["state"] = pluginStateToString(pluginInfo->state);
    health["healthy"] = pluginInfo->instance->isHealthy();
    health["enabled"] = isPluginEnabled(name);

    auto metadata = pluginInfo->instance->getMetadata();
    health["version"] = metadata.version;

    auto lastError = pluginInfo->instance->getLastError();
    if (!lastError.empty()) {
        health["lastError"] = lastError;
    }

    return health;
}

auto PluginManager::getSystemStatus() const -> json {
    auto allPlugins = loader_->getAllPlugins();

    json status;
    status["totalPlugins"] = allPlugins.size();

    size_t enabledCount = 0;
    size_t healthyCount = 0;
    size_t commandPlugins = 0;
    size_t controllerPlugins = 0;

    json pluginList = json::array();

    for (const auto& plugin : allPlugins) {
        if (isPluginEnabled(plugin.name)) {
            enabledCount++;
        }
        if (plugin.instance->isHealthy()) {
            healthyCount++;
        }
        if (plugin.type == LoadedPluginInfo::Type::Command ||
            plugin.type == LoadedPluginInfo::Type::Full) {
            commandPlugins++;
        }
        if (plugin.type == LoadedPluginInfo::Type::Controller ||
            plugin.type == LoadedPluginInfo::Type::Full) {
            controllerPlugins++;
        }

        pluginList.push_back({
            {"name", plugin.name},
            {"type", static_cast<int>(plugin.type)},
            {"enabled", isPluginEnabled(plugin.name)},
            {"healthy", plugin.instance->isHealthy()}
        });
    }

    status["enabledPlugins"] = enabledCount;
    status["healthyPlugins"] = healthyCount;
    status["commandPlugins"] = commandPlugins;
    status["controllerPlugins"] = controllerPlugins;
    status["plugins"] = pluginList;

    return status;
}

auto PluginManager::loadConfiguration() -> bool {
    if (!std::filesystem::exists(config_.configFile)) {
        LOG_WARN("Plugin configuration file not found: {}",
                 config_.configFile.string());
        return false;
    }

    try {
        std::ifstream file(config_.configFile);
        json configJson;
        file >> configJson;

        if (configJson.contains("plugins") && configJson["plugins"].is_array()) {
            for (const auto& pluginConfig : configJson["plugins"]) {
                std::string name = pluginConfig.value("name", "");
                if (name.empty()) {
                    continue;
                }

                json config = pluginConfig.value("config", json::object());
                loader_->setPluginConfig(name, config);

                if (pluginConfig.value("autoLoad", false)) {
                    loadPlugin(name, config);
                }
            }
        }

        LOG_INFO("Loaded plugin configuration from: {}",
                 config_.configFile.string());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load plugin configuration: {}", e.what());
        return false;
    }
}

auto PluginManager::saveConfiguration() const -> bool {
    try {
        json configJson;
        json pluginsArray = json::array();

        auto allPlugins = loader_->getAllPlugins();
        for (const auto& plugin : allPlugins) {
            json pluginEntry;
            pluginEntry["name"] = plugin.name;
            pluginEntry["path"] = plugin.path;
            pluginEntry["config"] = plugin.config;
            pluginEntry["enabled"] = isPluginEnabled(plugin.name);
            pluginEntry["autoLoad"] = true;

            pluginsArray.push_back(pluginEntry);
        }

        configJson["plugins"] = pluginsArray;

        // Ensure directory exists
        std::filesystem::create_directories(config_.configFile.parent_path());

        std::ofstream file(config_.configFile);
        file << configJson.dump(2);

        LOG_INFO("Saved plugin configuration to: {}",
                 config_.configFile.string());
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save plugin configuration: {}", e.what());
        return false;
    }
}

void PluginManager::updatePluginConfig(std::string_view name, const json& config) {
    loader_->setPluginConfig(name, config);
}

auto PluginManager::getPluginConfig(std::string_view name) const
    -> std::optional<json> {
    return loader_->getPluginConfig(name);
}

auto PluginManager::getLoader() const -> std::shared_ptr<PluginLoader> {
    return loader_;
}

void PluginManager::registerPluginCommands(const std::string& name,
                                            std::shared_ptr<ICommandPlugin> plugin) {
    LOG_DEBUG("Registering commands for plugin: {}", name);

    try {
        plugin->registerCommands(dispatcher_);

        auto commandIds = plugin->getCommandIds();
        LOG_INFO("Plugin {} registered {} commands", name, commandIds.size());

        for (const auto& cmdId : commandIds) {
            LOG_DEBUG("  - {}", cmdId);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to register commands for plugin {}: {}", name, e.what());
    }
}

void PluginManager::unregisterPluginCommands(const std::string& name,
                                              std::shared_ptr<ICommandPlugin> plugin) {
    LOG_DEBUG("Unregistering commands for plugin: {}", name);

    try {
        plugin->unregisterCommands(dispatcher_);

        auto commandIds = plugin->getCommandIds();
        LOG_INFO("Plugin {} unregistered {} commands", name, commandIds.size());
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to unregister commands for plugin {}: {}", name, e.what());
    }
}

void PluginManager::registerPluginRoutes(const std::string& name,
                                          std::shared_ptr<IControllerPlugin> plugin) {
    LOG_DEBUG("Registering routes for plugin: {}", name);

    try {
        plugin->registerRoutes(*app_);

        auto routePaths = plugin->getRoutePaths();
        LOG_INFO("Plugin {} registered {} routes with prefix: {}",
                 name, routePaths.size(), plugin->getRoutePrefix());

        for (const auto& path : routePaths) {
            LOG_DEBUG("  - {}", path);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to register routes for plugin {}: {}", name, e.what());
    }
}

void PluginManager::notifyEvent(PluginEvent event, const std::string& pluginName,
                                 const json& data) {
    if (!config_.enableEventNotifications) {
        return;
    }

    std::shared_lock lock(mutex_);
    for (const auto& [_, callback] : eventSubscribers_) {
        try {
            callback(event, pluginName, data);
        } catch (const std::exception& e) {
            LOG_WARN("Plugin event callback threw exception: {}", e.what());
        }
    }
}

// ============================================================================
// Extended API - Batch Operations
// ============================================================================

auto PluginManager::batchLoad(const std::vector<std::string>& names) -> size_t {
    size_t loaded = 0;
    for (const auto& name : names) {
        auto result = loadPlugin(name);
        if (result) {
            loaded++;
        }
    }
    return loaded;
}

auto PluginManager::batchUnload(const std::vector<std::string>& names) -> size_t {
    size_t unloaded = 0;
    for (const auto& name : names) {
        auto result = unloadPlugin(name);
        if (result) {
            unloaded++;
        }
    }
    return unloaded;
}

auto PluginManager::batchEnable(const std::vector<std::string>& names) -> size_t {
    size_t enabled = 0;
    for (const auto& name : names) {
        if (enablePlugin(name)) {
            enabled++;
        }
    }
    return enabled;
}

auto PluginManager::batchDisable(const std::vector<std::string>& names) -> size_t {
    size_t disabled = 0;
    for (const auto& name : names) {
        if (disablePlugin(name)) {
            disabled++;
        }
    }
    return disabled;
}

// ============================================================================
// Extended API - Group Management
// ============================================================================

void PluginManager::createGroup(const PluginGroup& group) {
    std::unique_lock lock(mutex_);
    groups_[group.name] = group;
    LOG_INFO("Created plugin group: {}", group.name);
}

void PluginManager::deleteGroup(const std::string& name) {
    std::unique_lock lock(mutex_);
    groups_.erase(name);
    LOG_INFO("Deleted plugin group: {}", name);
}

auto PluginManager::getGroup(const std::string& name) const
    -> std::optional<PluginGroup> {
    std::shared_lock lock(mutex_);
    auto it = groups_.find(name);
    if (it != groups_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PluginManager::getAllGroups() const -> std::vector<PluginGroup> {
    std::shared_lock lock(mutex_);
    std::vector<PluginGroup> result;
    result.reserve(groups_.size());
    for (const auto& [_, group] : groups_) {
        result.push_back(group);
    }
    return result;
}

void PluginManager::addToGroup(const std::string& pluginName,
                               const std::string& groupName) {
    std::unique_lock lock(mutex_);
    auto it = groups_.find(groupName);
    if (it != groups_.end()) {
        auto& plugins = it->second.plugins;
        if (std::find(plugins.begin(), plugins.end(), pluginName) == plugins.end()) {
            plugins.push_back(pluginName);
        }
    }
}

void PluginManager::removeFromGroup(const std::string& pluginName,
                                    const std::string& groupName) {
    std::unique_lock lock(mutex_);
    auto it = groups_.find(groupName);
    if (it != groups_.end()) {
        auto& plugins = it->second.plugins;
        plugins.erase(std::remove(plugins.begin(), plugins.end(), pluginName),
                      plugins.end());
    }
}

auto PluginManager::enableGroup(const std::string& groupName) -> size_t {
    auto group = getGroup(groupName);
    if (!group) {
        return 0;
    }
    return batchEnable(group->plugins);
}

auto PluginManager::disableGroup(const std::string& groupName) -> size_t {
    auto group = getGroup(groupName);
    if (!group) {
        return 0;
    }
    return batchDisable(group->plugins);
}

// ============================================================================
// Extended API - Plugin Execution
// ============================================================================

auto PluginManager::executeAction(std::string_view pluginName,
                                  const std::string& action,
                                  const json& params) -> json {
    auto result = loader_->executePluginAction(pluginName, action, params);
    notifyEvent(PluginEvent::ActionExecuted, toStdString(pluginName),
                {{"action", action}, {"params", params}});
    return result;
}

auto PluginManager::executeCommand(std::string_view pluginName,
                                   const std::string& commandId,
                                   const json& params) -> json {
    return loader_->executePluginCommand(pluginName, commandId, params);
}

auto PluginManager::getPluginActions(std::string_view pluginName) const
    -> std::vector<std::string> {
    return loader_->getPluginActions(pluginName);
}

// ============================================================================
// Extended API - Plugin Queries
// ============================================================================

auto PluginManager::getPluginsByCapability(const std::string& capability) const
    -> std::vector<LoadedPluginInfo> {
    return loader_->getPluginsByCapability(capability);
}

auto PluginManager::getPluginsByTag(const std::string& tag) const
    -> std::vector<LoadedPluginInfo> {
    return loader_->getPluginsByTag(tag);
}

auto PluginManager::getPluginsByGroup(const std::string& groupName) const
    -> std::vector<LoadedPluginInfo> {
    auto group = getGroup(groupName);
    if (!group) {
        return {};
    }

    std::vector<LoadedPluginInfo> result;
    for (const auto& name : group->plugins) {
        auto info = loader_->getPlugin(name);
        if (info) {
            result.push_back(*info);
        }
    }
    return result;
}

auto PluginManager::searchPlugins(const std::string& pattern) const
    -> std::vector<LoadedPluginInfo> {
    auto allPlugins = loader_->getAllPlugins();
    std::vector<LoadedPluginInfo> result;

    // Simple wildcard matching (* at start/end)
    bool startsWithWildcard = !pattern.empty() && pattern.front() == '*';
    bool endsWithWildcard = !pattern.empty() && pattern.back() == '*';

    std::string searchPattern = pattern;
    if (startsWithWildcard) {
        searchPattern = searchPattern.substr(1);
    }
    if (endsWithWildcard && !searchPattern.empty()) {
        searchPattern = searchPattern.substr(0, searchPattern.size() - 1);
    }

    for (const auto& plugin : allPlugins) {
        bool match = false;

        if (startsWithWildcard && endsWithWildcard) {
            match = plugin.name.find(searchPattern) != std::string::npos;
        } else if (startsWithWildcard) {
            match = plugin.name.size() >= searchPattern.size() &&
                    plugin.name.compare(plugin.name.size() - searchPattern.size(),
                                        searchPattern.size(), searchPattern) == 0;
        } else if (endsWithWildcard) {
            match = plugin.name.compare(0, searchPattern.size(), searchPattern) == 0;
        } else {
            match = plugin.name == pattern;
        }

        if (match) {
            result.push_back(plugin);
        }
    }

    return result;
}

// ============================================================================
// Extended API - Plugin State Control
// ============================================================================

auto PluginManager::pausePlugin(std::string_view name) -> bool {
    bool result = loader_->pausePlugin(name);
    if (result) {
        notifyEvent(PluginEvent::Paused, toStdString(name), {});
    }
    return result;
}

auto PluginManager::resumePlugin(std::string_view name) -> bool {
    bool result = loader_->resumePlugin(name);
    if (result) {
        notifyEvent(PluginEvent::Resumed, toStdString(name), {});
    }
    return result;
}

auto PluginManager::restartPlugin(std::string_view name) -> bool {
    bool wasEnabled = isPluginEnabled(name);

    if (wasEnabled) {
        disablePlugin(name);
    }

    auto result = reloadPlugin(name);
    if (!result) {
        return false;
    }

    if (wasEnabled) {
        enablePlugin(name);
    }

    return true;
}

// ============================================================================
// Extended API - Schema and Documentation
// ============================================================================

auto PluginManager::getCommandSchema(std::string_view pluginName,
                                     const std::string& commandId) const -> json {
    return loader_->getCommandSchema(pluginName, commandId);
}

auto PluginManager::getAllCommandSchemas(std::string_view pluginName) const
    -> json {
    auto pluginInfo = loader_->getPlugin(pluginName);
    if (!pluginInfo) {
        return {};
    }

    auto cmdPlugin = pluginInfo->asCommandPlugin();
    if (!cmdPlugin) {
        return {};
    }

    json schemas;
    for (const auto& cmdId : cmdPlugin->getCommandIds()) {
        schemas[cmdId] = {
            {"description", cmdPlugin->getCommandDescription(cmdId)},
            {"schema", cmdPlugin->getCommandSchema(cmdId)}
        };
    }
    return schemas;
}

auto PluginManager::getRouteInfo(std::string_view pluginName) const
    -> std::vector<RouteInfo> {
    return loader_->getRouteInfo(pluginName);
}

auto PluginManager::getOpenApiSpec(std::string_view pluginName) const -> json {
    return loader_->getOpenApiSpec(pluginName);
}

auto PluginManager::getCombinedOpenApiSpec() const -> json {
    json combined;
    combined["openapi"] = "3.0.0";
    combined["info"] = {
        {"title", "Lithium Server Plugin API"},
        {"version", "1.0.0"}
    };
    combined["paths"] = json::object();

    auto allPlugins = loader_->getAllPlugins();
    for (const auto& plugin : allPlugins) {
        auto ctrlPlugin = plugin.asControllerPlugin();
        if (!ctrlPlugin) {
            continue;
        }

        auto spec = ctrlPlugin->getOpenApiSpec();
        if (spec.contains("paths") && spec["paths"].is_object()) {
            for (auto& [path, pathItem] : spec["paths"].items()) {
                combined["paths"][path] = pathItem;
            }
        }
    }

    return combined;
}

// ============================================================================
// Extended API - Statistics and Monitoring
// ============================================================================

auto PluginManager::getPluginStatistics(std::string_view name) const
    -> std::optional<PluginStatistics> {
    return loader_->getPluginStatistics(name);
}

auto PluginManager::getAllStatistics() const -> json {
    json stats;
    auto allPlugins = loader_->getAllPlugins();

    for (const auto& plugin : allPlugins) {
        auto pluginStats = loader_->getPluginStatistics(plugin.name);
        if (pluginStats) {
            stats[plugin.name] = {
                {"callCount", pluginStats->callCount},
                {"errorCount", pluginStats->errorCount},
                {"avgResponseTimeMs", pluginStats->avgResponseTimeMs},
                {"memoryUsageBytes", pluginStats->memoryUsageBytes}
            };
        }
    }

    return stats;
}

void PluginManager::resetStatistics(std::string_view name) {
    // Statistics are managed by the loader
    // This would require adding a reset method to the loader
    LOG_INFO("Reset statistics for: {}", name.empty() ? "all plugins" : name);
}

void PluginManager::enablePerformanceMonitoring(bool enable) {
    performanceMonitoringEnabled_ = enable;
    LOG_INFO("Performance monitoring {}", enable ? "enabled" : "disabled");
}

// ============================================================================
// Extended API - Dependency Management
// ============================================================================

auto PluginManager::getPluginDependencies(std::string_view name) const
    -> std::vector<std::string> {
    auto pluginInfo = loader_->getPlugin(name);
    if (!pluginInfo || !pluginInfo->instance) {
        return {};
    }
    return pluginInfo->instance->getMetadata().dependencies;
}

auto PluginManager::getDependentPlugins(std::string_view name) const
    -> std::vector<std::string> {
    std::string targetName = toStdString(name);
    std::vector<std::string> dependents;

    auto allPlugins = loader_->getAllPlugins();
    for (const auto& plugin : allPlugins) {
        if (!plugin.instance) {
            continue;
        }

        const auto& deps = plugin.instance->getMetadata().dependencies;
        if (std::find(deps.begin(), deps.end(), targetName) != deps.end()) {
            dependents.push_back(plugin.name);
        }
    }

    return dependents;
}

auto PluginManager::hasConflicts(std::string_view name) const -> bool {
    return loader_->hasConflicts(name);
}

auto PluginManager::getConflictingPlugins(std::string_view name) const
    -> std::vector<std::string> {
    return loader_->getConflictingPlugins(name);
}

}  // namespace lithium::server::plugin
