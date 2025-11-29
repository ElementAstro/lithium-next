/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Common type definitions for shell script management
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_TYPES_HPP
#define LITHIUM_SCRIPT_SHELL_TYPES_HPP

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::shell {

/// Alias for script content
using Script = std::string;

/**
 * @brief Enumeration of supported script types
 */
enum class ScriptLanguage {
    Shell,       ///< Unix shell scripts (bash, sh)
    PowerShell,  ///< Windows PowerShell scripts
    Python,      ///< Python scripts
    Auto         ///< Auto-detect based on content/extension
};

/**
 * @brief Script execution progress information
 */
struct ScriptProgress {
    float percentage{0.0f};   ///< Progress 0.0-1.0
    std::string status;       ///< Current status message
    std::string currentStep;  ///< Current execution step
    std::chrono::system_clock::time_point timestamp;  ///< Last update time
    std::optional<std::string> output;  ///< Partial output if available
};

/**
 * @brief Script execution result with detailed information
 */
struct ScriptExecutionResult {
    bool success{false};      ///< Whether execution succeeded
    int exitCode{-1};         ///< Exit code
    std::string output;       ///< Standard output
    std::string errorOutput;  ///< Standard error
    std::chrono::milliseconds executionTime{0};  ///< Total execution time
    std::optional<std::string> exception;        ///< Exception message if any
    ScriptLanguage detectedLanguage{ScriptLanguage::Auto};  ///< Detected script type
};

/**
 * @brief Execution context passed to script executors
 */
struct ExecutionContext {
    std::unordered_map<std::string, std::string> arguments;     ///< Script arguments
    std::unordered_map<std::string, std::string> environment;   ///< Environment variables
    std::optional<std::filesystem::path> workingDirectory;      ///< Working directory
    std::optional<std::chrono::milliseconds> timeout;           ///< Execution timeout
    bool safe{true};                                            ///< Safe mode execution
    std::function<void(const ScriptProgress&)> progressCallback;///< Progress callback
};

/**
 * @brief Resource limits for script execution
 */
struct ScriptResourceLimits {
    size_t maxMemoryMB{1024};                     ///< Maximum memory in MB
    int maxCpuPercent{100};                       ///< Maximum CPU percentage
    std::chrono::seconds maxExecutionTime{3600};  ///< Maximum execution time
    size_t maxOutputSize{10 * 1024 * 1024};       ///< Max output size in bytes
    int maxConcurrentScripts{4};                  ///< Max concurrent executions
};

/**
 * @brief Callback types for hooks
 */
using PreExecutionHook = std::function<void(const std::string&)>;
using PostExecutionHook = std::function<void(const std::string&, int)>;
using ProgressCallback = std::function<void(const ScriptProgress&)>;
using TimeoutHandler = std::function<void()>;

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_TYPES_HPP
