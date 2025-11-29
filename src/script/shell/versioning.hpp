/*
 * versioning.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file versioning.hpp
 * @brief Script version management and rollback functionality
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_VERSIONING_HPP
#define LITHIUM_SCRIPT_SHELL_VERSIONING_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lithium::shell {

/**
 * @brief Represents a single script version
 */
struct ScriptVersion {
    size_t versionNumber{0};                          ///< Version number
    std::string content;                              ///< Script content
    std::chrono::system_clock::time_point timestamp;  ///< Creation timestamp
    std::string author;                               ///< Author of this version
    std::string changeDescription;                    ///< Description of changes
};

/**
 * @brief Version manager for script versioning and rollback
 *
 * This class provides comprehensive version control for scripts including:
 * - Saving script versions with timestamps
 * - Retrieving specific versions
 * - Rolling back to previous versions
 * - Managing version history with automatic cleanup
 * - Thread-safe operations via RAII mutex guards
 */
class VersionManager {
public:
    /**
     * @brief Constructor for VersionManager
     * @param maxVersions Maximum number of versions to keep per script
     *                   (default: 10)
     */
    explicit VersionManager(size_t maxVersions = 10) noexcept;

    /**
     * @brief Destructor for VersionManager
     */
    ~VersionManager() noexcept;

    // Disable copy operations to prevent data races
    VersionManager(const VersionManager&) = delete;
    VersionManager& operator=(const VersionManager&) = delete;

    // Allow move operations
    VersionManager(VersionManager&&) noexcept = default;
    VersionManager& operator=(VersionManager&&) noexcept = default;

    /**
     * @brief Save a new version of a script
     *
     * @param scriptName The name/identifier of the script
     * @param content The script content to save
     * @param author The author/user saving this version
     * @param changeDescription Description of changes in this version
     * @return Version number of the saved version
     *
     * @details Thread-safe operation. If the number of versions exceeds
     * maxVersions, the oldest version is automatically deleted.
     */
    auto saveVersion(std::string_view scriptName, std::string_view content,
                     std::string_view author = "",
                     std::string_view changeDescription = "") -> size_t;

    /**
     * @brief Get a specific version of a script
     *
     * @param scriptName The name/identifier of the script
     * @param versionNumber The version number to retrieve
     * @return Optional ScriptVersion if found, std::nullopt otherwise
     *
     * @details Thread-safe operation. Returns a copy of the version data.
     */
    [[nodiscard]] auto getVersion(std::string_view scriptName,
                                   size_t versionNumber) const
        -> std::optional<ScriptVersion>;

    /**
     * @brief Get the latest version of a script
     *
     * @param scriptName The name/identifier of the script
     * @return Optional ScriptVersion of the latest version, std::nullopt if
     * no versions exist
     *
     * @details Thread-safe operation.
     */
    [[nodiscard]] auto getLatestVersion(std::string_view scriptName) const
        -> std::optional<ScriptVersion>;

    /**
     * @brief Rollback a script to a specific version
     *
     * @param scriptName The name/identifier of the script
     * @param versionNumber The version number to rollback to
     * @return The content of the rolled-back version, or std::nullopt if
     * version not found
     *
     * @details Thread-safe operation. Note: rollback does not create a new
     * version automatically; use saveVersion() after rollback if you want to
     * preserve the rollback action.
     */
    [[nodiscard]] auto rollback(std::string_view scriptName,
                                 size_t versionNumber) const
        -> std::optional<std::string>;

    /**
     * @brief Get the full version history of a script
     *
     * @param scriptName The name/identifier of the script
     * @return Vector of ScriptVersion objects in chronological order
     *
     * @details Thread-safe operation. Returns versions sorted from oldest to
     * newest.
     */
    [[nodiscard]] auto getVersionHistory(std::string_view scriptName) const
        -> std::vector<ScriptVersion>;

    /**
     * @brief Get version count for a script
     *
     * @param scriptName The name/identifier of the script
     * @return Number of versions for the script, 0 if script not found
     *
     * @details Thread-safe operation.
     */
    [[nodiscard]] auto getVersionCount(std::string_view scriptName) const
        -> size_t;

    /**
     * @brief Set the maximum number of versions to keep
     *
     * @param maxVersions Maximum number of versions per script
     *
     * @details Thread-safe operation. Existing versions exceeding the new
     * limit are pruned, keeping the most recent versions.
     */
    void setMaxVersions(size_t maxVersions) noexcept;

    /**
     * @brief Get the maximum number of versions per script
     *
     * @return Current maximum version count
     *
     * @details Thread-safe operation.
     */
    [[nodiscard]] auto getMaxVersions() const noexcept -> size_t;

    /**
     * @brief Clear all versions for a specific script
     *
     * @param scriptName The name/identifier of the script
     *
     * @details Thread-safe operation.
     */
    void clearVersionHistory(std::string_view scriptName);

    /**
     * @brief Clear version history for all scripts
     *
     * @details Thread-safe operation. Completely empties the version storage.
     */
    void clearAllVersionHistory() noexcept;

    /**
     * @brief Check if a script has any versions
     *
     * @param scriptName The name/identifier of the script
     * @return true if script has at least one version, false otherwise
     *
     * @details Thread-safe operation.
     */
    [[nodiscard]] auto hasVersions(std::string_view scriptName) const noexcept
        -> bool;

    /**
     * @brief Get all scripts that have version history
     *
     * @return Vector of script names with versions
     *
     * @details Thread-safe operation.
     */
    [[nodiscard]] auto getAllVersionedScripts() const -> std::vector<std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;  ///< Private implementation
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_VERSIONING_HPP
