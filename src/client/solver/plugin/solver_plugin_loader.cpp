/*
 * solver_plugin_loader.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "solver_plugin_loader.hpp"

#include "../service/solver_factory.hpp"
#include "../service/solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace lithium::solver {

auto SolverPluginLoader::getInstance() -> SolverPluginLoader& {
    static SolverPluginLoader instance;
    return instance;
}

SolverPluginLoader::~SolverPluginLoader() {
    unloadAll();
}

auto SolverPluginLoader::loadPlugin(const std::filesystem::path& path)
    -> PluginLoadResult {
    return loadPlugin(path, nlohmann::json::object());
}

auto SolverPluginLoader::loadPlugin(const std::filesystem::path& path,
                                     const nlohmann::json& config)
    -> PluginLoadResult {
    PluginLoadResult result;

    // Check if file exists
    if (!std::filesystem::exists(path)) {
        result.error = "Plugin file not found: " + path.string();
        LOG_ERROR("{}", result.error);
        loadFailures_++;
        return result;
    }

    // Check API version first
    int apiVersion = getPluginApiVersion(path);
    if (apiVersion != SOLVER_PLUGIN_API_VERSION && apiVersion != -1) {
        result.error = "API version mismatch: expected " +
                       std::to_string(SOLVER_PLUGIN_API_VERSION) +
                       ", got " + std::to_string(apiVersion);
        LOG_ERROR("{}", result.error);
        loadFailures_++;
        return result;
    }

    // Load library
    void* handle = loadLibrary(path);
    if (!handle) {
        result.error = "Failed to load library: " + path.string();
        LOG_ERROR("{}", result.error);
        loadFailures_++;
        return result;
    }

    // Get create function
    auto createFunc = getFunction<CreateSolverPluginFunc>(handle, "createSolverPlugin");
    if (!createFunc) {
        result.error = "Missing createSolverPlugin function in " + path.string();
        LOG_ERROR("{}", result.error);
        unloadLibrary(handle);
        loadFailures_++;
        return result;
    }

    // Create plugin instance
    ISolverPlugin* rawPlugin = nullptr;
    try {
        rawPlugin = createFunc();
    } catch (const std::exception& e) {
        result.error = "Exception creating plugin: " + std::string(e.what());
        LOG_ERROR("{}", result.error);
        unloadLibrary(handle);
        loadFailures_++;
        return result;
    }

    if (!rawPlugin) {
        result.error = "createSolverPlugin returned null";
        LOG_ERROR("{}", result.error);
        unloadLibrary(handle);
        loadFailures_++;
        return result;
    }

    // Wrap in shared_ptr with custom deleter
    auto destroyFunc = getFunction<DestroySolverPluginFunc>(handle, "destroySolverPlugin");
    std::shared_ptr<ISolverPlugin> plugin;

    if (destroyFunc) {
        plugin = std::shared_ptr<ISolverPlugin>(rawPlugin,
            [destroyFunc](ISolverPlugin* p) {
                if (p) destroyFunc(p);
            });
    } else {
        plugin = std::shared_ptr<ISolverPlugin>(rawPlugin);
    }

    // Get metadata
    result.metadata = plugin->getSolverMetadata();
    std::string pluginName = result.metadata.name;

    // Check for duplicate
    {
        std::unique_lock lock(mutex_);
        if (loadedPlugins_.contains(pluginName)) {
            result.error = "Plugin already loaded: " + pluginName;
            LOG_WARN("{}", result.error);
            return result;
        }
    }

    // Initialize plugin
    if (!plugin->initialize(config)) {
        result.error = "Failed to initialize plugin: " + pluginName;
        LOG_ERROR("{}", result.error);
        loadFailures_++;
        return result;
    }

    // Register types and creators
    auto& registry = SolverTypeRegistry::getInstance();
    auto& factory = SolverFactory::getInstance();

    plugin->registerSolverTypes(registry);
    plugin->registerSolverCreators(factory);

    // Store plugin
    {
        std::unique_lock lock(mutex_);
        LoadedPluginInfo info;
        info.plugin = plugin;
        info.path = path;
        info.libraryHandle = handle;
        info.metadata = result.metadata;
        info.initialized = true;
        loadedPlugins_[pluginName] = std::move(info);
    }

    totalLoaded_++;
    result.success = true;

    LOG_INFO("Loaded solver plugin: {} v{} from {}",
             pluginName, result.metadata.version, path.string());

    return result;
}

auto SolverPluginLoader::unloadPlugin(const std::string& pluginName) -> bool {
    std::unique_lock lock(mutex_);

    auto it = loadedPlugins_.find(pluginName);
    if (it == loadedPlugins_.end()) {
        LOG_WARN("Plugin not found for unload: {}", pluginName);
        return false;
    }

    auto& info = it->second;

    // Unregister types and creators
    auto& registry = SolverTypeRegistry::getInstance();
    auto& factory = SolverFactory::getInstance();

    if (info.plugin) {
        info.plugin->unregisterSolverCreators(factory);
        info.plugin->unregisterSolverTypes(registry);
        info.plugin->shutdown();
    }

    // Save handle before erasing
    void* handle = info.libraryHandle;

    // Remove from map
    loadedPlugins_.erase(it);

    // Unload library
    if (handle) {
        unloadLibrary(handle);
    }

    totalUnloaded_++;

    LOG_INFO("Unloaded solver plugin: {}", pluginName);
    return true;
}

auto SolverPluginLoader::reloadPlugin(const std::string& pluginName)
    -> PluginLoadResult {
    PluginLoadResult result;

    std::filesystem::path pluginPath;
    nlohmann::json config;

    // Get current plugin info
    {
        std::shared_lock lock(mutex_);
        auto it = loadedPlugins_.find(pluginName);
        if (it == loadedPlugins_.end()) {
            result.error = "Plugin not found: " + pluginName;
            return result;
        }
        pluginPath = it->second.path;
        if (it->second.plugin) {
            config = it->second.plugin->getConfig();
        }
    }

    // Unload
    if (!unloadPlugin(pluginName)) {
        result.error = "Failed to unload plugin for reload";
        return result;
    }

    // Reload
    return loadPlugin(pluginPath, config);
}

auto SolverPluginLoader::discoverPlugins(const std::filesystem::path& directory)
    -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> plugins;

    if (!std::filesystem::exists(directory)) {
        LOG_WARN("Plugin directory does not exist: {}", directory.string());
        return plugins;
    }

    std::string extension = getLibraryExtension();

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == extension) {
            plugins.push_back(entry.path());
        }
    }

    LOG_DEBUG("Discovered {} plugins in {}", plugins.size(), directory.string());
    return plugins;
}

auto SolverPluginLoader::loadAllPlugins(const std::filesystem::path& directory)
    -> size_t {
    return loadAllPlugins(directory, nlohmann::json::object());
}

auto SolverPluginLoader::loadAllPlugins(const std::filesystem::path& directory,
                                         const nlohmann::json& config)
    -> size_t {
    auto plugins = discoverPlugins(directory);
    size_t loaded = 0;

    for (const auto& path : plugins) {
        auto result = loadPlugin(path, config);
        if (result.success) {
            loaded++;
        }
    }

    LOG_INFO("Loaded {}/{} plugins from {}", loaded, plugins.size(),
             directory.string());
    return loaded;
}

void SolverPluginLoader::addSearchPath(const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);
    searchPaths_.push_back(path);
}

auto SolverPluginLoader::getSearchPaths() const
    -> std::vector<std::filesystem::path> {
    std::shared_lock lock(mutex_);
    return searchPaths_;
}

auto SolverPluginLoader::getPlugin(const std::string& name) const
    -> std::shared_ptr<ISolverPlugin> {
    std::shared_lock lock(mutex_);

    auto it = loadedPlugins_.find(name);
    if (it == loadedPlugins_.end()) {
        return nullptr;
    }
    return it->second.plugin;
}

auto SolverPluginLoader::getAllPlugins() const
    -> std::vector<std::shared_ptr<ISolverPlugin>> {
    std::shared_lock lock(mutex_);

    std::vector<std::shared_ptr<ISolverPlugin>> plugins;
    plugins.reserve(loadedPlugins_.size());

    for (const auto& [name, info] : loadedPlugins_) {
        plugins.push_back(info.plugin);
    }

    return plugins;
}

auto SolverPluginLoader::getPluginNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    std::vector<std::string> names;
    names.reserve(loadedPlugins_.size());

    for (const auto& [name, info] : loadedPlugins_) {
        names.push_back(name);
    }

    return names;
}

auto SolverPluginLoader::isPluginLoaded(const std::string& name) const -> bool {
    std::shared_lock lock(mutex_);
    return loadedPlugins_.contains(name);
}

auto SolverPluginLoader::getPluginCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return loadedPlugins_.size();
}

auto SolverPluginLoader::probePlugin(const std::filesystem::path& path)
    -> std::optional<SolverPluginMetadata> {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    void* handle = loadLibrary(path);
    if (!handle) {
        return std::nullopt;
    }

    std::optional<SolverPluginMetadata> result;

    auto metadataFunc = getFunction<GetSolverPluginMetadataFunc>(
        handle, "getSolverPluginMetadata");

    if (metadataFunc) {
        try {
            result = metadataFunc();
        } catch (...) {
            // Ignore errors
        }
    }

    unloadLibrary(handle);
    return result;
}

auto SolverPluginLoader::getPluginApiVersion(const std::filesystem::path& path)
    -> int {
    if (!std::filesystem::exists(path)) {
        return -1;
    }

    void* handle = loadLibrary(path);
    if (!handle) {
        return -1;
    }

    int version = -1;

    auto versionFunc = getFunction<GetSolverPluginApiVersionFunc>(
        handle, "getSolverPluginApiVersion");

    if (versionFunc) {
        try {
            version = versionFunc();
        } catch (...) {
            version = -1;
        }
    }

    unloadLibrary(handle);
    return version;
}

auto SolverPluginLoader::initializeAll(const nlohmann::json& config) -> size_t {
    std::shared_lock lock(mutex_);

    size_t initialized = 0;
    for (auto& [name, info] : loadedPlugins_) {
        if (!info.initialized && info.plugin) {
            if (info.plugin->initialize(config)) {
                info.initialized = true;
                initialized++;
            }
        }
    }

    return initialized;
}

void SolverPluginLoader::shutdownAll() {
    std::shared_lock lock(mutex_);

    for (auto& [name, info] : loadedPlugins_) {
        if (info.initialized && info.plugin) {
            info.plugin->shutdown();
            info.initialized = false;
        }
    }
}

void SolverPluginLoader::unloadAll() {
    std::vector<std::string> names;

    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, info] : loadedPlugins_) {
            names.push_back(name);
        }
    }

    for (const auto& name : names) {
        unloadPlugin(name);
    }
}

auto SolverPluginLoader::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    return nlohmann::json{
        {"loadedPlugins", loadedPlugins_.size()},
        {"totalLoaded", totalLoaded_},
        {"totalUnloaded", totalUnloaded_},
        {"loadFailures", loadFailures_},
        {"searchPaths", searchPaths_.size()}
    };
}

auto SolverPluginLoader::getLibraryExtension() -> std::string {
#ifdef _WIN32
    return ".dll";
#elif __APPLE__
    return ".dylib";
#else
    return ".so";
#endif
}

auto SolverPluginLoader::loadLibrary(const std::filesystem::path& path)
    -> void* {
#ifdef _WIN32
    return LoadLibraryW(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void SolverPluginLoader::unloadLibrary(void* handle) {
    if (!handle) return;

#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

template <typename Func>
auto SolverPluginLoader::getFunction(void* handle, const char* name) -> Func {
    if (!handle) return nullptr;

#ifdef _WIN32
    return reinterpret_cast<Func>(
        GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return reinterpret_cast<Func>(dlsym(handle, name));
#endif
}

// Explicit template instantiations
template CreateSolverPluginFunc
SolverPluginLoader::getFunction<CreateSolverPluginFunc>(void*, const char*);
template DestroySolverPluginFunc
SolverPluginLoader::getFunction<DestroySolverPluginFunc>(void*, const char*);
template GetSolverPluginApiVersionFunc
SolverPluginLoader::getFunction<GetSolverPluginApiVersionFunc>(void*, const char*);
template GetSolverPluginMetadataFunc
SolverPluginLoader::getFunction<GetSolverPluginMetadataFunc>(void*, const char*);

}  // namespace lithium::solver
