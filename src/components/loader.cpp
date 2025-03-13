/*
 * loader.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: C++20 and Modules Loader

**************************************************/

#include "loader.hpp"

#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>
#include <memory>
#include <thread>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#else
#include <dlfcn.h>
#include <link.h>
#endif

#include "atom/algorithm/hash.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/utils/to_string.hpp"

namespace fs = std::filesystem;
namespace rx = std::ranges;
namespace ex = std::execution;

namespace lithium {

ModuleLoader::ModuleLoader(std::string_view dirName)
    : threadPool_(std::make_shared<atom::async::ThreadPool<>>(
          std::thread::hardware_concurrency())),
      modulesDir_(dirName) {
    DLOG_F(INFO, "Module manager initialized with directory: {}", dirName);
}

ModuleLoader::~ModuleLoader() {
    DLOG_F(INFO, "Module manager destroying...");
    try {
        auto result = unloadAllModules();
        if (!result) {
            LOG_F(ERROR, "Failed to unload all modules: {}", result.error());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during ModuleLoader destruction: {}", e.what());
    }
}

auto ModuleLoader::createShared() -> std::shared_ptr<ModuleLoader> {
    DLOG_F(INFO, "Creating shared ModuleLoader with default directory.");
    return std::make_shared<ModuleLoader>("modules");
}

auto ModuleLoader::createShared(std::string_view dirName)
    -> std::shared_ptr<ModuleLoader> {
    DLOG_F(INFO, "Creating shared ModuleLoader with directory: {}", dirName);
    return std::make_shared<ModuleLoader>(dirName);
}

auto ModuleLoader::loadModule(std::string_view path,
                              std::string_view name) -> ModuleResult<bool> {
    DLOG_F(INFO, "Loading module: {} from path: {}", name, path);

    if (name.empty() || path.empty()) {
        return std::unexpected("Module name or path cannot be empty");
    }

    std::unique_lock lock(sharedMutex_);

    if (hasModule(name)) {
        return std::unexpected("Module already loaded: " + toStdString(name));
    }

    fs::path modulePath(path);
    if (!fs::exists(modulePath)) {
        return std::unexpected("Module file not found: " + toStdString(path));
    }

    if (!verifyModuleIntegrity(modulePath)) {
        return std::unexpected("Module integrity check failed: " +
                               toStdString(path));
    }

    auto modInfo = std::make_shared<ModuleInfo>();
    try {
        modInfo->mLibrary =
            std::make_shared<atom::meta::DynamicLibrary>(toStdString(path));
        modInfo->path = toStdString(path);
        modInfo->hash = computeModuleHash(path);

        // Add to dependency graph
        dependencyGraph_.addNode(toStdString(name), Version());
        modules_.emplace(toStdString(name), modInfo);

        // Update statistics
        updateModuleStatistics(name);

        LOG_F(INFO, "Module {} loaded successfully", name);
        return true;
    } catch (const FFIException& ex) {
        LOG_F(ERROR, "FFI exception while loading module {}: {}", name,
              ex.what());
        return std::unexpected(std::string("FFI exception: ") + ex.what());
    } catch (const std::exception& ex) {
        LOG_F(ERROR, "Exception while loading module {}: {}", name, ex.what());
        return std::unexpected(std::string("Exception: ") + ex.what());
    }
}

auto ModuleLoader::unloadModule(std::string_view name) -> ModuleResult<bool> {
    DLOG_F(INFO, "Unloading module: {}", name);

    if (name.empty()) {
        return std::unexpected("Module name cannot be empty");
    }

    std::unique_lock lock(sharedMutex_);
    auto nameStr = toStdString(name);
    auto iter = modules_.find(nameStr);
    if (iter != modules_.end()) {
        try {
            // Check if module has dependent modules
            auto dependents = dependencyGraph_.getDependents(nameStr);
            if (!dependents.empty()) {
                return std::unexpected(
                    "Cannot unload module with dependents: " +
                    atom::utils::toString(dependents));
            }

            // Remove from dependency graph first
            dependencyGraph_.removeNode(nameStr);

            // Reset the library pointer before erasing from map
            iter->second->mLibrary.reset();
            modules_.erase(iter);

            LOG_F(INFO, "Module {} unloaded successfully", name);
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception while unloading module {}: {}", name,
                  e.what());
            return std::unexpected(std::string("Exception: ") + e.what());
        }
    }

    return std::unexpected("Module not loaded: " + toStdString(name));
}

auto ModuleLoader::unloadAllModules() -> ModuleResult<bool> {
    DLOG_F(INFO, "Unloading all modules.");
    std::unique_lock lock(sharedMutex_);

    try {
        // Get modules in reverse dependency order
        auto sortedModules = dependencyGraph_.topologicalSort();
        if (sortedModules) {
            // Unload in reverse order
            std::reverse(sortedModules->begin(), sortedModules->end());
            for (const auto& name : *sortedModules) {
                auto iter = modules_.find(name);
                if (iter != modules_.end()) {
                    iter->second->mLibrary.reset();
                }
            }
        }

        modules_.clear();
        dependencyGraph_.clear();
        LOG_F(INFO, "All modules unloaded");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception while unloading all modules: {}", e.what());
        return std::unexpected(std::string("Exception: ") + e.what());
    }
}

auto ModuleLoader::hasModule(std::string_view name) const -> bool {
    if (name.empty()) {
        return false;
    }

    std::shared_lock lock(sharedMutex_);
    bool result = modules_.contains(toStdString(name));
    DLOG_F(INFO, "Module {} is {}loaded.", name, result ? "" : "not ");
    return result;
}

auto ModuleLoader::getModule(std::string_view name) const
    -> std::shared_ptr<ModuleInfo> {
    DLOG_F(INFO, "Getting module: {}", name);

    if (name.empty()) {
        DLOG_F(WARNING, "Empty module name provided");
        return nullptr;
    }

    std::shared_lock lock(sharedMutex_);
    auto iter = modules_.find(toStdString(name));
    if (iter != modules_.end()) {
        DLOG_F(INFO, "Module {} found.", name);
        return iter->second;
    }
    DLOG_F(INFO, "Module {} not found.", name);
    return nullptr;
}

auto ModuleLoader::enableModule(std::string_view name) -> ModuleResult<bool> {
    DLOG_F(INFO, "Enabling module: {}", name);

    if (name.empty()) {
        return std::unexpected("Module name cannot be empty");
    }

    std::unique_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (!mod) {
        return std::unexpected("Module not found: " + toStdString(name));
    }

    if (!mod->enabled.load()) {
        // Check dependencies before enabling
        if (!validateDependencies(name)) {
            return std::unexpected("Dependencies not satisfied for module: " +
                                   toStdString(name));
        }

        mod->enabled.store(true);
        mod->currentStatus = ModuleInfo::Status::ACTIVE;
        LOG_F(INFO, "Module {} enabled.", name);
        return true;
    }

    LOG_F(WARNING, "Module {} is already enabled.", name);
    return std::unexpected("Module is already enabled: " + toStdString(name));
}

auto ModuleLoader::disableModule(std::string_view name) -> ModuleResult<bool> {
    DLOG_F(INFO, "Disabling module: {}", name);

    if (name.empty()) {
        return std::unexpected("Module name cannot be empty");
    }

    std::unique_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (!mod) {
        return std::unexpected("Module not found: " + toStdString(name));
    }

    if (mod->enabled.load()) {
        // Check if this module is required by other enabled modules
        auto dependents = dependencyGraph_.getDependents(toStdString(name));
        if (!dependents.empty()) {
            // Filter to only enabled dependents
            std::vector<std::string> enabledDependents;
            for (const auto& dep : dependents) {
                if (isModuleEnabled(dep)) {
                    enabledDependents.push_back(dep);
                }
            }

            if (!enabledDependents.empty()) {
                return std::unexpected(
                    "Cannot disable module required by: " +
                    atom::utils::toString(enabledDependents));
            }
        }

        mod->enabled.store(false);
        mod->currentStatus = ModuleInfo::Status::LOADED;
        LOG_F(INFO, "Module {} disabled.", name);
        return true;
    }

    LOG_F(WARNING, "Module {} is already disabled.", name);
    return std::unexpected("Module is already disabled: " + toStdString(name));
}

auto ModuleLoader::isModuleEnabled(std::string_view name) const -> bool {
    DLOG_F(INFO, "Checking if module {} is enabled.", name);

    if (name.empty()) {
        return false;
    }

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    bool result = mod && mod->enabled.load();
    DLOG_F(INFO, "Module {} is {}enabled.", name, result ? "" : "not ");
    return result;
}

auto ModuleLoader::getAllExistedModules() const -> std::vector<std::string> {
    DLOG_F(INFO, "Getting all loaded modules.");
    std::shared_lock lock(sharedMutex_);

    std::vector<std::string> moduleNames;
    moduleNames.reserve(modules_.size());

    // Use C++20 ranges to transform the map to a vector of names
    auto keys = modules_ | std::views::keys;
    std::ranges::copy(keys, std::back_inserter(moduleNames));

    // Sort for consistent results
    std::ranges::sort(moduleNames);

    DLOG_F(INFO, "Loaded modules: {}", atom::utils::toString(moduleNames));
    return moduleNames;
}

auto ModuleLoader::hasFunction(std::string_view name,
                               std::string_view functionName) const -> bool {
    DLOG_F(INFO, "Checking if module {} has function: {}", name, functionName);

    if (name.empty() || functionName.empty()) {
        return false;
    }

    std::shared_lock lock(sharedMutex_);
    auto iter = modules_.find(toStdString(name));
    if (iter == modules_.end() || !iter->second->mLibrary) {
        return false;
    }

    try {
        bool result =
            iter->second->mLibrary->hasFunction(toStdString(functionName));
        DLOG_F(INFO, "Module {} {} function: {}", name,
               result ? "has" : "does not have", functionName);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception checking function {} in module {}: {}",
              functionName, name, e.what());
        return false;
    }
}

