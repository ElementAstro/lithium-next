/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Logging system type definitions

**************************************************/

#ifndef LITHIUM_LOGGING_TYPES_HPP
#define LITHIUM_LOGGING_TYPES_HPP

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::logging {

// Forward declarations
class LoggingManager;

/**
 * @brief Log entry structure for buffered/streamed logs
 */
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    spdlog::level::level_enum level{spdlog::level::info};
    std::string logger_name;
    std::string message;
    std::string thread_id;
    std::string source_file;
    int source_line{0};

    /**
     * @brief Convert log entry to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create log entry from JSON
     */
    [[nodiscard]] static auto fromJson(const nlohmann::json& j) -> LogEntry;
};

/**
 * @brief Logger information structure
 */
struct LoggerInfo {
    std::string name;
    spdlog::level::level_enum level{spdlog::level::info};
    std::string pattern;
    std::vector<std::string> sink_names;

    /**
     * @brief Convert logger info to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Sink configuration structure
 */
struct SinkConfig {
    std::string name;
    std::string
        type;  // "console", "file", "rotating_file", "daily_file", "ringbuffer"
    spdlog::level::level_enum level{spdlog::level::trace};
    std::string pattern;

    // File sink options
    std::string file_path;
    size_t max_file_size{10 * 1024 * 1024};  // 10MB default
    size_t max_files{5};

    // Daily file options
    int rotation_hour{0};
    int rotation_minute{0};

    /**
     * @brief Convert sink config to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create sink config from JSON
     */
    [[nodiscard]] static auto fromJson(const nlohmann::json& j) -> SinkConfig;
};

/**
 * @brief Logging manager configuration
 *
 * This configuration is used by LoggingManager to initialize the logging
 * system. It can be created from JSON or converted from
 * lithium::config::LoggingConfig.
 */
struct LoggingConfig {
    spdlog::level::level_enum default_level{spdlog::level::info};
    std::string default_pattern{"[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v"};
    size_t ring_buffer_size{1000};
    bool async_logging{false};
    size_t async_queue_size{8192};
    size_t async_thread_count{1};
    std::vector<SinkConfig> sinks;

    // Console settings
    bool enable_console{true};
    bool console_color{true};

    // File settings
    bool enable_file{true};
    std::string log_dir{"logs"};
    std::string log_filename{"lithium"};

    /**
     * @brief Convert config to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create config from JSON
     */
    [[nodiscard]] static auto fromJson(const nlohmann::json& j)
        -> LoggingConfig;

    /**
     * @brief Create default configuration with console and file sinks
     */
    [[nodiscard]] static auto createDefault() -> LoggingConfig;
};

/**
 * @brief Convert level string to spdlog enum
 */
[[nodiscard]] auto levelFromString(const std::string& level)
    -> spdlog::level::level_enum;

/**
 * @brief Convert spdlog level enum to string
 */
[[nodiscard]] auto levelToString(spdlog::level::level_enum level)
    -> std::string;

/**
 * @brief Log search query parameters
 */
struct LogSearchQuery {
    std::optional<std::string> text_pattern;   ///< Text to search for
    std::optional<std::string> regex_pattern;  ///< Regex pattern
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
    std::vector<LogEntry> entries;
    size_t total_matches{0};
    size_t returned_count{0};
    bool has_more{false};
    std::chrono::milliseconds search_time{0};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_TYPES_HPP
