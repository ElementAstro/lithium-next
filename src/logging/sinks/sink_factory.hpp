/*
 * sink_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Factory for creating spdlog sinks from configuration

**************************************************/

#ifndef LITHIUM_LOGGING_SINKS_SINK_FACTORY_HPP
#define LITHIUM_LOGGING_SINKS_SINK_FACTORY_HPP

#include <memory>
#include <string>

#include <spdlog/spdlog.h>

#include "../types.hpp"

namespace lithium::logging {

/**
 * @brief Factory class for creating spdlog sinks
 *
 * Supports creating various sink types:
 * - Console (stdout with colors)
 * - Basic file
 * - Rotating file
 * - Daily file
 */
class SinkFactory {
public:
    /**
     * @brief Create a sink from configuration
     * @param config Sink configuration
     * @return Shared pointer to created sink, or nullptr on failure
     */
    [[nodiscard]] static auto createSink(const SinkConfig& config)
        -> spdlog::sink_ptr;

    /**
     * @brief Create a console sink
     * @param level Log level for the sink
     * @param pattern Optional format pattern
     * @return Console sink
     */
    [[nodiscard]] static auto createConsoleSink(
        spdlog::level::level_enum level = spdlog::level::trace,
        const std::string& pattern = "") -> spdlog::sink_ptr;

    /**
     * @brief Create a basic file sink
     * @param file_path Path to log file
     * @param level Log level
     * @param pattern Optional format pattern
     * @param truncate Whether to truncate existing file
     * @return File sink
     */
    [[nodiscard]] static auto createFileSink(
        const std::string& file_path,
        spdlog::level::level_enum level = spdlog::level::trace,
        const std::string& pattern = "", bool truncate = true)
        -> spdlog::sink_ptr;

    /**
     * @brief Create a rotating file sink
     * @param file_path Base path for log files
     * @param max_size Maximum file size before rotation
     * @param max_files Maximum number of rotated files to keep
     * @param level Log level
     * @param pattern Optional format pattern
     * @return Rotating file sink
     */
    [[nodiscard]] static auto createRotatingFileSink(
        const std::string& file_path, size_t max_size, size_t max_files,
        spdlog::level::level_enum level = spdlog::level::trace,
        const std::string& pattern = "") -> spdlog::sink_ptr;

    /**
     * @brief Create a daily file sink
     * @param file_path Base path for log files
     * @param rotation_hour Hour of day to rotate (0-23)
     * @param rotation_minute Minute of hour to rotate (0-59)
     * @param level Log level
     * @param pattern Optional format pattern
     * @return Daily file sink
     */
    [[nodiscard]] static auto createDailyFileSink(
        const std::string& file_path, int rotation_hour, int rotation_minute,
        spdlog::level::level_enum level = spdlog::level::trace,
        const std::string& pattern = "") -> spdlog::sink_ptr;

private:
    /**
     * @brief Ensure parent directory exists for file path
     */
    static void ensureDirectoryExists(const std::string& file_path);
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_SINKS_SINK_FACTORY_HPP
