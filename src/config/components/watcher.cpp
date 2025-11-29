/*
 * watcher.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration File Watcher Implementation

**************************************************/

#include "watcher.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <exception>

namespace lithium::config {

ConfigWatcher::ConfigWatcher(const WatcherOptions& options)
    : options_(options),
      start_time_(std::chrono::steady_clock::now()),
      logger_(spdlog::get("config_watcher")) {
    if (!logger_) {
        logger_ = spdlog::default_logger();
    }

    logger_->info(
        "ConfigWatcher initialized with poll_interval={}ms, "
        "debounce_delay={}ms",
        options_.poll_interval.count(), options_.debounce_delay.count());

    if (options_.poll_interval < std::chrono::milliseconds(10)) {
        logger_->warn("Poll interval too low ({}ms), adjusting to 10ms minimum",
                      options_.poll_interval.count());
        const_cast<WatcherOptions&>(options_).poll_interval =
            std::chrono::milliseconds(10);
    }

    if (options_.max_events_per_second == 0) {
        logger_->warn("Max events per second is 0, setting to 1000");
        const_cast<WatcherOptions&>(options_).max_events_per_second = 1000;
    }
}

ConfigWatcher::~ConfigWatcher() {
    stopWatching();
    logger_->debug("ConfigWatcher destroyed");
}

bool ConfigWatcher::watchFile(const std::filesystem::path& file_path,
                              FileChangeCallback callback) {
    if (!callback) {
        logger_->error("Cannot watch file '{}': callback is null",
                       file_path.string());
        return false;
    }

    if (!std::filesystem::exists(file_path)) {
        logger_->error("Cannot watch file '{}': file does not exist",
                       file_path.string());
        return false;
    }

    if (std::filesystem::is_directory(file_path)) {
        logger_->error("Cannot watch file '{}': path is a directory",
                       file_path.string());
        return false;
    }

    std::unique_lock lock(mutex_);
    const auto canonical_path = std::filesystem::canonical(file_path);
    const auto key = canonical_path.string();

    if (watched_paths_.find(key) != watched_paths_.end()) {
        logger_->warn("File '{}' is already being watched",
                      canonical_path.string());
        return true;
    }

    try {
        watched_paths_.emplace(
            key, WatchedPath(canonical_path, std::move(callback), false));
        logger_->info("Started watching file: {}", canonical_path.string());

        std::unique_lock stats_lock(stats_mutex_);
        stats_.watched_paths_count = watched_paths_.size();

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to watch file '{}': {}", canonical_path.string(),
                       e.what());
        return false;
    }
}

bool ConfigWatcher::watchDirectory(const std::filesystem::path& directory_path,
                                   FileChangeCallback callback) {
    if (!callback) {
        logger_->error("Cannot watch directory '{}': callback is null",
                       directory_path.string());
        return false;
    }

    if (!std::filesystem::exists(directory_path)) {
        logger_->error("Cannot watch directory '{}': directory does not exist",
                       directory_path.string());
        return false;
    }

    if (!std::filesystem::is_directory(directory_path)) {
        logger_->error("Cannot watch directory '{}': path is not a directory",
                       directory_path.string());
        return false;
    }

    std::unique_lock lock(mutex_);
    const auto canonical_path = std::filesystem::canonical(directory_path);
    const auto key = canonical_path.string();

    if (watched_paths_.find(key) != watched_paths_.end()) {
        logger_->warn("Directory '{}' is already being watched",
                      canonical_path.string());
        return true;
    }

    try {
        watched_paths_.emplace(
            key, WatchedPath(canonical_path, std::move(callback), true));
        logger_->info("Started watching directory: {} (recursive={})",
                      canonical_path.string(), options_.recursive);

        std::unique_lock stats_lock(stats_mutex_);
        stats_.watched_paths_count = watched_paths_.size();

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to watch directory '{}': {}",
                       canonical_path.string(), e.what());
        return false;
    }
}

bool ConfigWatcher::stopWatching(const std::filesystem::path& path) {
    std::unique_lock lock(mutex_);

    try {
        const auto canonical_path = std::filesystem::canonical(path);
        const auto key = canonical_path.string();

        const auto it = watched_paths_.find(key);
        if (it == watched_paths_.end()) {
            logger_->warn("Path '{}' is not being watched",
                          canonical_path.string());
            return false;
        }

        watched_paths_.erase(it);
        logger_->info("Stopped watching path: {}", canonical_path.string());

        std::unique_lock stats_lock(stats_mutex_);
        stats_.watched_paths_count = watched_paths_.size();

        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to stop watching path '{}': {}", path.string(),
                       e.what());
        return false;
    }
}

