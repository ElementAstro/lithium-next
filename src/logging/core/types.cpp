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

    // Parse ISO 8601 timestamp (e.g., "2024-11-28T12:34:56.789Z")
    if (j.contains("timestamp")) {
        std::string ts_str = j["timestamp"].get<std::string>();
        std::tm tm = {};
        int ms = 0;

        // Parse: YYYY-MM-DDTHH:MM:SS.mmmZ
        if (ts_str.length() >= 19) {
            std::istringstream ss(ts_str);
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

            // Parse milliseconds if present
            if (ts_str.length() > 20 && ts_str[19] == '.') {
                size_t dot_pos = 19;
                size_t end_pos = ts_str.find_first_of("Z+-", dot_pos + 1);
                if (end_pos == std::string::npos) {
                    end_pos = ts_str.length();
                }
                std::string ms_str =
                    ts_str.substr(dot_pos + 1, end_pos - dot_pos - 1);
                // Pad or truncate to 3 digits
                while (ms_str.length() < 3)
                    ms_str += '0';
                if (ms_str.length() > 3)
                    ms_str = ms_str.substr(0, 3);
                ms = std::stoi(ms_str);
            }

            // Convert to time_point (assuming UTC)
            auto time_t_val = std::mktime(&tm);
// Adjust for local timezone to UTC
#ifdef _WIN32
            time_t_val -= _timezone;
#else
            time_t_val -= timezone;
#endif
            entry.timestamp =
                std::chrono::system_clock::from_time_t(time_t_val) +
                std::chrono::milliseconds(ms);
        }
    } else {
        entry.timestamp = std::chrono::system_clock::now();
    }

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
        {"async_thread_count", async_thread_count},
        {"enable_console", enable_console},
        {"console_color", console_color},
        {"enable_file", enable_file},
        {"log_dir", log_dir},
        {"log_filename", log_filename},
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
    config.async_thread_count = j.value("async_thread_count", 1);
    config.enable_console = j.value("enable_console", true);
    config.console_color = j.value("console_color", true);
    config.enable_file = j.value("enable_file", true);
    config.log_dir = j.value("log_dir", "logs");
    config.log_filename = j.value("log_filename", "lithium");

    if (j.contains("sinks") && j["sinks"].is_array()) {
        for (const auto& sink_json : j["sinks"]) {
            config.sinks.push_back(SinkConfig::fromJson(sink_json));
        }
    }

    return config;
}

auto LoggingConfig::createDefault() -> LoggingConfig {
    LoggingConfig config;

    // Add console sink
    if (config.enable_console) {
        SinkConfig console_sink;
        console_sink.name = "console";
        console_sink.type = "console";
        console_sink.level = spdlog::level::info;
        config.sinks.push_back(console_sink);
    }

    // Add rotating file sink
    if (config.enable_file) {
        SinkConfig file_sink;
        file_sink.name = "file";
        file_sink.type = "rotating_file";
        file_sink.level = spdlog::level::trace;
        file_sink.file_path =
            config.log_dir + "/" + config.log_filename + ".log";
        file_sink.max_file_size = 10 * 1024 * 1024;  // 10 MB
        file_sink.max_files = 5;
        config.sinks.push_back(file_sink);
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

// ============================================================================
// LogSearchQuery Implementation
// ============================================================================

auto LogSearchQuery::fromJson(const nlohmann::json& j) -> LogSearchQuery {
    LogSearchQuery query;

    if (j.contains("text")) {
        query.text_pattern = j["text"].get<std::string>();
    }
    if (j.contains("regex")) {
        query.regex_pattern = j["regex"].get<std::string>();
    }
    if (j.contains("min_level")) {
        query.min_level = levelFromString(j["min_level"].get<std::string>());
    }
    if (j.contains("max_level")) {
        query.max_level = levelFromString(j["max_level"].get<std::string>());
    }
    if (j.contains("logger")) {
        query.logger_name = j["logger"].get<std::string>();
    }
    if (j.contains("limit")) {
        query.limit = j["limit"].get<size_t>();
    }
    if (j.contains("offset")) {
        query.offset = j["offset"].get<size_t>();
    }
    if (j.contains("case_sensitive")) {
        query.case_sensitive = j["case_sensitive"].get<bool>();
    }

    return query;
}

auto LogSearchQuery::toJson() const -> nlohmann::json {
    nlohmann::json j;

    if (text_pattern)
        j["text"] = *text_pattern;
    if (regex_pattern)
        j["regex"] = *regex_pattern;
    if (min_level)
        j["min_level"] = levelToString(*min_level);
    if (max_level)
        j["max_level"] = levelToString(*max_level);
    if (logger_name)
        j["logger"] = *logger_name;
    j["limit"] = limit;
    j["offset"] = offset;
    j["case_sensitive"] = case_sensitive;

    return j;
}

// ============================================================================
// LogSearchResult Implementation
// ============================================================================

auto LogSearchResult::toJson() const -> nlohmann::json {
    nlohmann::json entries_json = nlohmann::json::array();
    for (const auto& entry : entries) {
        entries_json.push_back(entry.toJson());
    }

    return {{"entries", entries_json},
            {"total_matches", total_matches},
            {"returned_count", returned_count},
            {"has_more", has_more},
            {"search_time_ms", search_time.count()}};
}

}  // namespace lithium::logging
