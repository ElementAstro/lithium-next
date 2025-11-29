/*
 * powershell_executor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file powershell_executor.hpp
 * @brief PowerShell script executor implementation
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_EXECUTION_POWERSHELL_EXECUTOR_HPP
#define LITHIUM_SCRIPT_SHELL_EXECUTION_POWERSHELL_EXECUTOR_HPP

#include <memory>
#include <string>
#include <vector>

#include "executor.hpp"

namespace lithium::shell {

/**
 * @brief PowerShell script executor implementation
 *
 * Executes Windows PowerShell scripts with support for:
 * - Module imports
 * - Environment variable injection
 * - Error action preference
 * - Progress tracking
 */
class PowerShellExecutor : public IScriptExecutor {
public:
    PowerShellExecutor();
    ~PowerShellExecutor() override;

    // Non-copyable, movable
    PowerShellExecutor(const PowerShellExecutor&) = delete;
    PowerShellExecutor& operator=(const PowerShellExecutor&) = delete;
    PowerShellExecutor(PowerShellExecutor&&) noexcept;
    PowerShellExecutor& operator=(PowerShellExecutor&&) noexcept;

    auto execute(const Script& script, const ExecutionContext& ctx)
        -> ScriptExecutionResult override;

    [[nodiscard]] auto supports(ScriptLanguage lang) const noexcept
        -> bool override;

    [[nodiscard]] auto primaryLanguage() const noexcept
        -> ScriptLanguage override;

    void abort() override;

    [[nodiscard]] auto isRunning() const noexcept -> bool override;

    /**
     * @brief Import a PowerShell module
     * @param moduleName Name of the module to import
     */
    void importModule(const std::string& moduleName);

    /**
     * @brief Get list of imported modules
     * @return Vector of imported module names
     */
    [[nodiscard]] auto getImportedModules() const -> std::vector<std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_EXECUTION_POWERSHELL_EXECUTOR_HPP
