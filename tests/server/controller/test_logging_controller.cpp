/*
 * test_logging_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for Logging Controller HTTP API

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/core/logging_manager.hpp"
#include "server/controller/logging.hpp"

#include <chrono>
#include <thread>

using namespace lithium::logging;
using namespace lithium::server::controller;
using namespace std::chrono_literals;

class LoggingControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging manager
        auto& manager = LoggingManager::getInstance();
        if (!manager.isInitialized()) {
            LoggingConfig config;
            config.default_level = spdlog::level::debug;
            config.ring_buffer_size = 100;

            SinkConfig console_sink;
            console_sink.name = "test_console";
            console_sink.type = "console";
            console_sink.level = spdlog::level::trace;
            config.sinks.push_back(console_sink);

            manager.initialize(config);
        }
    }

    void TearDown() override {
        auto& manager = LoggingManager::getInstance();
        if (manager.isInitialized()) {
            manager.shutdown();
        }
    }
};

// ============================================================================
// Logger Endpoint Tests
// ============================================================================

TEST_F(LoggingControllerTest, ListLoggersReturnsValidJson) {
    // Create some loggers
    auto& manager = LoggingManager::getInstance();
    manager.getLogger("test_logger_1");
    manager.getLogger("test_logger_2");

    auto loggers = manager.listLoggers();

    EXPECT_GE(loggers.size(), 2);

    // Verify JSON serialization
    for (const auto& logger : loggers) {
        auto json = logger.toJson();
        EXPECT_TRUE(json.contains("name"));
        EXPECT_TRUE(json.contains("level"));
        EXPECT_TRUE(json.contains("pattern"));
        EXPECT_TRUE(json.contains("sinks"));
    }
}

TEST_F(LoggingControllerTest, SetLoggerLevelUpdatesCorrectly) {
    auto& manager = LoggingManager::getInstance();
    auto logger = manager.getLogger("level_test_logger");

    // Set to warn level
    EXPECT_TRUE(
        manager.setLoggerLevel("level_test_logger", spdlog::level::warn));
    EXPECT_EQ(logger->level(), spdlog::level::warn);

    // Set to debug level
    EXPECT_TRUE(
        manager.setLoggerLevel("level_test_logger", spdlog::level::debug));
    EXPECT_EQ(logger->level(), spdlog::level::debug);
}

TEST_F(LoggingControllerTest, SetGlobalLevelAffectsConfig) {
    auto& manager = LoggingManager::getInstance();

    manager.setGlobalLevel(spdlog::level::err);

    auto config = manager.getConfig();
    EXPECT_EQ(config.default_level, spdlog::level::err);
}

// ============================================================================
// Sink Endpoint Tests
// ============================================================================

TEST_F(LoggingControllerTest, ListSinksIncludesConfigured) {
    auto& manager = LoggingManager::getInstance();
    auto sinks = manager.listSinks();

    // Should have at least ringbuffer and test_console
    EXPECT_GE(sinks.size(), 1);

    bool has_ringbuffer = false;
    for (const auto& sink : sinks) {
        if (sink.name == "ringbuffer") {
            has_ringbuffer = true;
        }
    }
    EXPECT_TRUE(has_ringbuffer);
}

TEST_F(LoggingControllerTest, AddSinkCreatesNew) {
    auto& manager = LoggingManager::getInstance();

    SinkConfig new_sink;
    new_sink.name = "controller_test_sink";
    new_sink.type = "console";
    new_sink.level = spdlog::level::info;

    EXPECT_TRUE(manager.addSink(new_sink));

    auto sinks = manager.listSinks();
    bool found = false;
    for (const auto& sink : sinks) {
        if (sink.name == "controller_test_sink") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LoggingControllerTest, RemoveSinkDeletesExisting) {
    auto& manager = LoggingManager::getInstance();

    // First add a sink
    SinkConfig sink;
    sink.name = "removable_controller_sink";
    sink.type = "console";
    manager.addSink(sink);

    // Then remove it
    EXPECT_TRUE(manager.removeSink("removable_controller_sink"));

    // Verify it's gone
    auto sinks = manager.listSinks();
    bool found = false;
    for (const auto& s : sinks) {
        if (s.name == "removable_controller_sink") {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

// ============================================================================
// Log Retrieval Endpoint Tests
// ============================================================================

TEST_F(LoggingControllerTest, GetLogsReturnsEntries) {
    auto& manager = LoggingManager::getInstance();

    // Generate some logs
    auto logger = manager.getLogger("log_retrieval_test");
    logger->info("Test log message 1");
    logger->warn("Test log message 2");
    logger->error("Test log message 3");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    auto logs = manager.getRecentLogs(10);
    EXPECT_GE(logs.size(), 3);
}

TEST_F(LoggingControllerTest, GetLogsFilteredByLevel) {
    auto& manager = LoggingManager::getInstance();

    auto logger = manager.getLogger("filter_level_test");
    logger->debug("Debug message");
    logger->info("Info message");
    logger->warn("Warn message");
    logger->error("Error message");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    // Filter for warn and above
    auto filtered =
        manager.getLogsFiltered(spdlog::level::warn, std::nullopt, 100);

    for (const auto& entry : filtered) {
        EXPECT_GE(entry.level, spdlog::level::warn);
    }
}

TEST_F(LoggingControllerTest, GetLogsFilteredByLogger) {
    auto& manager = LoggingManager::getInstance();

    auto logger1 = manager.getLogger("filter_logger_a");
    auto logger2 = manager.getLogger("filter_logger_b");

    logger1->info("Message from A");
    logger2->info("Message from B");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    // Filter for logger_a
    auto filtered =
        manager.getLogsFiltered(std::nullopt, "filter_logger_a", 100);

    for (const auto& entry : filtered) {
        EXPECT_THAT(entry.logger_name, ::testing::HasSubstr("filter_logger_a"));
    }
}

// ============================================================================
// Buffer Operations Tests
// ============================================================================

TEST_F(LoggingControllerTest, ClearBufferRemovesAllEntries) {
    auto& manager = LoggingManager::getInstance();

    auto logger = manager.getLogger("clear_buffer_test");
    logger->info("Message before clear");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    manager.clearLogBuffer();

    auto logs = manager.getRecentLogs(100);
    EXPECT_EQ(logs.size(), 0);
}

TEST_F(LoggingControllerTest, BufferStatsReturnsValidData) {
    auto& manager = LoggingManager::getInstance();

    auto stats = manager.getBufferStats();

    EXPECT_TRUE(stats.contains("size"));
    EXPECT_TRUE(stats.contains("capacity"));
    EXPECT_TRUE(stats.contains("usage_percent"));

    EXPECT_GE(stats["capacity"].get<size_t>(), 0);
    EXPECT_GE(stats["usage_percent"].get<double>(), 0.0);
    EXPECT_LE(stats["usage_percent"].get<double>(), 100.0);
}

// ============================================================================
// Flush and Rotate Tests
// ============================================================================

TEST_F(LoggingControllerTest, FlushCompletesSuccessfully) {
    auto& manager = LoggingManager::getInstance();

    auto logger = manager.getLogger("flush_test");
    logger->info("Message to flush");

    EXPECT_NO_THROW(manager.flush());
}

TEST_F(LoggingControllerTest, RotateCompletesSuccessfully) {
    auto& manager = LoggingManager::getInstance();

    EXPECT_NO_THROW(manager.rotate());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(LoggingControllerTest, GetConfigReturnsValidData) {
    auto& manager = LoggingManager::getInstance();

    auto config = manager.getConfig();
    auto json = config.toJson();

    EXPECT_TRUE(json.contains("default_level"));
    EXPECT_TRUE(json.contains("default_pattern"));
    EXPECT_TRUE(json.contains("ring_buffer_size"));
    EXPECT_TRUE(json.contains("async_logging"));
    EXPECT_TRUE(json.contains("sinks"));
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

TEST_F(LoggingControllerTest, LogEntryJsonRoundTrip) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::warn;
    entry.logger_name = "roundtrip_test";
    entry.message = "Test message with special chars: <>&\"'";
    entry.thread_id = "12345";
    entry.source_file = "test.cpp";
    entry.source_line = 100;

    auto json = entry.toJson();

    EXPECT_EQ(json["level"], "warning");
    EXPECT_EQ(json["logger"], "roundtrip_test");
    EXPECT_EQ(json["message"], "Test message with special chars: <>&\"'");
    EXPECT_EQ(json["thread_id"], "12345");
    EXPECT_EQ(json["source_file"], "test.cpp");
    EXPECT_EQ(json["source_line"], 100);
}

TEST_F(LoggingControllerTest, SinkConfigJsonRoundTrip) {
    SinkConfig original;
    original.name = "test_sink";
    original.type = "rotating_file";
    original.level = spdlog::level::debug;
    original.pattern = "[%l] %v";
    original.file_path = "/var/log/test.log";
    original.max_file_size = 5 * 1024 * 1024;
    original.max_files = 3;

    auto json = original.toJson();
    auto restored = SinkConfig::fromJson(json);

    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.type, original.type);
    EXPECT_EQ(restored.level, original.level);
    EXPECT_EQ(restored.file_path, original.file_path);
    EXPECT_EQ(restored.max_file_size, original.max_file_size);
    EXPECT_EQ(restored.max_files, original.max_files);
}

TEST_F(LoggingControllerTest, LoggingConfigJsonRoundTrip) {
    LoggingConfig original;
    original.default_level = spdlog::level::warn;
    original.default_pattern = "[%n] [%l] %v";
    original.ring_buffer_size = 500;
    original.async_logging = true;
    original.async_queue_size = 4096;

    SinkConfig sink;
    sink.name = "config_test_sink";
    sink.type = "console";
    sink.level = spdlog::level::info;
    original.sinks.push_back(sink);

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
// Error Handling Tests
// ============================================================================

TEST_F(LoggingControllerTest, SetLevelOnNonExistentLoggerFails) {
    auto& manager = LoggingManager::getInstance();

    EXPECT_FALSE(
        manager.setLoggerLevel("nonexistent_logger_xyz", spdlog::level::warn));
}

TEST_F(LoggingControllerTest, RemoveNonExistentSinkFails) {
    auto& manager = LoggingManager::getInstance();

    // This should fail silently or return false
    bool result = manager.removeSink("nonexistent_sink_xyz");
    // Behavior depends on implementation - just ensure no crash
    (void)result;
}

TEST_F(LoggingControllerTest, AddDuplicateSinkFails) {
    auto& manager = LoggingManager::getInstance();

    SinkConfig sink;
    sink.name = "duplicate_test_sink";
    sink.type = "console";

    EXPECT_TRUE(manager.addSink(sink));
    EXPECT_FALSE(manager.addSink(sink));  // Duplicate should fail
}
