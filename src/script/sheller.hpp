/*
 * sheller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-13

Description: System Script Manager

**************************************************/

#ifndef LITHIUM_SCRIPT_SHELLER_HPP
#define LITHIUM_SCRIPT_SHELLER_HPP

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using Script = std::string;

namespace lithium {

/**
 * @brief Forward declaration of the implementation class for ScriptManager.
 */
class ScriptManagerImpl;

/**
 * @brief The ScriptManager class provides an interface to manage and execute
 * system scripts.
 *
 * This class supports registering, updating, and deleting scripts. It can run
 * scripts sequentially or concurrently and retrieve the output or status of a
 * script. Additional features include script versioning, conditional
 * execution, logging, and retry mechanisms.
 */
class ScriptManager {
    std::unique_ptr<ScriptManagerImpl>
        pImpl_;  ///< Pointer to the implementation of ScriptManager.

public:
    /**
     * @brief Constructs a ScriptManager object.
     */
    ScriptManager();

    /**
     * @brief Destructs the ScriptManager object.
     */
    ~ScriptManager();

    /**
     * @brief Registers a new script with a given name.
     *
     * @param name The name of the script to register.
     * @param script The script content as a string.
     */
    void registerScript(std::string_view name, const Script& script);

    /**
     * @brief Registers a new PowerShell script with a given name.
     *
     * @param name The name of the PowerShell script to register.
     * @param script The PowerShell script content as a string.
     */
    void registerPowerShellScript(std::string_view name, const Script& script);

    /**
     * @brief Retrieves all registered scripts.
     *
     * @return A map of script names to their content.
     */
    auto getAllScripts() const -> std::unordered_map<std::string, Script>;

    /**
     * @brief Deletes a script by its name.
     *
     * @param name The name of the script to delete.
     */
    void deleteScript(std::string_view name);

    /**
     * @brief Updates an existing script with new content.
     *
     * @param name The name of the script to update.
     * @param script The new content of the script.
     */
    void updateScript(std::string_view name, const Script& script);

    /**
     * @brief Runs a script with the given arguments.
     *
     * @param name The name of the script to run.
     * @param args A map of arguments to pass to the script.
     * @param safe A flag indicating whether to run the script in a safe mode
     * (default: true).
     * @param timeoutMs An optional timeout in milliseconds for the script
     * execution.
     * @param retryCount The number of times to retry the script execution on
     * failure.
     * @return An optional pair containing the script output and exit status.
     */
    auto runScript(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        bool safe = true, std::optional<int> timeoutMs = std::nullopt,
        int retryCount = 0) -> std::optional<std::pair<std::string, int>>;

    /**
     * @brief 异步执行脚本并返回future对象
     */
    auto runScriptAsync(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        bool safe = true)
        -> std::future<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief 获取脚本执行进度
     */
    auto getScriptProgress(std::string_view name) const -> float;

    /**
     * @brief 中止正在执行的脚本
     */
    void abortScript(std::string_view name);

    /**
     * @brief 添加脚本执行前的钩子函数
     */
    void addPreExecutionHook(std::string_view name,
                             std::function<void(const std::string&)> hook);

    /**
     * @brief 添加脚本执行后的钩子函数
     */
    void addPostExecutionHook(
        std::string_view name,
        std::function<void(const std::string&, int)> hook);

    /**
     * @brief 设置脚本的环境变量
     */
    void setScriptEnvironmentVars(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& vars);

    /**
     * @brief 导入PowerShell模块
     */
    void importPowerShellModule(std::string_view moduleName);

    /**
     * @brief Retrieves the output of a script.
     *
     * @param name The name of the script whose output is to be retrieved.
     * @return An optional string containing the script output.
     */
    auto getScriptOutput(std::string_view name) const
        -> std::optional<std::string>;

    /**
     * @brief Retrieves the status of a script.
     *
     * @param name The name of the script whose status is to be retrieved.
     * @return An optional integer representing the script's exit status.
     */
    auto getScriptStatus(std::string_view name) const -> std::optional<int>;

    /**
     * @brief Runs a sequence of scripts in order.
     *
     * @param scripts A vector of script names and their arguments to run
     * sequentially.
     * @param safe A flag indicating whether to run the scripts in a safe mode
     * (default: true).
     * @param retryCount The number of times to retry each script on failure.
     * @return A vector of optional pairs containing the script output and exit
     * status for each script.
     */
    auto runScriptsSequentially(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>&
            scripts,
        bool safe = true, int retryCount = 0)
        -> std::vector<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Runs multiple scripts concurrently.
     *
     * @param scripts A vector of script names and their arguments to run
     * concurrently.
     * @param safe A flag indicating whether to run the scripts in a safe mode
     * (default: true).
     * @param retryCount The number of times to retry each script on failure.
     * @return A vector of optional pairs containing the script output and exit
     * status for each script.
     */
    auto runScriptsConcurrently(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>&
            scripts,
        bool safe = true, int retryCount = 0)
        -> std::vector<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Enables versioning for the scripts.
     *
     * When versioning is enabled, changes to scripts are tracked and previous
     * versions can be restored.
     */
    void enableVersioning();

    /**
     * @brief Rolls back a script to a specific version.
     *
     * @param name The name of the script to roll back.
     * @param version The version number to roll back to.
     * @return True if the rollback was successful, otherwise false.
     */
    auto rollbackScript(std::string_view name, int version) -> bool;

    /**
     * @brief Sets a condition under which a script can be executed.
     *
     * @param name The name of the script to set the condition for.
     * @param condition A function that returns true if the script should be
     * executed, false otherwise.
     */
    void setScriptCondition(std::string_view name,
                            std::function<bool()> condition);

    /**
     * @brief Sets the execution environment for a script.
     *
     * @param name The name of the script to set the environment for.
     * @param environment A string representing the environment settings for the
     * script execution.
     */
    void setExecutionEnvironment(std::string_view name,
                                 const std::string& environment);

    /**
     * @brief Sets the maximum number of script versions to keep.
     *
     * @param maxVersions The maximum number of versions to retain for each
     * script.
     */
    void setMaxScriptVersions(int maxVersions);

    /**
     * @brief Retrieves the execution logs for a script.
     *
     * @param name The name of the script.
     * @return A vector of log entries.
     */
    [[nodiscard]] auto getScriptLogs(std::string_view name) const
        -> std::vector<std::string>;

    /**
     * @brief Retrieves information about a script.
     *
     * @param name The name of the script.
     * @return A string containing information about the script.
     */
    [[nodiscard]] auto getScriptInfo(std::string_view name) const
        -> std::string;
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_SHELLER_HPP
