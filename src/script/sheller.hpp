/*
 * sheller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file sheller.hpp
 * @brief Enhanced System Script Manager with Python Integration
 * @date 2024-1-13
 * @version 2.0.0
 *
 * This module provides a comprehensive script management system that supports:
 * - Shell/Bash script execution
 * - PowerShell script execution (Windows)
 * - Python script integration via PythonWrapper
 * - Script versioning and rollback
 * - Async execution with progress tracking
 * - Resource management and pooling
 * - Pre/Post execution hooks
 * - Retry strategies with configurable backoff
 */

#ifndef LITHIUM_SCRIPT_SHELLER_HPP
#define LITHIUM_SCRIPT_SHELLER_HPP

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

using Script = std::string;

namespace lithium {

/**
 * @brief Enumeration of supported script types
 */
enum class ScriptLanguage {
    Shell,       ///< Unix shell scripts (bash, sh)
    PowerShell,  ///< Windows PowerShell scripts
    Python,      ///< Python scripts
    Auto         ///< Auto-detect based on content/extension
};

/**
 * @brief Script execution progress information
 */
struct ScriptProgress {
    float percentage{0.0f};   ///< Progress 0.0-1.0
    std::string status;       ///< Current status message
    std::string currentStep;  ///< Current execution step
    std::chrono::system_clock::time_point timestamp;  ///< Last update time
    std::optional<std::string> output;  ///< Partial output if available
};

/**
 * @brief Extended script metadata with Python support
 */
struct ScriptMetadata {
    std::string description;                          ///< Script description
    std::vector<std::string> tags;                    ///< Categorization tags
    std::chrono::system_clock::time_point createdAt;  ///< Creation timestamp
    std::chrono::system_clock::time_point
        lastModified;   ///< Last modification time
    size_t version{1};  ///< Version number
    std::unordered_map<std::string, std::string>
        parameters;                                 ///< Script parameters
    ScriptLanguage language{ScriptLanguage::Auto};  ///< Script language type
    std::string author;                             ///< Script author
    std::vector<std::string> dependencies;          ///< Required dependencies
    std::optional<std::filesystem::path>
        sourcePath;        ///< Original source file path
    bool isPython{false};  ///< Quick check for Python scripts
};

/**
 * @brief Script execution result with detailed information
 */
struct ScriptExecutionResult {
    bool success{false};      ///< Whether execution succeeded
    int exitCode{-1};         ///< Exit code
    std::string output;       ///< Standard output
    std::string errorOutput;  ///< Standard error
    std::chrono::milliseconds executionTime{0};  ///< Total execution time
    std::optional<std::string> exception;        ///< Exception message if any
    ScriptLanguage detectedLanguage{
        ScriptLanguage::Auto};  ///< Detected script type
};

/**
 * @brief Python script configuration for enhanced integration
 */
struct PythonScriptConfig {
    std::string moduleName;             ///< Python module name
    std::string entryFunction;          ///< Entry function to call
    std::vector<std::string> sysPaths;  ///< Additional sys.path entries
    std::unordered_map<std::string, std::string>
        envVars;                                ///< Environment variables
    bool useVirtualEnv{false};                  ///< Use virtual environment
    std::string virtualEnvPath;                 ///< Virtual environment path
    std::vector<std::string> requiredPackages;  ///< Required pip packages
    bool enableProfiling{false};        ///< Enable performance profiling
    size_t memoryLimitMB{0};            ///< Memory limit (0 = unlimited)
    std::chrono::seconds timeout{300};  ///< Execution timeout
};

/**
 * @brief Resource limits for script execution
 */
struct ScriptResourceLimits {
    size_t maxMemoryMB{1024};                     ///< Maximum memory in MB
    int maxCpuPercent{100};                       ///< Maximum CPU percentage
    std::chrono::seconds maxExecutionTime{3600};  ///< Maximum execution time
    size_t maxOutputSize{10 * 1024 * 1024};       ///< Max output size in bytes
    int maxConcurrentScripts{4};                  ///< Max concurrent executions
};

/**
 * @brief Retry strategy configuration
 */
enum class RetryStrategy {
    None,         ///< No retry
    Linear,       ///< Linear backoff
    Exponential,  ///< Exponential backoff
    Custom        ///< Custom retry logic
};

/**
 * @brief Retry configuration with detailed options
 */
struct RetryConfig {
    RetryStrategy strategy{RetryStrategy::None};  ///< Retry strategy
    int maxRetries{3};                            ///< Maximum retry attempts
    std::chrono::milliseconds initialDelay{100};  ///< Initial delay
    std::chrono::milliseconds maxDelay{30000};    ///< Maximum delay
    double multiplier{2.0};                       ///< Backoff multiplier
    std::function<bool(int, const ScriptExecutionResult&)>
        shouldRetry;  ///< Custom retry predicate
};

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

    // =========================================================================
    // Enhanced Python Integration Methods
    // =========================================================================

    /**
     * @brief Registers a Python script with configuration.
     *
     * @param name The name/alias for the Python script.
     * @param config Python script configuration.
     */
    void registerPythonScriptWithConfig(std::string_view name,
                                        const PythonScriptConfig& config);

