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
 */
struct LoggingConfig {
    spdlog::level::level_enum default_level{spdlog::level::info};
    std::string default_pattern{"[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v"};
    size_t ring_buffer_size{1000};
    bool async_logging{false};
    size_t async_queue_size{8192};
    std::vector<SinkConfig> sinks;

    /**
     * @brief Convert config to JSON
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Create config from JSON
     */
    [[nodiscard]] static auto fromJson(const nlohmann::json& j)
        -> LoggingConfig;
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

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_TYPES_HPP
