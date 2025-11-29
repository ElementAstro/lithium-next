/*
 * log_statistics.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Log statistics and analysis utilities

**************************************************/

#ifndef LITHIUM_LOGGING_LOG_STATISTICS_HPP
#define LITHIUM_LOGGING_LOG_STATISTICS_HPP

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::logging {

/**
 * @brief Statistics for a single log level
 */
struct LevelStats {
    std::atomic<uint64_t> count{0};
    std::atomic<uint64_t> total_bytes{0};
    std::chrono::system_clock::time_point last_occurrence;

    void reset() {
        count = 0;
        total_bytes = 0;
        last_occurrence = std::chrono::system_clock::time_point{};
    }
};

/**
 * @brief Statistics for a single logger
 */
struct LoggerStats {
    std::string name;
    std::atomic<uint64_t> total_messages{0};
    std::unordered_map<spdlog::level::level_enum, uint64_t> level_counts;
    std::chrono::system_clock::time_point first_message;
    std::chrono::system_clock::time_point last_message;
    std::atomic<uint64_t> total_bytes{0};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Log statistics collector and analyzer
 *
 * Collects statistics about log messages:
 * - Message counts per level
 * - Message counts per logger
 * - Message rate over time
 * - Error/warning trends
 */
class LogStatistics {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> LogStatistics&;

    /**
     * @brief Record a log message for statistics
     * @param level Log level
     * @param logger_name Logger name
     * @param message_size Size of message in bytes
     */
    void recordMessage(spdlog::level::level_enum level,
                       const std::string& logger_name, size_t message_size);

    /**
     * @brief Get statistics for all levels
     * @return JSON with level statistics
     */
    [[nodiscard]] auto getLevelStats() const -> nlohmann::json;

    /**
     * @brief Get statistics for all loggers
     * @return JSON with logger statistics
     */
    [[nodiscard]] auto getLoggerStats() const -> nlohmann::json;

    /**
     * @brief Get overall statistics summary
     * @return JSON with summary statistics
     */
    [[nodiscard]] auto getSummary() const -> nlohmann::json;

    /**
     * @brief Get message rate (messages per second) over recent period
     * @param seconds Number of seconds to calculate rate over
     * @return Messages per second
     */
    [[nodiscard]] auto getMessageRate(int seconds = 60) const -> double;

    /**
     * @brief Get error rate (errors per minute) over recent period
     * @return Errors per minute
     */
    [[nodiscard]] auto getErrorRate() const -> double;

    /**
     * @brief Reset all statistics
     */
    void reset();

    /**
     * @brief Get uptime since statistics collection started
     * @return Duration since start
     */
    [[nodiscard]] auto getUptime() const -> std::chrono::seconds;

    /**
     * @brief Get total message count
     */
    [[nodiscard]] auto getTotalMessages() const -> uint64_t;

    /**
     * @brief Get total bytes logged
     */
    [[nodiscard]] auto getTotalBytes() const -> uint64_t;

private:
    LogStatistics();
    ~LogStatistics() = default;

    LogStatistics(const LogStatistics&) = delete;
    LogStatistics& operator=(const LogStatistics&) = delete;

    mutable std::mutex mutex_;
    std::chrono::system_clock::time_point start_time_;

    // Level statistics
    std::array<LevelStats, 7>
        level_stats_;  // trace, debug, info, warn, error, critical, off

    // Logger statistics
    std::unordered_map<std::string, LoggerStats> logger_stats_;

    // Rate tracking
    struct RateWindow {
        std::chrono::system_clock::time_point timestamp;
        uint64_t message_count{0};
        uint64_t error_count{0};
    };
    std::vector<RateWindow> rate_windows_;
    static constexpr size_t MAX_RATE_WINDOWS = 60;  // Keep 60 seconds of data

    // Totals
    std::atomic<uint64_t> total_messages_{0};
    std::atomic<uint64_t> total_bytes_{0};

    void updateRateWindow();
};

/**
 * @brief Log search query parameters
 */
struct LogSearchQuery {
    std::optional<std::string> text_pattern;   // Text to search for
    std::optional<std::string> regex_pattern;  // Regex pattern
    std::optional<spdlog::level::level_enum> min_level;
    std::optional<spdlog::level::level_enum> max_level;
    std::optional<std::string> logger_name;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    size_t limit{100};
    size_t offset{0};
    bool case_sensitive{false};

    [[nodiscard]] static auto fromJson(const nlohmann::json& j)
        -> LogSearchQuery;
    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Log search result
 */
struct LogSearchResult {
    std::vector<struct LogEntry> entries;
    size_t total_matches{0};
    size_t returned_count{0};
    bool has_more{false};
    std::chrono::milliseconds search_time{0};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_LOG_STATISTICS_HPP
