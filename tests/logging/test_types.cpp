/*
 * test_types.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for logging types

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/types.hpp"

#include <chrono>
#include <string>

using namespace lithium::logging;
using namespace std::chrono_literals;

// ============================================================================
// LogEntry Tests
// ============================================================================

class LogEntryTest : public ::testing::Test {
protected:
    LogEntry createTestEntry() {
        LogEntry entry;
        entry.timestamp = std::chrono::system_clock::now();
        entry.level = spdlog::level::info;
        entry.logger_name = "test_logger";
        entry.message = "Test message";
        entry.thread_id = "12345";
        entry.source_file = "test.cpp";
        entry.source_line = 42;
        return entry;
    }
};

TEST_F(LogEntryTest, DefaultConstruction) {
    LogEntry entry;
    EXPECT_EQ(entry.level, spdlog::level::info);
    EXPECT_TRUE(entry.logger_name.empty());
    EXPECT_TRUE(entry.message.empty());
    EXPECT_TRUE(entry.thread_id.empty());
    EXPECT_TRUE(entry.source_file.empty());
    EXPECT_EQ(entry.source_line, 0);
}

TEST_F(LogEntryTest, ToJsonContainsAllFields) {
    auto entry = createTestEntry();
    auto json = entry.toJson();

    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_TRUE(json.contains("level"));
    EXPECT_TRUE(json.contains("logger"));
    EXPECT_TRUE(json.contains("message"));
    EXPECT_TRUE(json.contains("thread_id"));
    EXPECT_TRUE(json.contains("source_file"));
    EXPECT_TRUE(json.contains("source_line"));
}

TEST_F(LogEntryTest, ToJsonCorrectValues) {
    auto entry = createTestEntry();
    auto json = entry.toJson();

    EXPECT_EQ(json["level"], "info");
    EXPECT_EQ(json["logger"], "test_logger");
    EXPECT_EQ(json["message"], "Test message");
    EXPECT_EQ(json["thread_id"], "12345");
    EXPECT_EQ(json["source_file"], "test.cpp");
    EXPECT_EQ(json["source_line"], 42);
}

TEST_F(LogEntryTest, ToJsonTimestampFormat) {
    auto entry = createTestEntry();
    auto json = entry.toJson();

    std::string timestamp = json["timestamp"].get<std::string>();
    // Should be ISO 8601 format with milliseconds
    EXPECT_TRUE(timestamp.find('T') != std::string::npos);
    EXPECT_TRUE(timestamp.find('Z') != std::string::npos);
}

TEST_F(LogEntryTest, ToJsonAllLevels) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();

    entry.level = spdlog::level::trace;
    EXPECT_EQ(entry.toJson()["level"], "trace");

    entry.level = spdlog::level::debug;
    EXPECT_EQ(entry.toJson()["level"], "debug");

    entry.level = spdlog::level::info;
    EXPECT_EQ(entry.toJson()["level"], "info");

    entry.level = spdlog::level::warn;
    EXPECT_EQ(entry.toJson()["level"], "warning");

    entry.level = spdlog::level::err;
    EXPECT_EQ(entry.toJson()["level"], "error");

    entry.level = spdlog::level::critical;
    EXPECT_EQ(entry.toJson()["level"], "critical");

    entry.level = spdlog::level::off;
    EXPECT_EQ(entry.toJson()["level"], "off");
}

TEST_F(LogEntryTest, FromJsonBasic) {
    nlohmann::json json = {{"logger", "my_logger"},
                           {"message", "Hello world"},
                           {"level", "warn"},
                           {"thread_id", "99999"},
                           {"source_file", "main.cpp"},
                           {"source_line", 100}};

    auto entry = LogEntry::fromJson(json);

    EXPECT_EQ(entry.logger_name, "my_logger");
    EXPECT_EQ(entry.message, "Hello world");
    EXPECT_EQ(entry.level, spdlog::level::warn);
    EXPECT_EQ(entry.thread_id, "99999");
    EXPECT_EQ(entry.source_file, "main.cpp");
    EXPECT_EQ(entry.source_line, 100);
}

TEST_F(LogEntryTest, FromJsonMissingFields) {
    nlohmann::json json = {{"message", "Only message"}};

    auto entry = LogEntry::fromJson(json);

    EXPECT_TRUE(entry.logger_name.empty());
    EXPECT_EQ(entry.message, "Only message");
    EXPECT_EQ(entry.level, spdlog::level::info);  // Default
    EXPECT_TRUE(entry.thread_id.empty());
    EXPECT_TRUE(entry.source_file.empty());
    EXPECT_EQ(entry.source_line, 0);
}

TEST_F(LogEntryTest, FromJsonEmptyObject) {
    nlohmann::json json = nlohmann::json::object();

    auto entry = LogEntry::fromJson(json);

    EXPECT_TRUE(entry.logger_name.empty());
    EXPECT_TRUE(entry.message.empty());
    EXPECT_EQ(entry.level, spdlog::level::info);
}

TEST_F(LogEntryTest, RoundTripConversion) {
    auto original = createTestEntry();
    auto json = original.toJson();
    auto restored = LogEntry::fromJson(json);

    EXPECT_EQ(restored.logger_name, original.logger_name);
    EXPECT_EQ(restored.message, original.message);
    EXPECT_EQ(restored.level, original.level);
    EXPECT_EQ(restored.thread_id, original.thread_id);
    EXPECT_EQ(restored.source_file, original.source_file);
    EXPECT_EQ(restored.source_line, original.source_line);
}

TEST_F(LogEntryTest, SpecialCharactersInMessage) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.message = "Special chars: \t\n\r\"'\\{}[]<>&";

    auto json = entry.toJson();
    EXPECT_EQ(json["message"], entry.message);
}

TEST_F(LogEntryTest, UnicodeInMessage) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.message = "Unicode: 你好世界 🌍 αβγδ";

    auto json = entry.toJson();
    EXPECT_EQ(json["message"], entry.message);
}

// ============================================================================
// LoggerInfo Tests
// ============================================================================

class LoggerInfoTest : public ::testing::Test {
protected:
    LoggerInfo createTestInfo() {
        LoggerInfo info;
        info.name = "test_logger";
        info.level = spdlog::level::debug;
        info.pattern = "[%l] %v";
        info.sink_names = {"console", "file"};
        return info;
    }
};

TEST_F(LoggerInfoTest, DefaultConstruction) {
    LoggerInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_EQ(info.level, spdlog::level::info);
    EXPECT_TRUE(info.pattern.empty());
    EXPECT_TRUE(info.sink_names.empty());
}

TEST_F(LoggerInfoTest, ToJsonContainsAllFields) {
    auto info = createTestInfo();
    auto json = info.toJson();

    EXPECT_TRUE(json.contains("name"));
    EXPECT_TRUE(json.contains("level"));
    EXPECT_TRUE(json.contains("pattern"));
    EXPECT_TRUE(json.contains("sinks"));
}

TEST_F(LoggerInfoTest, ToJsonCorrectValues) {
    auto info = createTestInfo();
    auto json = info.toJson();

    EXPECT_EQ(json["name"], "test_logger");
    EXPECT_EQ(json["level"], "debug");
    EXPECT_EQ(json["pattern"], "[%l] %v");
    EXPECT_EQ(json["sinks"].size(), 2);
    EXPECT_EQ(json["sinks"][0], "console");
    EXPECT_EQ(json["sinks"][1], "file");
}

TEST_F(LoggerInfoTest, ToJsonEmptySinks) {
    LoggerInfo info;
    info.name = "empty_sinks";
    info.level = spdlog::level::info;

    auto json = info.toJson();
    EXPECT_TRUE(json["sinks"].empty());
}

TEST_F(LoggerInfoTest, ToJsonAllLevels) {
    LoggerInfo info;
    info.name = "level_test";

    info.level = spdlog::level::trace;
    EXPECT_EQ(info.toJson()["level"], "trace");

    info.level = spdlog::level::debug;
    EXPECT_EQ(info.toJson()["level"], "debug");

    info.level = spdlog::level::info;
    EXPECT_EQ(info.toJson()["level"], "info");

    info.level = spdlog::level::warn;
    EXPECT_EQ(info.toJson()["level"], "warning");

    info.level = spdlog::level::err;
    EXPECT_EQ(info.toJson()["level"], "error");

    info.level = spdlog::level::critical;
    EXPECT_EQ(info.toJson()["level"], "critical");
}

// ============================================================================
// SinkConfig Tests
// ============================================================================

class SinkConfigTest : public ::testing::Test {
protected:
    SinkConfig createConsoleSink() {
        SinkConfig config;
        config.name = "console";
        config.type = "console";
        config.level = spdlog::level::info;
        config.pattern = "[%l] %v";
        return config;
    }

    SinkConfig createFileSink() {
        SinkConfig config;
        config.name = "file";
        config.type = "file";
        config.level = spdlog::level::debug;
        config.file_path = "/var/log/test.log";
        return config;
    }

    SinkConfig createRotatingFileSink() {
        SinkConfig config;
        config.name = "rotating";
        config.type = "rotating_file";
        config.level = spdlog::level::info;
        config.file_path = "/var/log/rotating.log";
        config.max_file_size = 1024 * 1024;  // 1MB
        config.max_files = 3;
        return config;
    }

    SinkConfig createDailyFileSink() {
        SinkConfig config;
        config.name = "daily";
        config.type = "daily_file";
        config.level = spdlog::level::warn;
        config.file_path = "/var/log/daily.log";
        config.rotation_hour = 0;
        config.rotation_minute = 0;
        return config;
    }
};

TEST_F(SinkConfigTest, DefaultConstruction) {
    SinkConfig config;
    EXPECT_TRUE(config.name.empty());
    EXPECT_TRUE(config.type.empty());
    EXPECT_EQ(config.level, spdlog::level::trace);
    EXPECT_TRUE(config.pattern.empty());
    EXPECT_TRUE(config.file_path.empty());
    EXPECT_EQ(config.max_file_size, 10 * 1024 * 1024);  // 10MB default
    EXPECT_EQ(config.max_files, 5);
    EXPECT_EQ(config.rotation_hour, 0);
    EXPECT_EQ(config.rotation_minute, 0);
}

TEST_F(SinkConfigTest, ConsoleSinkToJson) {
    auto config = createConsoleSink();
    auto json = config.toJson();

    EXPECT_EQ(json["name"], "console");
    EXPECT_EQ(json["type"], "console");
    EXPECT_EQ(json["level"], "info");
    EXPECT_EQ(json["pattern"], "[%l] %v");
    // Console sink should not have file_path in JSON
    EXPECT_FALSE(json.contains("file_path") && !json["file_path"].empty());
}

TEST_F(SinkConfigTest, FileSinkToJson) {
    auto config = createFileSink();
    auto json = config.toJson();

    EXPECT_EQ(json["name"], "file");
    EXPECT_EQ(json["type"], "file");
    EXPECT_EQ(json["file_path"], "/var/log/test.log");
}

TEST_F(SinkConfigTest, RotatingFileSinkToJson) {
    auto config = createRotatingFileSink();
    auto json = config.toJson();

    EXPECT_EQ(json["name"], "rotating");
    EXPECT_EQ(json["type"], "rotating_file");
    EXPECT_EQ(json["file_path"], "/var/log/rotating.log");
    EXPECT_EQ(json["max_file_size"], 1024 * 1024);
    EXPECT_EQ(json["max_files"], 3);
}

TEST_F(SinkConfigTest, DailyFileSinkToJson) {
    auto config = createDailyFileSink();
    auto json = config.toJson();

    EXPECT_EQ(json["name"], "daily");
    EXPECT_EQ(json["type"], "daily_file");
    EXPECT_EQ(json["file_path"], "/var/log/daily.log");
    EXPECT_EQ(json["rotation_hour"], 0);
    EXPECT_EQ(json["rotation_minute"], 0);
}

TEST_F(SinkConfigTest, FromJsonConsole) {
    nlohmann::json json = {
        {"name", "console"}, {"type", "console"}, {"level", "info"}};

    auto config = SinkConfig::fromJson(json);

    EXPECT_EQ(config.name, "console");
    EXPECT_EQ(config.type, "console");
    EXPECT_EQ(config.level, spdlog::level::info);
}

TEST_F(SinkConfigTest, FromJsonRotatingFile) {
    nlohmann::json json = {{"name", "rotating"},
                           {"type", "rotating_file"},
                           {"level", "debug"},
                           {"file_path", "/var/log/app.log"},
                           {"max_file_size", 5242880},
                           {"max_files", 10}};

    auto config = SinkConfig::fromJson(json);

    EXPECT_EQ(config.name, "rotating");
    EXPECT_EQ(config.type, "rotating_file");
    EXPECT_EQ(config.level, spdlog::level::debug);
    EXPECT_EQ(config.file_path, "/var/log/app.log");
    EXPECT_EQ(config.max_file_size, 5242880);
    EXPECT_EQ(config.max_files, 10);
}

TEST_F(SinkConfigTest, FromJsonDailyFile) {
    nlohmann::json json = {{"name", "daily"},
                           {"type", "daily_file"},
                           {"level", "warn"},
                           {"file_path", "/var/log/daily.log"},
                           {"rotation_hour", 2},
                           {"rotation_minute", 30}};

    auto config = SinkConfig::fromJson(json);

    EXPECT_EQ(config.name, "daily");
    EXPECT_EQ(config.type, "daily_file");
    EXPECT_EQ(config.level, spdlog::level::warn);
    EXPECT_EQ(config.rotation_hour, 2);
    EXPECT_EQ(config.rotation_minute, 30);
}

TEST_F(SinkConfigTest, FromJsonMissingFields) {
    nlohmann::json json = {{"name", "minimal"}};

    auto config = SinkConfig::fromJson(json);

    EXPECT_EQ(config.name, "minimal");
    EXPECT_EQ(config.type, "console");  // Default
    EXPECT_EQ(config.level, spdlog::level::trace);
    EXPECT_EQ(config.max_file_size, 10 * 1024 * 1024);  // Default
    EXPECT_EQ(config.max_files, 5);                     // Default
}

TEST_F(SinkConfigTest, FromJsonEmptyObject) {
    nlohmann::json json = nlohmann::json::object();

    auto config = SinkConfig::fromJson(json);

    EXPECT_TRUE(config.name.empty());
    EXPECT_EQ(config.type, "console");
}

TEST_F(SinkConfigTest, RoundTripConversion) {
    auto original = createRotatingFileSink();
    auto json = original.toJson();
    auto restored = SinkConfig::fromJson(json);

    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.type, original.type);
    EXPECT_EQ(restored.level, original.level);
    EXPECT_EQ(restored.file_path, original.file_path);
    EXPECT_EQ(restored.max_file_size, original.max_file_size);
    EXPECT_EQ(restored.max_files, original.max_files);
}

// ============================================================================
// LoggingConfig Tests
// ============================================================================

class LoggingConfigTest : public ::testing::Test {
protected:
    LoggingConfig createTestConfig() {
        LoggingConfig config;
        config.default_level = spdlog::level::debug;
        config.default_pattern = "[%Y-%m-%d %H:%M:%S] [%l] %v";
        config.ring_buffer_size = 500;
        config.async_logging = true;
        config.async_queue_size = 4096;

        SinkConfig console;
        console.name = "console";
        console.type = "console";
        console.level = spdlog::level::info;
        config.sinks.push_back(console);

        SinkConfig file;
        file.name = "file";
        file.type = "rotating_file";
        file.level = spdlog::level::debug;
        file.file_path = "/var/log/app.log";
        file.max_file_size = 1024 * 1024;
        file.max_files = 3;
        config.sinks.push_back(file);

        return config;
    }
};

TEST_F(LoggingConfigTest, DefaultConstruction) {
    LoggingConfig config;
    EXPECT_EQ(config.default_level, spdlog::level::info);
    EXPECT_FALSE(config.default_pattern.empty());
    EXPECT_EQ(config.ring_buffer_size, 1000);
    EXPECT_FALSE(config.async_logging);
    EXPECT_EQ(config.async_queue_size, 8192);
    EXPECT_TRUE(config.sinks.empty());
}

TEST_F(LoggingConfigTest, ToJsonContainsAllFields) {
    auto config = createTestConfig();
    auto json = config.toJson();

    EXPECT_TRUE(json.contains("default_level"));
    EXPECT_TRUE(json.contains("default_pattern"));
    EXPECT_TRUE(json.contains("ring_buffer_size"));
    EXPECT_TRUE(json.contains("async_logging"));
    EXPECT_TRUE(json.contains("async_queue_size"));
    EXPECT_TRUE(json.contains("sinks"));
}

TEST_F(LoggingConfigTest, ToJsonCorrectValues) {
    auto config = createTestConfig();
    auto json = config.toJson();

    EXPECT_EQ(json["default_level"], "debug");
    EXPECT_EQ(json["default_pattern"], "[%Y-%m-%d %H:%M:%S] [%l] %v");
    EXPECT_EQ(json["ring_buffer_size"], 500);
    EXPECT_EQ(json["async_logging"], true);
    EXPECT_EQ(json["async_queue_size"], 4096);
    EXPECT_EQ(json["sinks"].size(), 2);
}

TEST_F(LoggingConfigTest, ToJsonSinksContent) {
    auto config = createTestConfig();
    auto json = config.toJson();

    EXPECT_EQ(json["sinks"][0]["name"], "console");
    EXPECT_EQ(json["sinks"][0]["type"], "console");
    EXPECT_EQ(json["sinks"][1]["name"], "file");
    EXPECT_EQ(json["sinks"][1]["type"], "rotating_file");
}

TEST_F(LoggingConfigTest, FromJsonBasic) {
    nlohmann::json json = {{"default_level", "error"},
                           {"default_pattern", "[%n] %v"},
                           {"ring_buffer_size", 200},
                           {"async_logging", false},
                           {"async_queue_size", 2048}};

    auto config = LoggingConfig::fromJson(json);

    EXPECT_EQ(config.default_level, spdlog::level::err);
    EXPECT_EQ(config.default_pattern, "[%n] %v");
    EXPECT_EQ(config.ring_buffer_size, 200);
    EXPECT_FALSE(config.async_logging);
    EXPECT_EQ(config.async_queue_size, 2048);
}

TEST_F(LoggingConfigTest, FromJsonWithSinks) {
    nlohmann::json json = {
        {"default_level", "info"},
        {"sinks",
         nlohmann::json::array(
             {{{"name", "console"}, {"type", "console"}, {"level", "info"}},
              {{"name", "file"},
               {"type", "file"},
               {"level", "debug"},
               {"file_path", "/tmp/test.log"}}})}};

    auto config = LoggingConfig::fromJson(json);

    EXPECT_EQ(config.sinks.size(), 2);
    EXPECT_EQ(config.sinks[0].name, "console");
    EXPECT_EQ(config.sinks[1].name, "file");
    EXPECT_EQ(config.sinks[1].file_path, "/tmp/test.log");
}

TEST_F(LoggingConfigTest, FromJsonMissingFields) {
    nlohmann::json json = nlohmann::json::object();

    auto config = LoggingConfig::fromJson(json);

    EXPECT_EQ(config.default_level, spdlog::level::info);
    EXPECT_FALSE(config.default_pattern.empty());
    EXPECT_EQ(config.ring_buffer_size, 1000);
    EXPECT_FALSE(config.async_logging);
    EXPECT_EQ(config.async_queue_size, 8192);
    EXPECT_TRUE(config.sinks.empty());
}

TEST_F(LoggingConfigTest, FromJsonInvalidSinksArray) {
    nlohmann::json json = {{"default_level", "info"},
                           {"sinks", "not_an_array"}};

    auto config = LoggingConfig::fromJson(json);
    EXPECT_TRUE(config.sinks.empty());
}

TEST_F(LoggingConfigTest, RoundTripConversion) {
    auto original = createTestConfig();
    auto json = original.toJson();
    auto restored = LoggingConfig::fromJson(json);

    EXPECT_EQ(restored.default_level, original.default_level);
    EXPECT_EQ(restored.default_pattern, original.default_pattern);
    EXPECT_EQ(restored.ring_buffer_size, original.ring_buffer_size);
    EXPECT_EQ(restored.async_logging, original.async_logging);
    EXPECT_EQ(restored.async_queue_size, original.async_queue_size);
    EXPECT_EQ(restored.sinks.size(), original.sinks.size());
}

// ============================================================================
// Level Conversion Tests
// ============================================================================

class LevelConversionTest : public ::testing::Test {};

TEST_F(LevelConversionTest, LevelFromStringTrace) {
    EXPECT_EQ(levelFromString("trace"), spdlog::level::trace);
}

TEST_F(LevelConversionTest, LevelFromStringDebug) {
    EXPECT_EQ(levelFromString("debug"), spdlog::level::debug);
}

TEST_F(LevelConversionTest, LevelFromStringInfo) {
    EXPECT_EQ(levelFromString("info"), spdlog::level::info);
}

TEST_F(LevelConversionTest, LevelFromStringWarn) {
    EXPECT_EQ(levelFromString("warn"), spdlog::level::warn);
}

TEST_F(LevelConversionTest, LevelFromStringWarning) {
    EXPECT_EQ(levelFromString("warning"), spdlog::level::warn);
}

TEST_F(LevelConversionTest, LevelFromStringError) {
    EXPECT_EQ(levelFromString("error"), spdlog::level::err);
}

TEST_F(LevelConversionTest, LevelFromStringErr) {
    EXPECT_EQ(levelFromString("err"), spdlog::level::err);
}

TEST_F(LevelConversionTest, LevelFromStringCritical) {
    EXPECT_EQ(levelFromString("critical"), spdlog::level::critical);
}

TEST_F(LevelConversionTest, LevelFromStringFatal) {
    EXPECT_EQ(levelFromString("fatal"), spdlog::level::critical);
}

TEST_F(LevelConversionTest, LevelFromStringOff) {
    EXPECT_EQ(levelFromString("off"), spdlog::level::off);
}

TEST_F(LevelConversionTest, LevelFromStringUnknown) {
    EXPECT_EQ(levelFromString("unknown"), spdlog::level::info);
    EXPECT_EQ(levelFromString("invalid"), spdlog::level::info);
    EXPECT_EQ(levelFromString(""), spdlog::level::info);
}

TEST_F(LevelConversionTest, LevelToStringTrace) {
    EXPECT_EQ(levelToString(spdlog::level::trace), "trace");
}

TEST_F(LevelConversionTest, LevelToStringDebug) {
    EXPECT_EQ(levelToString(spdlog::level::debug), "debug");
}

TEST_F(LevelConversionTest, LevelToStringInfo) {
    EXPECT_EQ(levelToString(spdlog::level::info), "info");
}

TEST_F(LevelConversionTest, LevelToStringWarn) {
    // spdlog returns "warning" for warn level
    EXPECT_EQ(levelToString(spdlog::level::warn), "warning");
}

TEST_F(LevelConversionTest, LevelToStringError) {
    EXPECT_EQ(levelToString(spdlog::level::err), "error");
}

TEST_F(LevelConversionTest, LevelToStringCritical) {
    EXPECT_EQ(levelToString(spdlog::level::critical), "critical");
}

TEST_F(LevelConversionTest, LevelToStringOff) {
    EXPECT_EQ(levelToString(spdlog::level::off), "off");
}

TEST_F(LevelConversionTest, RoundTripConversion) {
    // Test all levels can be converted to string and back
    std::vector<spdlog::level::level_enum> levels = {
        spdlog::level::trace, spdlog::level::debug,    spdlog::level::info,
        spdlog::level::warn,  spdlog::level::err,      spdlog::level::critical,
        spdlog::level::off};

    for (auto level : levels) {
        auto str = levelToString(level);
        auto restored = levelFromString(str);
        EXPECT_EQ(restored, level) << "Failed for level: " << str;
    }
}
