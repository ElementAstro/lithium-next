/*
 * solver_plugin_loader.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver plugin loader for dynamic plugin loading

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_PLUGIN_LOADER_HPP
#define LITHIUM_CLIENT_SOLVER_PLUGIN_LOADER_HPP

#include <filesystem>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

#include "solver_plugin_interface.hpp"

namespace lithium::solver {

/**
 * @brief Plugin load result
 */
struct PluginLoadResult {
    bool success{false};
    std::string error;
    SolverPluginMetadata metadata;

    explicit operator bool() const { return success; }
};

/**
 * @brief Loader for solver plugins
 *
 * Manages discovery, loading, and lifecycle of solver plugins.
 * Thread-safe singleton implementation.
 */
class SolverPluginLoader {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> SolverPluginLoader&;

    // Disable copy and move
    SolverPluginLoader(const SolverPluginLoader&) = delete;
    SolverPluginLoader& operator=(const SolverPluginLoader&) = delete;
    SolverPluginLoader(SolverPluginLoader&&) = delete;
    SolverPluginLoader& operator=(SolverPluginLoader&&) = delete;

    // ==================== Plugin Loading ====================

    /**
     * @brief Load a plugin from path
     * @param path Path to plugin library
     * @return Load result
     */
    auto loadPlugin(const std::filesystem::path& path) -> PluginLoadResult;

    /**
     * @brief Load a plugin from path with configuration
     * @param path Path to plugin library
     * @param config Plugin configuration
     * @return Load result
     */
    auto loadPlugin(const std::filesystem::path& path,
                    const nlohmann::json& config) -> PluginLoadResult;

    /**
     * @brief Unload a plugin by name
     * @param pluginName Plugin name
     * @return true if unloaded successfully
     */
    auto unloadPlugin(const std::string& pluginName) -> bool;

    /**
     * @brief Reload a plugin
     * @param pluginName Plugin name
     * @return Load result
     */
    auto reloadPlugin(const std::string& pluginName) -> PluginLoadResult;

    // ==================== Plugin Discovery ====================

    /**
     * @brief Discover plugins in a directory
     * @param directory Directory to search
     * @return Vector of plugin paths
     */
    [[nodiscard]] auto discoverPlugins(const std::filesystem::path& directory)
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Load all plugins from a directory
     * @param directory Directory to search
     * @return Number of plugins loaded
     */
    auto loadAllPlugins(const std::filesystem::path& directory) -> size_t;

    /**
     * @brief Load all plugins from a directory with config
     * @param directory Directory to search
     * @param config Configuration for all plugins
     * @return Number of plugins loaded
     */
    auto loadAllPlugins(const std::filesystem::path& directory,
                        const nlohmann::json& config) -> size_t;

    /**
     * @brief Add a search path for plugins
     * @param path Search path to add
     */
    void addSearchPath(const std::filesystem::path& path);

    /**
     * @brief Get all search paths
     * @return Vector of search paths
     */
    [[nodiscard]] auto getSearchPaths() const
        -> std::vector<std::filesystem::path>;

    // ==================== Plugin Query ====================

    /**
     * @brief Get a loaded plugin by name
     * @param name Plugin name
     * @return Plugin pointer or nullptr
     */
    [[nodiscard]] auto getPlugin(const std::string& name) const
        -> std::shared_ptr<ISolverPlugin>;

    /**
     * @brief Get all loaded plugins
     * @return Vector of plugins
     */
    [[nodiscard]] auto getAllPlugins() const
        -> std::vector<std::shared_ptr<ISolverPlugin>>;

    /**
     * @brief Get names of loaded plugins
     * @return Vector of plugin names
     */
    [[nodiscard]] auto getPluginNames() const -> std::vector<std::string>;

    /**
     * @brief Check if a plugin is loaded
     * @param name Plugin name
     * @return true if loaded
     */
    [[nodiscard]] auto isPluginLoaded(const std::string& name) const -> bool;

    /**
     * @brief Get number of loaded plugins
     * @return Plugin count
     */
    [[nodiscard]] auto getPluginCount() const -> size_t;

    // ==================== Plugin Metadata ====================

    /**
     * @brief Get metadata for a plugin without loading it
     * @param path Path to plugin library
     * @return Metadata if available
     */
    [[nodiscard]] auto probePlugin(const std::filesystem::path& path)
        -> std::optional<SolverPluginMetadata>;

    /**
     * @brief Get API version from a plugin without loading it
     * @param path Path to plugin library
     * @return API version or -1 if not available
     */
    [[nodiscard]] auto getPluginApiVersion(const std::filesystem::path& path)
        -> int;

    // ==================== Lifecycle ====================

    /**
     * @brief Initialize all loaded plugins
     * @param config Common configuration
     * @return Number of plugins initialized successfully
     */
    auto initializeAll(const nlohmann::json& config = {}) -> size_t;

    /**
     * @brief Shutdown all loaded plugins
     */
    void shutdownAll();

    /**
     * @brief Unload all plugins
     */
    void unloadAll();

    // ==================== Statistics ====================

    /**
     * @brief Get loader statistics
     * @return Statistics as JSON
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

private:
    SolverPluginLoader() = default;
    ~SolverPluginLoader();

    /**
     * @brief Information about a loaded plugin
     */
    struct LoadedPluginInfo {
        std::shared_ptr<ISolverPlugin> plugin;
        std::filesystem::path path;
        void* libraryHandle{nullptr};
        SolverPluginMetadata metadata;
        bool initialized{false};
    };

    /**
     * @brief Get platform-specific library extension
     */
    [[nodiscard]] static auto getLibraryExtension() -> std::string;

    /**
     * @brief Load a dynamic library
     */
    auto loadLibrary(const std::filesystem::path& path) -> void*;

    /**
     * @brief Unload a dynamic library
     */
    void unloadLibrary(void* handle);

    /**
     * @brief Get function from library
     */
    template <typename Func>
    auto getFunction(void* handle, const char* name) -> Func;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, LoadedPluginInfo> loadedPlugins_;
    std::vector<std::filesystem::path> searchPaths_;

    // Statistics
    size_t totalLoaded_{0};
    size_t totalUnloaded_{0};
    size_t loadFailures_{0};
};

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_PLUGIN_LOADER_HPP
