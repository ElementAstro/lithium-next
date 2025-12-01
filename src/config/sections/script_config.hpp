/*
 * script_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Unified script engine configuration

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_SCRIPT_CONFIG_HPP
#define LITHIUM_CONFIG_SECTIONS_SCRIPT_CONFIG_HPP

#include <chrono>
#include <string>
#include <vector>

#include "../core/config_section.hpp"

namespace lithium::config {

/**
 * @brief Isolation level enumeration
 */
enum class IsolationLevel {
    None,        ///< No isolation - use embedded interpreter
    Subprocess,  ///< Subprocess isolation (separate process)
    Sandboxed    ///< Sandboxed subprocess with resource limits
};

/**
 * @brief Convert IsolationLevel to string
 */
[[nodiscard]] inline std::string isolationLevelToString(IsolationLevel level) {
    switch (level) {
        case IsolationLevel::None: return "none";
        case IsolationLevel::Subprocess: return "subprocess";
        case IsolationLevel::Sandboxed: return "sandboxed";
    }
    return "subprocess";
}

/**
 * @brief Convert string to IsolationLevel
 */
[[nodiscard]] inline IsolationLevel isolationLevelFromString(const std::string& str) {
    if (str == "none") return IsolationLevel::None;
    if (str == "subprocess") return IsolationLevel::Subprocess;
    if (str == "sandboxed") return IsolationLevel::Sandboxed;
    return IsolationLevel::Subprocess;
}

/**
 * @brief Interpreter pool configuration
 */
struct InterpreterPoolConfig {
    size_t poolSize{4};                      ///< Number of interpreters in pool
    size_t maxQueuedTasks{1000};             ///< Maximum queued tasks
    size_t taskTimeoutMs{30000};             ///< Default task timeout (ms)
    size_t acquireTimeoutMs{5000};           ///< Interpreter acquire timeout (ms)
    bool enableStatistics{true};             ///< Enable execution statistics
    bool preloadModules{false};              ///< Preload common modules
    std::vector<std::string> modulesToPreload;  ///< Modules to preload
    bool useSubinterpreters{false};          ///< Use Python 3.12+ sub-interpreters
    size_t workerThreads{0};                 ///< Worker threads (0 = poolSize)

    [[nodiscard]] json toJson() const {
        return {
            {"poolSize", poolSize},
            {"maxQueuedTasks", maxQueuedTasks},
            {"taskTimeoutMs", taskTimeoutMs},
            {"acquireTimeoutMs", acquireTimeoutMs},
            {"enableStatistics", enableStatistics},
            {"preloadModules", preloadModules},
            {"modulesToPreload", modulesToPreload},
            {"useSubinterpreters", useSubinterpreters},
            {"workerThreads", workerThreads}
        };
    }

    [[nodiscard]] static InterpreterPoolConfig fromJson(const json& j) {
        InterpreterPoolConfig cfg;
        cfg.poolSize = j.value("poolSize", cfg.poolSize);
        cfg.maxQueuedTasks = j.value("maxQueuedTasks", cfg.maxQueuedTasks);
        cfg.taskTimeoutMs = j.value("taskTimeoutMs", cfg.taskTimeoutMs);
        cfg.acquireTimeoutMs = j.value("acquireTimeoutMs", cfg.acquireTimeoutMs);
        cfg.enableStatistics = j.value("enableStatistics", cfg.enableStatistics);
        cfg.preloadModules = j.value("preloadModules", cfg.preloadModules);
        if (j.contains("modulesToPreload") && j["modulesToPreload"].is_array()) {
            cfg.modulesToPreload = j["modulesToPreload"].get<std::vector<std::string>>();
        }
        cfg.useSubinterpreters = j.value("useSubinterpreters", cfg.useSubinterpreters);
        cfg.workerThreads = j.value("workerThreads", cfg.workerThreads);
        return cfg;
    }
};

/**
 * @brief Isolation configuration for subprocess execution
 */
