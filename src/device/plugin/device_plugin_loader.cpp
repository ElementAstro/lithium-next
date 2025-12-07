/*
 * device_plugin_loader.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device plugin loader implementation

**************************************************/

#include "device_plugin_loader.hpp"

#include "device/service/device_factory.hpp"
#include "device/service/device_type_registry.hpp"

#include <loguru.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace lithium::device {

// ============================================================================
// PluginLoadEvent Implementation
// ============================================================================

auto PluginLoadEvent::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["pluginName"] = pluginName;
    j["pluginPath"] = pluginPath.string();
    j["message"] = message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["data"] = data;
    return j;
}

// ============================================================================
// LoadedPluginInfo Implementation
// ============================================================================

auto LoadedPluginInfo::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["name"] = name;
    j["path"] = path.string();
    j["loadedAt"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        loadedAt.time_since_epoch())
                        .count();
    j["reloadCount"] = reloadCount;
    j["isBuiltIn"] = isBuiltIn;

    if (plugin) {
        j["state"] = devicePluginStateToString(plugin->getDevicePluginState());
        j["metadata"] = plugin->getDeviceMetadata().toJson();
    }

    return j;
}

// ============================================================================
// DevicePluginLoader Implementation
// ============================================================================

DevicePluginLoader::DevicePluginLoader() = default;

DevicePluginLoader::~DevicePluginLoader() {
    if (initialized_) {
        shutdown();
    }
}

auto DevicePluginLoader::getInstance() -> DevicePluginLoader& {
    static DevicePluginLoader instance;
    return instance;
}

void DevicePluginLoader::initialize(const nlohmann::json& config) {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        return;
    }

    config_ = config;

    // Parse plugin paths from config
    if (config.contains("plugin_paths")) {
        for (const auto& pathStr : config["plugin_paths"]) {
            pluginPaths_.emplace_back(pathStr.get<std::string>());
        }
    }

    // Add default paths if none configured
    if (pluginPaths_.empty()) {
        pluginPaths_.emplace_back("plugins/devices");
        pluginPaths_.emplace_back("plugins");
    }

    initialized_ = true;
    LOG_F(INFO, "DevicePluginLoader initialized with %zu search paths",
          pluginPaths_.size());
}

void DevicePluginLoader::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    LOG_F(INFO, "Shutting down DevicePluginLoader...");

    // Unload all plugins (release lock temporarily)
    lock.unlock();
    unloadAllPlugins();
    lock.lock();

    // Clear event subscribers
    {
        std::unique_lock eventLock(eventMutex_);
        eventSubscribers_.clear();
    }

    pluginPaths_.clear();
    initialized_ = false;

    LOG_F(INFO, "DevicePluginLoader shutdown complete");
}

void DevicePluginLoader::setPluginPaths(
    const std::vector<std::filesystem::path>& paths) {
    std::unique_lock lock(mutex_);
    pluginPaths_ = paths;
}

void DevicePluginLoader::addPluginPath(const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);
    pluginPaths_.push_back(path);
}

auto DevicePluginLoader::discoverPlugins() const
    -> std::vector<PluginDiscoveryResult> {
    std::shared_lock lock(mutex_);

    std::vector<PluginDiscoveryResult> results;
    for (const auto& path : pluginPaths_) {
        auto pathResults = discoverPluginsIn(path);
        results.insert(results.end(), pathResults.begin(), pathResults.end());
    }

    return results;
}

