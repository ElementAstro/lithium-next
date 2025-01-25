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

#include <memory>

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

namespace lithium {
ModuleLoader::ModuleLoader(std::string dirName)
    : threadPool_(std::make_shared<atom::async::ThreadPool<>>(
          std::thread::hardware_concurrency())) {
    DLOG_F(INFO, "Module manager {} initialized.", dirName);
}

ModuleLoader::~ModuleLoader() {
    DLOG_F(INFO, "Module manager destroyed.");
    unloadAllModules();
}

auto ModuleLoader::createShared() -> std::shared_ptr<ModuleLoader> {
    DLOG_F(INFO, "Creating shared ModuleLoader with default directory.");
    return std::make_shared<ModuleLoader>("modules");
}

auto ModuleLoader::createShared(std::string dirName)
    -> std::shared_ptr<ModuleLoader> {
    DLOG_F(INFO, "Creating shared ModuleLoader with directory: {}", dirName);
    return std::make_shared<ModuleLoader>(std::move(dirName));
}

auto ModuleLoader::loadModule(const std::string& path,
                              const std::string& name) -> bool {
    DLOG_F(INFO, "Loading module: {} from path: {}", name, path);
    std::unique_lock lock(sharedMutex_);

    if (hasModule(name)) {
        LOG_F(ERROR, "Module {} already loaded", name);
        return false;
    }

    if (!atom::io::isFileExists(path)) {
        LOG_F(ERROR, "Module {} not found at path: {}", name, path);
        return false;
    }

    auto modInfo = std::make_shared<ModuleInfo>();
    try {
        modInfo->mLibrary = std::make_shared<atom::meta::DynamicLibrary>(path);
        modInfo->path = path;
        // 添加到依赖图
        dependencyGraph_.addNode(name, Version());
        modules_[name] = modInfo;
    } catch (const FFIException& ex) {
        LOG_F(ERROR, "Failed to load module {}: {}", name, ex.what());
        return false;
    }

    LOG_F(INFO, "Module {} loaded successfully", name);
    return true;
}

auto ModuleLoader::unloadModule(const std::string& name) -> bool {
    DLOG_F(INFO, "Unloading module: {}", name);
    std::unique_lock lock(sharedMutex_);
    auto iter = modules_.find(name);
    if (iter != modules_.end()) {
        iter->second->mLibrary.reset();
        modules_.erase(iter);
        LOG_F(INFO, "Module {} unloaded successfully", name);
        return true;
    }
    LOG_F(ERROR, "Module {} not loaded", name);
    return false;
}

auto ModuleLoader::unloadAllModules() -> bool {
    DLOG_F(INFO, "Unloading all modules.");
    std::unique_lock lock(sharedMutex_);
    modules_.clear();
    LOG_F(INFO, "All modules unloaded");
    return true;
}

auto ModuleLoader::hasModule(const std::string& name) const -> bool {
    DLOG_F(INFO, "Checking if module {} is loaded.", name);
    std::shared_lock lock(sharedMutex_);
    bool result = modules_.find(name) != modules_.end();
    DLOG_F(INFO, "Module {} is {}loaded.", name, result ? "" : "not ");
    return result;
}

auto ModuleLoader::getModule(const std::string& name) const
    -> std::shared_ptr<ModuleInfo> {
    DLOG_F(INFO, "Getting module: {}", name);
    std::shared_lock lock(sharedMutex_);
    auto iter = modules_.find(name);
    if (iter != modules_.end()) {
        DLOG_F(INFO, "Module {} found.", name);
        return iter->second;
    }
    DLOG_F(INFO, "Module {} not found.", name);
    return nullptr;
}

auto ModuleLoader::enableModule(const std::string& name) -> bool {
    DLOG_F(INFO, "Enabling module: {}", name);
    std::unique_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (mod && !mod->enabled.load()) {
        mod->enabled.store(true);
        LOG_F(INFO, "Module {} enabled.", name);
        return true;
    }
    LOG_F(WARNING, "Module {} is already enabled or not found.", name);
    return false;
}

auto ModuleLoader::disableModule(const std::string& name) -> bool {
    DLOG_F(INFO, "Disabling module: {}", name);
    std::unique_lock lock(sharedMutex_);
    auto mod = getModule(name);
    if (mod && mod->enabled.load()) {
        mod->enabled.store(false);
        LOG_F(INFO, "Module {} disabled.", name);
        return true;
    }
    LOG_F(WARNING, "Module {} is already disabled or not found.", name);
    return false;
}

auto ModuleLoader::isModuleEnabled(const std::string& name) const -> bool {
    DLOG_F(INFO, "Checking if module {} is enabled.", name);
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
    for (const auto& [name, _] : modules_) {
        moduleNames.push_back(name);
    }
    DLOG_F(INFO, "Loaded modules: {}", atom::utils::toString(moduleNames));
    return moduleNames;
}

auto ModuleLoader::hasFunction(const std::string& name,
                               const std::string& functionName) -> bool {
    DLOG_F(INFO, "Checking if module {} has function: {}", name, functionName);
    std::shared_lock lock(sharedMutex_);
    auto iter = modules_.find(name);
    bool result = iter != modules_.end() &&
                  iter->second->mLibrary->hasFunction(functionName);
    DLOG_F(INFO, "Module {} {} function: {}", name,
           result ? "has" : "does not have", functionName);
    return result;
}

auto ModuleLoader::reloadModule(const std::string& name) -> bool {
    if (!unloadModule(name)) {
        return false;
    }
    auto mod = getModule(name);
    if (!mod) {
        return false;
    }
    return loadModule(mod->path, name);
}

auto ModuleLoader::getModuleStatus(const std::string& name) const
    -> ModuleInfo::Status {
    std::shared_lock lock(sharedMutex_);
    auto mod = getModule(name);
    return mod ? mod->currentStatus : ModuleInfo::Status::UNLOADED;
}

auto ModuleLoader::validateDependencies(const std::string& name) const -> bool {
    return dependencyGraph_.validateDependencies(name);
}

auto ModuleLoader::loadModulesInOrder() -> bool {
    auto sortedModules = dependencyGraph_.topologicalSort();
    if (!sortedModules) {
        LOG_F(ERROR, "Failed to sort modules due to circular dependencies");
        return false;
    }

    bool success = true;
    for (const auto& name : *sortedModules) {
        auto mod = getModule(name);
        if (!mod || !loadModule(mod->path, name)) {
            success = false;
        }
    }
    return success;
}

auto ModuleLoader::computeModuleHash(const std::string& path) -> std::size_t {
    return atom::algorithm::computeHash(path);
}

auto ModuleLoader::loadModuleAsync(
    const std::string& path, const std::string& name) -> std::future<bool> {
    return threadPool_->enqueue([this, path, name]() {
        auto startTime = std::chrono::system_clock::now();
        bool result = loadModule(path, name);
        auto endTime = std::chrono::system_clock::now();

        if (auto modInfo = getModule(name)) {
            modInfo->hash = computeModuleHash(path);
            modInfo->stats.loadCount++;
            if (!result)
                modInfo->stats.failureCount++;

            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);
            modInfo->stats.averageLoadTime =
                (modInfo->stats.averageLoadTime *
                     (modInfo->stats.loadCount - 1) +
                 duration.count()) /
                modInfo->stats.loadCount;
            modInfo->stats.lastAccess = endTime;
        }

        return result;
    });
}

