/*
 * loader.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: C++20 and Modules Loader

**************************************************/

#ifndef LITHIUM_ADDON_LOADER_HPP
#define LITHIUM_ADDON_LOADER_HPP

#include <cstdio>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/function/ffi.hpp"
#include "atom/type/json_fwd.hpp"

#include "module.hpp"

using json = nlohmann::json;

#define MODULE_HANDLE void*

namespace lithium {

/**
 * @brief Class for managing and loading modules.
 */
class ModuleLoader {
public:
    /**
     * @brief Constructs a ModuleLoader with a specified directory name.
     * @param dirName The directory name where modules are located.
     */
    explicit ModuleLoader(std::string dirName);

    /**
     * @brief Destructor for ModuleLoader.
     */
    ~ModuleLoader();

    /**
     * @brief Creates a shared ModuleLoader with the default directory.
     * @return A shared pointer to the created ModuleLoader.
     */
    static auto createShared() -> std::shared_ptr<ModuleLoader>;

    /**
     * @brief Creates a shared ModuleLoader with a specified directory name.
     * @param dirName The directory name where modules are located.
     * @return A shared pointer to the created ModuleLoader.
     */
    static auto createShared(std::string dirName)
        -> std::shared_ptr<ModuleLoader>;

    /**
     * @brief Loads a module from a specified path.
     * @param path The path to the module.
     * @param name The name of the module.
     * @return True if the module was loaded successfully, false otherwise.
     */
    auto loadModule(const std::string& path, const std::string& name) -> bool;

    /**
     * @brief Unloads a module by name.
     * @param name The name of the module.
     * @return True if the module was unloaded successfully, false otherwise.
     */
    auto unloadModule(const std::string& name) -> bool;

    /**
     * @brief Unloads all loaded modules.
     * @return True if all modules were unloaded successfully, false otherwise.
     */
    auto unloadAllModules() -> bool;

    /**
     * @brief Checks if a module is loaded.
     * @param name The name of the module.
     * @return True if the module is loaded, false otherwise.
     */
    auto hasModule(const std::string& name) const -> bool;

    /**
     * @brief Gets a module by name.
     * @param name The name of the module.
     * @return A shared pointer to the ModuleInfo of the module, or nullptr if
     * not found.
     */
    auto getModule(const std::string& name) const
        -> std::shared_ptr<ModuleInfo>;

    /**
     * @brief Enables a module by name.
     * @param name The name of the module.
     * @return True if the module was enabled successfully, false otherwise.
     */
    auto enableModule(const std::string& name) -> bool;

    /**
     * @brief Disables a module by name.
     * @param name The name of the module.
     * @return True if the module was disabled successfully, false otherwise.
     */
    auto disableModule(const std::string& name) -> bool;

    /**
     * @brief Checks if a module is enabled.
     * @param name The name of the module.
     * @return True if the module is enabled, false otherwise.
     */
    auto isModuleEnabled(const std::string& name) const -> bool;

    /**
     * @brief Gets the names of all loaded modules.
     * @return A vector of strings containing the names of all loaded modules.
     */
    auto getAllExistedModules() const -> std::vector<std::string>;

    /**
     * @brief Gets a function from a module.
     * @tparam T The type of the function.
     * @param name The name of the module.
     * @param functionName The name of the function.
     * @return A std::function object representing the function, or nullptr if
     * not found.
     */
    template <typename T>
    auto getFunction(const std::string& name,
                     const std::string& functionName) -> std::function<T>;

    /**
     * @brief Gets an instance of a class from a module.
     * @tparam T The type of the class.
     * @param name The name of the module.
     * @param config The configuration for the instance.
     * @param symbolName The name of the symbol representing the instance
     * function.
     * @return A shared pointer to the instance, or nullptr if not found.
     */
    template <typename T>
    auto getInstance(const std::string& name, const json& config,
                     const std::string& symbolName) -> std::shared_ptr<T>;

    /**
     * @brief Gets a unique instance of a class from a module.
     * @tparam T The type of the class.
     * @param name The name of the module.
     * @param config The configuration for the instance.
     * @param instanceFunctionName The name of the function to create the
     * instance.
     * @return A unique pointer to the instance, or nullptr if not found.
     */
    template <typename T>
    auto getUniqueInstance(const std::string& name, const json& config,
                           const std::string& instanceFunctionName)
        -> std::unique_ptr<T>;

