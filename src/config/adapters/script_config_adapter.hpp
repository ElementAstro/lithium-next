/*
 * script_config_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Adapter to convert unified ScriptConfig to legacy component configs

**************************************************/

#ifndef LITHIUM_CONFIG_ADAPTERS_SCRIPT_CONFIG_ADAPTER_HPP
#define LITHIUM_CONFIG_ADAPTERS_SCRIPT_CONFIG_ADAPTER_HPP

#include "../sections/script_config.hpp"
#include "script/interpreter_pool.hpp"
#include "script/isolated/types.hpp"

namespace lithium::config {

/**
 * @brief Convert unified InterpreterPoolConfig to script engine's config
 */
[[nodiscard]] inline lithium::InterpreterPoolConfig toInterpreterPoolConfig(
    const config::InterpreterPoolConfig& unified) {
    lithium::InterpreterPoolConfig config;
    config.poolSize = unified.poolSize;
    config.maxQueuedTasks = unified.maxQueuedTasks;
    config.taskTimeout = std::chrono::milliseconds(unified.taskTimeoutMs);
    config.acquireTimeout = std::chrono::milliseconds(unified.acquireTimeoutMs);
    config.enableStatistics = unified.enableStatistics;
    config.preloadModules = unified.preloadModules;
    config.modulesToPreload = unified.modulesToPreload;
    config.useSubinterpreters = unified.useSubinterpreters;
    config.workerThreads = unified.workerThreads;
    return config;
}

/**
 * @brief Convert unified IsolationConfig to isolated runner's config
 */
[[nodiscard]] inline lithium::isolated::IsolationConfig toIsolationConfig(
    const config::IsolationConfig& unified) {
    lithium::isolated::IsolationConfig config;

    // Convert level string to enum
    if (unified.level == "none") {
        config.level = lithium::isolated::IsolationLevel::None;
    } else if (unified.level == "subprocess") {
        config.level = lithium::isolated::IsolationLevel::Subprocess;
    } else if (unified.level == "sandboxed") {
        config.level = lithium::isolated::IsolationLevel::Sandboxed;
    }

    config.maxMemoryMB = unified.maxMemoryMB;
    config.maxCpuPercent = unified.maxCpuPercent;
    config.timeout = std::chrono::seconds(unified.timeoutSeconds);
    config.allowNetwork = unified.allowNetwork;
    config.allowFilesystem = unified.allowFilesystem;

    // Convert string paths to filesystem paths
    for (const auto& path : unified.allowedPaths) {
        config.allowedPaths.emplace_back(path);
    }

    config.allowedImports = unified.allowedImports;
    config.blockedImports = unified.blockedImports;

    if (!unified.pythonExecutable.empty()) {
        config.pythonExecutable = unified.pythonExecutable;
    }
    if (!unified.executorScript.empty()) {
        config.executorScript = unified.executorScript;
    }
    if (!unified.workingDirectory.empty()) {
        config.workingDirectory = unified.workingDirectory;
    }

    config.captureOutput = unified.captureOutput;
    config.enableProfiling = unified.enableProfiling;
    config.inheritEnvironment = unified.inheritEnvironment;

    return config;
}

/**
 * @brief Convert isolated runner's IsolationConfig back to unified format
 */
[[nodiscard]] inline config::IsolationConfig fromIsolationConfig(
    const lithium::isolated::IsolationConfig& legacy) {
    config::IsolationConfig config;

    // Convert enum to string
    switch (legacy.level) {
        case lithium::isolated::IsolationLevel::None:
            config.level = "none";
            break;
        case lithium::isolated::IsolationLevel::Subprocess:
            config.level = "subprocess";
            break;
        case lithium::isolated::IsolationLevel::Sandboxed:
            config.level = "sandboxed";
            break;
    }

    config.maxMemoryMB = legacy.maxMemoryMB;
    config.maxCpuPercent = legacy.maxCpuPercent;
    config.timeoutSeconds = legacy.timeout.count();
    config.allowNetwork = legacy.allowNetwork;
    config.allowFilesystem = legacy.allowFilesystem;

    // Convert filesystem paths to strings
    for (const auto& path : legacy.allowedPaths) {
        config.allowedPaths.push_back(path.string());
    }

    config.allowedImports = legacy.allowedImports;
    config.blockedImports = legacy.blockedImports;
    config.pythonExecutable = legacy.pythonExecutable.string();
    config.executorScript = legacy.executorScript.string();
    config.workingDirectory = legacy.workingDirectory.string();
    config.captureOutput = legacy.captureOutput;
    config.enableProfiling = legacy.enableProfiling;
    config.inheritEnvironment = legacy.inheritEnvironment;

    return config;
}

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_ADAPTERS_SCRIPT_CONFIG_ADAPTER_HPP
