/*
 * runner.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file runner.hpp
 * @brief Isolated Python Script Runner - Public API
 * @date 2024
 * @version 1.0.0
 *
 * This is the main public interface for isolated Python script execution.
 * It provides a clean facade over the internal execution components.
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_RUNNER_HPP
#define LITHIUM_SCRIPT_ISOLATED_RUNNER_HPP

#include "types.hpp"

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lithium::isolated {

/**
 * @brief Isolated Python Runner
 *
 * Executes Python scripts in isolated subprocesses with resource
 * limiting, timeout handling, and cancellation support.
 */
class PythonRunner {
public:
    /**
     * @brief Constructs a PythonRunner with default configuration
     */
    PythonRunner();

    /**
     * @brief Constructs a PythonRunner with specified configuration
     */
    explicit PythonRunner(const IsolationConfig& config);

    /**
     * @brief Destructor - kills any running process
     */
    ~PythonRunner();

    // Disable copy
    PythonRunner(const PythonRunner&) = delete;
    PythonRunner& operator=(const PythonRunner&) = delete;

    // Enable move
    PythonRunner(PythonRunner&&) noexcept;
    PythonRunner& operator=(PythonRunner&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Sets the isolation configuration
     */
    void setConfig(const IsolationConfig& config);

    /**
     * @brief Gets the current configuration
     */
    [[nodiscard]] const IsolationConfig& getConfig() const;

    /**
     * @brief Sets the Python executable path
     */
    void setPythonExecutable(const std::filesystem::path& path);

    /**
     * @brief Sets the executor script path
     */
    void setExecutorScript(const std::filesystem::path& path);

    /**
     * @brief Sets the progress callback
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Sets the log callback
     */
    void setLogCallback(LogCallback callback);

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Executes a Python script synchronously
     * @param scriptContent Python code to execute
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult execute(
        std::string_view scriptContent,
        const nlohmann::json& args = nlohmann::json::object());

    /**
     * @brief Executes a Python script file synchronously
     * @param scriptPath Path to the script file
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult executeFile(
        const std::filesystem::path& scriptPath,
        const nlohmann::json& args = nlohmann::json::object());

    /**
     * @brief Executes a Python function synchronously
     * @param moduleName Module containing the function
     * @param functionName Function to call
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult executeFunction(
        std::string_view moduleName,
        std::string_view functionName,
        const nlohmann::json& args = nlohmann::json::object());

    /**
     * @brief Executes a Python script asynchronously
     */
    [[nodiscard]] std::future<ExecutionResult> executeAsync(
        std::string_view scriptContent,
        const nlohmann::json& args = nlohmann::json::object());

    /**
     * @brief Executes a Python script file asynchronously
     */
    [[nodiscard]] std::future<ExecutionResult> executeFileAsync(
        const std::filesystem::path& scriptPath,
        const nlohmann::json& args = nlohmann::json::object());

    /**
     * @brief Executes a Python function asynchronously
     */
    [[nodiscard]] std::future<ExecutionResult> executeFunctionAsync(
        std::string_view moduleName,
        std::string_view functionName,
        const nlohmann::json& args = nlohmann::json::object());

    // =========================================================================
    // Control
    // =========================================================================

    /**
     * @brief Cancels the current execution
     * @return true if cancellation was sent
     */
    bool cancel();

    /**
     * @brief Checks if an execution is currently running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Gets the current process ID (if running)
     */
    [[nodiscard]] std::optional<int> getProcessId() const;

    /**
     * @brief Gets the current memory usage (if running)
     */
    [[nodiscard]] std::optional<size_t> getCurrentMemoryUsage() const;

    /**
     * @brief Gets the current CPU usage percentage (if running)
     */
    [[nodiscard]] std::optional<double> getCurrentCpuUsage() const;

    /**
     * @brief Kills the subprocess forcefully
     */
    void kill();

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Validates the configuration
     * @return Success or error
     */
    [[nodiscard]] Result<void> validateConfig() const;

    /**
     * @brief Finds the default executor script path
     */
    [[nodiscard]] static std::optional<std::filesystem::path> findExecutorScript();

    /**
     * @brief Finds Python interpreter
     */
    [[nodiscard]] static std::optional<std::filesystem::path> findPythonExecutable();

    /**
     * @brief Gets the Python version of the configured interpreter
     */
    [[nodiscard]] std::optional<std::string> getPythonVersion() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Factory for creating isolated runners
 */
class RunnerFactory {
public:
    /**
     * @brief Creates a runner with default settings
     */
    [[nodiscard]] static std::unique_ptr<PythonRunner> create();

    /**
     * @brief Creates a runner for quick scripts (minimal isolation)
     */
    [[nodiscard]] static std::unique_ptr<PythonRunner> createQuick();

    /**
     * @brief Creates a runner with maximum security
     */
    [[nodiscard]] static std::unique_ptr<PythonRunner> createSecure();

    /**
     * @brief Creates a runner optimized for scientific computing
     */
    [[nodiscard]] static std::unique_ptr<PythonRunner> createScientific();

    /**
     * @brief Creates a runner with custom configuration
     */
    [[nodiscard]] static std::unique_ptr<PythonRunner> create(
        const IsolationConfig& config);
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_RUNNER_HPP