    /**
     * @brief Gets a shared pointer to an instance of a class from a module.
     * @tparam T The type of the class.
     * @param name The name of the module.
     * @param config The configuration for the instance.
     * @param instanceFunctionName The name of the function to create the
     * instance.
     * @return A shared pointer to the instance, or nullptr if not found.
     */
    template <typename T>
    auto getInstancePointer(const std::string& name, const json& config,
                            const std::string& instanceFunctionName)
        -> std::shared_ptr<T>;

    /**
     * @brief Checks if a module has a specific function.
     * @param name The name of the module.
     * @param functionName The name of the function.
     * @return True if the module has the function, false otherwise.
     */
    auto hasFunction(const std::string& name,
                     const std::string& functionName) -> bool;

    /**
     * @brief Reloads a module by name.
     * @param name The name of the module.
     * @return True if the module was reloaded successfully, false otherwise.
     */
    auto reloadModule(const std::string& name) -> bool;

    /**
     * @brief Gets the status of a module.
     * @param name The name of the module.
     * @return The status of the module.
     */
    auto getModuleStatus(const std::string& name) const -> ModuleInfo::Status;

    /**
     * @brief Gets the statistics of a module.
     * @param name The name of the module.
     * @return The statistics of the module.
     */
    auto getModuleStatistics(const std::string& name) const
        -> ModuleInfo::Statistics;

    /**
     * @brief Sets the priority of a module.
     * @param name The name of the module.
     * @param priority The priority value to set.
     * @return True if the priority was set successfully, false otherwise.
     */
    auto setModulePriority(const std::string& name, int priority) -> bool;

    /**
     * @brief Loads modules in the order of their dependencies.
     * @return True if all modules were loaded successfully, false otherwise.
     */
    auto loadModulesInOrder() -> bool;

    /**
     * @brief Gets the dependencies of a module.
     * @param name The name of the module.
     * @return A vector of strings containing the names of the dependencies.
     */
    auto getDependencies(const std::string& name) const
        -> std::vector<std::string>;

    /**
     * @brief Validates the dependencies of a module.
     * @param name The name of the module.
     * @return True if all dependencies are satisfied, false otherwise.
     */
    auto validateDependencies(const std::string& name) const -> bool;

private:
    std::unordered_map<std::string, std::shared_ptr<ModuleInfo>>
        modules_;  ///< Map of module names to ModuleInfo objects.
    mutable std::shared_mutex
        sharedMutex_;  ///< Mutex for thread-safe access to modules.

    auto loadModuleFunctions(const std::string& name)
        -> std::vector<std::unique_ptr<FunctionInfo>>;
    auto getHandle(const std::string& name) const
        -> std::shared_ptr<atom::meta::DynamicLibrary>;
    auto checkModuleExists(const std::string& name) const -> bool;

    std::unordered_map<std::string, std::vector<std::string>>
        dependencyGraph_;  ///< Dependency graph of modules.
    auto buildDependencyGraph() -> void;
    auto topologicalSort() const -> std::vector<std::string>;
    auto updateModuleStatistics(const std::string& name) -> void;
};

template <typename T>
auto ModuleLoader::getFunction(const std::string& name,
                               const std::string& functionName)
    -> std::function<T> {
    std::shared_lock lock(sharedMutex_);
    auto it = modules_.find(name);
    if (it == modules_.end()) {
        return nullptr;
    }

    try {
        return it->second->mLibrary->getFunction<T>(functionName);
    } catch (const FFIException& e) {
    }
    return nullptr;
}

template <typename T>
auto ModuleLoader::getInstance(const std::string& name, const json& config,
                               const std::string& symbolName)
    -> std::shared_ptr<T> {
    if (auto getInstanceFunc =
            getFunction<std::shared_ptr<T>(const json&)>(name, symbolName);
        getInstanceFunc) {
        return getInstanceFunc(config);
    }
    return nullptr;
}

template <typename T>
auto ModuleLoader::getUniqueInstance(
    const std::string& name, const json& config,
    const std::string& instanceFunctionName) -> std::unique_ptr<T> {
    if (auto getInstanceFunc = getFunction<std::unique_ptr<T>(const json&)>(
            name, instanceFunctionName);
        getInstanceFunc) {
        return getInstanceFunc(config);
    }
    return nullptr;
}

template <typename T>
auto ModuleLoader::getInstancePointer(
    const std::string& name, const json& config,
    const std::string& instanceFunctionName) -> std::shared_ptr<T> {
    return getInstance<T>(name, config, instanceFunctionName);
}

}  // namespace lithium

#endif  // LITHIUM_ADDON_LOADER_HPP