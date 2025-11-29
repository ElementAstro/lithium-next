/*
 * executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file executor.hpp
 * @brief Script executor interface and factory
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_EXECUTION_EXECUTOR_HPP
#define LITHIUM_SCRIPT_SHELL_EXECUTION_EXECUTOR_HPP

#include <memory>
#include <string>
#include <string_view>

#include "../types.hpp"

namespace lithium::shell {

/**
 * @brief Abstract interface for script executors
 *
 * This interface defines the contract for all script execution implementations.
 * Each executor type (Shell, PowerShell, Python) implements this interface.
 */
class IScriptExecutor {
public:
    virtual ~IScriptExecutor() = default;

    /**
     * @brief Execute a script with the given context
     * @param script Script content to execute
     * @param ctx Execution context containing arguments, environment, etc.
     * @return Execution result with output and status
     */
    virtual auto execute(const Script& script, const ExecutionContext& ctx)
        -> ScriptExecutionResult = 0;

    /**
     * @brief Check if this executor supports the given script language
     * @param lang Script language to check
     * @return true if supported, false otherwise
     */
    [[nodiscard]] virtual auto supports(ScriptLanguage lang) const noexcept
        -> bool = 0;

    /**
     * @brief Get the primary language this executor handles
     * @return The primary script language
     */
    [[nodiscard]] virtual auto primaryLanguage() const noexcept
        -> ScriptLanguage = 0;

    /**
     * @brief Abort current execution if running
     */
    virtual void abort() = 0;

    /**
     * @brief Check if executor is currently running a script
     * @return true if running, false otherwise
     */
    [[nodiscard]] virtual auto isRunning() const noexcept -> bool = 0;
};

/**
 * @brief Factory for creating script executors
 */
class ScriptExecutorFactory {
public:
    /**
     * @brief Create an executor for the specified language
     * @param lang Script language
     * @return Unique pointer to the executor
     */
    static auto create(ScriptLanguage lang) -> std::unique_ptr<IScriptExecutor>;

    /**
     * @brief Create an executor by auto-detecting script content
     * @param scriptContent Script content to analyze
     * @return Unique pointer to the appropriate executor
     */
    static auto createForScript(std::string_view scriptContent)
        -> std::unique_ptr<IScriptExecutor>;

    /**
     * @brief Detect script language from content
     * @param content Script content to analyze
     * @return Detected script language
     */
    [[nodiscard]] static auto detectLanguage(std::string_view content)
        -> ScriptLanguage;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_EXECUTION_EXECUTOR_HPP
