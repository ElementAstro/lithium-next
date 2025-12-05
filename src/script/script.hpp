/**
 * @file script.hpp
 * @brief Unified facade header for the lithium script module.
 *
 * This header provides access to all script module components:
 * - ScriptService: Unified script execution facade
 * - PythonWrapper: In-process Python execution
 * - InterpreterPool: Concurrent Python execution
 * - ScriptAnalyzer: Script security analysis
 * - Shell scripts, tools, venv management
 *
 * @par Usage Example:
 * @code
 * #include "script/script.hpp"
 *
 * using namespace lithium;
 *
 * // Create script service
 * auto service = createScriptService();
 * service->initialize();
 *
 * // Execute Python code
 * auto result = service->executePython("print('Hello')");
 *
 * // Execute shell script
 * auto [output, exitCode] = service->executeShellScript("backup");
 * @endcode
 *
 * @date 2024
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_SCRIPT_HPP
#define LITHIUM_SCRIPT_HPP

#include <memory>
#include <string>

// Core script components
#include "check.hpp"
#include "interpreter_pool.hpp"
#include "python_caller.hpp"
#include "script_service.hpp"

namespace lithium {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Script module version.
 */
inline constexpr const char* SCRIPT_VERSION = "1.1.0";

/**
 * @brief Get script module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getScriptVersion() noexcept {
    return SCRIPT_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to ScriptService
using ScriptServicePtr = std::shared_ptr<ScriptService>;

/// Shared pointer to PythonWrapper
using PythonWrapperPtr = std::shared_ptr<PythonWrapper>;

/// Shared pointer to InterpreterPool
using InterpreterPoolPtr = std::shared_ptr<InterpreterPool>;

/// Shared pointer to ScriptAnalyzer
using ScriptAnalyzerPtr = std::shared_ptr<ScriptAnalyzer>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new ScriptService instance.
 * @return Shared pointer to the new ScriptService.
 */
[[nodiscard]] inline ScriptServicePtr createScriptService() {
    return std::make_shared<ScriptService>();
}

/**
 * @brief Create a new ScriptService instance with custom config.
 * @param config Service configuration.
 * @return Shared pointer to the new ScriptService.
 */
[[nodiscard]] inline ScriptServicePtr createScriptService(
    const ScriptServiceConfig& config) {
    return std::make_shared<ScriptService>(config);
}

/**
 * @brief Create a new InterpreterPool instance.
 * @param poolSize Number of interpreters in the pool.
 * @return Shared pointer to the new InterpreterPool.
 */
[[nodiscard]] inline InterpreterPoolPtr createInterpreterPool(
    size_t poolSize = 4) {
    return std::make_shared<InterpreterPool>(poolSize);
}

/**
 * @brief Create a new ScriptAnalyzer instance.
 * @return Shared pointer to the new ScriptAnalyzer.
 */
[[nodiscard]] inline ScriptAnalyzerPtr createScriptAnalyzer() {
    return std::make_shared<ScriptAnalyzer>();
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Create default ScriptServiceConfig.
 * @return Default ScriptServiceConfig.
 */
[[nodiscard]] inline ScriptServiceConfig createDefaultScriptConfig() {
    return ScriptServiceConfig{};
}

/**
 * @brief Create ScriptServiceConfig with custom pool size.
 * @param poolSize Number of interpreters in the pool.
 * @return ScriptServiceConfig with custom pool size.
 */
[[nodiscard]] inline ScriptServiceConfig createScriptConfig(size_t poolSize) {
    ScriptServiceConfig config;
    config.poolSize = poolSize;
    return config;
}

/**
 * @brief Create default ScriptExecutionConfig.
 * @return Default ScriptExecutionConfig.
 */
[[nodiscard]] inline ScriptExecutionConfig createDefaultExecutionConfig() {
    return ScriptExecutionConfig{};
}

/**
 * @brief Create ScriptExecutionConfig for isolated execution.
 * @return ScriptExecutionConfig with isolated mode.
 */
[[nodiscard]] inline ScriptExecutionConfig createIsolatedExecutionConfig() {
    ScriptExecutionConfig config;
    config.mode = ExecutionMode::Isolated;
    config.validateBeforeExecution = true;
    return config;
}

/**
 * @brief Create ScriptExecutionConfig for pooled execution.
 * @return ScriptExecutionConfig with pooled mode.
 */
[[nodiscard]] inline ScriptExecutionConfig createPooledExecutionConfig() {
    ScriptExecutionConfig config;
    config.mode = ExecutionMode::Pooled;
    return config;
}

/**
 * @brief Convert ScriptServiceError to string.
 * @param error The error code.
 * @return String representation of the error.
 */
[[nodiscard]] inline std::string scriptErrorToString(ScriptServiceError error) {
    return std::string(scriptServiceErrorToString(error));
}

/**
 * @brief Check if script execution was successful.
 * @param result The execution result.
 * @return True if execution was successful.
 */
[[nodiscard]] inline bool isExecutionSuccessful(
    const ScriptExecutionResult& result) {
    return result.success;
}

/**
 * @brief Get execution time in milliseconds.
 * @param result The execution result.
 * @return Execution time in milliseconds.
 */
[[nodiscard]] inline int64_t getExecutionTimeMs(
    const ScriptExecutionResult& result) {
    return result.executionTime.count();
}

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_HPP
