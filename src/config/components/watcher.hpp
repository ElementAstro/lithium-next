/*
 * watcher.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration File Watcher Component

**************************************************/

#ifndef LITHIUM_CONFIG_COMPONENTS_WATCHER_HPP
#define LITHIUM_CONFIG_COMPONENTS_WATCHER_HPP

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

namespace lithium::config {

/**
 * @brief File change event types for configuration watching
 */
enum class FileEvent {
    CREATED,   ///< File was created
    MODIFIED,  ///< File was modified
    DELETED,   ///< File was deleted
    MOVED      ///< File was moved/renamed
};

/**
 * @brief File change callback function signature
 * @param path The file path that changed
 * @param event The type of change event
 */
using FileChangeCallback =
    std::function<void(const std::filesystem::path&, FileEvent)>;

/**
 * @brief Configuration file watcher for automatic reload functionality
 *
 * This component provides file system monitoring capabilities to automatically
 * detect changes in configuration files and trigger reload operations. It
 * supports both individual file watching and directory monitoring with
 * recursive capabilities.
 *
 * Features:
 * - Cross-platform file system monitoring
 * - Configurable polling intervals
 * - Recursive directory watching
 * - Debounced change detection to avoid rapid successive events
 * - Thread-safe operations
 * - Comprehensive event filtering
 *
 * @thread_safety This class is thread-safe for all public operations
 */
class ConfigWatcher {
public:
    /**
     * @brief Configuration options for the file watcher
     */
    struct WatcherOptions {
        std::chrono::milliseconds poll_interval{
            100};  ///< Polling interval for file changes
        std::chrono::milliseconds debounce_delay{
            250};               ///< Debounce delay to avoid rapid events
        bool recursive{false};  ///< Enable recursive directory watching
        bool watch_subdirectories{true};  ///< Watch for new subdirectories
        std::vector<std::string>
            file_extensions;  ///< Filter by file extensions (empty = all)
        size_t max_events_per_second{100};  ///< Rate limiting for events

        WatcherOptions()
            : poll_interval(100),
              debounce_delay(250),
              recursive(false),
              watch_subdirectories(true),
              max_events_per_second(100) {}

        WatcherOptions(std::chrono::milliseconds poll,
                       std::chrono::milliseconds debounce, bool rec = false,
                       bool watch_subdirs = true,
                       std::vector<std::string> extensions = {},
                       size_t max_events = 100)
            : poll_interval(poll),
              debounce_delay(debounce),
              recursive(rec),
              watch_subdirectories(watch_subdirs),
              file_extensions(std::move(extensions)),
              max_events_per_second(max_events) {}
    };

    /**
     * @brief Construct a new ConfigWatcher
     * @param options Configuration options for the watcher
     */
    explicit ConfigWatcher(const WatcherOptions& options = {});

    /**
     * @brief Destructor - automatically stops watching
     */
    ~ConfigWatcher();

    ConfigWatcher(const ConfigWatcher&) = delete;
    ConfigWatcher& operator=(const ConfigWatcher&) = delete;

    /**
     * @brief Start watching a specific file
     * @param file_path Path to the file to watch
     * @param callback Callback function to invoke on changes
     * @return true if watching started successfully
     */
    [[nodiscard]] bool watchFile(const std::filesystem::path& file_path,
                                 FileChangeCallback callback);

    /**
     * @brief Start watching a directory
     * @param directory_path Path to the directory to watch
     * @param callback Callback function to invoke on changes
     * @return true if watching started successfully
     */
    [[nodiscard]] bool watchDirectory(
        const std::filesystem::path& directory_path,
        FileChangeCallback callback);

    /**
     * @brief Stop watching a specific file or directory
     * @param path Path to stop watching
     * @return true if path was being watched and is now stopped
     */
    bool stopWatching(const std::filesystem::path& path);

    /**
     * @brief Stop watching all files and directories
     */
    void stopAll();

    /**
     * @brief Check if a path is currently being watched
     * @param path Path to check
     * @return true if path is being watched
     */
    [[nodiscard]] bool isWatching(const std::filesystem::path& path) const;

    /**
     * @brief Get list of all watched paths
     * @return Vector of currently watched paths
     */
    [[nodiscard]] std::vector<std::filesystem::path> getWatchedPaths() const;

    /**
     * @brief Start the file watching service
     * @return true if service started successfully
     */
    bool startWatching();

    /**
     * @brief Check if the watcher is currently running
     * @return true if watcher is active
     */
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

    /**
     * @brief Update watcher options
     * @param options New options to apply
     * @note This will restart the watcher if it's currently running
     */
    void updateOptions(const WatcherOptions& options);

