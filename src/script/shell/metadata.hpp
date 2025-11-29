/*
 * metadata.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file metadata.hpp
 * @brief Script metadata management
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_METADATA_HPP
#define LITHIUM_SCRIPT_SHELL_METADATA_HPP

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace lithium::shell {

/**
 * @brief Extended script metadata
 */
struct ScriptMetadata {
    std::string description;                          ///< Script description
    std::vector<std::string> tags;                    ///< Categorization tags
    std::chrono::system_clock::time_point createdAt;  ///< Creation timestamp
    std::chrono::system_clock::time_point lastModified;  ///< Last modification
    size_t version{1};                                ///< Version number
    std::unordered_map<std::string, std::string> parameters;  ///< Parameters
    ScriptLanguage language{ScriptLanguage::Auto};    ///< Script language
    std::string author;                               ///< Script author
    std::vector<std::string> dependencies;            ///< Dependencies
    std::optional<std::filesystem::path> sourcePath;  ///< Source file path
    bool isPython{false};                             ///< Is Python script

    /**
     * @brief Create metadata with current timestamp
     */
    static auto create() -> ScriptMetadata;

    /**
     * @brief Update the lastModified timestamp to now
     */
    void touch();
};

/**
 * @brief Manages script metadata storage and retrieval
 *
 * Thread-safe manager for script metadata with support for
 * tagging, searching, and filtering scripts.
 */
class MetadataManager {
public:
    MetadataManager();
    ~MetadataManager();

    // Non-copyable, movable
    MetadataManager(const MetadataManager&) = delete;
    MetadataManager& operator=(const MetadataManager&) = delete;
    MetadataManager(MetadataManager&&) noexcept;
    MetadataManager& operator=(MetadataManager&&) noexcept;

    /**
     * @brief Set metadata for a script
     * @param scriptName Script identifier
     * @param metadata Metadata to store
     */
    void setMetadata(std::string_view scriptName, const ScriptMetadata& metadata);

    /**
     * @brief Get metadata for a script
     * @param scriptName Script identifier
     * @return Metadata if found
     */
    [[nodiscard]] auto getMetadata(std::string_view scriptName) const
        -> std::optional<ScriptMetadata>;

    /**
     * @brief Remove metadata for a script
     * @param scriptName Script identifier
     * @return true if removed
     */
    auto removeMetadata(std::string_view scriptName) -> bool;

    /**
     * @brief Check if metadata exists for a script
     * @param scriptName Script identifier
     * @return true if metadata exists
     */
    [[nodiscard]] auto hasMetadata(std::string_view scriptName) const -> bool;

    /**
     * @brief Get all script names that have metadata
     * @return Vector of script names
     */
    [[nodiscard]] auto getAllScriptNames() const -> std::vector<std::string>;

    /**
     * @brief Find scripts by tag
     * @param tag Tag to search for
     * @return Vector of script names with matching tag
     */
    [[nodiscard]] auto findByTag(std::string_view tag) const
        -> std::vector<std::string>;

    /**
     * @brief Find scripts by language
     * @param language Script language
     * @return Vector of script names with matching language
     */
    [[nodiscard]] auto findByLanguage(ScriptLanguage language) const
        -> std::vector<std::string>;

    /**
     * @brief Find scripts by author
     * @param author Author name
     * @return Vector of script names by author
     */
    [[nodiscard]] auto findByAuthor(std::string_view author) const
        -> std::vector<std::string>;

    /**
     * @brief Clear all metadata
     */
    void clear();

    /**
     * @brief Get count of stored metadata
     * @return Number of scripts with metadata
     */
    [[nodiscard]] auto size() const -> size_t;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_METADATA_HPP
