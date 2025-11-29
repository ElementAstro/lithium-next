/*
 * hooks.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file hooks.hpp
 * @brief Hook Manager for Shell Script Execution
 * @date 2024-11-29
 * @version 1.0.0
 *
 * This module provides a hook management system for shell script execution:
 * - Pre-execution hooks (before script runs)
 * - Post-execution hooks (after script completes)
 * - Hook registration and removal
 * - Thread-safe hook execution
 * - Execution status and error handling
 * - Hook execution history and logging
 */

#ifndef LITHIUM_SCRIPT_SHELL_HOOKS_HPP
#define LITHIUM_SCRIPT_SHELL_HOOKS_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lithium::shell {

/**
 * @brief Hook callback type for pre-execution hooks
 *
 * Called before script execution with script identifier
 */
using PreHook = std::function<void(const std::string&)>;

/**
 * @brief Hook callback type for post-execution hooks
 *
 * Called after script execution with script identifier and exit code
 */
using PostHook = std::function<void(const std::string&, int)>;

/**
 * @brief Hook execution result information
 */
struct HookResult {
    bool success{true};                ///< Whether hook executed successfully
    std::string hookId;                ///< Hook identifier
    std::string scriptId;              ///< Associated script identifier
    std::string hookType;              ///< "pre" or "post"
    std::string errorMessage;          ///< Error message if failed
    std::chrono::milliseconds executionTime{0};  ///< Hook execution time
    std::chrono::system_clock::time_point timestamp;  ///< Execution timestamp
};

/**
 * @brief Hook Manager for shell script execution
 *
 * Manages pre-execution and post-execution hooks for shell scripts.
 * Provides thread-safe registration, execution, and removal of hooks.
 * Maintains history of hook executions and handles errors gracefully.
 *
 * Example usage:
 * ```cpp
 * auto manager = std::make_shared<HookManager>();
 *
 * // Register pre-execution hook
 * manager->addPreHook("prepare_env", [](const std::string& script_id) {
 *     std::cout << "Preparing environment for: " << script_id << std::endl;
 * });
 *
 * // Register post-execution hook
 * manager->addPostHook("cleanup", [](const std::string& script_id, int exit_code) {
 *     std::cout << "Cleanup after: " << script_id << " with exit code: " << exit_code << std::endl;
 * });
 *
 * // Execute all registered pre-hooks
 * manager->executePreHooks("script_001");
 *
 * // ... script execution ...
 *
 * // Execute all registered post-hooks
 * manager->executePostHooks("script_001", 0);
 *
 * // Remove a hook
 * manager->removeHook("prepare_env");
 * ```
 */
class HookManager {
public:
    /**
     * @brief Construct a new HookManager instance
     */
    HookManager();

    /**
     * @brief Destroy the HookManager instance
     */
    ~HookManager();

    /**
     * @brief Add a pre-execution hook
     *
     * Pre-hooks are executed before the script runs. They receive the script
     * identifier as a parameter.
     *
     * @param hookId Unique identifier for the hook
     * @param hook Callback function to execute
     * @return true if hook was added successfully, false if ID already exists
     * @throws std::invalid_argument if hookId is empty
     */
    bool addPreHook(std::string_view hookId, PreHook hook);

    /**
     * @brief Add a post-execution hook
     *
     * Post-hooks are executed after the script completes. They receive the
     * script identifier and exit code as parameters.
     *
     * @param hookId Unique identifier for the hook
     * @param hook Callback function to execute
     * @return true if hook was added successfully, false if ID already exists
     * @throws std::invalid_argument if hookId is empty
     */
    bool addPostHook(std::string_view hookId, PostHook hook);

    /**
     * @brief Remove a hook by its ID
     *
     * Removes either a pre-hook or post-hook with the specified ID.
     *
     * @param hookId Identifier of the hook to remove
     * @return true if hook was found and removed, false otherwise
     */
    bool removeHook(std::string_view hookId);

    /**
     * @brief Remove a pre-hook by its ID
     *
     * @param hookId Identifier of the pre-hook to remove
     * @return true if hook was found and removed, false otherwise
     */
    bool removePreHook(std::string_view hookId);