auto DevicePluginLoader::discoverPluginsIn(
    const std::filesystem::path& directory) const
    -> std::vector<PluginDiscoveryResult> {
    std::vector<PluginDiscoveryResult> results;

    if (!std::filesystem::exists(directory) ||
        !std::filesystem::is_directory(directory)) {
        return results;
    }

    try {
        for (const auto& entry :
             std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && hasValidExtension(entry.path())) {
                auto result = probePlugin(entry.path());
                if (!result.error.empty() || result.isDevicePlugin) {
                    results.push_back(std::move(result));
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_F(WARNING, "Error discovering plugins in %s: %s",
              directory.string().c_str(), e.what());
    }

    return results;
}

auto DevicePluginLoader::probePlugin(const std::filesystem::path& path) const
    -> PluginDiscoveryResult {
    PluginDiscoveryResult result;
    result.path = path;
    result.name = path.stem().string();

    // Try to load the library temporarily to probe it
    void* handle = nullptr;

#ifdef _WIN32
    handle = LoadLibraryW(path.c_str());
    if (!handle) {
        result.error = "Failed to load library: " +
                       std::to_string(GetLastError());
        return result;
    }
#else
    handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        result.error = std::string("Failed to load library: ") + dlerror();
        return result;
    }
#endif

    // Check for device plugin entry point
    auto createFunc =
        getFunction<CreateDevicePluginFunc>(handle, "createDevicePlugin");
    if (createFunc) {
        result.isDevicePlugin = true;

        // Try to get version
        auto versionFunc = getFunction<GetDevicePluginApiVersionFunc>(
            handle, "getDevicePluginApiVersion");
        if (versionFunc) {
            result.version = std::to_string(versionFunc());
        }

        // Try to get metadata by creating a temporary instance
        try {
            auto* plugin = createFunc();
            if (plugin) {
                result.metadata = plugin->getDeviceMetadata().toJson();
                result.name = plugin->getMetadata().name;
                result.version = plugin->getMetadata().version;

                // Clean up
                auto destroyFunc = getFunction<DestroyDevicePluginFunc>(
                    handle, "destroyDevicePlugin");
                if (destroyFunc) {
                    destroyFunc(plugin);
                } else {
                    delete plugin;
                }
            }
        } catch (...) {
            // Ignore errors when probing
        }
    }

    // Unload the library
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif

    return result;
}

auto DevicePluginLoader::loadPlugin(const std::filesystem::path& path,
                                    const nlohmann::json& config)
    -> DeviceResult<bool> {
    // Emit loading event
    emitEvent(createEvent(PluginLoadEventType::Loading, path.stem().string(),
                          path, "Loading plugin"));

    // Load the library
    auto handleResult = loadLibrary(path);
    if (!handleResult) {
        emitEvent(createEvent(PluginLoadEventType::LoadFailed,
                              path.stem().string(), path,
                              handleResult.error().message));
        loadFailures_++;
        return std::unexpected(handleResult.error());
    }

    void* handle = *handleResult;

    // Get the create function
    auto createFunc =
        getFunction<CreateDevicePluginFunc>(handle, "createDevicePlugin");
    if (!createFunc) {
        unloadLibrary(handle);
        auto err = error::pluginLoadFailed(
            path.string(), "Missing createDevicePlugin entry point");
        emitEvent(createEvent(PluginLoadEventType::LoadFailed,
                              path.stem().string(), path, err.message));
        loadFailures_++;
        return std::unexpected(err);
    }

    // Check API version
    auto versionFunc = getFunction<GetDevicePluginApiVersionFunc>(
        handle, "getDevicePluginApiVersion");
    if (versionFunc) {
        int version = versionFunc();
        if (version != DEVICE_PLUGIN_API_VERSION) {
            unloadLibrary(handle);
            auto err = error::pluginLoadFailed(
                path.string(),
                "API version mismatch: expected " +
                    std::to_string(DEVICE_PLUGIN_API_VERSION) + ", got " +
                    std::to_string(version));
            emitEvent(createEvent(PluginLoadEventType::LoadFailed,
                                  path.stem().string(), path, err.message));
            loadFailures_++;
            return std::unexpected(err);
        }
    }

    // Create plugin instance
    IDevicePlugin* rawPlugin = nullptr;
    try {
        rawPlugin = createFunc();
    } catch (const std::exception& e) {
        unloadLibrary(handle);
        auto err = error::pluginLoadFailed(
            path.string(),
            std::string("Plugin creation failed: ") + e.what());
        emitEvent(createEvent(PluginLoadEventType::LoadFailed,
                              path.stem().string(), path, err.message));
        loadFailures_++;
        return std::unexpected(err);
    }

    if (!rawPlugin) {
        unloadLibrary(handle);
        auto err = error::pluginLoadFailed(path.string(),
                                           "createDevicePlugin returned null");
        emitEvent(createEvent(PluginLoadEventType::LoadFailed,
                              path.stem().string(), path, err.message));
        loadFailures_++;
        return std::unexpected(err);
    }

    // Wrap in shared_ptr with custom deleter
    auto destroyFunc =
        getFunction<DestroyDevicePluginFunc>(handle, "destroyDevicePlugin");
    std::shared_ptr<IDevicePlugin> plugin(
        rawPlugin, [destroyFunc](IDevicePlugin* p) {
            if (destroyFunc) {
                destroyFunc(p);
            } else {
                delete p;
            }
        });

    // Get plugin name
    std::string pluginName = plugin->getMetadata().name;

    // Check if already loaded
    {
        std::shared_lock lock(mutex_);
        if (loadedPlugins_.contains(pluginName)) {
            unloadLibrary(handle);
            auto err = error::pluginError(pluginName, "Plugin already loaded");
            emitEvent(createEvent(PluginLoadEventType::LoadFailed, pluginName,
                                  path, err.message));
            loadFailures_++;
            return std::unexpected(err);
        }
    }

    // Initialize the plugin
    if (!plugin->initialize(config)) {
        unloadLibrary(handle);
        auto err = error::pluginLoadFailed(
            pluginName, "Initialization failed: " + plugin->getLastError());
        emitEvent(createEvent(PluginLoadEventType::LoadFailed, pluginName, path,
                              err.message));
        loadFailures_++;
        return std::unexpected(err);
    }

    // Register types and creators
    auto registerResult = registerPluginTypes(plugin.get());
    if (!registerResult) {
        plugin->shutdown();
        unloadLibrary(handle);
        emitEvent(createEvent(PluginLoadEventType::LoadFailed, pluginName, path,
                              registerResult.error().message));
        loadFailures_++;
        return std::unexpected(registerResult.error());
    }

    // Store plugin info
    {
        std::unique_lock lock(mutex_);

        LoadedPluginInfo info;
        info.name = pluginName;
        info.path = path;
        info.plugin = plugin;
        info.handle = handle;
        info.loadedAt = std::chrono::system_clock::now();
        info.isBuiltIn = false;

        loadedPlugins_[pluginName] = std::move(info);
    }

    totalLoads_++;

    emitEvent(createEvent(PluginLoadEventType::Loaded, pluginName, path,
                          "Plugin loaded successfully"));

    LOG_F(INFO, "Loaded device plugin: %s from %s", pluginName.c_str(),
          path.string().c_str());

    return true;
}

auto DevicePluginLoader::loadPluginByName(const std::string& name,
                                          const nlohmann::json& config)
    -> DeviceResult<bool> {
    std::shared_lock lock(mutex_);

    // Search in plugin paths
    for (const auto& basePath : pluginPaths_) {
        std::filesystem::path pluginPath =
            basePath / (name + getPluginExtension());
        if (std::filesystem::exists(pluginPath)) {
            lock.unlock();
            return loadPlugin(pluginPath, config);
        }
    }

    return std::unexpected(error::pluginNotFound(name));
}

auto DevicePluginLoader::loadAllPlugins(
    const std::unordered_map<std::string, nlohmann::json>& configs) -> size_t {
    auto discovered = discoverPlugins();
    size_t loaded = 0;

    for (const auto& result : discovered) {
        if (!result.isDevicePlugin || !result.error.empty()) {
            continue;
        }

        nlohmann::json config;
        auto it = configs.find(result.name);
        if (it != configs.end()) {
            config = it->second;
        }

        auto loadResult = loadPlugin(result.path, config);
        if (loadResult) {
            loaded++;
        }
    }

    return loaded;
}

auto DevicePluginLoader::registerBuiltInPlugin(
    std::shared_ptr<IDevicePlugin> plugin, const nlohmann::json& config)
    -> DeviceResult<bool> {
    if (!plugin) {
        return std::unexpected(
            error::invalidArgument("plugin", "Plugin is null"));
    }

    std::string pluginName = plugin->getMetadata().name;

    // Check if already loaded
    {
        std::shared_lock lock(mutex_);
        if (loadedPlugins_.contains(pluginName)) {
            return std::unexpected(
                error::pluginError(pluginName, "Plugin already loaded"));
        }
    }

    // Initialize the plugin
    if (!plugin->initialize(config)) {
        return std::unexpected(error::pluginLoadFailed(
            pluginName, "Initialization failed: " + plugin->getLastError()));
    }

    // Register types and creators
    auto registerResult = registerPluginTypes(plugin.get());
    if (!registerResult) {
        plugin->shutdown();
        return std::unexpected(registerResult.error());
    }

    // Store plugin info
    {
        std::unique_lock lock(mutex_);

        LoadedPluginInfo info;
        info.name = pluginName;
        info.plugin = plugin;
        info.handle = nullptr;
        info.loadedAt = std::chrono::system_clock::now();
        info.isBuiltIn = true;

        loadedPlugins_[pluginName] = std::move(info);
    }

    totalLoads_++;

    LOG_F(INFO, "Registered built-in device plugin: %s", pluginName.c_str());

    return true;
}

auto DevicePluginLoader::unloadPlugin(const std::string& name)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    auto it = loadedPlugins_.find(name);
    if (it == loadedPlugins_.end()) {
        return std::unexpected(error::pluginNotFound(name));
    }

    auto& info = it->second;

    emitEvent(createEvent(PluginLoadEventType::Unloading, name, info.path,
                          "Unloading plugin"));

    // Unregister types and creators
    unregisterPluginTypes(info.plugin.get());

    // Shutdown the plugin
    info.plugin->shutdown();

    // Store handle before erasing
    void* handle = info.handle;
    std::filesystem::path path = info.path;

    // Clear the shared_ptr (triggers custom deleter)
    info.plugin.reset();

    // Remove from map
    loadedPlugins_.erase(it);

    // Unload library if not built-in
    if (handle) {
        unloadLibrary(handle);
    }

    totalUnloads_++;

    emitEvent(createEvent(PluginLoadEventType::Unloaded, name, path,
                          "Plugin unloaded successfully"));

    LOG_F(INFO, "Unloaded device plugin: %s", name.c_str());

    return true;
}