auto ModuleLoader::reloadModule(std::string_view name) -> ModuleResult<bool> {
    DLOG_F(INFO, "Reloading module: {}", name);

    if (name.empty()) {
        return std::unexpected("Module name cannot be empty");
    }

    std::unique_lock lock(sharedMutex_);

    // Store path before unloading
    std::string path;
    auto mod = getModule(name);
    if (!mod) {
        return std::unexpected("Module not found: " + toStdString(name));
    }

    path = mod->path;

    // Release lock and unload module
    lock.unlock();
    auto unloadResult = unloadModule(name);
    if (!unloadResult) {
        return std::unexpected("Failed to unload module for reload: " +
                               unloadResult.error());
    }

    // Load the module again
    return loadModule(path, name);
}

auto ModuleLoader::getModuleStatus(std::string_view name) const
    -> ModuleInfo::Status {
    DLOG_F(INFO, "Getting status for module: {}", name);

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    auto status = mod ? mod->currentStatus : ModuleInfo::Status::UNLOADED;

    DLOG_F(INFO, "Module {} status: {}", name, static_cast<int>(status));
    return status;
}

auto ModuleLoader::getModuleStatistics(std::string_view name) const
    -> ModuleInfo::Statistics {
    DLOG_F(INFO, "Getting statistics for module: {}", name);

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (!mod) {
        LOG_F(WARNING, "Tried to get statistics for non-existent module: {}",
              name);
        return {};
    }

    return mod->stats;
}

