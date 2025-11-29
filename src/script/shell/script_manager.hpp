/*
 * script_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file script_manager.hpp
 * @brief Unified script management facade
 * @date 2024-1-13
 * @version 2.1.0
 *
 * This module provides a high-level facade for script management,
 * delegating to specialized components for execution, versioning,
 * hooks, metadata, and resource management.
 */

#ifndef LITHIUM_SCRIPT_SHELL_SCRIPT_MANAGER_HPP
#define LITHIUM_SCRIPT_SHELL_SCRIPT_MANAGER_HPP

#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "execution/executor.hpp"
#include "hooks.hpp"
#include "metadata.hpp"
#include "resource_limits.hpp"
#include "retry.hpp"
#include "types.hpp"
#include "versioning.hpp"

namespace lithium::shell {

/**
 * @brief Unified script management facade
 *
 * ScriptManager provides a high-level API for managing and executing
 * scripts of various types (Shell, PowerShell, Python). It delegates
 * to specialized components for specific functionality:
 *
 * - Execution: IScriptExecutor implementations
 * - Versioning: VersionManager
 * - Hooks: HookManager
 * - Metadata: MetadataManager
 * - Resources: ResourceManager
 * - Retry: RetryExecutor
 */
class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();

    // Non-copyable, movable
    ScriptManager(const ScriptManager&) = delete;
    ScriptManager& operator=(const ScriptManager&) = delete;
    ScriptManager(ScriptManager&&) noexcept;
    ScriptManager& operator=(ScriptManager&&) noexcept;

    // =========================================================================
    // Script Registration
    // =========================================================================

    /**
     * @brief Register a script with auto-detected language
     * @param name Script identifier
     * @param script Script content
     */
    void registerScript(std::string_view name, const Script& script);

    /**
     * @brief Delete a registered script
     * @param name Script identifier
     */
    void deleteScript(std::string_view name);

    /**
     * @brief Update an existing script
     * @param name Script identifier
     * @param script New script content
     */
    void updateScript(std::string_view name, const Script& script);

    /**
     * @brief Get all registered scripts
     * @return Map of script name to content
     */
    [[nodiscard]] auto getAllScripts() const
        -> std::unordered_map<std::string, Script>;

    /**
     * @brief Get script content by name
     * @param name Script identifier
     * @return Script content if found
     */
    [[nodiscard]] auto getScriptContent(std::string_view name) const
        -> std::optional<std::string>;

    /**
     * @brief Import multiple scripts at once
     * @param scripts Span of (name, content) pairs
     */
    void importScripts(std::span<const std::pair<std::string, Script>> scripts);

    // =========================================================================
    // Script Execution
    // =========================================================================

    /**
     * @brief Execute a script synchronously
     * @param name Script identifier
     * @param args Script arguments
     * @param safe Run in safe mode
     * @param timeoutMs Execution timeout
     * @return Execution result (output, exit code)
     */
    auto runScript(std::string_view name,
                   const std::unordered_map<std::string, std::string>& args = {},
                   bool safe = true,
                   std::optional<int> timeoutMs = std::nullopt)
        -> std::optional<std::pair<std::string, int>>;

    /**
     * @brief Execute a script asynchronously
     * @param name Script identifier
     * @param args Script arguments
     * @param safe Run in safe mode
     * @return Future with execution result
     */
    auto runScriptAsync(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args = {},
        bool safe = true)
        -> std::future<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Execute with full configuration
     * @param name Script identifier
     * @param args Script arguments
     * @param retryConfig Retry configuration
     * @param resourceLimits Resource limits
     * @return Detailed execution result
     */
    auto executeWithConfig(
        std::string_view name,
        const std::unordered_map<std::string, std::string>& args = {},
        const RetryConfig& retryConfig = {},
        const std::optional<ScriptResourceLimits>& resourceLimits = std::nullopt)
        -> ScriptExecutionResult;

    /**
     * @brief Execute multiple scripts sequentially
     * @param scripts Vector of (name, args) pairs
     * @param safe Run in safe mode
     * @return Vector of results
     */
    auto runScriptsSequentially(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>& scripts,
        bool safe = true)
        -> std::vector<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Execute multiple scripts concurrently
     * @param scripts Vector of (name, args) pairs
     * @param safe Run in safe mode
     * @return Vector of results
     */
    auto runScriptsConcurrently(
        const std::vector<std::pair<
            std::string, std::unordered_map<std::string, std::string>>>& scripts,
        bool safe = true)
        -> std::vector<std::optional<std::pair<std::string, int>>>;