    /**
     * @brief Remove a post-hook by its ID
     *
     * @param hookId Identifier of the post-hook to remove
     * @return true if hook was found and removed, false otherwise
     */
    bool removePostHook(std::string_view hookId);

    /**
     * @brief Execute all registered pre-hooks for a script
     *
     * Executes all pre-execution hooks in the order they were registered.
     * If a hook throws an exception, it is caught and logged, and execution
     * continues with the next hook.
     *
     * @param scriptId Identifier of the script being executed
     * @return Vector of hook execution results
     */
    std::vector<HookResult> executePreHooks(std::string_view scriptId);

    /**
     * @brief Execute all registered post-hooks for a script
     *
     * Executes all post-execution hooks in the order they were registered.
     * If a hook throws an exception, it is caught and logged, and execution
     * continues with the next hook.
     *
     * @param scriptId Identifier of the script that was executed
     * @param exitCode Exit code of the script
     * @return Vector of hook execution results
     */
    std::vector<HookResult> executePostHooks(std::string_view scriptId, int exitCode);

    /**
     * @brief Get the count of registered pre-hooks
     *
     * @return Number of registered pre-execution hooks
     */
    size_t getPreHookCount() const;

    /**
     * @brief Get the count of registered post-hooks
     *
     * @return Number of registered post-execution hooks
     */
    size_t getPostHookCount() const;

    /**
     * @brief Check if a hook with the given ID exists
     *
     * @param hookId Hook identifier to check
     * @return true if the hook exists, false otherwise
     */
    bool hasHook(std::string_view hookId) const;

    /**
     * @brief Clear all registered hooks
     *
     * Removes all pre-execution and post-execution hooks.
     */
    void clearAllHooks();

    /**
     * @brief Clear all pre-execution hooks
     */
    void clearPreHooks();

    /**
     * @brief Clear all post-execution hooks
     */
    void clearPostHooks();

    /**
     * @brief Get execution history of hooks
     *
     * Returns the most recent hook executions, limited to a maximum count.
     *
     * @param maxEntries Maximum number of history entries to return (0 = all)
     * @return Vector of hook execution results in reverse chronological order
     */
    std::vector<HookResult> getExecutionHistory(size_t maxEntries = 100) const;

    /**
     * @brief Get execution history for a specific script
     *
     * @param scriptId Script identifier to filter by
     * @param maxEntries Maximum number of history entries to return (0 = all)
     * @return Vector of hook execution results for the given script
     */
    std::vector<HookResult> getScriptHistory(std::string_view scriptId,
                                             size_t maxEntries = 100) const;

    /**
     * @brief Clear execution history
     */
    void clearHistory();

    /**
     * @brief Get execution history size
     *
     * @return Number of entries in the execution history
     */
    size_t getHistorySize() const;

    /**
     * @brief Enable or disable hook execution
     *
     * When disabled, executePreHooks() and executePostHooks() will return
     * empty result vectors without executing any hooks.
     *
     * @param enabled true to enable hooks, false to disable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if hooks are enabled
     *
     * @return true if hooks are enabled, false otherwise
     */
    bool isEnabled() const;

private:
    /// Map of pre-execution hooks by ID
    std::unordered_map<std::string, PreHook> preHooks_;

    /// Map of post-execution hooks by ID
    std::unordered_map<std::string, PostHook> postHooks_;

    /// Execution history
    std::vector<HookResult> executionHistory_;

    /// Shared mutex for thread-safe access
    mutable std::shared_mutex hooksMutex_;

    /// Mutex for history access
    mutable std::shared_mutex historyMutex_;

    /// Maximum history entries to maintain
    static constexpr size_t MAX_HISTORY_SIZE = 10000;

    /// Flag to enable/disable hook execution
    std::atomic<bool> enabled_{true};

    /**
     * @brief Record a hook execution result in history
     *
     * @param result Hook execution result to record
     */
    void recordHookExecution(const HookResult& result);
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_HOOKS_HPP