auto DevicePluginLoader::unloadAllPlugins() -> size_t {
    std::vector<std::string> names;

    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, info] : loadedPlugins_) {
            names.push_back(name);
        }
    }

    size_t unloaded = 0;
    for (const auto& name : names) {
        if (unloadPlugin(name)) {
            unloaded++;
        }
    }

    return unloaded;
}

auto DevicePluginLoader::reloadPlugin(const std::string& name,
                                      const nlohmann::json& config)
    -> DeviceResult<bool> {
    std::filesystem::path pluginPath;
    std::shared_ptr<IDevicePlugin> oldPlugin;

    // Get plugin info
    {
        std::shared_lock lock(mutex_);
        auto it = loadedPlugins_.find(name);
        if (it == loadedPlugins_.end()) {
            return std::unexpected(error::pluginNotFound(name));
        }

        if (it->second.isBuiltIn) {
            return std::unexpected(
                error::pluginError(name, "Cannot reload built-in plugins"));
        }

        pluginPath = it->second.path;
        oldPlugin = it->second.plugin;
    }

    // Check hot-plug support
    if (!oldPlugin->supportsHotPlug()) {
        return std::unexpected(
            error::pluginError(name, "Plugin does not support hot-plug"));
    }

    // Set hot-plug flag
    hotPlugInProgress_ = true;
    hotPlugPluginName_ = name;

    emitEvent(createEvent(PluginLoadEventType::Reloading, name, pluginPath,
                          "Starting hot-plug reload"));

    // Step 1: Prepare hot-plug (save device states)
    auto prepareResult = oldPlugin->prepareHotPlug();
    if (!prepareResult) {
        hotPlugInProgress_ = false;
        emitEvent(createEvent(PluginLoadEventType::ReloadFailed, name,
                              pluginPath,
                              "Failed to prepare: " +
                                  prepareResult.error().message));
        return std::unexpected(prepareResult.error());
    }
    hotPlugMigrationContexts_ = *prepareResult;

    // Step 2: Unload old plugin
    auto unloadResult = unloadPlugin(name);
    if (!unloadResult) {
        // Try to abort hot-plug
        oldPlugin->abortHotPlug(hotPlugMigrationContexts_);
        hotPlugInProgress_ = false;
        hotPlugMigrationContexts_.clear();
        emitEvent(createEvent(PluginLoadEventType::ReloadFailed, name,
                              pluginPath,
                              "Failed to unload: " +
                                  unloadResult.error().message));
        return std::unexpected(unloadResult.error());
    }

    // Step 3: Load new plugin
    nlohmann::json loadConfig = config.empty() ? oldPlugin->getConfig() : config;
    auto loadResult = loadPlugin(pluginPath, loadConfig);
    if (!loadResult) {
        hotPlugInProgress_ = false;
        hotPlugMigrationContexts_.clear();
        emitEvent(createEvent(PluginLoadEventType::ReloadFailed, name,
                              pluginPath,
                              "Failed to reload: " +
                                  loadResult.error().message));
        return std::unexpected(loadResult.error());
    }

    // Step 4: Complete hot-plug (restore device states)
    std::shared_ptr<IDevicePlugin> newPlugin;
    {
        std::shared_lock lock(mutex_);
        auto it = loadedPlugins_.find(name);
        if (it != loadedPlugins_.end()) {
            newPlugin = it->second.plugin;
        }
    }

    if (newPlugin) {
        auto completeResult =
            newPlugin->completeHotPlug(hotPlugMigrationContexts_);
        if (!completeResult) {
            LOG_F(WARNING,
                  "Hot-plug completion warning for %s: %s (devices may need "
                  "manual reconnection)",
                  name.c_str(), completeResult.error().message.c_str());
        }

        // Update reload count
        {
            std::unique_lock lock(mutex_);
            auto it = loadedPlugins_.find(name);
            if (it != loadedPlugins_.end()) {
                it->second.reloadCount++;
            }
        }
    }

    hotPlugInProgress_ = false;
    hotPlugMigrationContexts_.clear();
    totalReloads_++;

    emitEvent(createEvent(PluginLoadEventType::Reloaded, name, pluginPath,
                          "Hot-plug reload completed"));

    LOG_F(INFO, "Hot-plug reload completed for plugin: %s", name.c_str());

    return true;
}

