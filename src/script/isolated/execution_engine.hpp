/*
 * execution_engine.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_EXECUTION_ENGINE_HPP
#define LITHIUM_SCRIPT_ISOLATED_EXECUTION_ENGINE_HPP

#include "../ipc/channel.hpp"
#include "../ipc/message.hpp"
#include "config_discovery.hpp"
#include "lifecycle.hpp"
#include "message_handlers.hpp"
#include "process_spawning.hpp"
#include "resource_monitor.hpp"
#include "types.hpp"

#include <memory>
#include <string_view>

namespace lithium::isolated {

/**
 * @brief Main execution engine for isolated Python scripts
 *
 * Orchestrates the entire execution flow including:
 * - Configuration validation
 * - Process spawning
 * - IPC communication
 * - Resource monitoring
 * - Timeout handling
 * - Result collection
 */
class ExecutionEngine {
public:
    ExecutionEngine();
    ~ExecutionEngine();

    // Non-copyable
    ExecutionEngine(const ExecutionEngine&) = delete;
    ExecutionEngine& operator=(const ExecutionEngine&) = delete;

    // Movable
    ExecutionEngine(ExecutionEngine&&) noexcept;
    ExecutionEngine& operator=(ExecutionEngine&&) noexcept;

    /**
     * @brief Set the isolation configuration
     */
    void setConfig(const IsolationConfig& config);

    /**
     * @brief Get the current configuration
     */
    [[nodiscard]] const IsolationConfig& getConfig() const;

    /**
     * @brief Set progress callback
     */
    void setProgressCallback(ProgressCallback callback);

    /**
     * @brief Set log callback
     */
    void setLogCallback(LogCallback callback);

    /**
     * @brief Execute a Python script
     * @param scriptContent Script content to execute
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult execute(std::string_view scriptContent,
                                          const nlohmann::json& args);

    /**
     * @brief Execute a Python script from file
     * @param scriptPath Path to script file
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult executeFile(
        const std::filesystem::path& scriptPath, const nlohmann::json& args);

    /**
     * @brief Execute a Python function
     * @param moduleName Module containing the function
     * @param functionName Function to call
     * @param args Arguments as JSON
     * @return Execution result
     */
    [[nodiscard]] ExecutionResult executeFunction(std::string_view moduleName,
                                                  std::string_view functionName,
                                                  const nlohmann::json& args);

    /**
     * @brief Cancel current execution
     */
    bool cancel();

    /**
     * @brief Check if running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Get current process ID
     */
    [[nodiscard]] std::optional<int> getProcessId() const;

    /**
     * @brief Get current memory usage
     */
    [[nodiscard]] std::optional<size_t> getCurrentMemoryUsage() const;

    /**
     * @brief Kill the subprocess
     */
    void kill();

private:
    /**
     * @brief Internal execution implementation
     */
    ExecutionResult executeInternal(std::string_view scriptContent,
                                    const std::string& scriptPath,
                                    const std::string& moduleName,
                                    const std::string& functionName,
                                    const nlohmann::json& args);

    IsolationConfig config_;
    std::shared_ptr<ipc::BidirectionalChannel> channel_;
    ProcessLifecycle lifecycle_;
    MessageHandler messageHandler_;
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_EXECUTION_ENGINE_HPP
