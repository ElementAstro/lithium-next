/*
 * loader.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "loader.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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
#include "atom/utils/to_string.hpp"
#include "spdlog/spdlog.h"


namespace fs = std::filesystem;

namespace lithium {

ModuleLoader::ModuleLoader(std::string_view dirName)
    : threadPool_(std::make_shared<atom::async::ThreadPool<>>(
          std::thread::hardware_concurrency())),
      modulesDir_(dirName) {
    spdlog::debug("Module manager initialized with directory: {}", dirName);
}

ModuleLoader::~ModuleLoader() {
    spdlog::debug("Module manager destroying...");
    try {
        auto result = unloadAllModules();
        if (!result) {
            spdlog::error("Failed to unload all modules: {}", result.error());
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception during ModuleLoader destruction: {}",
                      e.what());
    }
}

auto ModuleLoader::createShared() -> std::shared_ptr<ModuleLoader> {
    spdlog::debug("Creating shared ModuleLoader with default directory.");
    return std::make_shared<ModuleLoader>("modules");
}

auto ModuleLoader::createShared(std::string_view dirName)
    -> std::shared_ptr<ModuleLoader> {
    spdlog::debug("Creating shared ModuleLoader with directory: {}", dirName);
    return std::make_shared<ModuleLoader>(dirName);
}

auto ModuleLoader::loadModule(std::string_view path, std::string_view name)
    -> ModuleResult<bool> {
    spdlog::debug("Loading module: {} from path: {}", name, path);

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

        spdlog::info("Module {} loaded successfully", name);
        return true;
    } catch (const FFIException& ex) {
        spdlog::error("FFI exception while loading module {}: {}", name,
                      ex.what());
        return std::unexpected(std::string("FFI exception: ") + ex.what());
    } catch (const std::exception& ex) {
        spdlog::error("Exception while loading module {}: {}", name, ex.what());
        return std::unexpected(std::string("Exception: ") + ex.what());
    }
}

auto ModuleLoader::unloadModule(std::string_view name) -> ModuleResult<bool> {
    spdlog::debug("Unloading module: {}", name);

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

            spdlog::info("Module {} unloaded successfully", name);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Exception while unloading module {}: {}", name,
                          e.what());
            return std::unexpected(std::string("Exception: ") + e.what());
        }
    }

    return std::unexpected("Module not loaded: " + toStdString(name));
}

auto ModuleLoader::unloadAllModules() -> ModuleResult<bool> {
    spdlog::debug("Unloading all modules.");
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
        spdlog::info("All modules unloaded");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while unloading all modules: {}", e.what());
        return std::unexpected(std::string("Exception: ") + e.what());
    }
}

auto ModuleLoader::hasModule(std::string_view name) const -> bool {
    if (name.empty()) {
        return false;
    }

    std::shared_lock lock(sharedMutex_);
    bool result = modules_.contains(toStdString(name));
    spdlog::debug("Module {} is {}loaded.", name, result ? "" : "not ");
    return result;
}

auto ModuleLoader::getModule(std::string_view name) const
    -> std::shared_ptr<ModuleInfo> {
    spdlog::debug("Getting module: {}", name);

    if (name.empty()) {
        spdlog::warn("Empty module name provided");
        return nullptr;
    }

    std::shared_lock lock(sharedMutex_);
    auto iter = modules_.find(toStdString(name));
    if (iter != modules_.end()) {
        spdlog::debug("Module {} found.", name);
        return iter->second;
    }
    spdlog::debug("Module {} not found.", name);
    return nullptr;
}

auto ModuleLoader::enableModule(std::string_view name) -> ModuleResult<bool> {
    spdlog::debug("Enabling module: {}", name);

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
        mod->currentStatus = ModuleInfo::Status::LOADED;
        spdlog::info("Module {} enabled.", name);
        return true;
    }

    spdlog::warn("Module {} is already enabled.", name);
    return std::unexpected("Module is already enabled: " + toStdString(name));
}

auto ModuleLoader::disableModule(std::string_view name) -> ModuleResult<bool> {
    spdlog::debug("Disabling module: {}", name);

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
        spdlog::info("Module {} disabled.", name);
        return true;
    }

    spdlog::warn("Module {} is already disabled.", name);
    return std::unexpected("Module is already disabled: " + toStdString(name));
}

auto ModuleLoader::isModuleEnabled(std::string_view name) const -> bool {
    spdlog::debug("Checking if module {} is enabled.", name);

    if (name.empty()) {
        return false;
    }

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    bool result = mod && mod->enabled.load();
    spdlog::debug("Module {} is {}enabled.", name, result ? "" : "not ");
    return result;
}

auto ModuleLoader::getAllExistedModules() const -> std::vector<std::string> {
    spdlog::debug("Getting all loaded modules.");
    std::shared_lock lock(sharedMutex_);

    std::vector<std::string> moduleNames;
    moduleNames.reserve(modules_.size());

    // Use C++20 ranges to transform the map to a vector of names
    auto keys = modules_ | std::views::keys;
    std::ranges::copy(keys, std::back_inserter(moduleNames));

    // Sort for consistent results
    std::ranges::sort(moduleNames);

    spdlog::debug("Loaded modules: {}", atom::utils::toString(moduleNames));
    return moduleNames;
}

auto ModuleLoader::hasFunction(std::string_view name,
                               std::string_view functionName) const -> bool {
    spdlog::debug("Checking if module {} has function: {}", name, functionName);

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
        spdlog::debug("Module {} {} function: {}", name,
                      result ? "has" : "does not have", functionName);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception checking function {} in module {}: {}",
                      functionName, name, e.what());
        return false;
    }
}

auto ModuleLoader::reloadModule(std::string_view name) -> ModuleResult<bool> {
    spdlog::debug("Reloading module: {}", name);

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
    spdlog::debug("Getting status for module: {}", name);

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    auto status = mod ? mod->currentStatus : ModuleInfo::Status::UNLOADED;

    spdlog::debug("Module {} status: {}", name, static_cast<int>(status));
    return status;
}

auto ModuleLoader::getModuleStatistics(std::string_view name) const
    -> ModuleInfo::Statistics {
    spdlog::debug("Getting statistics for module: {}", name);

    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (!mod) {
        spdlog::warn("Tried to get statistics for non-existent module: {}",
                     name);
        return {};
    }

    return mod->stats;
}

auto ModuleLoader::setModulePriority(std::string_view name, int priority)
    -> ModuleResult<bool> {
    spdlog::debug("Setting priority for module {} to {}", name, priority);

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
    spdlog::info("Priority for module {} set to {}", name, priority);
    return true;
}

auto ModuleLoader::validateDependencies(std::string_view name) const -> bool {
    spdlog::debug("Validating dependencies for module: {}", name);

    if (name.empty()) {
        spdlog::warn("Empty module name provided for dependency validation");
        return false;
    }

    return dependencyGraph_.validateDependencies(toStdString(name));
}

auto ModuleLoader::loadModulesInOrder() -> ModuleResult<bool> {
    spdlog::debug("Loading modules in dependency order");

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
                spdlog::error("Failed to load module {}: {}",
                              modulesToLoad[i].second, result.error());
            }
        }

        if (!failedModules.empty()) {
            return std::unexpected("Failed to load modules: " +
                                   atom::utils::toString(failedModules));
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception during module loading in order: {}", e.what());
        return std::unexpected(std::string("Exception: ") + e.what());
    }
}

auto ModuleLoader::getDependencies(std::string_view name) const
    -> std::vector<std::string> {
    spdlog::debug("Getting dependencies for module: {}", name);

    if (name.empty()) {
        spdlog::warn("Empty module name provided for dependency retrieval");
        return {};
    }

    return dependencyGraph_.getDependencies(toStdString(name));
}

void ModuleLoader::setThreadPoolSize(size_t size) {
    spdlog::debug("Setting thread pool size to {}", size);

    if (size == 0) {
        spdlog::error("Thread pool size cannot be zero");
        throw std::invalid_argument("Thread pool size cannot be zero");
    }

    threadPool_ = std::make_shared<atom::async::ThreadPool<>>(size);
}

auto ModuleLoader::loadModulesAsync(
    std::span<const std::pair<std::string, std::string>> modules)
    -> std::vector<std::future<ModuleResult<bool>>> {
    spdlog::debug("Asynchronously loading {} modules", modules.size());

    std::vector<std::future<ModuleResult<bool>>> results;
    results.reserve(modules.size());

    // Add all modules to dependency graph first
    {
        std::unique_lock lock(sharedMutex_);
        for (const auto& [_, name] : modules) {
            if (dependencyGraph_.getDependencies(name).empty()) {
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
    spdlog::debug("Looking for module with hash: {}", hash);

    std::shared_lock lock(sharedMutex_);

    // Use C++20 ranges to find module with matching hash
    auto it = std::ranges::find_if(modules_, [hash](const auto& pair) {
        return pair.second && pair.second->hash == hash;
    });

    if (it != modules_.end()) {
        spdlog::debug("Found module {} with matching hash", it->first);
        return it->second;
    }

    spdlog::debug("No module found with hash {}", hash);
    return nullptr;
}

auto ModuleLoader::computeModuleHash(std::string_view path) const
    -> std::size_t {
    try {
        return atom::algorithm::computeHash(toStdString(path));
    } catch (const std::exception& e) {
        spdlog::error("Exception computing hash for {}: {}", path, e.what());
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
    spdlog::debug("Verifying integrity of module: {}", path.string());

    try {
        // Basic file existence check
        if (!atom::io::isFileExists(path.string())) {
            spdlog::error("Module file does not exist: {}", path.string());
            return false;
        }

        // Empty file check
        if (fs::is_empty(path)) {
            spdlog::error("Module file is empty: {}", path.string());
            return false;
        }

        // File permissions check
        std::error_code ec;
        auto perms = fs::status(path, ec).permissions();
        if (ec) {
            spdlog::error("Failed to check file permissions for {}: {}",
                          path.string(), ec.message());
            return false;
        }

        // Ensure file is readable
        if ((perms & fs::perms::owner_read) == fs::perms::none &&
            (perms & fs::perms::group_read) == fs::perms::none &&
            (perms & fs::perms::others_read) == fs::perms::none) {
            spdlog::error("Module file is not readable: {}", path.string());
            return false;
        }

        if (atom::io::getFileSize(path.string()) == 0) {
            spdlog::error("Module file is empty: {}", path.string());
            return false;
        }

        auto fileSize = atom::io::getFileSize(path.string());
        if (fileSize < 1024) {  // Minimum reasonable size for a shared library
            spdlog::warn("Module file is suspiciously small: {} ({} bytes)",
                         path.string(), fileSize);
        }

        // Check file extension matches expected platform
        auto extension = path.extension().string();
        bool validExtension = false;

#ifdef _WIN32
        if (extension == ".dll") {
            validExtension = true;
        }
#elif defined(__APPLE__)
        if (extension == ".dylib" || extension == ".so") {
            validExtension = true;
        }
#else  // Assume Linux/Unix
        if (extension == ".so") {
            validExtension = true;
        }
#endif

        if (!validExtension) {
            spdlog::warn("Module has unexpected extension: {} (expected: {})",
                         path.string(),
#ifdef _WIN32
                         ".dll"
#elif defined(__APPLE__)
                         ".dylib or .so"
#else
                         ".so"
#endif
            );
            // Continue despite warning
        }

        // File header signature check
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for header verification: {}",
                          path.string());
            return false;
        }

        // Read first few bytes for format identification
        std::array<unsigned char, 4> header;
        if (!file.read(reinterpret_cast<char*>(header.data()), header.size())) {
            spdlog::error("Failed to read file header from {}", path.string());
            return false;
        }

#ifdef _WIN32
        // Windows PE files start with MZ signature (0x4D 0x5A)
        if (header[0] != 0x4D || header[1] != 0x5A) {
            spdlog::error("File is not a valid PE/DLL format: {}",
                          path.string());
            return false;
        }
#elif defined(__linux__) || defined(__unix__)
        // ELF files start with 0x7F 'E' 'L' 'F'
        if (header[0] != 0x7F || header[1] != 'E' || header[2] != 'L' ||
            header[3] != 'F') {
            spdlog::error("File is not a valid ELF format: {}", path.string());
            return false;
        }
#elif defined(__APPLE__)
        // Mach-O format has several possible magic numbers
        // This is a simplification - would need more detailed checks for
        // production
        const std::array<std::array<unsigned char, 4>, 4> machoMagics = {{
            {0xCA, 0xFE, 0xBA, 0xBE},  // Universal binary
            {0xFE, 0xED, 0xFA, 0xCE},  // 32-bit
            {0xFE, 0xED, 0xFA, 0xCF},  // 64-bit
            {0xCE, 0xFA, 0xED, 0xFE}   // Little endian variant
        }};

        bool validMacho = std::any_of(
            machoMagics.begin(), machoMagics.end(),
            [&header](const auto& magic) {
                return std::equal(magic.begin(), magic.end(), header.begin());
            });

        if (!validMacho) {
            spdlog::error("File is not a valid Mach-O format: {}",
                          path.string());
            return false;
        }
#endif

        // Compute and store hash for future integrity comparisons
        auto hash = computeModuleHash(path.string());
        spdlog::debug("Module hash calculated: {} - {}", path.string(), hash);

        spdlog::info("Module integrity verification passed for {}",
                     path.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception during module integrity check for {}: {}",
                      path.string(), e.what());
        return false;
    }
}

auto ModuleLoader::updateModuleStatistics(std::string_view name) -> void {
    auto nameStr = toStdString(name);
    auto it = modules_.find(nameStr);
    if (it != modules_.end()) {
        it->second->stats.functionCalls++;
        it->second->stats.lastAccess = std::chrono::system_clock::now();
    }
}

auto ModuleLoader::buildDependencyGraph() -> void {
    spdlog::debug("Building dependency graph");

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
                            dependencyGraph_.addDependency(name, dep,
                                                           Version());
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error(
                        "Exception getting dependencies for module {}: {}",
                        name, e.what());
                }
            }
        }

        // Validate the graph
        if (dependencyGraph_.hasCycle()) {
            spdlog::error("Dependency graph validation failed: cycle detected");
        } else {
            spdlog::info("Dependency graph built and validated successfully");
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception building dependency graph: {}", e.what());
    }
}

auto ModuleLoader::topologicalSort() const -> std::vector<std::string> {
    try {
        auto sorted = dependencyGraph_.topologicalSort();
        if (!sorted) {
            spdlog::error(
                "Topological sort failed due to circular dependencies");
            return {};
        }
        return *sorted;
    } catch (const std::exception& e) {
        spdlog::error("Exception during topological sort: {}", e.what());
        return {};
    }
}

}  // namespace lithium