/*
 * python_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file python_executor.hpp
 * @brief Python script executor implementation
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_EXECUTION_PYTHON_EXECUTOR_HPP
#define LITHIUM_SCRIPT_SHELL_EXECUTION_PYTHON_EXECUTOR_HPP

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "executor.hpp"

namespace lithium::shell {

/**
 * @brief Python script configuration
 */
struct PythonScriptConfig {
    std::string moduleName;             ///< Python module name
    std::string entryFunction;          ///< Entry function to call
    std::vector<std::string> sysPaths;  ///< Additional sys.path entries
    std::unordered_map<std::string, std::string> envVars;  ///< Environment variables
    bool useVirtualEnv{false};                  ///< Use virtual environment
    std::string virtualEnvPath;                 ///< Virtual environment path
    std::vector<std::string> requiredPackages;  ///< Required pip packages
    bool enableProfiling{false};        ///< Enable performance profiling
    size_t memoryLimitMB{0};            ///< Memory limit (0 = unlimited)
    std::chrono::seconds timeout{300};  ///< Execution timeout
};

/**
 * @brief Python script executor implementation
 *
 * Executes Python scripts with support for:
 * - Virtual environment activation
 * - Module and function calls
 * - sys.path management
 * - Progress tracking
 */
class PythonExecutor : public IScriptExecutor {
public:
    PythonExecutor();
    ~PythonExecutor() override;

    // Non-copyable, movable
    PythonExecutor(const PythonExecutor&) = delete;
    PythonExecutor& operator=(const PythonExecutor&) = delete;
    PythonExecutor(PythonExecutor&&) noexcept;
    PythonExecutor& operator=(PythonExecutor&&) noexcept;

    auto execute(const Script& script, const ExecutionContext& ctx)
        -> ScriptExecutionResult override;

    [[nodiscard]] auto supports(ScriptLanguage lang) const noexcept
        -> bool override;

    [[nodiscard]] auto primaryLanguage() const noexcept
        -> ScriptLanguage override;

    void abort() override;

    [[nodiscard]] auto isRunning() const noexcept -> bool override;

    /**
     * @brief Execute with specific Python configuration
     * @param config Python script configuration
     * @param ctx Execution context
     * @return Execution result
     */
    auto executeWithConfig(const PythonScriptConfig& config,
                           const ExecutionContext& ctx)
        -> ScriptExecutionResult;

    /**
     * @brief Add a path to Python's sys.path
     * @param path Path to add
     */
    void addSysPath(const std::filesystem::path& path);

    /**
     * @brief Set the Python executable to use
     * @param path Path to Python executable
     */
    void setPythonExecutable(const std::filesystem::path& path);

    /**
     * @brief Check if Python is available
     * @return true if Python is available
     */
    [[nodiscard]] auto isPythonAvailable() const -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_EXECUTION_PYTHON_EXECUTOR_HPP