auto DevicePluginLoader::isHotPlugInProgress() const -> bool {
    return hotPlugInProgress_;
}

auto DevicePluginLoader::getHotPlugStatus() const -> nlohmann::json {
    nlohmann::json status;
    status["inProgress"] = hotPlugInProgress_.load();
    status["pluginName"] = hotPlugPluginName_;
    status["migrationCount"] = hotPlugMigrationContexts_.size();
    return status;
}

auto DevicePluginLoader::isPluginLoaded(const std::string& name) const -> bool {
    std::shared_lock lock(mutex_);
    return loadedPlugins_.contains(name);
}

auto DevicePluginLoader::getPlugin(const std::string& name) const
    -> std::shared_ptr<IDevicePlugin> {
    std::shared_lock lock(mutex_);
    auto it = loadedPlugins_.find(name);
    if (it != loadedPlugins_.end()) {
        return it->second.plugin;
    }
    return nullptr;
}

auto DevicePluginLoader::getLoadedPlugins() const
    -> std::unordered_map<std::string, LoadedPluginInfo> {
    std::shared_lock lock(mutex_);
    return loadedPlugins_;
}

auto DevicePluginLoader::getPluginNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(loadedPlugins_.size());
    for (const auto& [name, info] : loadedPlugins_) {
        names.push_back(name);
    }
    return names;
}

