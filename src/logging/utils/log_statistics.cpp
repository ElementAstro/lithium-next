/*
 * log_statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "log_statistics.hpp"
#include "../core/types.hpp"

#include <algorithm>
#include <iomanip>
#include <regex>
#include <sstream>

namespace lithium::logging {

// ============================================================================
// LoggerStats Implementation
// ============================================================================

auto LoggerStats::toJson() const -> nlohmann::json {
    nlohmann::json level_counts_json;
    for (const auto& [level, count] : level_counts) {
        level_counts_json[levelToString(level)] = count;
    }

    auto format_time =
        [](const std::chrono::system_clock::time_point& tp) -> std::string {
        if (tp == std::chrono::system_clock::time_point{}) {
            return "";
        }
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    };

    return {{"name", name},
            {"total_messages", total_messages.load()},
            {"level_counts", level_counts_json},
            {"first_message", format_time(first_message)},
            {"last_message", format_time(last_message)},
            {"total_bytes", total_bytes.load()}};
}

// ============================================================================
// LogStatistics Implementation
// ============================================================================

LogStatistics::LogStatistics() : start_time_(std::chrono::system_clock::now()) {
    rate_windows_.reserve(MAX_RATE_WINDOWS);
}

auto LogStatistics::getInstance() -> LogStatistics& {
    static LogStatistics instance;
    return instance;
}

void LogStatistics::recordMessage(spdlog::level::level_enum level,
                                  const std::string& logger_name,
                                  size_t message_size) {
    auto now = std::chrono::system_clock::now();

    // Update level stats
    auto level_idx = static_cast<size_t>(level);
    if (level_idx < level_stats_.size()) {
        level_stats_[level_idx].count++;
        level_stats_[level_idx].total_bytes += message_size;
        level_stats_[level_idx].last_occurrence = now;
    }

    // Update logger stats
    {
        std::unique_lock lock(mutex_);
        auto& stats = logger_stats_[logger_name];
        if (stats.name.empty()) {
            stats.name = logger_name;
            stats.first_message = now;
        }
        stats.total_messages++;
        stats.level_counts[level]++;
        stats.last_message = now;
        stats.total_bytes += message_size;
    }

    // Update totals
    total_messages_++;
    total_bytes_ += message_size;

    // Update rate tracking
    updateRateWindow();
}

void LogStatistics::updateRateWindow() {
    auto now = std::chrono::system_clock::now();
    auto now_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

    std::unique_lock lock(mutex_);

    // Check if we need a new window
    if (rate_windows_.empty() ||
        std::chrono::time_point_cast<std::chrono::seconds>(
            rate_windows_.back().timestamp) != now_seconds) {
        // Remove old windows
        auto cutoff = now - std::chrono::seconds(MAX_RATE_WINDOWS);
        rate_windows_.erase(
            std::remove_if(
                rate_windows_.begin(), rate_windows_.end(),
                [cutoff](const RateWindow& w) { return w.timestamp < cutoff; }),
            rate_windows_.end());

        // Add new window
        rate_windows_.push_back({now, 1, 0});
    } else {
        rate_windows_.back().message_count++;
    }
}

auto LogStatistics::getLevelStats() const -> nlohmann::json {
    nlohmann::json result;

    const std::array<std::string, 7> level_names = {
        "trace", "debug", "info", "warn", "error", "critical", "off"};

    auto format_time =
        [](const std::chrono::system_clock::time_point& tp) -> std::string {
        if (tp == std::chrono::system_clock::time_point{}) {
            return "";
        }
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    };

    for (size_t i = 0; i < level_stats_.size(); ++i) {
        result[level_names[i]] = {
            {"count", level_stats_[i].count.load()},
            {"total_bytes", level_stats_[i].total_bytes.load()},
            {"last_occurrence", format_time(level_stats_[i].last_occurrence)}};
    }

    return result;
}

auto LogStatistics::getLoggerStats() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json result = nlohmann::json::array();
    for (const auto& [name, stats] : logger_stats_) {
        result.push_back(stats.toJson());
    }

    return result;
}

auto LogStatistics::getSummary() const -> nlohmann::json {
    auto uptime = getUptime();
    auto msg_rate = getMessageRate();
    auto err_rate = getErrorRate();

    uint64_t error_count =
        level_stats_[static_cast<size_t>(spdlog::level::err)].count.load();
    uint64_t warn_count =
        level_stats_[static_cast<size_t>(spdlog::level::warn)].count.load();
    uint64_t critical_count =
        level_stats_[static_cast<size_t>(spdlog::level::critical)].count.load();

    size_t logger_count;
    {
        std::shared_lock lock(mutex_);
        logger_count = logger_stats_.size();
    }

    return {{"uptime_seconds", uptime.count()},
            {"total_messages", total_messages_.load()},
            {"total_bytes", total_bytes_.load()},
            {"message_rate_per_second", msg_rate},
            {"error_rate_per_minute", err_rate},
            {"error_count", error_count},
            {"warning_count", warn_count},
            {"critical_count", critical_count},
            {"logger_count", logger_count}};
}

auto LogStatistics::getMessageRate(int seconds) const -> double {
    std::shared_lock lock(mutex_);

    if (rate_windows_.empty()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::seconds(seconds);

    uint64_t count = 0;
    int window_count = 0;

    for (const auto& window : rate_windows_) {
        if (window.timestamp >= cutoff) {
            count += window.message_count;
            window_count++;
        }
    }

    if (window_count == 0) {
        return 0.0;
    }

    return static_cast<double>(count) / window_count;
}

auto LogStatistics::getErrorRate() const -> double {
    auto uptime = getUptime();
    if (uptime.count() == 0) {
        return 0.0;
    }

    uint64_t error_count =
        level_stats_[static_cast<size_t>(spdlog::level::err)].count.load();
    uint64_t critical_count =
        level_stats_[static_cast<size_t>(spdlog::level::critical)].count.load();

    double minutes = static_cast<double>(uptime.count()) / 60.0;
    return static_cast<double>(error_count + critical_count) / minutes;
}

void LogStatistics::reset() {
    std::unique_lock lock(mutex_);

    for (auto& stats : level_stats_) {
        stats.reset();
    }

    logger_stats_.clear();
    rate_windows_.clear();

    total_messages_ = 0;
    total_bytes_ = 0;
    start_time_ = std::chrono::system_clock::now();
}

auto LogStatistics::getUptime() const -> std::chrono::seconds {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
}

auto LogStatistics::getTotalMessages() const -> uint64_t {
    return total_messages_.load();
}

auto LogStatistics::getTotalBytes() const -> uint64_t {
    return total_bytes_.load();
}

}  // namespace lithium::logging
