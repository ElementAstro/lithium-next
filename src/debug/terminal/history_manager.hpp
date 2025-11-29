/**
 * @file history_manager.hpp
 * @brief Command history management with persistence and search
 *
 * This component provides advanced command history features including
 * persistence, search, favorites, and statistics.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_HISTORY_MANAGER_HPP
#define LITHIUM_DEBUG_TERMINAL_HISTORY_MANAGER_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::debug::terminal {

/**
 * @brief History manager configuration
 */
struct HistoryConfig {
    size_t maxEntries{1000};
    bool persistOnAdd{false};
    bool deduplicateConsecutive{true};
    bool trackTimestamps{true};
    bool trackResults{false};
    std::string historyFile;
};

/**
 * @brief History search options
 */
struct HistorySearchOptions {
    bool caseSensitive{false};
    bool regexSearch{false};
    bool prefixMatch{false};
    bool reverseOrder{true};
    size_t maxResults{50};
    std::optional<std::chrono::system_clock::time_point> afterTime;
    std::optional<std::chrono::system_clock::time_point> beforeTime;
    std::vector<std::string> tags;
};

/**
 * @brief History statistics
 */
struct HistoryStats {
    size_t totalEntries{0};
    size_t uniqueCommands{0};
    size_t favoriteCount{0};
    std::chrono::system_clock::time_point oldestEntry;
    std::chrono::system_clock::time_point newestEntry;
    std::vector<std::pair<std::string, size_t>>
        topCommands;  // command -> count
};

/**
 * @brief Command history manager
 */
class HistoryManager {
public:
    /**
     * @brief Construct history manager with configuration
     */
    explicit HistoryManager(const HistoryConfig& config = HistoryConfig{});

    ~HistoryManager();

    // Non-copyable, movable
    HistoryManager(const HistoryManager&) = delete;
    HistoryManager& operator=(const HistoryManager&) = delete;
    HistoryManager(HistoryManager&&) noexcept;
    HistoryManager& operator=(HistoryManager&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void setConfig(const HistoryConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const HistoryConfig& getConfig() const;

    // =========================================================================
    // Entry Management
    // =========================================================================

    /**
     * @brief Add command to history
     */
    void add(const std::string& command);

    /**
     * @brief Add command with result
     */
    void add(const std::string& command, const CommandResult& result);

    /**
     * @brief Add full history entry
     */
    void add(const HistoryEntry& entry);

    /**
     * @brief Get entry by index (0 = oldest)
     */
    [[nodiscard]] std::optional<HistoryEntry> get(size_t index) const;

    /**
     * @brief Get most recent entry
     */
    [[nodiscard]] std::optional<HistoryEntry> getLast() const;

    /**
     * @brief Get entry relative to current position
     * @param offset Negative for older, positive for newer
     */
    [[nodiscard]] std::optional<HistoryEntry> getRelative(int offset) const;

    /**
     * @brief Remove entry by index
     */
    bool remove(size_t index);

    /**
     * @brief Remove entries matching predicate
     */
    size_t removeIf(std::function<bool(const HistoryEntry&)> predicate);

    /**
     * @brief Clear all history
     */
    void clear();

    /**
     * @brief Get total number of entries
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Check if history is empty
     */
    [[nodiscard]] bool empty() const;

    // =========================================================================
    // Navigation
    // =========================================================================

    /**
     * @brief Move to previous entry
     * @return The previous entry, or nullopt if at beginning
     */
    std::optional<HistoryEntry> previous();

    /**
     * @brief Move to next entry
     * @return The next entry, or nullopt if at end
     */
    std::optional<HistoryEntry> next();

    /**
     * @brief Reset navigation to end (most recent)
     */
    void resetNavigation();

    /**
     * @brief Get current navigation position
     */
    [[nodiscard]] size_t getPosition() const;

    /**
     * @brief Set navigation position
     */
    void setPosition(size_t pos);

    // =========================================================================
    // Search
    // =========================================================================

    /**
     * @brief Search history with pattern
     */
    [[nodiscard]] std::vector<HistoryEntry> search(
        const std::string& pattern,
        const HistorySearchOptions& options = HistorySearchOptions{}) const;

    /**
     * @brief Search for commands starting with prefix
     */
    [[nodiscard]] std::vector<HistoryEntry> searchPrefix(
        const std::string& prefix, size_t maxResults = 10) const;

    /**
     * @brief Reverse search (like Ctrl+R in bash)
     */
    [[nodiscard]] std::optional<HistoryEntry> reverseSearch(
        const std::string& pattern, size_t startPos = 0) const;

    /**
     * @brief Get commands containing substring
     */
    [[nodiscard]] std::vector<std::string> getMatching(
        const std::string& substring) const;

    // =========================================================================
    // Favorites
    // =========================================================================

    /**
     * @brief Mark entry as favorite
     */
    bool setFavorite(size_t index, bool favorite = true);

    /**
     * @brief Toggle favorite status
     */
    bool toggleFavorite(size_t index);

    /**
     * @brief Get all favorite entries
     */
    [[nodiscard]] std::vector<HistoryEntry> getFavorites() const;

    // =========================================================================
    // Tags
    // =========================================================================

    /**
     * @brief Add tag to entry
     */
    bool addTag(size_t index, const std::string& tag);

    /**
     * @brief Remove tag from entry
     */
    bool removeTag(size_t index, const std::string& tag);

    /**
     * @brief Get entries with tag
     */
    [[nodiscard]] std::vector<HistoryEntry> getByTag(
        const std::string& tag) const;

    /**
     * @brief Get all unique tags
     */
    [[nodiscard]] std::vector<std::string> getAllTags() const;

    // =========================================================================
    // Persistence
    // =========================================================================

    /**
     * @brief Load history from file
     */
    bool load(const std::string& path);

    /**
     * @brief Load from configured file
     */
    bool load();

    /**
     * @brief Save history to file
     */
    bool save(const std::string& path) const;

    /**
     * @brief Save to configured file
     */
    bool save() const;

    /**
     * @brief Export history as JSON
     */
    [[nodiscard]] std::string exportJson() const;

    /**
     * @brief Import history from JSON
     */
    bool importJson(const std::string& json);

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get history statistics
     */
    [[nodiscard]] HistoryStats getStats() const;

    /**
     * @brief Get command frequency
     */
    [[nodiscard]] std::vector<std::pair<std::string, size_t>>
    getCommandFrequency(size_t topN = 10) const;

    /**
     * @brief Get commands used in time range
     */
    [[nodiscard]] std::vector<HistoryEntry> getInTimeRange(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) const;

    // =========================================================================
    // Iteration
    // =========================================================================

    /**
     * @brief Get all entries
     */
    [[nodiscard]] std::vector<HistoryEntry> getAll() const;

    /**
     * @brief Get recent entries
     */
    [[nodiscard]] std::vector<HistoryEntry> getRecent(size_t count) const;

    /**
     * @brief Iterate over entries
     */
    void forEach(std::function<void(const HistoryEntry&)> callback) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_HISTORY_MANAGER_HPP
