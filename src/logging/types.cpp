/*
 * types.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "types.hpp"

#include <iomanip>
#include <sstream>

namespace lithium::logging {

// ============================================================================
// LogEntry Implementation
// ============================================================================

auto LogEntry::toJson() const -> nlohmann::json {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamp.time_since_epoch()) %
              1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    return {{"timestamp", oss.str()},
            {"level", spdlog::level::to_string_view(level).data()},
            {"logger", logger_name},
            {"message", message},
            {"thread_id", thread_id},
            {"source_file", source_file},
            {"source_line", source_line}};
}

auto LogEntry::fromJson(const nlohmann::json& j) -> LogEntry {
    LogEntry entry;
    entry.logger_name = j.value("logger", "");
    entry.message = j.value("message", "");
    entry.thread_id = j.value("thread_id", "");
    entry.source_file = j.value("source_file", "");
    entry.source_line = j.value("source_line", 0);

    std::string level_str = j.value("level", "info");
    entry.level = levelFromString(level_str);

    return entry;
}

// ============================================================================
// LoggerInfo Implementation
// ============================================================================

auto LoggerInfo::toJson() const -> nlohmann::json {
    return {{"name", name},
            {"level", spdlog::level::to_string_view(level).data()},
            {"pattern", pattern},
            {"sinks", sink_names}};
}

// ============================================================================
// SinkConfig Implementation
// ============================================================================

auto SinkConfig::toJson() const -> nlohmann::json {
    nlohmann::json j = {{"name", name},
                        {"type", type},
                        {"level", spdlog::level::to_string_view(level).data()},
                        {"pattern", pattern}};

    if (type == "file" || type == "rotating_file" || type == "daily_file") {
        j["file_path"] = file_path;
    }
    if (type == "rotating_file") {
        j["max_file_size"] = max_file_size;
        j["max_files"] = max_files;
    }
    if (type == "daily_file") {
        j["rotation_hour"] = rotation_hour;
        j["rotation_minute"] = rotation_minute;
    }

    return j;
}

auto SinkConfig::fromJson(const nlohmann::json& j) -> SinkConfig {
    SinkConfig config;
    config.name = j.value("name", "");
    config.type = j.value("type", "console");
    config.level = levelFromString(j.value("level", "trace"));
    config.pattern = j.value("pattern", "");
    config.file_path = j.value("file_path", "");
    config.max_file_size = j.value("max_file_size", 10 * 1024 * 1024);
    config.max_files = j.value("max_files", 5);
    config.rotation_hour = j.value("rotation_hour", 0);
    config.rotation_minute = j.value("rotation_minute", 0);
    return config;
}

// ============================================================================
// LoggingConfig Implementation
// ============================================================================

auto LoggingConfig::toJson() const -> nlohmann::json {
    nlohmann::json sinks_json = nlohmann::json::array();
    for (const auto& sink : sinks) {
        sinks_json.push_back(sink.toJson());
    }

    return {
        {"default_level", spdlog::level::to_string_view(default_level).data()},
        {"default_pattern", default_pattern},
        {"ring_buffer_size", ring_buffer_size},
        {"async_logging", async_logging},
        {"async_queue_size", async_queue_size},
        {"sinks", sinks_json}};
}

auto LoggingConfig::fromJson(const nlohmann::json& j) -> LoggingConfig {
    LoggingConfig config;
    config.default_level = levelFromString(j.value("default_level", "info"));
    config.default_pattern = j.value(
        "default_pattern", "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v");
    config.ring_buffer_size = j.value("ring_buffer_size", 1000);
    config.async_logging = j.value("async_logging", false);
    config.async_queue_size = j.value("async_queue_size", 8192);

    if (j.contains("sinks") && j["sinks"].is_array()) {
        for (const auto& sink_json : j["sinks"]) {
            config.sinks.push_back(SinkConfig::fromJson(sink_json));
        }
    }

    return config;
}

// ============================================================================
// Level Conversion Functions
// ============================================================================

auto levelFromString(const std::string& level) -> spdlog::level::level_enum {
    if (level == "trace")
        return spdlog::level::trace;
    if (level == "debug")
        return spdlog::level::debug;
    if (level == "info")
        return spdlog::level::info;
    if (level == "warn" || level == "warning")
        return spdlog::level::warn;
    if (level == "error" || level == "err")
        return spdlog::level::err;
    if (level == "critical" || level == "fatal")
        return spdlog::level::critical;
    if (level == "off")
        return spdlog::level::off;
    return spdlog::level::info;  // Default
}

auto levelToString(spdlog::level::level_enum level) -> std::string {
    auto sv = spdlog::level::to_string_view(level);
    return std::string(sv.data(), sv.size());
}

}  // namespace lithium::logging