void ConfigWatcher::stopAll() {
    std::unique_lock lock(mutex_);
    const size_t count = watched_paths_.size();
    watched_paths_.clear();

    logger_->info("Stopped watching all {} paths", count);

    std::unique_lock stats_lock(stats_mutex_);
    stats_.watched_paths_count = 0;
}

bool ConfigWatcher::isWatching(const std::filesystem::path& path) const {
    std::shared_lock lock(mutex_);

    try {
        const auto canonical_path = std::filesystem::canonical(path);
        const auto key = canonical_path.string();
        return watched_paths_.find(key) != watched_paths_.end();
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<std::filesystem::path> ConfigWatcher::getWatchedPaths() const {
    std::shared_lock lock(mutex_);
    std::vector<std::filesystem::path> paths;
    paths.reserve(watched_paths_.size());

    for (const auto& [key, watched_path] : watched_paths_) {
        paths.push_back(watched_path.path);
    }

    return paths;
}

bool ConfigWatcher::startWatching() {
    if (running_.load()) {
        logger_->warn("ConfigWatcher is already running");
        return true;
    }

    try {
        running_.store(true);
        watch_thread_ =
            std::make_unique<std::thread>(&ConfigWatcher::watchLoop, this);

        logger_->info("ConfigWatcher started successfully");
        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        logger_->error("Failed to start ConfigWatcher: {}", e.what());
        return false;
    }
}

void ConfigWatcher::stopWatching() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (watch_thread_ && watch_thread_->joinable()) {
        watch_thread_->join();
        watch_thread_.reset();
    }

    logger_->info("ConfigWatcher stopped");
}

void ConfigWatcher::updateOptions(const WatcherOptions& options) {
    const bool was_running = running_.load();

    if (was_running) {
        stopWatching();
    }

    options_ = options;
    logger_->info(
        "Updated watcher options: poll_interval={}ms, debounce_delay={}ms",
        options_.poll_interval.count(), options_.debounce_delay.count());

    if (was_running) {
        startWatching();
    }
}

ConfigWatcher::Statistics ConfigWatcher::getStatistics() const {
    std::shared_lock lock(stats_mutex_);
    return stats_;
}

void ConfigWatcher::resetStatistics() {
    std::unique_lock lock(stats_mutex_);
    stats_.total_events_processed = 0;
    stats_.events_debounced = 0;
    stats_.events_rate_limited = 0;
    stats_.average_processing_time_ms = 0.0;
    start_time_ = std::chrono::steady_clock::now();

    logger_->debug("Statistics reset");
}

void ConfigWatcher::watchLoop() {
    logger_->debug("Watch loop started");

    while (running_.load()) {
        const auto loop_start = std::chrono::steady_clock::now();

        try {
            std::shared_lock lock(mutex_);
            auto paths_copy = watched_paths_;
            lock.unlock();

            for (auto& [key, watched_path] : paths_copy) {
                if (!running_.load())
                    break;
                checkPath(watched_path);
            }

            lock.lock();
            for (const auto& [key, watched_path] : paths_copy) {
                if (auto it = watched_paths_.find(key);
                    it != watched_paths_.end()) {
                    it->second.last_write_time = watched_path.last_write_time;
                    it->second.last_event_time = watched_path.last_event_time;
                    it->second.event_count_this_second =
                        watched_path.event_count_this_second;
                }
            }

        } catch (const std::exception& e) {
            logger_->error("Error in watch loop: {}", e.what());
        }

        const auto loop_end = std::chrono::steady_clock::now();
        const auto loop_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(loop_end -
                                                                  loop_start);

        if (loop_duration < options_.poll_interval) {
            std::this_thread::sleep_for(options_.poll_interval - loop_duration);
        }
    }

    logger_->debug("Watch loop ended");
}

void ConfigWatcher::checkPath(WatchedPath& watched_path) {
    try {
        if (!std::filesystem::exists(watched_path.path)) {
            if (watched_path.last_write_time !=
                std::filesystem::file_time_type::min()) {
                triggerEvent(watched_path.path, FileEvent::DELETED,
                             watched_path.callback);
                watched_path.last_write_time =
                    std::filesystem::file_time_type::min();
            }
            return;
        }

        if (watched_path.is_directory) {
            processDirectory(watched_path);
        } else {
            const auto current_time =
                std::filesystem::last_write_time(watched_path.path);

            if (current_time != watched_path.last_write_time) {
                if (!shouldDebounce(watched_path) &&
                    !shouldRateLimit(watched_path)) {
                    const auto event = (watched_path.last_write_time ==
                                        std::filesystem::file_time_type::min())
                                           ? FileEvent::CREATED
                                           : FileEvent::MODIFIED;

                    triggerEvent(watched_path.path, event,
                                 watched_path.callback);
                }

                watched_path.last_write_time = current_time;
                watched_path.last_event_time = std::chrono::steady_clock::now();
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error checking path '{}': {}",
                       watched_path.path.string(), e.what());
    }
}

bool ConfigWatcher::shouldDebounce(const WatchedPath& watched_path) {
    const auto now = std::chrono::steady_clock::now();
    const auto time_since_last = now - watched_path.last_event_time;

    if (time_since_last < options_.debounce_delay) {
        std::unique_lock lock(stats_mutex_);
        ++stats_.events_debounced;
        return true;
    }

    return false;
}

bool ConfigWatcher::shouldRateLimit(WatchedPath& watched_path) {
    const auto now = std::chrono::steady_clock::now();
    const auto second_boundary =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();

    const auto last_second_boundary =
        std::chrono::duration_cast<std::chrono::seconds>(
            watched_path.last_event_time.time_since_epoch())
            .count();

    if (second_boundary != last_second_boundary) {
        watched_path.event_count_this_second = 0;
    }

    if (watched_path.event_count_this_second >=
        options_.max_events_per_second) {
        std::unique_lock lock(stats_mutex_);
        ++stats_.events_rate_limited;
        return true;
    }

    ++watched_path.event_count_this_second;
    return false;
}

bool ConfigWatcher::shouldWatchFile(const std::filesystem::path& path) const {
    if (options_.file_extensions.empty()) {
        return true;
    }

    const auto extension = path.extension().string();
    return std::find(options_.file_extensions.begin(),
                     options_.file_extensions.end(),
                     extension) != options_.file_extensions.end();
}

void ConfigWatcher::processDirectory(WatchedPath& watched_path) {
    try {
        auto process_entry =
            [&](const std::filesystem::directory_entry& entry) {
                if (!running_.load())
                    return;

                if (entry.is_regular_file() && shouldWatchFile(entry.path())) {
                    try {
                        const auto current_time = entry.last_write_time();
                        if (current_time > watched_path.last_write_time) {
                            triggerEvent(entry.path(), FileEvent::MODIFIED,
                                         watched_path.callback);
                            watched_path.last_write_time = current_time;
                            watched_path.last_event_time =
                                std::chrono::steady_clock::now();
                        }
                    } catch (const std::exception& e) {
                        logger_->debug("Could not check file '{}': {}",
                                       entry.path().string(), e.what());
                    }
                }
            };

        if (options_.recursive) {
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(
                     watched_path.path)) {
                process_entry(entry);
            }
        } else {
            for (const auto& entry :
                 std::filesystem::directory_iterator(watched_path.path)) {
                process_entry(entry);
            }
        }

    } catch (const std::exception& e) {
        logger_->error("Error processing directory '{}': {}",
                       watched_path.path.string(), e.what());
    }
}

void ConfigWatcher::triggerEvent(const std::filesystem::path& path,
                                 FileEvent event,
                                 const FileChangeCallback& callback) {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        callback(path, event);

        std::unique_lock lock(stats_mutex_);
        ++stats_.total_events_processed;
        stats_.last_event_time = std::chrono::steady_clock::now();

        const auto processing_time =
            std::chrono::duration_cast<std::chrono::microseconds>(
                stats_.last_event_time - start_time)
                .count() /
            1000.0;

        if (stats_.total_events_processed == 1) {
            stats_.average_processing_time_ms = processing_time;
        } else {
            stats_.average_processing_time_ms =
                (stats_.average_processing_time_ms *
                     (stats_.total_events_processed - 1) +
                 processing_time) /
                stats_.total_events_processed;
        }

        static constexpr const char* event_names[] = {"CREATED", "MODIFIED",
                                                      "DELETED", "MOVED"};
        const auto event_idx = static_cast<size_t>(event);
        const char* event_name =
            (event_idx < 4) ? event_names[event_idx] : "UNKNOWN";

        logger_->debug("File event triggered: {} - {} (processing_time={}ms)",
                       event_name, path.string(), processing_time);

    } catch (const std::exception& e) {
        logger_->error("Error in callback for path '{}': {}", path.string(),
                       e.what());
    }
}

}  // namespace lithium::config