auto ModuleLoader::setModulePriority(std::string_view name,
                                     int priority) -> ModuleResult<bool> {
    DLOG_F(INFO, "Setting priority for module {} to {}", name, priority);

    if (name.empty()) {
        return std::unexpected("Module name cannot be empty");
    }

    if (priority < 0) {
        return std::unexpected("Priority cannot be negative");
    }

    std::unique_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (!mod) {
        return std::unexpected("Module not found: " + toStdString(name));
    }

    mod->priority = priority;
    LOG_F(INFO, "Priority for module {} set to {}", name, priority);
    return true;
}

auto ModuleLoader::validateDependencies(std::string_view name) const -> bool {
    DLOG_F(INFO, "Validating dependencies for module: {}", name);

    if (name.empty()) {
        LOG_F(WARNING, "Empty module name provided for dependency validation");
        return false;
    }

    return dependencyGraph_.validateDependencies(toStdString(name));
}

auto ModuleLoader::loadModulesInOrder() -> ModuleResult<bool> {
    DLOG_F(INFO, "Loading modules in dependency order");

    try {
        auto sortedModulesOpt = dependencyGraph_.topologicalSort();
        if (!sortedModulesOpt) {
            return std::unexpected(
                "Failed to sort modules due to circular dependencies");
        }

        auto& sortedModules = *sortedModulesOpt;
        std::vector<std::string> failedModules;

        // Lock for checking modules
        std::shared_lock readLock(sharedMutex_);
        std::vector<std::pair<std::string, std::string>> modulesToLoad;

        for (const auto& name : sortedModules) {
            auto mod = getModule(name);
            if (mod) {
                modulesToLoad.emplace_back(mod->path, name);
            }
        }
        readLock.unlock();

        // Load modules in parallel but respect dependencies
        auto futures = loadModulesAsync(modulesToLoad);
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();
            if (!result) {
                failedModules.push_back(modulesToLoad[i].second);
                LOG_F(ERROR, "Failed to load module {}: {}",
                      modulesToLoad[i].second, result.error());
            }
        }

        if (!failedModules.empty()) {
            return std::unexpected("Failed to load modules: " +
                                   atom::utils::toString(failedModules));
        }

        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during module loading in order: {}", e.what());
        return std::unexpected(std::string("Exception: ") + e.what());
    }
}

