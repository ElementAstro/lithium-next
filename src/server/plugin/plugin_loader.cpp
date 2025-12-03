/*
 * plugin_loader.cpp - Dynamic Plugin Loader Implementation
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "plugin_loader.hpp"

#include <algorithm>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

namespace lithium::server::plugin {

auto pluginLoadErrorToString(PluginLoadError error) -> std::string {
    switch (error) {
        case PluginLoadError::FileNotFound:
            return "Plugin file not found";
        case PluginLoadError::InvalidPlugin:
            return "Invalid plugin format";
        case PluginLoadError::ApiVersionMismatch:
            return "Plugin API version mismatch";
        case PluginLoadError::DependencyMissing:
            return "Plugin dependency missing";
        case PluginLoadError::InitializationFailed:
            return "Plugin initialization failed";
        case PluginLoadError::AlreadyLoaded:
            return "Plugin already loaded";
        case PluginLoadError::LoadFailed:
            return "Plugin load failed";
        case PluginLoadError::SymbolNotFound:
            return "Required symbol not found in plugin";
        default:
            return "Unknown error";
    }
}

auto LoadedPluginInfo::asCommandPlugin() const
    -> std::shared_ptr<ICommandPlugin> {
    if (type == Type::Command || type == Type::Full) {
        return std::dynamic_pointer_cast<ICommandPlugin>(instance);
    }
    return nullptr;
}

auto LoadedPluginInfo::asControllerPlugin() const
    -> std::shared_ptr<IControllerPlugin> {
    if (type == Type::Controller || type == Type::Full) {
        return std::dynamic_pointer_cast<IControllerPlugin>(instance);
    }
    return nullptr;
}

auto LoadedPluginInfo::asFullPlugin() const
    -> std::shared_ptr<IFullPlugin> {
    if (type == Type::Full) {
        return std::dynamic_pointer_cast<IFullPlugin>(instance);
    }
    return nullptr;
}

auto LoadedPluginInfo::toJson() const -> json {
    json j;
    j["name"] = name;
    j["path"] = path;
    j["type"] = static_cast<int>(type);
    j["state"] = pluginStateToString(state);
    j["group"] = group;
    j["loadTime"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        loadTime.time_since_epoch()).count();

    if (instance) {
        j["metadata"] = instance->getMetadata().toJson();
        j["healthy"] = instance->isHealthy();
        auto lastError = instance->getLastError();
        if (!lastError.empty()) {
            j["lastError"] = lastError;
        }
    }

    // Statistics
    j["statistics"] = {
        {"callCount", statistics.callCount},
        {"errorCount", statistics.errorCount},
        {"avgResponseTimeMs", statistics.avgResponseTimeMs},
        {"memoryUsageBytes", statistics.memoryUsageBytes}
    };

    return j;
}

PluginLoader::PluginLoader(const PluginLoaderConfig& config)
    : config_(config),
      moduleLoader_(lithium::ModuleLoader::createShared(
          config.pluginDirectory.string())) {
    LOG_INFO("PluginLoader initialized with directory: {}",
             config_.pluginDirectory.string());

    // Add search paths
    for (const auto& path : config_.searchPaths) {
        LOG_DEBUG("Added plugin search path: {}", path);
    }
}

PluginLoader::~PluginLoader() {
    unloadAll();
}

auto PluginLoader::createShared(const PluginLoaderConfig& config)
    -> std::shared_ptr<PluginLoader> {
    return std::make_shared<PluginLoader>(config);
}

auto PluginLoader::loadPlugin(const std::filesystem::path& path,
                               const json& config) -> PluginResult<LoadedPluginInfo> {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("Plugin file not found: {}", path.string());
        return std::unexpected(PluginLoadError::FileNotFound);
    }

    // Extract plugin name from filename
    std::string pluginName = path.stem().string();

    // Remove common prefixes like "lib" on Unix
#ifndef _WIN32
    if (pluginName.starts_with("lib")) {
        pluginName = pluginName.substr(3);
    }
#endif

    // Check if already loaded
    {
        std::shared_lock lock(mutex_);
        if (loadedPlugins_.contains(pluginName)) {
            LOG_WARN("Plugin already loaded: {}", pluginName);
            return std::unexpected(PluginLoadError::AlreadyLoaded);
        }
    }

    LOG_INFO("Loading plugin: {} from {}", pluginName, path.string());

    // Load the module using ModuleLoader
    auto loadResult = moduleLoader_->loadModule(path.string(), pluginName);
    if (!loadResult) {
        LOG_ERROR("Failed to load plugin module: {}", loadResult.error());
        return std::unexpected(PluginLoadError::LoadFailed);
    }

    // Check API version if function exists
    if (moduleLoader_->hasFunction(pluginName, "getPluginApiVersion")) {
        auto versionFuncResult =
            moduleLoader_->getFunction<int()>(pluginName, "getPluginApiVersion");
        if (versionFuncResult) {
            int pluginApiVersion = versionFuncResult.value()();
            if (pluginApiVersion != config_.apiVersion) {
                LOG_ERROR("Plugin API version mismatch: expected {}, got {}",
                          config_.apiVersion, pluginApiVersion);
                moduleLoader_->unloadModule(pluginName);
                return std::unexpected(PluginLoadError::ApiVersionMismatch);
            }
        }
    }

    // Get the createPlugin function
    auto createFuncResult =
        moduleLoader_->getFunction<IPlugin*()>(pluginName, "createPlugin");
    if (!createFuncResult) {
        LOG_ERROR("Plugin missing createPlugin function: {}",
                  createFuncResult.error());
        moduleLoader_->unloadModule(pluginName);
        return std::unexpected(PluginLoadError::SymbolNotFound);
    }

    // Create plugin instance
    IPlugin* rawPlugin = createFuncResult.value()();
    if (!rawPlugin) {
        LOG_ERROR("createPlugin returned nullptr for: {}", pluginName);
        moduleLoader_->unloadModule(pluginName);
        return std::unexpected(PluginLoadError::InvalidPlugin);
    }

    // Wrap in shared_ptr with custom deleter if destroyPlugin exists
    std::shared_ptr<IPlugin> plugin;
    if (moduleLoader_->hasFunction(pluginName, "destroyPlugin")) {
        auto destroyFuncResult =
            moduleLoader_->getFunction<void(IPlugin*)>(pluginName, "destroyPlugin");
        if (destroyFuncResult) {
            auto destroyFunc = destroyFuncResult.value();
            plugin = std::shared_ptr<IPlugin>(rawPlugin, [destroyFunc](IPlugin* p) {
                destroyFunc(p);
            });
        } else {
            plugin = std::shared_ptr<IPlugin>(rawPlugin);
        }
    } else {
        plugin = std::shared_ptr<IPlugin>(rawPlugin);
    }

    // Determine plugin type
    auto pluginType = determinePluginType(plugin);

    // Initialize the plugin
    json pluginConfig = config;
    if (pluginConfig.empty()) {
        std::shared_lock lock(mutex_);
        if (auto it = pluginConfigs_.find(pluginName); it != pluginConfigs_.end()) {
            pluginConfig = it->second;
        }
    }

    if (!plugin->initialize(pluginConfig)) {
        LOG_ERROR("Plugin initialization failed: {}", pluginName);
        moduleLoader_->unloadModule(pluginName);
        return std::unexpected(PluginLoadError::InitializationFailed);
    }

    // Create plugin info
    LoadedPluginInfo info;
    info.name = pluginName;
    info.path = path.string();
    info.instance = plugin;
    info.state = plugin->getState();
    info.loadTime = std::chrono::system_clock::now();
    info.config = pluginConfig;
    info.type = pluginType;

    // Store in map
    {
        std::unique_lock lock(mutex_);
        loadedPlugins_[pluginName] = info;
        if (!pluginConfig.empty()) {
            pluginConfigs_[pluginName] = pluginConfig;
        }
    }

    LOG_INFO("Successfully loaded plugin: {} (type: {})", pluginName,
             static_cast<int>(pluginType));

    return info;
}

auto PluginLoader::loadPluginByName(std::string_view name, const json& config)
    -> PluginResult<LoadedPluginInfo> {
    auto pluginPath = findPluginFile(name);
    if (!pluginPath) {
        LOG_ERROR("Could not find plugin: {}", name);
        return std::unexpected(PluginLoadError::FileNotFound);
    }

    return loadPlugin(*pluginPath, config);
}

auto PluginLoader::unloadPlugin(std::string_view name) -> PluginResult<bool> {
    std::string pluginName = toStdString(name);

    std::unique_lock lock(mutex_);
    auto it = loadedPlugins_.find(pluginName);
    if (it == loadedPlugins_.end()) {
        LOG_WARN("Plugin not loaded: {}", pluginName);
        return std::unexpected(PluginLoadError::FileNotFound);
    }

    LOG_INFO("Unloading plugin: {}", pluginName);

    // Shutdown the plugin
    if (it->second.instance) {
        it->second.instance->shutdown();
    }

    // Remove from map
    loadedPlugins_.erase(it);

    // Unload the module
    lock.unlock();
    auto result = moduleLoader_->unloadModule(pluginName);
    if (!result) {
        LOG_WARN("Module unload returned error: {}", result.error());
    }

    LOG_INFO("Plugin unloaded: {}", pluginName);
    return true;
}

auto PluginLoader::reloadPlugin(std::string_view name)
    -> PluginResult<LoadedPluginInfo> {
    std::string pluginName = toStdString(name);

    // Get current config and path
    std::filesystem::path pluginPath;
    json pluginConfig;

    {
        std::shared_lock lock(mutex_);
        auto it = loadedPlugins_.find(pluginName);
        if (it != loadedPlugins_.end()) {
            pluginPath = it->second.path;
            pluginConfig = it->second.config;
        } else {
            return std::unexpected(PluginLoadError::FileNotFound);
        }
    }

    // Unload
    auto unloadResult = unloadPlugin(name);
    if (!unloadResult) {
        return std::unexpected(unloadResult.error());
    }

    // Reload
    return loadPlugin(pluginPath, pluginConfig);
}

auto PluginLoader::isPluginLoaded(std::string_view name) const -> bool {
    std::shared_lock lock(mutex_);
    return loadedPlugins_.contains(toStdString(name));
}

auto PluginLoader::getPlugin(std::string_view name) const
    -> std::optional<LoadedPluginInfo> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it != loadedPlugins_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PluginLoader::getAllPlugins() const -> std::vector<LoadedPluginInfo> {
    std::shared_lock lock(mutex_);
    std::vector<LoadedPluginInfo> plugins;
    plugins.reserve(loadedPlugins_.size());
    for (const auto& [_, info] : loadedPlugins_) {
        plugins.push_back(info);
    }
    return plugins;
}

auto PluginLoader::getCommandPlugins() const
    -> std::vector<std::shared_ptr<ICommandPlugin>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<ICommandPlugin>> plugins;
    for (const auto& [_, info] : loadedPlugins_) {
        if (auto cmdPlugin = info.asCommandPlugin()) {
            plugins.push_back(cmdPlugin);
        }
    }
    return plugins;
}

auto PluginLoader::getControllerPlugins() const
    -> std::vector<std::shared_ptr<IControllerPlugin>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<IControllerPlugin>> plugins;
    for (const auto& [_, info] : loadedPlugins_) {
        if (auto ctrlPlugin = info.asControllerPlugin()) {
            plugins.push_back(ctrlPlugin);
        }
    }
    return plugins;
}

auto PluginLoader::discoverPlugins() const -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> discovered;
    std::string ext = getLibraryExtension();

    // Search in main plugin directory
    if (std::filesystem::exists(config_.pluginDirectory)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(config_.pluginDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ext) {
                discovered.push_back(entry.path());
            }
        }
    }

    // Search in additional paths
    for (const auto& searchPath : config_.searchPaths) {
        std::filesystem::path path(searchPath);
        if (std::filesystem::exists(path)) {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ext) {
                    discovered.push_back(entry.path());
                }
            }
        }
    }

    LOG_INFO("Discovered {} plugins", discovered.size());
    return discovered;
}

auto PluginLoader::loadAllDiscovered() -> size_t {
    auto plugins = discoverPlugins();
    size_t loaded = 0;

    for (const auto& path : plugins) {
        auto result = loadPlugin(path);
        if (result) {
            loaded++;
        } else {
            LOG_WARN("Failed to load plugin {}: {}", path.string(),
                     pluginLoadErrorToString(result.error()));
        }
    }

    LOG_INFO("Loaded {}/{} discovered plugins", loaded, plugins.size());
    return loaded;
}

void PluginLoader::unloadAll() {
    std::vector<std::string> pluginNames;

    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, _] : loadedPlugins_) {
            pluginNames.push_back(name);
        }
    }

    // Unload in reverse order (for dependency handling)
    std::reverse(pluginNames.begin(), pluginNames.end());

    for (const auto& name : pluginNames) {
        unloadPlugin(name);
    }

    LOG_INFO("All plugins unloaded");
}

auto PluginLoader::validateDependencies(std::string_view name) const -> bool {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end()) {
        return false;
    }

    const auto& metadata = it->second.instance->getMetadata();
    for (const auto& dep : metadata.dependencies) {
        if (!loadedPlugins_.contains(dep)) {
            LOG_WARN("Plugin {} missing dependency: {}", name, dep);
            return false;
        }
    }

    return true;
}

auto PluginLoader::getLoadOrder() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    // Simple topological sort based on dependencies
    std::vector<std::string> order;
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> inStack;

    std::function<bool(const std::string&)> visit = [&](const std::string& name) -> bool {
        if (inStack[name]) {
            LOG_ERROR("Circular dependency detected for plugin: {}", name);
            return false;
        }
        if (visited[name]) {
            return true;
        }

        inStack[name] = true;

        auto it = loadedPlugins_.find(name);
        if (it != loadedPlugins_.end()) {
            const auto& metadata = it->second.instance->getMetadata();
            for (const auto& dep : metadata.dependencies) {
                if (!visit(dep)) {
                    return false;
                }
            }
        }

        inStack[name] = false;
        visited[name] = true;
        order.push_back(name);
        return true;
    };

    for (const auto& [name, _] : loadedPlugins_) {
        if (!visited[name]) {
            visit(name);
        }
    }

    return order;
}

void PluginLoader::setPluginConfig(std::string_view name, const json& config) {
    std::unique_lock lock(mutex_);
    pluginConfigs_[toStdString(name)] = config;
}

auto PluginLoader::getPluginConfig(std::string_view name) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto it = pluginConfigs_.find(toStdString(name));
    if (it != pluginConfigs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PluginLoader::findPluginFile(std::string_view name) const
    -> std::optional<std::filesystem::path> {
    std::string pluginName = toStdString(name);
    std::string ext = getLibraryExtension();

    // Try different naming conventions
    std::vector<std::string> candidates = {
        pluginName + ext,
#ifndef _WIN32
        "lib" + pluginName + ext,
#endif
    };

    // Search in main directory
    for (const auto& candidate : candidates) {
        auto path = config_.pluginDirectory / candidate;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Search in additional paths
    for (const auto& searchPath : config_.searchPaths) {
        for (const auto& candidate : candidates) {
            auto path = std::filesystem::path(searchPath) / candidate;
            if (std::filesystem::exists(path)) {
                return path;
            }
        }
    }

    return std::nullopt;
}

auto PluginLoader::determinePluginType(const std::shared_ptr<IPlugin>& plugin)
    -> LoadedPluginInfo::Type {
    // Check if it's a full plugin (both command and controller)
    if (std::dynamic_pointer_cast<IFullPlugin>(plugin)) {
        return LoadedPluginInfo::Type::Full;
    }

    // Check if it's a command plugin
    if (std::dynamic_pointer_cast<ICommandPlugin>(plugin)) {
        return LoadedPluginInfo::Type::Command;
    }

    // Check if it's a controller plugin
    if (std::dynamic_pointer_cast<IControllerPlugin>(plugin)) {
        return LoadedPluginInfo::Type::Controller;
    }

    return LoadedPluginInfo::Type::Unknown;
}

auto PluginLoader::getLibraryExtension() -> std::string {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

// ============================================================================
// Extended API Implementation
// ============================================================================

auto PluginLoader::executePluginAction(std::string_view pluginName,
                                       const std::string& action,
                                       const json& params) -> json {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return {{"error", "Plugin not found"}};
    }

    if (!it->second.instance) {
        return {{"error", "Plugin instance is null"}};
    }

    return it->second.instance->executeAction(action, params);
}

auto PluginLoader::executePluginCommand(std::string_view pluginName,
                                        const std::string& commandId,
                                        const json& params) -> json {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return {{"error", "Plugin not found"}};
    }

    auto cmdPlugin = it->second.asCommandPlugin();
    if (!cmdPlugin) {
        return {{"error", "Plugin is not a command plugin"}};
    }

    return cmdPlugin->executeCommand(commandId, params);
}

auto PluginLoader::getPluginStatistics(std::string_view name) const
    -> std::optional<PluginStatistics> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end()) {
        return std::nullopt;
    }

    // Return plugin's own statistics if available, otherwise return stored stats
    if (it->second.instance) {
        return it->second.instance->getStatistics();
    }
    return it->second.statistics;
}

auto PluginLoader::getPluginsByCapability(const std::string& capability) const
    -> std::vector<LoadedPluginInfo> {
    std::shared_lock lock(mutex_);
    std::vector<LoadedPluginInfo> result;

    for (const auto& [_, info] : loadedPlugins_) {
        if (info.instance &&
            info.instance->getMetadata().hasCapability(capability)) {
            result.push_back(info);
        }
    }

    return result;
}

auto PluginLoader::getPluginsByTag(const std::string& tag) const
    -> std::vector<LoadedPluginInfo> {
    std::shared_lock lock(mutex_);
    std::vector<LoadedPluginInfo> result;

    for (const auto& [_, info] : loadedPlugins_) {
        if (info.instance) {
            const auto& tags = info.instance->getMetadata().tags;
            if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
                result.push_back(info);
            }
        }
    }

    return result;
}

auto PluginLoader::pausePlugin(std::string_view name) -> bool {
    std::unique_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return false;
    }

    bool result = it->second.instance->pause();
    if (result) {
        it->second.state = PluginState::Paused;
    }
    return result;
}

auto PluginLoader::resumePlugin(std::string_view name) -> bool {
    std::unique_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return false;
    }

    bool result = it->second.instance->resume();
    if (result) {
        it->second.state = it->second.instance->getState();
    }
    return result;
}

auto PluginLoader::updatePluginConfig(std::string_view name, const json& config)
    -> bool {
    std::unique_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return false;
    }

    bool result = it->second.instance->updateConfig(config);
    if (result) {
        it->second.config = config;
        pluginConfigs_[toStdString(name)] = config;
    }
    return result;
}

auto PluginLoader::validatePluginConfig(std::string_view name,
                                        const json& config) const
    -> std::pair<bool, std::string> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return {false, "Plugin not found"};
    }

    return it->second.instance->validateConfig(config);
}

auto PluginLoader::getPluginActions(std::string_view name) const
    -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return {};
    }

    return it->second.instance->getSupportedActions();
}

auto PluginLoader::getCommandSchema(std::string_view pluginName,
                                    const std::string& commandId) const
    -> json {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return {};
    }

    auto cmdPlugin = it->second.asCommandPlugin();
    if (!cmdPlugin) {
        return {};
    }

    return cmdPlugin->getCommandSchema(commandId);
}

auto PluginLoader::getRouteInfo(std::string_view pluginName) const
    -> std::vector<RouteInfo> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return {};
    }

    auto ctrlPlugin = it->second.asControllerPlugin();
    if (!ctrlPlugin) {
        return {};
    }

    return ctrlPlugin->getRouteInfo();
}

auto PluginLoader::getOpenApiSpec(std::string_view pluginName) const -> json {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(pluginName));
    if (it == loadedPlugins_.end()) {
        return {};
    }

    auto ctrlPlugin = it->second.asControllerPlugin();
    if (!ctrlPlugin) {
        return {};
    }

    return ctrlPlugin->getOpenApiSpec();
}

auto PluginLoader::hasCapability(std::string_view name,
                                 const std::string& capability) const -> bool {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return false;
    }

    return it->second.instance->getMetadata().hasCapability(capability);
}

auto PluginLoader::getConflictingPlugins(std::string_view name) const
    -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(toStdString(name));
    if (it == loadedPlugins_.end() || !it->second.instance) {
        return {};
    }

    std::vector<std::string> conflicts;
    const auto& pluginConflicts = it->second.instance->getMetadata().conflicts;

    for (const auto& conflict : pluginConflicts) {
        if (loadedPlugins_.contains(conflict)) {
            conflicts.push_back(conflict);
        }
    }

    return conflicts;
}

auto PluginLoader::hasConflicts(std::string_view name) const -> bool {
    return !getConflictingPlugins(name).empty();
}

auto PluginLoader::getModuleLoader() const
    -> std::shared_ptr<lithium::ModuleLoader> {
    return moduleLoader_;
}

void PluginLoader::updateStatistics(const std::string& name) {
    std::unique_lock lock(mutex_);
    auto it = loadedPlugins_.find(name);
    if (it != loadedPlugins_.end()) {
        it->second.statistics.callCount++;
        it->second.statistics.lastAccessTime = std::chrono::system_clock::now();
    }
}

}  // namespace lithium::server::plugin