    /**
     * @brief Execute scripts as a pipeline
     * @param scripts Script names in order
     * @param sharedContext Context passed between scripts
     * @param stopOnError Stop on first error
     * @return Vector of execution results
     */
    auto executePipeline(
        const std::vector<std::string>& scripts,
        const std::unordered_map<std::string, std::string>& sharedContext = {},
        bool stopOnError = true) -> std::vector<ScriptExecutionResult>;

    /**
     * @brief Abort a running script
     * @param name Script identifier
     */
    void abortScript(std::string_view name);

    /**
     * @brief Get execution progress
     * @param name Script identifier
     * @return Progress (0.0 to 1.0)
     */
    [[nodiscard]] auto getScriptProgress(std::string_view name) const -> float;

    // =========================================================================
    // Hooks
    // =========================================================================

    /**
     * @brief Add a pre-execution hook
     * @param name Script identifier
     * @param hook Hook function
     */
    void addPreExecutionHook(std::string_view name, PreExecutionHook hook);

    /**
     * @brief Add a post-execution hook
     * @param name Script identifier
     * @param hook Hook function
     */
    void addPostExecutionHook(std::string_view name, PostExecutionHook hook);

    // =========================================================================
    // Versioning
    // =========================================================================

    /**
     * @brief Enable versioning for scripts
     */
    void enableVersioning();

    /**
     * @brief Rollback script to a previous version
     * @param name Script identifier
     * @param version Version number
     * @return true if successful
     */
    auto rollbackScript(std::string_view name, int version) -> bool;

    /**
     * @brief Set maximum versions to keep
     * @param maxVersions Maximum version count
     */
    void setMaxScriptVersions(int maxVersions);

    // =========================================================================
    // Metadata
    // =========================================================================

    /**
     * @brief Get script metadata
     * @param name Script identifier
     * @return Metadata if found
     */
    [[nodiscard]] auto getScriptMetadata(std::string_view name) const
        -> std::optional<ScriptMetadata>;

    /**
     * @brief Set script metadata
     * @param name Script identifier
     * @param metadata Metadata to set
     */
    void setScriptMetadata(std::string_view name, const ScriptMetadata& metadata);

    // =========================================================================
    // Resources
    // =========================================================================

    /**
     * @brief Set resource limits
     * @param limits Resource limits configuration
     */
    void setResourceLimits(const ScriptResourceLimits& limits);

    /**
     * @brief Get current resource limits
     * @return Resource limits configuration
     */
    [[nodiscard]] auto getResourceLimits() const -> ScriptResourceLimits;

    /**
     * @brief Get current resource usage
     * @return Map of resource name to value
     */
    [[nodiscard]] auto getResourceUsage() const
        -> std::unordered_map<std::string, double>;

    // =========================================================================
    // Discovery
    // =========================================================================

    /**
     * @brief Discover and load scripts from directory
     * @param directory Directory to scan
     * @param extensions File extensions to consider
     * @param recursive Scan recursively
     * @return Number of scripts loaded
     */
    auto discoverScripts(const std::filesystem::path& directory,
                         const std::vector<std::string>& extensions = {".py", ".sh"},
                         bool recursive = true) -> size_t;

    /**
     * @brief Detect script language from content
     * @param content Script content
     * @return Detected language
     */
    [[nodiscard]] static auto detectScriptLanguage(std::string_view content)
        -> ScriptLanguage;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get script execution statistics
     * @param name Script identifier
     * @return Statistics map
     */
    [[nodiscard]] auto getScriptStatistics(std::string_view name) const
        -> std::unordered_map<std::string, double>;

    /**
     * @brief Get global statistics
     * @return Global statistics map
     */
    [[nodiscard]] auto getGlobalStatistics() const
        -> std::unordered_map<std::string, double>;

    /**
     * @brief Reset statistics
     * @param name Script name (empty for all)
     */
    void resetStatistics(std::string_view name = "");

    // =========================================================================
    // Component Access (for advanced use)
    // =========================================================================

    /**
     * @brief Get the metadata manager
     * @return Reference to metadata manager
     */
    [[nodiscard]] auto metadata() -> MetadataManager&;

    /**
     * @brief Get the hook manager
     * @return Reference to hook manager
     */
    [[nodiscard]] auto hooks() -> HookManager&;

    /**
     * @brief Get the version manager
     * @return Reference to version manager
     */
    [[nodiscard]] auto versions() -> VersionManager&;

    /**
     * @brief Get the resource manager
     * @return Reference to resource manager
     */
    [[nodiscard]] auto resources() -> ResourceManager&;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_SCRIPT_MANAGER_HPP