auto ModuleLoader::getDependencies(std::string_view name) const
    -> std::vector<std::string> {
    DLOG_F(INFO, "Getting dependencies for module: {}", name);

    if (name.empty()) {
        LOG_F(WARNING, "Empty module name provided for dependency retrieval");
        return {};
    }

    return dependencyGraph_.getDependencies(toStdString(name));
}

void ModuleLoader::setThreadPoolSize(size_t size) {
    DLOG_F(INFO, "Setting thread pool size to {}", size);

    if (size == 0) {
        LOG_F(ERROR, "Thread pool size cannot be zero");
        throw std::invalid_argument("Thread pool size cannot be zero");
    }

    threadPool_ = std::make_shared<atom::async::ThreadPool<>>(size);
}

auto ModuleLoader::loadModulesAsync(
    std::span<const std::pair<std::string, std::string>> modules)
    -> std::vector<std::future<ModuleResult<bool>>> {
    DLOG_F(INFO, "Asynchronously loading {} modules", modules.size());

    std::vector<std::future<ModuleResult<bool>>> results;
    results.reserve(modules.size());

    // Add all modules to dependency graph first
    {
        std::unique_lock lock(sharedMutex_);
        for (const auto& [_, name] : modules) {
            if (!dependencyGraph_.hasNode(name)) {
                dependencyGraph_.addNode(name, Version());
            }
        }
    }

    // Asynchronously load modules
    for (const auto& [path, name] : modules) {
        results.push_back(loadModuleAsync(path, name));
    }

    return results;
}

auto ModuleLoader::getModuleByHash(std::size_t hash) const
    -> std::shared_ptr<ModuleInfo> {
    DLOG_F(INFO, "Looking for module with hash: {}", hash);

    std::shared_lock lock(sharedMutex_);

    // Use C++20 ranges to find module with matching hash
    auto it = std::ranges::find_if(modules_, [hash](const auto& pair) {
        return pair.second && pair.second->hash == hash;
    });

    if (it != modules_.end()) {
        DLOG_F(INFO, "Found module {} with matching hash", it->first);
        return it->second;
    }

    DLOG_F(INFO, "No module found with hash {}", hash);
    return nullptr;
}

auto ModuleLoader::computeModuleHash(std::string_view path) const
    -> std::size_t {
    try {
        return atom::algorithm::computeHash(toStdString(path));
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception computing hash for {}: {}", path, e.what());
        return 0;
    }
}