struct IsolationConfig {
    std::string level{"subprocess"};         ///< Isolation level: none, subprocess, sandboxed
    size_t maxMemoryMB{512};                 ///< Maximum memory in MB (0 = unlimited)
    int maxCpuPercent{100};                  ///< Maximum CPU percentage (0 = unlimited)
    size_t timeoutSeconds{300};              ///< Execution timeout in seconds
    bool allowNetwork{false};                ///< Allow network access
    bool allowFilesystem{true};              ///< Allow filesystem access
    std::vector<std::string> allowedPaths;   ///< Allowed filesystem paths
    std::vector<std::string> allowedImports; ///< Allowed module imports
    std::vector<std::string> blockedImports; ///< Blocked module imports
    std::string pythonExecutable;            ///< Python interpreter path (empty = auto-detect)
    std::string executorScript;              ///< Path to executor Python script
    std::string workingDirectory;            ///< Working directory for script
    bool captureOutput{true};                ///< Capture stdout/stderr
    bool enableProfiling{false};             ///< Enable performance profiling
    bool inheritEnvironment{true};           ///< Inherit parent environment

    [[nodiscard]] json toJson() const {
        return {
            {"level", level},
            {"maxMemoryMB", maxMemoryMB},
            {"maxCpuPercent", maxCpuPercent},
            {"timeoutSeconds", timeoutSeconds},
            {"allowNetwork", allowNetwork},
            {"allowFilesystem", allowFilesystem},
            {"allowedPaths", allowedPaths},
            {"allowedImports", allowedImports},
            {"blockedImports", blockedImports},
            {"pythonExecutable", pythonExecutable},
            {"executorScript", executorScript},
            {"workingDirectory", workingDirectory},
            {"captureOutput", captureOutput},
            {"enableProfiling", enableProfiling},
            {"inheritEnvironment", inheritEnvironment}
        };
    }

    [[nodiscard]] static IsolationConfig fromJson(const json& j) {
        IsolationConfig cfg;
        cfg.level = j.value("level", cfg.level);
        cfg.maxMemoryMB = j.value("maxMemoryMB", cfg.maxMemoryMB);
        cfg.maxCpuPercent = j.value("maxCpuPercent", cfg.maxCpuPercent);
        cfg.timeoutSeconds = j.value("timeoutSeconds", cfg.timeoutSeconds);
        cfg.allowNetwork = j.value("allowNetwork", cfg.allowNetwork);
        cfg.allowFilesystem = j.value("allowFilesystem", cfg.allowFilesystem);
        if (j.contains("allowedPaths") && j["allowedPaths"].is_array()) {
            cfg.allowedPaths = j["allowedPaths"].get<std::vector<std::string>>();
        }
        if (j.contains("allowedImports") && j["allowedImports"].is_array()) {
            cfg.allowedImports = j["allowedImports"].get<std::vector<std::string>>();
        }
        if (j.contains("blockedImports") && j["blockedImports"].is_array()) {
            cfg.blockedImports = j["blockedImports"].get<std::vector<std::string>>();
        }
        cfg.pythonExecutable = j.value("pythonExecutable", cfg.pythonExecutable);
        cfg.executorScript = j.value("executorScript", cfg.executorScript);
        cfg.workingDirectory = j.value("workingDirectory", cfg.workingDirectory);
        cfg.captureOutput = j.value("captureOutput", cfg.captureOutput);
        cfg.enableProfiling = j.value("enableProfiling", cfg.enableProfiling);
        cfg.inheritEnvironment = j.value("inheritEnvironment", cfg.inheritEnvironment);
        return cfg;
    }
};

/**
 * @brief Virtual environment configuration
 */
struct VenvConfig {
    std::string defaultPath{".venv"};        ///< Default venv path
    bool autoCreate{true};                   ///< Auto-create venv if not exists
    bool useCondaIfAvailable{false};         ///< Prefer conda over venv
    std::string condaPath;                   ///< Path to conda executable (empty = auto-detect)
    std::string defaultPythonVersion;        ///< Default Python version for new venvs
    size_t operationTimeoutSeconds{300};     ///< Timeout for venv operations
    std::vector<std::string> defaultPackages;  ///< Packages to install by default

    [[nodiscard]] json toJson() const {
        return {
            {"defaultPath", defaultPath},
            {"autoCreate", autoCreate},
            {"useCondaIfAvailable", useCondaIfAvailable},
            {"condaPath", condaPath},
            {"defaultPythonVersion", defaultPythonVersion},
            {"operationTimeoutSeconds", operationTimeoutSeconds},
            {"defaultPackages", defaultPackages}
        };
    }

