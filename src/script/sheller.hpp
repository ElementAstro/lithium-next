/*
 * sheller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file sheller.hpp
 * @brief System Script Manager
 * @date 2024-1-13
 */

#ifndef LITHIUM_SCRIPT_SHELLER_HPP
#define LITHIUM_SCRIPT_SHELLER_HPP

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using Script = std::string;

namespace lithium {
struct ScriptProgress {
    float percentage;
    std::string status;
    std::chrono::system_clock::time_point timestamp;
};

struct ScriptMetadata {
    std::string description;
    std::vector<std::string> tags;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastModified;
    size_t version;
    std::unordered_map<std::string, std::string> parameters;
};

enum class RetryStrategy { None, Linear, Exponential, Custom };

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
    auto runScript(std::string_view name,
                   const std::unordered_map<std::string, std::string>& args,
                   bool safe = true,
                   std::optional<int> timeoutMs = std::nullopt,
                   int retryCount = 0)
        -> std::optional<std::pair<std::string, int>>;

    /**
     * @brief Executes a script asynchronously and returns a future object.
     *
     * @param name The name of the script to run asynchronously.
     * @param args A map of arguments to pass to the script.
     * @param safe A flag indicating whether to run the script in a safe mode.
     * @return A future object that will contain the script result when ready.
     */
    auto runScriptAsync(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        bool safe = true)
        -> std::future<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Gets the execution progress of a script.
     *
     * @param name The name of the script.
     * @return The current progress as a float between 0.0 and 1.0.
     */
    auto getScriptProgress(std::string_view name) const -> float;

    /**
     * @brief Aborts a currently executing script.
     *
     * @param name The name of the script to abort.
     */
    void abortScript(std::string_view name);

    /**
     * @brief Adds a hook function to execute before script execution.
     *
     * @param name The name of the script to add the hook for.
     * @param hook The function to call before script execution.
     */
    void addPreExecutionHook(std::string_view name,
                             std::function<void(const std::string&)> hook);

    /**
     * @brief Adds a hook function to execute after script execution.
     *
     * @param name The name of the script to add the hook for.
     * @param hook The function to call after script execution.
     */
    void addPostExecutionHook(
        std::string_view name,
        std::function<void(const std::string&, int)> hook);

    /**
     * @brief Sets environment variables for a script.
     *
     * @param name The name of the script.
     * @param vars A map of environment variable names to values.
     */
    void setScriptEnvironmentVars(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& vars);

    /**
     * @brief Imports a PowerShell module.
     *
     * @param moduleName The name of the PowerShell module to import.
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

    /**
     * @brief Imports multiple scripts in batch.
     *
     * @param scripts A span of pairs containing script names and content.
     */
    void importScripts(std::span<const std::pair<std::string, Script>> scripts);

    /**
     * @brief Gets metadata for a script.
     *
     * @param name The name of the script.
     * @return An optional ScriptMetadata object if metadata exists.
     */
    [[nodiscard]] auto getScriptMetadata(std::string_view name) const
        -> std::optional<ScriptMetadata>;

    /**
     * @brief Sets a timeout handler for a script.
     *
     * @param name The name of the script.
     * @param handler The function to call when script execution times out.
     */
    void setTimeoutHandler(std::string_view name,
                           std::function<void()> handler);

    /**
     * @brief Sets the retry strategy for a script.
     *
     * @param name The name of the script.
     * @param strategy The retry strategy to use.
     */
    void setRetryStrategy(std::string_view name, RetryStrategy strategy);

    /**
     * @brief Gets a list of currently running scripts.
     *
     * @return A vector of script names that are currently running.
     */
    [[nodiscard]] auto getRunningScripts() const -> std::vector<std::string>;
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_SHELLER_HPP