auto ModuleLoader::loadModuleAsync(std::string_view path, std::string_view name)
    -> std::future<ModuleResult<bool>> {
    return threadPool_->enqueue(
        [this, pathStr = toStdString(path), nameStr = toStdString(name)]() {
            auto startTime = std::chrono::system_clock::now();
            auto result = loadModule(pathStr, nameStr);
            auto endTime = std::chrono::system_clock::now();

            if (result) {
                std::unique_lock lock(sharedMutex_);
                if (auto modInfo = getModule(nameStr)) {
                    modInfo->stats.loadCount++;

                    auto duration =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            endTime - startTime);

                    // Update average load time using weighted average
                    modInfo->stats.averageLoadTime =
                        (modInfo->stats.averageLoadTime *
                             (modInfo->stats.loadCount - 1) +
                         duration.count()) /
                        modInfo->stats.loadCount;

                    modInfo->stats.lastAccess = endTime;
                }
            } else {
                // Update failure statistics
                std::unique_lock lock(sharedMutex_);
                if (auto modInfo = getModule(nameStr)) {
                    modInfo->stats.failureCount++;
                }
            }

            return result;
        });
}

auto ModuleLoader::verifyModuleIntegrity(
    const std::filesystem::path& path) const -> bool {
    DLOG_F(INFO, "Verifying integrity of module: {}", path.string());

    try {
        // Basic file checks
        if (!fs::exists(path)) {
            LOG_F(ERROR, "Module file does not exist: {}", path.string());
            return false;
        }

        if (fs::is_empty(path)) {
            LOG_F(ERROR, "Module file is empty: {}", path.string());
            return false;
        }

        if (fs::file_size(path) <
            1024) {  // Minimum reasonable size for a shared library
            LOG_F(WARNING, "Module file is suspiciously small: {} ({} bytes)",
                  path.string(), fs::file_size(path));
        }

// Platform-specific checks could be added here
#ifdef _WIN32
// Windows PE file format check
// This would require more detailed implementation
#else
// ELF format check for Linux/Unix
// This would require more detailed implementation
#endif

        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during module integrity check for {}: {}",
              path.string(), e.what());
        return false;
    }
}

auto ModuleLoader::updateModuleStatistics(std::string_view name) -> void {
    auto nameStr = toStdString(name);
    auto it = modules_.find(nameStr);
    if (it != modules_.end()) {
        it->second->stats.accessCount++;
        it->second->stats.lastAccess = std::chrono::system_clock::now();
    }
}

auto ModuleLoader::buildDependencyGraph() -> void {
    DLOG_F(INFO, "Building dependency graph");

    try {
        std::shared_lock lock(sharedMutex_);

        // Clear existing graph and rebuild
        dependencyGraph_.clear();

        // Add all modules as nodes
        for (const auto& [name, moduleInfo] : modules_) {
            dependencyGraph_.addNode(name, Version());

            // If the module has a getDependencies function, use it to get
            // dependencies
            if (hasFunction(name, "getDependencies")) {
                try {
                    auto getDepFunc =
                        moduleInfo->mLibrary
                            ->getFunction<std::vector<std::string>()>(
                                "getDependencies");
                    if (getDepFunc) {
                        auto deps = getDepFunc();
                        for (const auto& dep : deps) {
                            dependencyGraph_.addDependency(name, dep);
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR,
                          "Exception getting dependencies for module {}: {}",
                          name, e.what());
                }
            }
        }

        // Validate the graph
        auto result = dependencyGraph_.validate();
        if (!result) {
            LOG_F(ERROR, "Dependency graph validation failed: {}", *result);
        } else {
            LOG_F(INFO, "Dependency graph built and validated successfully");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception building dependency graph: {}", e.what());
    }
}

auto ModuleLoader::topologicalSort() const -> std::vector<std::string> {
    try {
        auto sorted = dependencyGraph_.topologicalSort();
        if (!sorted) {
            LOG_F(ERROR,
                  "Topological sort failed due to circular dependencies");
            return {};
        }
        return *sorted;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during topological sort: {}", e.what());
        return {};
    }
}
