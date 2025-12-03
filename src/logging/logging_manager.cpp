/*
 * logging_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "logging_manager.hpp"

#include <algorithm>
#include <regex>

#include <spdlog/async.h>

namespace lithium::logging {

auto LoggingManager::getInstance() -> LoggingManager& {
    static LoggingManager instance;
    return instance;
}

LoggingManager::~LoggingManager() {
    if (initialized_) {
        shutdown();
    }
}

void LoggingManager::initialize(const LoggingConfig& config) {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        spdlog::warn("LoggingManager already initialized, reinitializing...");
        // Don't call shutdown() here to avoid deadlock, just clean up
        spdlog::drop_all();
        sinks_.clear();
        ring_buffer_sink_.reset();
    }

    config_ = config;

    // Create ring buffer sink for log streaming
    ring_buffer_sink_ =
        std::make_shared<RingBufferSink>(config.ring_buffer_size);
    ring_buffer_sink_->set_level(spdlog::level::trace);  // Capture all levels
    sinks_["ringbuffer"] = ring_buffer_sink_;

    // Initialize async logging if enabled
    if (config.async_logging) {
        spdlog::init_thread_pool(config.async_queue_size,
                                 config.async_thread_count);
    }

    // Create configured sinks
    for (const auto& sink_config : config.sinks) {
        auto sink = SinkFactory::createSink(sink_config);
        if (sink) {
            sinks_[sink_config.name] = sink;
        }
    }

    // Setup default logger with all sinks
    setupDefaultLogger();

    initialized_ = true;
    spdlog::info("LoggingManager initialized with {} sinks", sinks_.size());
}

void LoggingManager::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    spdlog::info("LoggingManager shutting down...");

    // Flush all loggers
    registry_.flushAll();

    // Clear ring buffer callbacks
    if (ring_buffer_sink_) {
        ring_buffer_sink_->clear();
    }

    // Drop all loggers
    spdlog::drop_all();

    // Clear internal state
    sinks_.clear();
    ring_buffer_sink_.reset();

    initialized_ = false;
}

auto LoggingManager::isInitialized() const -> bool {
    std::shared_lock lock(mutex_);
    return initialized_;
}

auto LoggingManager::getLogger(const std::string& name)
    -> std::shared_ptr<spdlog::logger> {
    std::unique_lock lock(mutex_);

    std::vector<spdlog::sink_ptr> sink_list;
    for (const auto& [sink_name, sink] : sinks_) {
        sink_list.push_back(sink);
    }

    return registry_.getOrCreate(name, sink_list, config_.default_level,
                                 config_.default_pattern);
}

auto LoggingManager::listLoggers() const -> std::vector<LoggerInfo> {
    std::shared_lock lock(mutex_);
    return registry_.list();
}

auto LoggingManager::setLoggerLevel(const std::string& name,
                                    spdlog::level::level_enum level) -> bool {
    std::unique_lock lock(mutex_);

    bool result = registry_.setLevel(name, level);
    if (result) {
        spdlog::info("Logger '{}' level set to {}", name,
                     spdlog::level::to_string_view(level));
    }
    return result;
}

void LoggingManager::setGlobalLevel(spdlog::level::level_enum level) {
    std::unique_lock lock(mutex_);

    registry_.setGlobalLevel(level);
    config_.default_level = level;
    spdlog::info("Global log level set to {}",
                 spdlog::level::to_string_view(level));
}

auto LoggingManager::setLoggerPattern(const std::string& name,
                                      const std::string& pattern) -> bool {
    std::unique_lock lock(mutex_);
    return registry_.setPattern(name, pattern);
}

auto LoggingManager::removeLogger(const std::string& name) -> bool {
    std::unique_lock lock(mutex_);
    return registry_.remove(name);
}

auto LoggingManager::addSink(const SinkConfig& config) -> bool {
    std::unique_lock lock(mutex_);

    if (sinks_.contains(config.name)) {
        spdlog::warn("Sink '{}' already exists", config.name);
        return false;
    }

    auto sink = SinkFactory::createSink(config);
    if (!sink) {
        return false;
    }

    sinks_[config.name] = sink;

    // Add sink to all existing loggers
    registry_.addSinkToAll(sink);

    spdlog::info("Sink '{}' added", config.name);
    return true;
}

auto LoggingManager::removeSink(const std::string& name) -> bool {
    std::unique_lock lock(mutex_);

    if (name == "ringbuffer") {
        return false;  // Cannot remove ring buffer sink
    }

    auto it = sinks_.find(name);
    if (it == sinks_.end()) {
        return false;
    }

    auto sink_to_remove = it->second;
    sinks_.erase(it);

    // Remove sink from all loggers
    registry_.removeSinkFromAll(sink_to_remove);

    spdlog::info("Sink '{}' removed", name);
    return true;
}

auto LoggingManager::listSinks() const -> std::vector<SinkConfig> {
    std::shared_lock lock(mutex_);

    std::vector<SinkConfig> result;
    for (const auto& sink_config : config_.sinks) {
        result.push_back(sink_config);
    }

    // Add ring buffer sink info
    SinkConfig rb_config;
    rb_config.name = "ringbuffer";
    rb_config.type = "ringbuffer";
    rb_config.level = spdlog::level::trace;
    result.push_back(rb_config);

    return result;
}

auto LoggingManager::getRecentLogs(size_t count) const
    -> std::vector<LogEntry> {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        return {};
    }

    return ring_buffer_sink_->getEntries(count);
}

auto LoggingManager::getLogsSince(std::chrono::system_clock::time_point since)
    const -> std::vector<LogEntry> {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        return {};
    }

    return ring_buffer_sink_->getEntriesSince(since);
}

auto LoggingManager::getLogsFiltered(
    std::optional<spdlog::level::level_enum> level,
    std::optional<std::string> logger_name, size_t max_count) const
    -> std::vector<LogEntry> {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        return {};
    }

    return ring_buffer_sink_->getEntriesFiltered(level, logger_name, max_count);
}

void LoggingManager::clearLogBuffer() {
    std::unique_lock lock(mutex_);

    if (ring_buffer_sink_) {
        ring_buffer_sink_->clear();
        spdlog::info("Log buffer cleared");
    }
}

auto LoggingManager::getBufferStats() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        return {{"error", "Ring buffer not initialized"}};
    }

    return {{"size", ring_buffer_sink_->size()},
            {"capacity", ring_buffer_sink_->capacity()},
            {"usage_percent", static_cast<double>(ring_buffer_sink_->size()) /
                                  ring_buffer_sink_->capacity() * 100.0}};
}

void LoggingManager::subscribe(const std::string& subscriber_id,
                               LogCallback callback) {
    std::unique_lock lock(mutex_);

    if (ring_buffer_sink_) {
        ring_buffer_sink_->addCallback(subscriber_id, std::move(callback));
        spdlog::debug("Subscriber '{}' added to log stream", subscriber_id);
    }
}

void LoggingManager::unsubscribe(const std::string& subscriber_id) {
    std::unique_lock lock(mutex_);

    if (ring_buffer_sink_) {
        ring_buffer_sink_->removeCallback(subscriber_id);
        spdlog::debug("Subscriber '{}' removed from log stream", subscriber_id);
    }
}

void LoggingManager::flush() {
    std::shared_lock lock(mutex_);
    registry_.flushAll();
}

void LoggingManager::rotate() {
    std::unique_lock lock(mutex_);

    // For rotating file sinks, we can trigger rotation by flushing
    // and the next write will create a new file if size limit is reached
    registry_.flushAll();
    spdlog::info("Log rotation triggered");
}

auto LoggingManager::getConfig() const -> LoggingConfig {
    std::shared_lock lock(mutex_);
    return config_;
}

void LoggingManager::setupDefaultLogger() {
    std::vector<spdlog::sink_ptr> sink_list;
    for (const auto& [name, sink] : sinks_) {
        sink_list.push_back(sink);
    }

    auto default_logger = std::make_shared<spdlog::logger>(
        "default", sink_list.begin(), sink_list.end());

    default_logger->set_level(config_.default_level);
    default_logger->set_pattern(config_.default_pattern);

    spdlog::set_default_logger(default_logger);
}

// ============================================================================
// Statistics Methods
// ============================================================================

auto LoggingManager::getStatistics() -> LogStatistics& {
    return LogStatistics::getInstance();
}

auto LoggingManager::getStatsSummary() const -> nlohmann::json {
    return LogStatistics::getInstance().getSummary();
}

void LoggingManager::resetStatistics() { LogStatistics::getInstance().reset(); }

// ============================================================================
// Search Methods
// ============================================================================

auto LoggingManager::searchLogs(const LogSearchQuery& query) const
    -> LogSearchResult {
    auto start_time = std::chrono::steady_clock::now();

    LogSearchResult result;

    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        // Return empty result when ring buffer is not available
        return result;
    }

    // Get all entries from buffer
    auto all_entries = ring_buffer_sink_->getEntries(0);

    // Apply filters
    std::vector<LogEntry> filtered;
    for (const auto& entry : all_entries) {
        bool matches = true;

        // Level filter
        if (query.min_level && entry.level < *query.min_level) {
            matches = false;
        }
        if (query.max_level && entry.level > *query.max_level) {
            matches = false;
        }

        // Logger name filter
        if (query.logger_name &&
            entry.logger_name.find(*query.logger_name) == std::string::npos) {
            matches = false;
        }

        // Time filter
        if (query.start_time && entry.timestamp < *query.start_time) {
            matches = false;
        }
        if (query.end_time && entry.timestamp > *query.end_time) {
            matches = false;
        }

        // Text pattern filter
        if (query.text_pattern) {
            std::string msg = entry.message;
            std::string pattern = *query.text_pattern;

            if (!query.case_sensitive) {
                std::transform(msg.begin(), msg.end(), msg.begin(), ::tolower);
                std::transform(pattern.begin(), pattern.end(), pattern.begin(),
                               ::tolower);
            }

            if (msg.find(pattern) == std::string::npos) {
                matches = false;
            }
        }

        // Regex pattern filter
        if (query.regex_pattern && matches) {
            try {
                std::regex_constants::syntax_option_type flags =
                    std::regex_constants::ECMAScript;
                if (!query.case_sensitive) {
                    flags |= std::regex_constants::icase;
                }
                std::regex re(*query.regex_pattern, flags);
                if (!std::regex_search(entry.message, re)) {
                    matches = false;
                }
            } catch (const std::regex_error&) {
                // Invalid regex, skip this filter
            }
        }

        if (matches) {
            filtered.push_back(entry);
        }
    }

    result.total_matches = filtered.size();

    // Apply offset and limit
    size_t start_idx = std::min(query.offset, filtered.size());
    size_t end_idx = std::min(start_idx + query.limit, filtered.size());

    for (size_t i = start_idx; i < end_idx; ++i) {
        result.entries.push_back(filtered[i]);
    }

    result.returned_count = result.entries.size();
    result.has_more = end_idx < filtered.size();

    auto end_time = std::chrono::steady_clock::now();
    result.search_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

// ============================================================================
// Export Methods
// ============================================================================

auto LoggingManager::exportLogs(const ExportOptions& options,
                                size_t count) const -> ExportResult {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        ExportResult result;
        result.success = false;
        result.error_message = "Ring buffer not initialized";
        return result;
    }

    auto entries = ring_buffer_sink_->getEntries(count);
    return LogExporter::exportToString(entries, options);
}

auto LoggingManager::exportLogsToFile(const std::string& file_path,
                                      const ExportOptions& options,
                                      size_t count) const -> ExportResult {
    std::shared_lock lock(mutex_);

    if (!ring_buffer_sink_) {
        ExportResult result;
        result.success = false;
        result.error_message = "Ring buffer not initialized";
        return result;
    }

    auto entries = ring_buffer_sink_->getEntries(count);
    return LogExporter::exportToFile(entries, file_path, options);
}

// ============================================================================
// Level Conversion Helpers
// ============================================================================

auto LoggingManager::levelFromString(const std::string& level)
    -> spdlog::level::level_enum {
    return logging::levelFromString(level);
}

auto LoggingManager::levelToString(spdlog::level::level_enum level)
    -> std::string {
    return logging::levelToString(level);
}

}  // namespace lithium::logging