    [[nodiscard]] static VenvConfig fromJson(const json& j) {
        VenvConfig cfg;
        cfg.defaultPath = j.value("defaultPath", cfg.defaultPath);
        cfg.autoCreate = j.value("autoCreate", cfg.autoCreate);
        cfg.useCondaIfAvailable = j.value("useCondaIfAvailable", cfg.useCondaIfAvailable);
        cfg.condaPath = j.value("condaPath", cfg.condaPath);
        cfg.defaultPythonVersion = j.value("defaultPythonVersion", cfg.defaultPythonVersion);
        cfg.operationTimeoutSeconds = j.value("operationTimeoutSeconds", cfg.operationTimeoutSeconds);
        if (j.contains("defaultPackages") && j["defaultPackages"].is_array()) {
            cfg.defaultPackages = j["defaultPackages"].get<std::vector<std::string>>();
        }
        return cfg;
    }
};

/**
 * @brief Shell script configuration
 */
struct ShellScriptConfig {
    std::string defaultShell;                ///< Default shell (empty = system default)
    size_t timeoutSeconds{60};               ///< Default script timeout
    bool captureOutput{true};                ///< Capture output by default
    bool inheritEnvironment{true};           ///< Inherit parent environment
    std::string scriptsDirectory{"scripts"}; ///< Directory for script files

    [[nodiscard]] json toJson() const {
        return {
            {"defaultShell", defaultShell},
            {"timeoutSeconds", timeoutSeconds},
            {"captureOutput", captureOutput},
            {"inheritEnvironment", inheritEnvironment},
            {"scriptsDirectory", scriptsDirectory}
        };
    }

    [[nodiscard]] static ShellScriptConfig fromJson(const json& j) {
        ShellScriptConfig cfg;
        cfg.defaultShell = j.value("defaultShell", cfg.defaultShell);
        cfg.timeoutSeconds = j.value("timeoutSeconds", cfg.timeoutSeconds);
        cfg.captureOutput = j.value("captureOutput", cfg.captureOutput);
        cfg.inheritEnvironment = j.value("inheritEnvironment", cfg.inheritEnvironment);
        cfg.scriptsDirectory = j.value("scriptsDirectory", cfg.scriptsDirectory);
        return cfg;
    }
};

/**
 * @brief Unified script engine configuration
 *
 * Consolidates interpreter pool, isolation, venv, and shell script settings.
 */
struct ScriptConfig : ConfigSection<ScriptConfig> {
    /// Configuration path
    static constexpr std::string_view PATH = "/lithium/script";

    // ========================================================================
    // General Settings
    // ========================================================================

    bool enablePython{true};                 ///< Enable Python script support
    bool enableShell{true};                  ///< Enable shell script support
    std::string analysisConfigPath{"./config/script/analysis.json"};  ///< Script analysis config

    // ========================================================================
    // Interpreter Pool
    // ========================================================================

    InterpreterPoolConfig interpreterPool;

    // ========================================================================
    // Isolation
    // ========================================================================

    IsolationConfig isolation;

    // ========================================================================
    // Virtual Environment
    // ========================================================================

    VenvConfig venv;

    // ========================================================================
    // Shell Script
    // ========================================================================

    ShellScriptConfig shell;

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json serialize() const {
        return {
            // General
            {"enablePython", enablePython},
            {"enableShell", enableShell},
            {"analysisConfigPath", analysisConfigPath},
            // Nested configs
            {"interpreterPool", interpreterPool.toJson()},
            {"isolation", isolation.toJson()},
            {"venv", venv.toJson()},
            {"shell", shell.toJson()}
        };
    }

    [[nodiscard]] static ScriptConfig deserialize(const json& j) {
        ScriptConfig cfg;

        // General
        cfg.enablePython = j.value("enablePython", cfg.enablePython);
        cfg.enableShell = j.value("enableShell", cfg.enableShell);
        cfg.analysisConfigPath = j.value("analysisConfigPath", cfg.analysisConfigPath);

        // Nested configs
        if (j.contains("interpreterPool")) {
            cfg.interpreterPool = InterpreterPoolConfig::fromJson(j["interpreterPool"]);
        }
        if (j.contains("isolation")) {
            cfg.isolation = IsolationConfig::fromJson(j["isolation"]);
        }
        if (j.contains("venv")) {
            cfg.venv = VenvConfig::fromJson(j["venv"]);
        }
        if (j.contains("shell")) {
            cfg.shell = ShellScriptConfig::fromJson(j["shell"]);
        }

        return cfg;
    }

