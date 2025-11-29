/*
 * sink_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "sink_factory.hpp"

#include <filesystem>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace lithium::logging {

auto SinkFactory::createSink(const SinkConfig& config) -> spdlog::sink_ptr {
    try {
        spdlog::sink_ptr sink;

        if (config.type == "console" || config.type == "stdout") {
            sink = createConsoleSink(config.level, config.pattern);
        } else if (config.type == "file" || config.type == "basic_file") {
            sink =
                createFileSink(config.file_path, config.level, config.pattern);
        } else if (config.type == "rotating_file") {
            sink = createRotatingFileSink(
                config.file_path, config.max_file_size, config.max_files,
                config.level, config.pattern);
        } else if (config.type == "daily_file") {
            sink = createDailyFileSink(config.file_path, config.rotation_hour,
                                       config.rotation_minute, config.level,
                                       config.pattern);
        } else {
            spdlog::warn("Unknown sink type: {}", config.type);
            return nullptr;
        }

        return sink;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create sink '{}': {}", config.name, e.what());
        return nullptr;
    }
}

auto SinkFactory::createConsoleSink(spdlog::level::level_enum level,
                                    const std::string& pattern)
    -> spdlog::sink_ptr {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink->set_level(level);
    if (!pattern.empty()) {
        sink->set_pattern(pattern);
    }
    return sink;
}

auto SinkFactory::createFileSink(const std::string& file_path,
                                 spdlog::level::level_enum level,
                                 const std::string& pattern, bool truncate)
    -> spdlog::sink_ptr {
    ensureDirectoryExists(file_path);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path,
                                                                    truncate);
    sink->set_level(level);
    if (!pattern.empty()) {
        sink->set_pattern(pattern);
    }
    return sink;
}

auto SinkFactory::createRotatingFileSink(const std::string& file_path,
                                         size_t max_size, size_t max_files,
                                         spdlog::level::level_enum level,
                                         const std::string& pattern)
    -> spdlog::sink_ptr {
    ensureDirectoryExists(file_path);
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        file_path, max_size, max_files);
    sink->set_level(level);
    if (!pattern.empty()) {
        sink->set_pattern(pattern);
    }
    return sink;
}

auto SinkFactory::createDailyFileSink(const std::string& file_path,
                                      int rotation_hour, int rotation_minute,
                                      spdlog::level::level_enum level,
                                      const std::string& pattern)
    -> spdlog::sink_ptr {
    ensureDirectoryExists(file_path);
    auto sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
        file_path, rotation_hour, rotation_minute);
    sink->set_level(level);
    if (!pattern.empty()) {
        sink->set_pattern(pattern);
    }
    return sink;
}

void SinkFactory::ensureDirectoryExists(const std::string& file_path) {
    std::filesystem::path path(file_path);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
}

}  // namespace lithium::logging
