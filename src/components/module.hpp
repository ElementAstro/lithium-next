/*
 * module.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-8-6

Description: Module Information

**************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "atom/function/ffi.hpp"
#include "atom/macro.hpp"

namespace lithium {

/**
 * @brief Represents information about a function in a module.
 */
struct FunctionInfo {
    std::string name;                     ///< The name of the function.
    void* address;                        ///< The address of the function.
    std::vector<std::string> parameters;  ///< The parameters of the function.

    /**
     * @brief Constructs a FunctionInfo object with default values.
     */
    FunctionInfo() : address(nullptr) {}
} ATOM_ALIGNAS(64);

/**
 * @brief Represents information about a module.
 */
struct ModuleInfo {
    std::string name;         ///< The name of the module.
    std::string description;  ///< A brief description of the module.
    std::string version;      ///< The version of the module.
    std::string status;       ///< The current status of the module.
    std::string type;         ///< The type of the module.
    std::string author;       ///< The author of the module.
    std::string license;      ///< The license of the module.
    std::string path;         ///< The file path to the module.
    std::string configPath;   ///< The configuration path for the module.
    std::string configFile;   ///< The configuration file for the module.

    std::atomic_bool enabled;  ///< Indicates whether the module is enabled.

    std::vector<std::unique_ptr<FunctionInfo>>
        functions;  ///< All functions in the module (dynamically loaded).

    std::shared_ptr<atom::meta::DynamicLibrary>
        mLibrary;  ///< The dynamic library associated with the module.

    std::vector<std::string>
        dependencies;  ///< List of dependencies for the module.

    std::chrono::system_clock::time_point
        loadTime;  ///< The time when the module was loaded.

    std::size_t hash;  ///< 模块哈希值

    /**
     * @brief Represents the status of the module.
     */
    enum class Status {
        UNLOADED,     ///< The module is not loaded.
        LOADING,      ///< The module is currently loading.
        LOADED,       ///< The module is loaded.
        ERROR         ///< There was an error loading the module.
    } currentStatus;  ///< The current status of the module.

    std::string
        lastError;  ///< The last error message encountered by the module.

    int priority{0};  ///< The priority of the module.

    /**
     * @brief Represents statistics about the module.
     */
    struct Statistics {
        size_t functionCalls{
            0};            ///< The number of function calls made to the module.
        size_t errors{0};  ///< The number of errors encountered by the module.
        double avgResponseTime{
            0.0};              ///< The average response time of the module.
        double averageLoadTime;     ///< 平均加载时间
        std::size_t loadCount;      ///< 加载次数
        std::size_t failureCount;   ///< 失败次数
        std::chrono::system_clock::time_point lastAccess;  ///< 最后访问时间
    } ATOM_ALIGNAS(32) stats;  ///< Statistics about the module.
} ATOM_ALIGNAS(128);

}  // namespace lithium