auto DevicePluginLoader::getPluginsByTag(const std::string& tag) const
    -> std::vector<std::shared_ptr<IDevicePlugin>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<IDevicePlugin>> result;

    for (const auto& [name, info] : loadedPlugins_) {
        const auto& tags = info.plugin->getMetadata().tags;
        if (std::find(tags.begin(), tags.end(), tag) != tags.end()) {
            result.push_back(info.plugin);
        }
    }

    return result;
}

auto DevicePluginLoader::getPluginsByCapability(
    const std::string& capability) const
    -> std::vector<std::shared_ptr<IDevicePlugin>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<IDevicePlugin>> result;

    for (const auto& [name, info] : loadedPlugins_) {
        if (info.plugin->getMetadata().hasCapability(capability)) {
            result.push_back(info.plugin);
        }
    }

    return result;
}

void DevicePluginLoader::setTypeRegistry(DeviceTypeRegistry* registry) {
    std::unique_lock lock(mutex_);
    typeRegistry_ = registry;
}

void DevicePluginLoader::setDeviceFactory(DeviceFactory* factory) {
    std::unique_lock lock(mutex_);
    deviceFactory_ = factory;
}

auto DevicePluginLoader::subscribe(PluginLoadEventCallback callback)
    -> uint64_t {
    std::unique_lock lock(eventMutex_);
    uint64_t id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void DevicePluginLoader::unsubscribe(uint64_t subscriptionId) {
    std::unique_lock lock(eventMutex_);
    eventSubscribers_.erase(subscriptionId);
}

auto DevicePluginLoader::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json stats;
    stats["loadedPlugins"] = loadedPlugins_.size();
    stats["totalLoads"] = totalLoads_;
    stats["totalUnloads"] = totalUnloads_;
    stats["totalReloads"] = totalReloads_;
    stats["loadFailures"] = loadFailures_;
    stats["pluginPaths"] = nlohmann::json::array();
    for (const auto& path : pluginPaths_) {
        stats["pluginPaths"].push_back(path.string());
    }

    return stats;
}