auto ModuleLoader::loadModulesAsync(
    const std::vector<std::pair<std::string, std::string>>& modules)
    -> std::vector<std::future<bool>> {
    std::vector<std::future<bool>> results;
    results.reserve(modules.size());

    for (const auto& [path, name] : modules) {
        // 先添加到依赖图
        dependencyGraph_.addNode(name, Version());
        results.push_back(loadModuleAsync(path, name));
    }

    // 在加载完成后验证依赖关系
    for (const auto& [_, name] : modules) {
        if (!dependencyGraph_.validateDependencies(name)) {
            LOG_F(ERROR, "Dependencies validation failed for module {}", name);
        }
    }

    return results;
}

void ModuleLoader::setThreadPoolSize(size_t size) {
    threadPool_ = std::make_shared<atom::async::ThreadPool<>>(size);
}

auto ModuleLoader::getModuleByHash(std::size_t hash)
    -> std::shared_ptr<ModuleInfo> {
    std::shared_lock lock(sharedMutex_);
    for (const auto& [_, module] : modules_) {
        if (module->hash == hash) {
            return module;
        }
    }
    return nullptr;
}

auto ModuleLoader::getDependencies(const std::string& name) const
    -> std::vector<std::string> {
    return dependencyGraph_.getDependencies(name);
}

}  // namespace lithium