    /**
     * @brief Executes a Python script by module and function.
     *
     * @param moduleName The Python module name.
     * @param functionName The function to call within the module.
     * @param args Arguments to pass to the function.
     * @return Execution result with output and status.
     */
    auto executePythonFunction(
        std::string_view moduleName, std::string_view functionName,
        const std::unordered_map<std::string, std::string>& args = {})
        -> ScriptExecutionResult;

    /**
     * @brief Loads Python scripts from a directory.
     *
     * @param directory Path to directory containing Python scripts.
     * @param recursive Whether to search recursively.
     * @return Number of scripts loaded.
     */
    auto loadPythonScriptsFromDirectory(const std::filesystem::path& directory,
                                        bool recursive = false) -> size_t;

    /**
     * @brief Gets the Python wrapper instance for direct access.
     *
     * @return Shared pointer to the Python wrapper.
     */
    [[nodiscard]] auto getPythonWrapper() const
        -> std::shared_ptr<class PythonWrapper>;

    /**
     * @brief Sets the Python wrapper instance.
     *
     * @param wrapper Shared pointer to Python wrapper.
     */
    void setPythonWrapper(std::shared_ptr<class PythonWrapper> wrapper);

    /**
     * @brief Checks if Python integration is available.
     *
     * @return True if Python is available and initialized.
     */
    [[nodiscard]] auto isPythonAvailable() const -> bool;

    /**
     * @brief Adds a path to Python's sys.path.
     *
     * @param path Path to add.
     */
    void addPythonSysPath(const std::filesystem::path& path);

    // =========================================================================
    // Resource Management Methods
    // =========================================================================

    /**
     * @brief Sets resource limits for script execution.
     *
     * @param limits Resource limit configuration.
     */
    void setResourceLimits(const ScriptResourceLimits& limits);

    /**
     * @brief Gets current resource limits.
     *
     * @return Current resource limits configuration.
     */
    [[nodiscard]] auto getResourceLimits() const -> ScriptResourceLimits;

    /**
     * @brief Gets current resource usage statistics.
     *
     * @return Map of resource name to usage value.
     */
    [[nodiscard]] auto getResourceUsage() const
        -> std::unordered_map<std::string, double>;

    // =========================================================================
    // Script Discovery and Auto-Loading
    // =========================================================================

    /**
     * @brief Discovers and loads scripts from a directory.
     *
     * @param directory Directory to scan for scripts.
     * @param extensions File extensions to consider (e.g., ".py", ".sh").
     * @param recursive Whether to scan recursively.
     * @return Number of scripts discovered and loaded.
     */
    auto discoverScripts(const std::filesystem::path& directory,
                         const std::vector<std::string>& extensions = {".py",
                                                                       ".sh"},
                         bool recursive = true) -> size_t;

    /**
     * @brief Detects the language of a script from its content.
     *
     * @param content Script content.
     * @return Detected script language.
     */
    [[nodiscard]] static auto detectScriptLanguage(std::string_view content)
        -> ScriptLanguage;

    /**
     * @brief Gets script content by name.
     *
     * @param name Script name.
     * @return Script content if found.
     */
    [[nodiscard]] auto getScriptContent(std::string_view name) const
        -> std::optional<std::string>;

    /**
     * @brief Sets script metadata.
     *
     * @param name Script name.
     * @param metadata Metadata to set.
     */
    void setScriptMetadata(std::string_view name,
                           const ScriptMetadata& metadata);

    // =========================================================================
    // Enhanced Execution Methods
    // =========================================================================

    /**
     * @brief Executes a script with full configuration.
     *
     * @param name Script name.
     * @param args Script arguments.
     * @param config Retry configuration.
     * @param resourceLimits Resource limits for this execution.
     * @return Detailed execution result.
     */
    auto executeWithConfig(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        const RetryConfig& config = {},
        const std::optional<ScriptResourceLimits>& resourceLimits =
            std::nullopt) -> ScriptExecutionResult;

    /**
     * @brief Executes a script and returns a future for async handling.
     *
     * @param name Script name.
     * @param args Script arguments.
     * @param progressCallback Optional callback for progress updates.
     * @return Future containing the execution result.
     */
    auto executeAsync(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args,
        std::function<void(const ScriptProgress&)> progressCallback = nullptr)
        -> std::future<ScriptExecutionResult>;

    /**
     * @brief Executes multiple scripts as a pipeline.
     *
     * @param scripts Vector of script names to execute in order.
     * @param sharedContext Shared context passed between scripts.
     * @param stopOnError Whether to stop on first error.
     * @return Vector of execution results.
     */
    auto executePipeline(
        const std::vector<std::string>& scripts,
        const std::unordered_map<std::string, std::string>& sharedContext = {},
        bool stopOnError = true) -> std::vector<ScriptExecutionResult>;

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Gets execution statistics for a script.
     *
     * @param name Script name.
     * @return Statistics including execution count, average time, etc.
     */
    [[nodiscard]] auto getScriptStatistics(std::string_view name) const
        -> std::unordered_map<std::string, double>;

    /**
     * @brief Gets global execution statistics.
     *
     * @return Overall statistics for all scripts.
     */
    [[nodiscard]] auto getGlobalStatistics() const
        -> std::unordered_map<std::string, double>;

    /**
     * @brief Resets statistics for a script or all scripts.
     *
     * @param name Script name, or empty for all scripts.
     */
    void resetStatistics(std::string_view name = "");
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_SHELLER_HPP
