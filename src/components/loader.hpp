/*
 * loader.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ADDON_LOADER_HPP
#define LITHIUM_ADDON_LOADER_HPP

#include <concepts>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/async/pool.hpp"
#include "atom/function/ffi.hpp"
#include "atom/type/json_fwd.hpp"
#include "dependency.hpp"
#include "module.hpp"

using json = nlohmann::json;

#define MODULE_HANDLE void*

// Define custom exception types
class ModuleLoadError : public std::runtime_error {
public:
    explicit ModuleLoadError(const std::string& message)
        : std::runtime_error(message) {}
};

class ModuleNotFoundError : public std::runtime_error {
public:
    explicit ModuleNotFoundError(const std::string& message)
        : std::runtime_error(message) {}
};

namespace lithium {

// Define concepts for type constraints
template <typename T>
concept ConfigurableModule = requires(T t, const json& config) {
    { T::configure(config) } -> std::same_as<bool>;
};

template <typename T>
concept ModuleFunction =
    std::is_function_v<T> || std::is_function_v<std::remove_pointer_t<T>>;

// Module operation result type using C++23 std::expected
template <typename T>
using ModuleResult = std::expected<T, std::string>;

/**
 * @brief Class for managing and loading modules.
 */
class ModuleLoader {
public:
    /**
     * @brief Constructs a ModuleLoader with a specified directory name.
     * @param dirName The directory name where modules are located.
     */
    explicit ModuleLoader(std::string_view dirName);

    /**
     * @brief Destructor for ModuleLoader.
     */
    ~ModuleLoader();

    // Delete copy and move operations to prevent resource management issues
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;
    ModuleLoader(ModuleLoader&&) = delete;
    ModuleLoader& operator=(ModuleLoader&&) = delete;

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
    static auto createShared(std::string_view dirName)
        -> std::shared_ptr<ModuleLoader>;

    /**
     * @brief Loads a module from a specified path.
     * @param path The path to the module.
     * @param name The name of the module.
     * @return Result indicating success or error message.
     */
    auto loadModule(std::string_view path, std::string_view name)
        -> ModuleResult<bool>;

    /**
     * @brief Unloads a module by name.
     * @param name The name of the module.
     * @return Result indicating success or error message.
     */
    auto unloadModule(std::string_view name) -> ModuleResult<bool>;

    /**
     * @brief Unloads all loaded modules.
     * @return Result indicating success or error message.
     */
    auto unloadAllModules() -> ModuleResult<bool>;

    /**
     * @brief Checks if a module is loaded.
     * @param name The name of the module.
     * @return True if the module is loaded, false otherwise.
     */
    [[nodiscard]] auto hasModule(std::string_view name) const -> bool;

    /**
     * @brief Gets a module by name.
     * @param name The name of the module.
     * @return A shared pointer to the ModuleInfo of the module, or nullptr if
     * not found.
     */
    [[nodiscard]] auto getModule(std::string_view name) const
        -> std::shared_ptr<ModuleInfo>;

    /**
     * @brief Enables a module by name.
     * @param name The name of the module.
     * @return Result indicating success or error message.
     */
    auto enableModule(std::string_view name) -> ModuleResult<bool>;

    /**
     * @brief Disables a module by name.
     * @param name The name of the module.
     * @return Result indicating success or error message.
     */
    auto disableModule(std::string_view name) -> ModuleResult<bool>;

    /**
     * @brief Checks if a module is enabled.
     * @param name The name of the module.
     * @return True if the module is enabled, false otherwise.
     */
    [[nodiscard]] auto isModuleEnabled(std::string_view name) const -> bool;

    /**
     * @brief Gets the names of all loaded modules.
     * @return A vector of strings containing the names of all loaded modules.
     */
    [[nodiscard]] auto getAllExistedModules() const -> std::vector<std::string>;

    /**
     * @brief Gets a function from a module.
     * @tparam T The type of the function.
     * @param name The name of the module.
     * @param functionName The name of the function.
     * @return A std::function object representing the function, or nullptr if
     * not found.
     */
    template <ModuleFunction T>
    auto getFunction(std::string_view name, std::string_view functionName)
        -> ModuleResult<std::function<T>>;

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
    auto getInstance(std::string_view name, const json& config,
                     std::string_view symbolName)
        -> ModuleResult<std::shared_ptr<T>>;

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
    auto getUniqueInstance(std::string_view name, const json& config,
                           std::string_view instanceFunctionName)
        -> ModuleResult<std::unique_ptr<T>>;

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
    auto getInstancePointer(std::string_view name, const json& config,
                            std::string_view instanceFunctionName)
        -> ModuleResult<std::shared_ptr<T>>;