auto DevicePluginLoader::loadLibrary(const std::filesystem::path& path)
    -> DeviceResult<void*> {
#ifdef _WIN32
    void* handle = LoadLibraryW(path.c_str());
    if (!handle) {
        DWORD error = GetLastError();
        return std::unexpected(error::pluginLoadFailed(
            path.string(), "LoadLibrary failed with error " +
                               std::to_string(error)));
    }
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        return std::unexpected(error::pluginLoadFailed(
            path.string(), std::string("dlopen failed: ") + dlerror()));
    }
#endif

    return handle;
}

void DevicePluginLoader::unloadLibrary(void* handle) {
    if (!handle) {
        return;
    }

#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

template <typename Func>
auto DevicePluginLoader::getFunction(void* handle, const char* name) -> Func {
#ifdef _WIN32
    return reinterpret_cast<Func>(
        GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return reinterpret_cast<Func>(dlsym(handle, name));
#endif
}

auto DevicePluginLoader::registerPluginTypes(IDevicePlugin* plugin)
    -> DeviceResult<bool> {
    if (!typeRegistry_) {
        LOG_F(WARNING, "No type registry set, skipping type registration");
    } else {
        auto result = plugin->registerDeviceTypes(*typeRegistry_);
        if (!result) {
            return std::unexpected(result.error());
        }
        LOG_F(INFO, "Registered %zu device types from plugin %s", *result,
              plugin->getMetadata().name.c_str());
    }

    if (!deviceFactory_) {
        LOG_F(WARNING, "No device factory set, skipping creator registration");
    } else {
        plugin->registerDeviceCreators(*deviceFactory_);
        LOG_F(INFO, "Registered device creators from plugin %s",
              plugin->getMetadata().name.c_str());
    }

    return true;
}

void DevicePluginLoader::unregisterPluginTypes(IDevicePlugin* plugin) {
    if (typeRegistry_) {
        size_t count = plugin->unregisterDeviceTypes(*typeRegistry_);
        LOG_F(INFO, "Unregistered %zu device types from plugin %s", count,
              plugin->getMetadata().name.c_str());
    }

    if (deviceFactory_) {
        plugin->unregisterDeviceCreators(*deviceFactory_);
        LOG_F(INFO, "Unregistered device creators from plugin %s",
              plugin->getMetadata().name.c_str());
    }
}

void DevicePluginLoader::emitEvent(const PluginLoadEvent& event) {
    std::shared_lock lock(eventMutex_);
    for (const auto& [id, callback] : eventSubscribers_) {
        try {
            callback(event);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

auto DevicePluginLoader::createEvent(PluginLoadEventType type,
                                      const std::string& pluginName,
                                      const std::filesystem::path& path,
                                      const std::string& message) const
    -> PluginLoadEvent {
    PluginLoadEvent event;
    event.type = type;
    event.pluginName = pluginName;
    event.pluginPath = path;
    event.message = message;
    event.timestamp = std::chrono::system_clock::now();
    return event;
}

auto DevicePluginLoader::getPluginExtension() -> std::string {
#ifdef _WIN32
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

auto DevicePluginLoader::hasValidExtension(const std::filesystem::path& path)
    -> bool {
    std::string ext = path.extension().string();
#ifdef _WIN32
    return ext == ".dll";
#elif defined(__APPLE__)
    return ext == ".dylib" || ext == ".so";
#else
    return ext == ".so";
#endif
}

// Explicit template instantiations
template auto DevicePluginLoader::getFunction<CreateDevicePluginFunc>(
    void*, const char*) -> CreateDevicePluginFunc;
template auto DevicePluginLoader::getFunction<DestroyDevicePluginFunc>(
    void*, const char*) -> DestroyDevicePluginFunc;
template auto DevicePluginLoader::getFunction<GetDevicePluginApiVersionFunc>(
    void*, const char*) -> GetDevicePluginApiVersionFunc;

}  // namespace lithium::device
