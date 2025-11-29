/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Isolated Python Runner type definitions
 * @date 2024
 * @version 1.0.0
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_TYPES_HPP
#define LITHIUM_SCRIPT_ISOLATED_TYPES_HPP

#include <nlohmann/json.hpp>

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lithium::isolated {

/**
 * @brief Isolation level for Python execution
 */
enum class IsolationLevel {
    None,           ///< No isolation - use embedded interpreter
    Subprocess,     ///< Subprocess isolation (separate process)
    Sandboxed       ///< Sandboxed subprocess with resource limits
};

/**
 * @brief Get string representation of IsolationLevel
 */
[[nodiscard]] constexpr std::string_view isolationLevelToString(IsolationLevel level) noexcept {
    switch (level) {
        case IsolationLevel::None: return "None";
        case IsolationLevel::Subprocess: return "Subprocess";
        case IsolationLevel::Sandboxed: return "Sandboxed";
    }
    return "Unknown";
}

/**
 * @brief Error codes for isolated runner
 */
enum class RunnerError {
    Success = 0,
    ProcessSpawnFailed,
    ProcessCrashed,
    ProcessKilled,
    HandshakeFailed,
    CommunicationError,
    Timeout,
    MemoryLimitExceeded,
    CpuLimitExceeded,
    ExecutionFailed,
    Cancelled,
    InvalidConfiguration,
    PythonNotFound,
    ExecutorNotFound,
    UnknownError
};

/**
 * @brief Get string representation of RunnerError
 */
[[nodiscard]] constexpr std::string_view runnerErrorToString(RunnerError error) noexcept {
    switch (error) {
        case RunnerError::Success: return "Success";
        case RunnerError::ProcessSpawnFailed: return "Process spawn failed";
        case RunnerError::ProcessCrashed: return "Process crashed";
        case RunnerError::ProcessKilled: return "Process killed";
        case RunnerError::HandshakeFailed: return "Handshake failed";
        case RunnerError::CommunicationError: return "Communication error";
        case RunnerError::Timeout: return "Timeout";
        case RunnerError::MemoryLimitExceeded: return "Memory limit exceeded";
        case RunnerError::CpuLimitExceeded: return "CPU limit exceeded";
        case RunnerError::ExecutionFailed: return "Execution failed";
        case RunnerError::Cancelled: return "Cancelled";
        case RunnerError::InvalidConfiguration: return "Invalid configuration";
        case RunnerError::PythonNotFound: return "Python not found";
        case RunnerError::ExecutorNotFound: return "Executor script not found";
        case RunnerError::UnknownError: return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for isolated runner operations
 */
template<typename T>
using Result = std::expected<T, RunnerError>;

/**
 * @brief Configuration for isolated Python execution
 */
struct IsolationConfig {
    IsolationLevel level{IsolationLevel::Subprocess};

    // Resource limits
    size_t maxMemoryMB{512};            ///< Maximum memory in MB (0 = unlimited)
    int maxCpuPercent{100};             ///< Maximum CPU percentage (0 = unlimited)
    std::chrono::seconds timeout{300};  ///< Execution timeout

    // Security
    bool allowNetwork{false};           ///< Allow network access
    bool allowFilesystem{true};         ///< Allow filesystem access
    std::vector<std::filesystem::path> allowedPaths;  ///< Allowed filesystem paths
    std::vector<std::string> allowedImports;          ///< Allowed module imports
    std::vector<std::string> blockedImports;          ///< Blocked module imports

    // Environment
    std::filesystem::path workingDirectory;  ///< Working directory for script
    std::filesystem::path pythonExecutable;  ///< Python interpreter path
    std::filesystem::path executorScript;    ///< Path to executor Python script
    std::vector<std::string> pythonPath;     ///< Additional Python paths
    std::unordered_map<std::string, std::string> environmentVariables;  ///< Env vars

    // Options
    bool captureOutput{true};           ///< Capture stdout/stderr
    bool enableProfiling{false};        ///< Enable performance profiling
    bool inheritEnvironment{true};      ///< Inherit parent environment
};

/**
 * @brief Result of isolated script execution
 */
struct ExecutionResult {
    bool success{false};                ///< Whether execution succeeded
    int exitCode{-1};                   ///< Process exit code
    std::string output;                 ///< Captured stdout
    std::string errorOutput;            ///< Captured stderr
    nlohmann::json result;              ///< Result data (if any)
    std::string exception;              ///< Exception message
    std::string exceptionType;          ///< Exception type name
    std::string traceback;              ///< Python traceback
    std::chrono::milliseconds executionTime{0};  ///< Total execution time
    size_t peakMemoryUsage{0};          ///< Peak memory usage in bytes
    std::optional<RunnerError> error;   ///< Error code if failed
};

/**
 * @brief Progress callback type
 */
using ProgressCallback = std::function<void(
    float percentage,
    std::string_view message,
    std::string_view currentStep
)>;

/**
 * @brief Log callback type
 */
using LogCallback = std::function<void(
    std::string_view level,
    std::string_view message
)>;

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_TYPES_HPP
