/*
 * shell_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file shell_executor.hpp
 * @brief Shell/Bash script executor implementation
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_EXECUTION_SHELL_EXECUTOR_HPP
#define LITHIUM_SCRIPT_SHELL_EXECUTION_SHELL_EXECUTOR_HPP

#include <atomic>
#include <memory>

#include "executor.hpp"

namespace lithium::shell {

/**
 * @brief Shell/Bash script executor implementation
 *
 * Executes Unix shell scripts (bash, sh) with support for:
 * - Environment variable injection
 * - Argument passing
 * - Progress tracking
 * - Timeout handling
 * - Abort capability
 */
class ShellExecutor : public IScriptExecutor {
public:
    ShellExecutor();
    ~ShellExecutor() override;

    // Non-copyable, movable
    ShellExecutor(const ShellExecutor&) = delete;
    ShellExecutor& operator=(const ShellExecutor&) = delete;
    ShellExecutor(ShellExecutor&&) noexcept;
    ShellExecutor& operator=(ShellExecutor&&) noexcept;

    auto execute(const Script& script, const ExecutionContext& ctx)
        -> ScriptExecutionResult override;

    [[nodiscard]] auto supports(ScriptLanguage lang) const noexcept
        -> bool override;

    [[nodiscard]] auto primaryLanguage() const noexcept
        -> ScriptLanguage override;

    void abort() override;

    [[nodiscard]] auto isRunning() const noexcept -> bool override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_EXECUTION_SHELL_EXECUTOR_HPP