    /**
     * @brief Checks if a module has a specific function.
     * @param name The name of the module.
     * @param functionName The name of the function.
     * @return True if the module has the function, false otherwise.
     */
    [[nodiscard]] auto hasFunction(std::string_view name,
                                   std::string_view functionName) const -> bool;

    /**
     * @brief Reloads a module by name.
     * @param name The name of the module.
     * @return Result indicating success or error message.
     */
    auto reloadModule(std::string_view name) -> ModuleResult<bool>;

    /**
     * @brief Gets the status of a module.
     * @param name The name of the module.
     * @return The status of the module.
     */
    [[nodiscard]] auto getModuleStatus(std::string_view name) const
        -> ModuleInfo::Status;

    /**
     * @brief Gets the statistics of a module.
     * @param name The name of the module.
     * @return The statistics of the module.
     */
    [[nodiscard]] auto getModuleStatistics(std::string_view name) const
        -> ModuleInfo::Statistics;

    /**
     * @brief Sets the priority of a module.
     * @param name The name of the module.
     * @param priority The priority value to set.
     * @return Result indicating success or error message.
     */
    auto setModulePriority(std::string_view name, int priority)
        -> ModuleResult<bool>;

    /**
     * @brief Loads modules in the order of their dependencies.
     * @return Result indicating success or error message.
     */
    auto loadModulesInOrder() -> ModuleResult<bool>;

