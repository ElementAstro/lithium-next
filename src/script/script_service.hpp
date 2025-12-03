/*
 * script_service.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file script_service.hpp
 * @brief Unified Script Service - Facade for all script execution capabilities
 * @date 2024
 * @version 1.0.0
 *
 * This module provides a unified interface for all script-related functionality:
 * - In-process Python execution (PythonWrapper)
 * - Concurrent Python execution (InterpreterPool)
 * - Isolated/sandboxed execution (IsolatedRunner)
 * - Shell script management (ScriptManager)
 * - Python tool registry (ToolRegistry)
 * - Virtual environment management (VenvManager)
 * - Script security analysis (ScriptAnalyzer)
 * - NumPy/scientific computing support
 */

#ifndef LITHIUM_SCRIPT_SERVICE_HPP
#define LITHIUM_SCRIPT_SERVICE_HPP

#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/json_fwd.hpp"

namespace lithium {

// Forward declarations
class PythonWrapper;
class InterpreterPool;
class ScriptAnalyzer;

namespace isolated {
class PythonRunner;
}

namespace shell {
class ScriptManager;
}

namespace tools {
class PythonToolRegistry;
}

namespace venv {
class VenvManager;
}

/**
 * @brief Error codes for ScriptService operations
 */
enum class ScriptServiceError {
    Success = 0,
    NotInitialized,
    ExecutionFailed,
    ValidationFailed,
    SecurityViolation,
    Timeout,
    ResourceExhausted,
    ModuleNotFound,
    FunctionNotFound,
    InvalidArguments,
    InternalError
};

/**
 * @brief Convert error to string
 */
[[nodiscard]] constexpr std::string_view scriptServiceErrorToString(
    ScriptServiceError error) noexcept {
    switch (error) {
        case ScriptServiceError::Success: return "Success";
        case ScriptServiceError::NotInitialized: return "Service not initialized";
        case ScriptServiceError::ExecutionFailed: return "Execution failed";
        case ScriptServiceError::ValidationFailed: return "Validation failed";
        case ScriptServiceError::SecurityViolation: return "Security violation";
        case ScriptServiceError::Timeout: return "Operation timed out";
        case ScriptServiceError::ResourceExhausted: return "Resources exhausted";
        case ScriptServiceError::ModuleNotFound: return "Module not found";
        case ScriptServiceError::FunctionNotFound: return "Function not found";
        case ScriptServiceError::InvalidArguments: return "Invalid arguments";
        case ScriptServiceError::InternalError: return "Internal error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for script operations
 */
template<typename T>
using ScriptResult = std::expected<T, ScriptServiceError>;

/**
 * @brief Execution mode for Python scripts
 */
enum class ExecutionMode {
    InProcess,      ///< Direct execution via PythonWrapper (fastest, least isolated)
    Pooled,         ///< Execution via InterpreterPool (concurrent, moderate isolation)
    Isolated,       ///< Sandboxed subprocess execution (slowest, most secure)
    Auto            ///< Automatically select based on script analysis
};

/**
 * @brief Configuration for script execution
 */
struct ScriptExecutionConfig {
    ExecutionMode mode{ExecutionMode::Auto};
    std::chrono::milliseconds timeout{30000};
    size_t maxMemoryMB{512};
    bool validateBeforeExecution{true};
    bool captureOutput{true};
    std::vector<std::string> allowedImports;
    std::vector<std::string> blockedImports;
    std::filesystem::path workingDirectory;
};

/**
 * @brief Result of script execution
 */
struct ScriptExecutionResult {
    bool success{false};
    nlohmann::json result;
    std::string stdoutOutput;
    std::string stderrOutput;
    std::string errorMessage;
    std::chrono::milliseconds executionTime{0};
    size_t memoryUsed{0};
    ExecutionMode actualMode{ExecutionMode::InProcess};
};

/**
 * @brief Configuration for ScriptService initialization
 */
struct ScriptServiceConfig {
    // InterpreterPool settings
    size_t poolSize{4};
    size_t maxQueuedTasks{1000};

    // Security settings
    bool enableSecurityAnalysis{true};
    std::filesystem::path analysisConfigPath{"./config/script/analysis.json"};

    // Virtual environment settings
    std::filesystem::path defaultVenvPath;
    bool autoActivateVenv{false};

    // Tool registry settings
    std::filesystem::path toolsDirectory{"./python/tools"};
    bool autoDiscoverTools{true};

    // Shell script settings
    std::filesystem::path scriptsDirectory{"./scripts"};
};

/**
 * @brief Progress callback for long-running operations
 */
using ScriptProgressCallback = std::function<void(double progress, const std::string& message)>;

/**
 * @brief Log callback for script output
 */
using ScriptLogCallback = std::function<void(const std::string& level, const std::string& message)>;

/**
 * @brief Unified Script Service
 *
 * Provides a single entry point for all script-related operations,
 * coordinating multiple subsystems for optimal execution.
 */
class ScriptService {
public:
    /**
     * @brief Constructs ScriptService with default configuration
     */
    ScriptService();

    /**
     * @brief Constructs ScriptService with specified configuration
     */
    explicit ScriptService(const ScriptServiceConfig& config);

    /**
     * @brief Destructor
     */
    ~ScriptService();

    // Disable copy
    ScriptService(const ScriptService&) = delete;
    ScriptService& operator=(const ScriptService&) = delete;

    // Enable move
    ScriptService(ScriptService&&) noexcept;
    ScriptService& operator=(ScriptService&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the service with all subsystems
     * @return Success or error
     */
    [[nodiscard]] ScriptResult<void> initialize();

    /**
     * @brief Shutdown the service gracefully
     * @param waitForPending Wait for pending tasks to complete
     */
    void shutdown(bool waitForPending = true);

    /**
     * @brief Check if service is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    // =========================================================================
    // Python Execution
    // =========================================================================

    /**
     * @brief Execute Python code
     * @param code Python code to execute
     * @param args Arguments as JSON
     * @param config Execution configuration
     * @return Execution result
     */
    [[nodiscard]] ScriptExecutionResult executePython(
        std::string_view code,
        const nlohmann::json& args = {},
        const ScriptExecutionConfig& config = {});

    /**
     * @brief Execute Python file
     * @param path Path to Python file
     * @param args Arguments as JSON
     * @param config Execution configuration
     * @return Execution result
     */
    [[nodiscard]] ScriptExecutionResult executePythonFile(
        const std::filesystem::path& path,
        const nlohmann::json& args = {},
        const ScriptExecutionConfig& config = {});

    /**
     * @brief Execute Python function
     * @param moduleName Module containing the function
     * @param functionName Function to call
     * @param args Arguments as JSON
     * @param config Execution configuration
     * @return Execution result
     */
    [[nodiscard]] ScriptExecutionResult executePythonFunction(
        std::string_view moduleName,
        std::string_view functionName,
        const nlohmann::json& args = {},
        const ScriptExecutionConfig& config = {});

    /**
     * @brief Execute Python code asynchronously
     * @param code Python code to execute
     * @param args Arguments as JSON
     * @param config Execution configuration
     * @return Future with execution result
     */
    [[nodiscard]] std::future<ScriptExecutionResult> executePythonAsync(
        std::string_view code,
        const nlohmann::json& args = {},
        const ScriptExecutionConfig& config = {});

    // =========================================================================
    // Shell Script Execution
    // =========================================================================

    /**
     * @brief Execute a registered shell script
     * @param scriptName Name of the registered script
     * @param args Script arguments
     * @param safe Whether to run in safe mode
     * @return Output and exit code
     */
    [[nodiscard]] ScriptResult<std::pair<std::string, int>> executeShellScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args = {},
        bool safe = true);

    /**
     * @brief List all registered shell scripts
     */
    [[nodiscard]] std::vector<std::string> listShellScripts() const;

    // =========================================================================
    // Tool Registry
    // =========================================================================

    /**
     * @brief Invoke a registered Python tool function
     * @param toolName Name of the tool
     * @param functionName Function to invoke
     * @param args Arguments as JSON
     * @return Invocation result
     */
    [[nodiscard]] ScriptResult<nlohmann::json> invokeTool(
        const std::string& toolName,
        const std::string& functionName,
        const nlohmann::json& args = {});

    /**
     * @brief List all registered tools
     */
    [[nodiscard]] std::vector<std::string> listTools() const;

    /**
     * @brief Discover new tools in the tools directory
     * @return Number of newly discovered tools
     */
    [[nodiscard]] ScriptResult<size_t> discoverTools();

    // =========================================================================
    // Virtual Environment
    // =========================================================================

    /**
     * @brief Create a new virtual environment
     * @param path Path for the new venv
     * @param pythonVersion Optional Python version
     * @return Venv info on success
     */
    [[nodiscard]] ScriptResult<nlohmann::json> createVirtualEnv(
        const std::filesystem::path& path,
        const std::string& pythonVersion = "");

    /**
     * @brief Activate a virtual environment
     * @param path Path to the venv
     * @return Success or error
     */
    [[nodiscard]] ScriptResult<void> activateVirtualEnv(
        const std::filesystem::path& path);

    /**
     * @brief Deactivate current virtual environment
     */
    [[nodiscard]] ScriptResult<void> deactivateVirtualEnv();

    /**
     * @brief Install a Python package
     * @param package Package name (with optional version)
     * @param upgrade Whether to upgrade if already installed
     */
    [[nodiscard]] ScriptResult<void> installPackage(
        const std::string& package,
        bool upgrade = false);

    /**
     * @brief List installed packages
     */
    [[nodiscard]] ScriptResult<std::vector<nlohmann::json>> listPackages() const;

    // =========================================================================
    // Security & Analysis
    // =========================================================================

    /**
     * @brief Analyze script for security issues
     * @param script Script content to analyze
     * @return Analysis result with danger items
     */
    [[nodiscard]] nlohmann::json analyzeScript(const std::string& script);

    /**
     * @brief Validate script is safe to execute
     * @param script Script content to validate
     * @return True if script passes security checks
     */
    [[nodiscard]] bool validateScript(const std::string& script);

    /**
     * @brief Get safe version of a script
     * @param script Original script
     * @return Sanitized script
     */
    [[nodiscard]] std::string getSafeScript(const std::string& script);

    // =========================================================================
    // NumPy/Scientific Computing
    // =========================================================================

    /**
     * @brief Execute NumPy array operation
     * @param operation Operation name
     * @param arrays Input arrays as JSON
     * @param params Operation parameters
     * @return Result array as JSON
     */
    [[nodiscard]] ScriptResult<nlohmann::json> executeNumpyOp(
        const std::string& operation,
        const nlohmann::json& arrays,
        const nlohmann::json& params = {});

    // =========================================================================
    // Subsystem Access
    // =========================================================================

    /**
     * @brief Get the underlying PythonWrapper
     * @note Use with caution - prefer high-level API
     */
    [[nodiscard]] std::shared_ptr<PythonWrapper> getPythonWrapper() const;

    /**
     * @brief Get the underlying InterpreterPool
     * @note Use with caution - prefer high-level API
     */
    [[nodiscard]] std::shared_ptr<InterpreterPool> getInterpreterPool() const;

    /**
     * @brief Get the underlying IsolatedRunner
     * @note Use with caution - prefer high-level API
     */
    [[nodiscard]] std::shared_ptr<isolated::PythonRunner> getIsolatedRunner() const;

    /**
     * @brief Get the underlying ScriptManager
     */
    [[nodiscard]] std::shared_ptr<shell::ScriptManager> getScriptManager() const;

    /**
     * @brief Get the underlying ToolRegistry
     */
    [[nodiscard]] std::shared_ptr<tools::PythonToolRegistry> getToolRegistry() const;

    /**
     * @brief Get the underlying VenvManager
     */
    [[nodiscard]] std::shared_ptr<venv::VenvManager> getVenvManager() const;

    /**
     * @brief Get the underlying ScriptAnalyzer
     */
    [[nodiscard]] std::shared_ptr<ScriptAnalyzer> getScriptAnalyzer() const;

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set progress callback for long-running operations
     */
    void setProgressCallback(ScriptProgressCallback callback);

    /**
     * @brief Set log callback for script output
     */
    void setLogCallback(ScriptLogCallback callback);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get service statistics
     */
    [[nodiscard]] nlohmann::json getStatistics() const;

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_SERVICE_HPP