    /**
     * @brief Get current watcher options
     * @return Current configuration options
     */
    [[nodiscard]] const WatcherOptions& getOptions() const noexcept {
        return options_;
    }

    /**
     * @brief Get watcher statistics
     */
    struct Statistics {
        size_t watched_paths_count{0};     ///< Number of watched paths
        size_t total_events_processed{0};  ///< Total events processed
        size_t events_debounced{0};        ///< Events filtered by debouncing
        size_t events_rate_limited{0};     ///< Events filtered by rate limiting
        std::chrono::steady_clock::time_point
            last_event_time;  ///< Last event timestamp
        double average_processing_time_ms{
            0.0};  ///< Average event processing time
    };

    /**
     * @brief Get current statistics
     * @return Current watcher statistics
     */
    [[nodiscard]] Statistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Pause file watching temporarily
     * @note Events will be queued and processed when resumed
     */
    void pause();

    /**
     * @brief Resume file watching after pause
     */
    void resume();

    /**
     * @brief Check if watcher is paused
     * @return True if paused
     */
    [[nodiscard]] bool isPaused() const noexcept;

    /**
     * @brief Force check all watched paths immediately
     * @note Useful for manual refresh without waiting for poll interval
     */
    void forceCheck();

    /**
     * @brief Get the number of pending events (when paused)
     * @return Number of queued events
     */
    [[nodiscard]] size_t getPendingEventCount() const;

    // ========================================================================
    // Hook Support
    // ========================================================================

    /**
     * @brief Watcher lifecycle event types for hooks
     */
    enum class WatcherEvent {
        STARTED,         ///< Watcher started
        STOPPED,         ///< Watcher stopped
        PAUSED,          ///< Watcher paused
        RESUMED,         ///< Watcher resumed
        PATH_ADDED,      ///< New path added to watch
        PATH_REMOVED,    ///< Path removed from watch
        FILE_CHANGED,    ///< File change detected (before callback)
        ERROR_OCCURRED   ///< Error occurred during watching
    };

    /**
     * @brief Watcher hook callback signature
     * @param event Type of watcher event
     * @param path Related path (if applicable)
     * @param fileEvent File event type (for FILE_CHANGED)
     */
    using WatcherHook = std::function<void(WatcherEvent event,
                                           const std::filesystem::path& path,
                                           std::optional<FileEvent> fileEvent)>;

    /**
     * @brief Register a watcher event hook
     * @param hook Callback function for watcher events
     * @return Hook ID for later removal
     */
    size_t addHook(WatcherHook hook);

    /**
     * @brief Remove a registered hook
     * @param hookId ID of the hook to remove
     * @return True if hook was removed
     */
    bool removeHook(size_t hookId);

    /**
     * @brief Clear all registered hooks
     */
    void clearHooks();

    /**
     * @brief Set error handler for watcher errors
     * @param handler Error handler function
     */
    void setErrorHandler(std::function<void(const std::string& error)> handler);

private:
    /**
     * @brief Internal structure to track watched paths
     */
    struct WatchedPath {
        std::filesystem::path path;
        FileChangeCallback callback;
        std::filesystem::file_time_type last_write_time;
        bool is_directory;
        std::chrono::steady_clock::time_point last_event_time;
        mutable size_t event_count_this_second{0};

        WatchedPath(std::filesystem::path p, FileChangeCallback cb, bool is_dir)
            : path(std::move(p)),
              callback(std::move(cb)),
              is_directory(is_dir),
              last_event_time(std::chrono::steady_clock::now()) {
            try {
                if (std::filesystem::exists(path)) {
                    last_write_time = std::filesystem::last_write_time(path);
                }
            } catch (const std::exception&) {
                last_write_time = std::filesystem::file_time_type::min();
            }
        }
    };

    void watchLoop();
    void stopWatching();
    void checkPath(WatchedPath& watched_path);
    bool shouldDebounce(const WatchedPath& watched_path);
    bool shouldRateLimit(WatchedPath& watched_path);
    bool shouldWatchFile(const std::filesystem::path& path) const;
    void processDirectory(WatchedPath& watched_path);
    void triggerEvent(const std::filesystem::path& path, FileEvent event,
                      const FileChangeCallback& callback);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, WatchedPath> watched_paths_;
    WatcherOptions options_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> watch_thread_;

    mutable std::shared_mutex stats_mutex_;
    Statistics stats_;
    std::chrono::steady_clock::time_point start_time_;

    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_COMPONENTS_WATCHER_HPP