    [[nodiscard]] static json generateSchema() {
        return {
            {"type", "object"},
            {"properties", {
                // General
                {"enablePython", {{"type", "boolean"}, {"default", true}}},
                {"enableShell", {{"type", "boolean"}, {"default", true}}},
                {"analysisConfigPath", {{"type", "string"}}},
                // Interpreter Pool
                {"interpreterPool", {
                    {"type", "object"},
                    {"properties", {
                        {"poolSize", {{"type", "integer"}, {"minimum", 1}, {"maximum", 32}, {"default", 4}}},
                        {"maxQueuedTasks", {{"type", "integer"}, {"minimum", 1}, {"default", 1000}}},
                        {"taskTimeoutMs", {{"type", "integer"}, {"minimum", 0}, {"default", 30000}}},
                        {"acquireTimeoutMs", {{"type", "integer"}, {"minimum", 0}, {"default", 5000}}},
                        {"enableStatistics", {{"type", "boolean"}, {"default", true}}},
                        {"preloadModules", {{"type", "boolean"}, {"default", false}}},
                        {"modulesToPreload", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                        {"useSubinterpreters", {{"type", "boolean"}, {"default", false}}},
                        {"workerThreads", {{"type", "integer"}, {"minimum", 0}, {"default", 0}}}
                    }}
                }},
                // Isolation
                {"isolation", {
                    {"type", "object"},
                    {"properties", {
                        {"level", {{"type", "string"}, {"enum", {"none", "subprocess", "sandboxed"}}, {"default", "subprocess"}}},
                        {"maxMemoryMB", {{"type", "integer"}, {"minimum", 0}, {"default", 512}}},
                        {"maxCpuPercent", {{"type", "integer"}, {"minimum", 0}, {"maximum", 100}, {"default", 100}}},
                        {"timeoutSeconds", {{"type", "integer"}, {"minimum", 0}, {"default", 300}}},
                        {"allowNetwork", {{"type", "boolean"}, {"default", false}}},
                        {"allowFilesystem", {{"type", "boolean"}, {"default", true}}},
                        {"allowedPaths", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                        {"allowedImports", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                        {"blockedImports", {{"type", "array"}, {"items", {{"type", "string"}}}}},
                        {"captureOutput", {{"type", "boolean"}, {"default", true}}},
                        {"enableProfiling", {{"type", "boolean"}, {"default", false}}},
                        {"inheritEnvironment", {{"type", "boolean"}, {"default", true}}}
                    }}
                }},
                // Venv
                {"venv", {
                    {"type", "object"},
                    {"properties", {
                        {"defaultPath", {{"type", "string"}, {"default", ".venv"}}},
                        {"autoCreate", {{"type", "boolean"}, {"default", true}}},
                        {"useCondaIfAvailable", {{"type", "boolean"}, {"default", false}}},
                        {"condaPath", {{"type", "string"}}},
                        {"defaultPythonVersion", {{"type", "string"}}},
                        {"operationTimeoutSeconds", {{"type", "integer"}, {"minimum", 0}, {"default", 300}}},
                        {"defaultPackages", {{"type", "array"}, {"items", {{"type", "string"}}}}}
                    }}
                }},
                // Shell
                {"shell", {
                    {"type", "object"},
                    {"properties", {
                        {"defaultShell", {{"type", "string"}}},
                        {"timeoutSeconds", {{"type", "integer"}, {"minimum", 0}, {"default", 60}}},
                        {"captureOutput", {{"type", "boolean"}, {"default", true}}},
                        {"inheritEnvironment", {{"type", "boolean"}, {"default", true}}},
                        {"scriptsDirectory", {{"type", "string"}, {"default", "scripts"}}}
                    }}
                }}
            }}
        };
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_SCRIPT_CONFIG_HPP