    /**
     * @brief Gets the dependencies of a module.
     * @param name The name of the module.
     * @return A vector of strings containing the names of the dependencies.
     */
    [[nodiscard]] auto getDependencies(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Validates the dependencies of a module.
     * @param name The name of the module.
     * @return True if all dependencies are satisfied, false otherwise.
     */
    [[nodiscard]] auto validateDependencies(std::string_view name) const
        -> bool;

    /**
     * @brief Sets the size of the thread pool.
     * @param size The size of the thread pool.
     * @throws std::invalid_argument if size is 0
     */
    void setThreadPoolSize(size_t size);

    /**
     * @brief Asynchronously loads multiple modules.
     * @param modules A view of pairs containing the paths and names of the
     * modules.
     * @return A vector of futures representing the loading results.
     */
    auto loadModulesAsync(
        std::span<const std::pair<std::string, std::string>> modules)
        -> std::vector<std::future<ModuleResult<bool>>>;

    /**
     * @brief Gets a module by its hash.
     * @param hash The hash of the module.
     * @return A shared pointer to the ModuleInfo of the module, or nullptr if
     * not found.
     */
    [[nodiscard]] auto getModuleByHash(std::size_t hash) const
        -> std::shared_ptr<ModuleInfo>;

    /**
     * @brief Batch process modules with a specified operation
     * @tparam Func The type of function to apply to each module
     * @param func The operation to perform
     * @return Number of successfully processed modules
     */
    template <typename Func>
        requires std::invocable<Func, std::shared_ptr<ModuleInfo>>
    auto batchProcessModules(Func&& func) -> size_t;

private:
    std::unordered_map<std::string, std::shared_ptr<ModuleInfo>>
        modules_;  ///< Map of module names to ModuleInfo objects.
    mutable std::shared_mutex
        sharedMutex_;  ///< Mutex for thread-safe access to modules.
    std::shared_ptr<atom::async::ThreadPool<>> threadPool_;
    DependencyGraph dependencyGraph_;   // Dependency graph member
    std::filesystem::path modulesDir_;  // Store the path to modules directory

    auto loadModuleFunctions(std::string_view name)
        -> std::vector<std::unique_ptr<FunctionInfo>>;
    [[nodiscard]] auto getHandle(std::string_view name) const
        -> std::shared_ptr<atom::meta::DynamicLibrary>;
    [[nodiscard]] auto checkModuleExists(std::string_view name) const -> bool;

    auto buildDependencyGraph() -> void;
    [[nodiscard]] auto topologicalSort() const -> std::vector<std::string>;
    auto updateModuleStatistics(std::string_view name) -> void;

    /**
     * @brief Computes the hash of a module.
     * @param path The path to the module.
     * @return The hash of the module.
     */
    [[nodiscard]] auto computeModuleHash(std::string_view path) const
        -> std::size_t;

    /**
     * @brief Asynchronously loads a module.
     * @param path The path to the module.
     * @param name The name of the module.
     * @return A future representing the loading result.
     */
    auto loadModuleAsync(std::string_view path, std::string_view name)
        -> std::future<ModuleResult<bool>>;

    /**
     * @brief Verifies module integrity
     * @param path Path to the module file
     * @return True if module is valid, false otherwise
     */
    [[nodiscard]] auto verifyModuleIntegrity(
        const std::filesystem::path& path) const -> bool;

    /**
     * @brief Convert string_view to string safely
     * @param sv The string_view to convert
     * @return std::string version
     */
    [[nodiscard]] static auto toStdString(std::string_view sv) -> std::string {
        return std::string(sv);
    }
};

template <ModuleFunction T>
auto ModuleLoader::getFunction(std::string_view name,
                               std::string_view functionName)
    -> ModuleResult<std::function<T>> {
    std::shared_lock lock(sharedMutex_);

    try {
        auto it = modules_.find(toStdString(name));
        if (it == modules_.end()) {
            return std::unexpected("Module not found: " + toStdString(name));
        }

        auto func =
            it->second->mLibrary->getFunction<T>(toStdString(functionName));
        if (!func) {
            return std::unexpected("Function not found: " +
                                   toStdString(functionName));
        }

        // Update access statistics
        updateModuleStatistics(name);
        return func;
    } catch (const FFIException& e) {
        return std::unexpected(std::string("FFI exception: ") + e.what());
    } catch (const std::exception& e) {
        return std::unexpected(
            std::string("Exception when accessing function: ") + e.what());
    }
}

template <typename T>
auto ModuleLoader::getInstance(std::string_view name, const json& config,
                               std::string_view symbolName)
    -> ModuleResult<std::shared_ptr<T>> {
    auto getInstanceFuncResult =
        getFunction<std::shared_ptr<T>(const json&)>(name, symbolName);
    if (!getInstanceFuncResult) {
        return std::unexpected(getInstanceFuncResult.error());
    }

    try {
        auto instance = getInstanceFuncResult.value()(config);
        if (!instance) {
            return std::unexpected(
                "Failed to create instance with provided configuration");
        }
        return instance;
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Exception creating instance: ") +
                               e.what());
    }
}

template <typename T>
auto ModuleLoader::getUniqueInstance(std::string_view name, const json& config,
                                     std::string_view instanceFunctionName)
    -> ModuleResult<std::unique_ptr<T>> {
    auto getInstanceFuncResult = getFunction<std::unique_ptr<T>(const json&)>(
        name, instanceFunctionName);

    if (!getInstanceFuncResult) {
        return std::unexpected(getInstanceFuncResult.error());
    }

    try {
        auto instance = getInstanceFuncResult.value()(config);
        if (!instance) {
            return std::unexpected(
                "Failed to create unique instance with provided configuration");
        }
        return instance;
    } catch (const std::exception& e) {
        return std::unexpected(
            std::string("Exception creating unique instance: ") + e.what());
    }
}

template <typename T>
auto ModuleLoader::getInstancePointer(std::string_view name, const json& config,
                                      std::string_view instanceFunctionName)
    -> ModuleResult<std::shared_ptr<T>> {
    return getInstance<T>(name, config, instanceFunctionName);
}

template <typename Func>
    requires std::invocable<Func, std::shared_ptr<ModuleInfo>>
auto ModuleLoader::batchProcessModules(Func&& func) -> size_t {
    std::shared_lock lock(sharedMutex_);
    size_t successCount = 0;

    for (auto& [_, moduleInfo] : modules_) {
        try {
            if (std::invoke(std::forward<Func>(func), moduleInfo)) {
                successCount++;
            }
        } catch (const std::exception& e) {
        }
    }

    return successCount;
}

}  // namespace lithium

#endif  // LITHIUM_ADDON_LOADER_HPP
