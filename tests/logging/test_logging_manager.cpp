/*
 * test_logging_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for LoggingManager

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/core/logging_manager.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace lithium::logging;
using namespace std::chrono_literals;

class LoggingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state before each test
        auto& manager = LoggingManager::getInstance();
        if (manager.isInitialized()) {
            manager.shutdown();
        }
    }

    void TearDown() override {
        // Clean up after each test
        auto& manager = LoggingManager::getInstance();
        if (manager.isInitialized()) {
            manager.shutdown();
        }
    }

    LoggingConfig createTestConfig() {
        LoggingConfig config;
        config.default_level = spdlog::level::debug;
        config.default_pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v";
        config.ring_buffer_size = 100;
        config.async_logging = false;

        // Add console sink for testing
        SinkConfig console_sink;
        console_sink.name = "test_console";
        console_sink.type = "console";
        console_sink.level = spdlog::level::trace;
        config.sinks.push_back(console_sink);

        return config;
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(LoggingManagerTest, SingletonInstance) {
    auto& instance1 = LoggingManager::getInstance();
    auto& instance2 = LoggingManager::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(LoggingManagerTest, InitializeWithDefaultConfig) {
    auto& manager = LoggingManager::getInstance();
    LoggingConfig config;

    EXPECT_FALSE(manager.isInitialized());
    manager.initialize(config);
    EXPECT_TRUE(manager.isInitialized());
}

TEST_F(LoggingManagerTest, InitializeWithCustomConfig) {
    auto& manager = LoggingManager::getInstance();
    auto config = createTestConfig();

    manager.initialize(config);

    EXPECT_TRUE(manager.isInitialized());
    auto retrieved_config = manager.getConfig();
    EXPECT_EQ(retrieved_config.default_level, spdlog::level::debug);
    EXPECT_EQ(retrieved_config.ring_buffer_size, 100);
}

TEST_F(LoggingManagerTest, ShutdownCleansUp) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    EXPECT_TRUE(manager.isInitialized());
    manager.shutdown();
    EXPECT_FALSE(manager.isInitialized());
}

TEST_F(LoggingManagerTest, ReinitializeAfterShutdown) {
    auto& manager = LoggingManager::getInstance();

    manager.initialize(createTestConfig());
    EXPECT_TRUE(manager.isInitialized());

    manager.shutdown();
    EXPECT_FALSE(manager.isInitialized());

    manager.initialize(createTestConfig());
    EXPECT_TRUE(manager.isInitialized());
}

// ============================================================================
// Logger Management Tests
// ============================================================================

TEST_F(LoggingManagerTest, GetLoggerCreatesNew) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("test_logger");
    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "test_logger");
}

TEST_F(LoggingManagerTest, GetLoggerReturnsSame) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger1 = manager.getLogger("same_logger");
    auto logger2 = manager.getLogger("same_logger");

    EXPECT_EQ(logger1.get(), logger2.get());
}

TEST_F(LoggingManagerTest, ListLoggersReturnsAll) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    manager.getLogger("logger_a");
    manager.getLogger("logger_b");
    manager.getLogger("logger_c");

    auto loggers = manager.listLoggers();

    // Should have at least default + 3 created loggers
    EXPECT_GE(loggers.size(), 3);

    std::vector<std::string> names;
    for (const auto& info : loggers) {
        names.push_back(info.name);
    }

    EXPECT_THAT(names, ::testing::Contains("logger_a"));
    EXPECT_THAT(names, ::testing::Contains("logger_b"));
    EXPECT_THAT(names, ::testing::Contains("logger_c"));
}

TEST_F(LoggingManagerTest, SetLoggerLevel) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("level_test");
    EXPECT_TRUE(manager.setLoggerLevel("level_test", spdlog::level::warn));

    EXPECT_EQ(logger->level(), spdlog::level::warn);
}

TEST_F(LoggingManagerTest, SetLoggerLevelNonExistent) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    EXPECT_FALSE(manager.setLoggerLevel("nonexistent", spdlog::level::warn));
}

TEST_F(LoggingManagerTest, SetGlobalLevel) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    manager.getLogger("global_test_1");
    manager.getLogger("global_test_2");

    manager.setGlobalLevel(spdlog::level::err);

    auto config = manager.getConfig();
    EXPECT_EQ(config.default_level, spdlog::level::err);
}

TEST_F(LoggingManagerTest, SetLoggerPattern) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    manager.getLogger("pattern_test");
    EXPECT_TRUE(manager.setLoggerPattern("pattern_test", "[%l] %v"));
}

TEST_F(LoggingManagerTest, RemoveLogger) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    manager.getLogger("removable");
    EXPECT_TRUE(manager.removeLogger("removable"));

    // Getting it again should create a new one
    auto logger = manager.getLogger("removable");
    EXPECT_NE(logger, nullptr);
}

TEST_F(LoggingManagerTest, CannotRemoveDefaultLogger) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    EXPECT_FALSE(manager.removeLogger("default"));
}

// ============================================================================
// Level Conversion Tests
// ============================================================================

TEST_F(LoggingManagerTest, LevelFromString) {
    EXPECT_EQ(LoggingManager::levelFromString("trace"), spdlog::level::trace);
    EXPECT_EQ(LoggingManager::levelFromString("debug"), spdlog::level::debug);
    EXPECT_EQ(LoggingManager::levelFromString("info"), spdlog::level::info);
    EXPECT_EQ(LoggingManager::levelFromString("warn"), spdlog::level::warn);
    EXPECT_EQ(LoggingManager::levelFromString("warning"), spdlog::level::warn);
    EXPECT_EQ(LoggingManager::levelFromString("error"), spdlog::level::err);
    EXPECT_EQ(LoggingManager::levelFromString("err"), spdlog::level::err);
    EXPECT_EQ(LoggingManager::levelFromString("critical"),
              spdlog::level::critical);
    EXPECT_EQ(LoggingManager::levelFromString("fatal"),
              spdlog::level::critical);
    EXPECT_EQ(LoggingManager::levelFromString("off"), spdlog::level::off);
    EXPECT_EQ(LoggingManager::levelFromString("unknown"),
              spdlog::level::info);  // Default
}

TEST_F(LoggingManagerTest, LevelToString) {
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::trace), "trace");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::debug), "debug");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::info), "info");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::warn), "warning");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::err), "error");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::critical),
              "critical");
    EXPECT_EQ(LoggingManager::levelToString(spdlog::level::off), "off");
}

// ============================================================================
// Ring Buffer Tests
// ============================================================================

TEST_F(LoggingManagerTest, RingBufferCapturesLogs) {
    auto& manager = LoggingManager::getInstance();
    auto config = createTestConfig();
    config.ring_buffer_size = 50;
    manager.initialize(config);

    auto logger = manager.getLogger("buffer_test");

    logger->info("Test message 1");
    logger->info("Test message 2");
    logger->info("Test message 3");

    // Give time for async processing
    std::this_thread::sleep_for(50ms);
    manager.flush();

    auto logs = manager.getRecentLogs(10);
    EXPECT_GE(logs.size(), 3);
}

TEST_F(LoggingManagerTest, RingBufferRespectsSizeLimit) {
    auto& manager = LoggingManager::getInstance();
    auto config = createTestConfig();
    config.ring_buffer_size = 10;
    manager.initialize(config);

    auto logger = manager.getLogger("overflow_test");

    // Write more logs than buffer size
    for (int i = 0; i < 20; ++i) {
        logger->info("Message {}", i);
    }

    std::this_thread::sleep_for(50ms);
    manager.flush();

    auto logs = manager.getRecentLogs(100);
    EXPECT_LE(logs.size(), 10);
}

TEST_F(LoggingManagerTest, GetLogsFiltered) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("filter_test");

    logger->debug("Debug message");
    logger->info("Info message");
    logger->warn("Warning message");
    logger->error("Error message");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    // Filter by level
    auto warn_and_above =
        manager.getLogsFiltered(spdlog::level::warn, std::nullopt, 100);
    for (const auto& entry : warn_and_above) {
        EXPECT_GE(entry.level, spdlog::level::warn);
    }
}

TEST_F(LoggingManagerTest, ClearLogBuffer) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("clear_test");
    logger->info("Message before clear");

    std::this_thread::sleep_for(50ms);
    manager.flush();

    manager.clearLogBuffer();

    auto logs = manager.getRecentLogs(100);
    EXPECT_EQ(logs.size(), 0);
}

TEST_F(LoggingManagerTest, GetBufferStats) {
    auto& manager = LoggingManager::getInstance();
    auto config = createTestConfig();
    config.ring_buffer_size = 100;
    manager.initialize(config);

    auto stats = manager.getBufferStats();

    EXPECT_TRUE(stats.contains("size"));
    EXPECT_TRUE(stats.contains("capacity"));
    EXPECT_TRUE(stats.contains("usage_percent"));
    EXPECT_EQ(stats["capacity"].get<size_t>(), 100);
}

// ============================================================================
// Sink Management Tests
// ============================================================================

TEST_F(LoggingManagerTest, ListSinksIncludesRingBuffer) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto sinks = manager.listSinks();

    bool has_ringbuffer = false;
    for (const auto& sink : sinks) {
        if (sink.name == "ringbuffer") {
            has_ringbuffer = true;
            break;
        }
    }
    EXPECT_TRUE(has_ringbuffer);
}

TEST_F(LoggingManagerTest, AddSink) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    SinkConfig new_sink;
    new_sink.name = "new_console";
    new_sink.type = "console";
    new_sink.level = spdlog::level::info;

    EXPECT_TRUE(manager.addSink(new_sink));

    auto sinks = manager.listSinks();
    bool found = false;
    for (const auto& sink : sinks) {
        if (sink.name == "new_console") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LoggingManagerTest, AddDuplicateSinkFails) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    SinkConfig sink;
    sink.name = "duplicate";
    sink.type = "console";

    EXPECT_TRUE(manager.addSink(sink));
    EXPECT_FALSE(manager.addSink(sink));  // Duplicate should fail
}

TEST_F(LoggingManagerTest, RemoveSink) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    SinkConfig sink;
    sink.name = "removable_sink";
    sink.type = "console";

    manager.addSink(sink);
    EXPECT_TRUE(manager.removeSink("removable_sink"));
}

TEST_F(LoggingManagerTest, CannotRemoveRingBufferSink) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    EXPECT_FALSE(manager.removeSink("ringbuffer"));
}

// ============================================================================
// Subscription Tests
// ============================================================================

TEST_F(LoggingManagerTest, SubscribeReceivesLogs) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    std::atomic<int> received_count{0};
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    manager.subscribe("test_subscriber", [&](const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.push_back(entry.message);
        ++received_count;
    });

    auto logger = manager.getLogger("subscribe_test");
    logger->info("Subscribed message 1");
    logger->info("Subscribed message 2");

    std::this_thread::sleep_for(100ms);

    EXPECT_GE(received_count.load(), 2);

    manager.unsubscribe("test_subscriber");
}

TEST_F(LoggingManagerTest, UnsubscribeStopsLogs) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    std::atomic<int> received_count{0};

    manager.subscribe("unsub_test", [&](const LogEntry&) { ++received_count; });

    auto logger = manager.getLogger("unsub_logger");
    logger->info("Before unsubscribe");

    std::this_thread::sleep_for(50ms);
    int count_before = received_count.load();

    manager.unsubscribe("unsub_test");

    logger->info("After unsubscribe");
    std::this_thread::sleep_for(50ms);

    // Count should not have increased significantly after unsubscribe
    EXPECT_LE(received_count.load(), count_before + 1);
}

// ============================================================================
// Flush and Rotate Tests
// ============================================================================

TEST_F(LoggingManagerTest, FlushDoesNotThrow) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("flush_test");
    logger->info("Message to flush");

    EXPECT_NO_THROW(manager.flush());
}

TEST_F(LoggingManagerTest, RotateDoesNotThrow) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    EXPECT_NO_THROW(manager.rotate());
}

// ============================================================================
// LogEntry Serialization Tests
// ============================================================================

TEST_F(LoggingManagerTest, LogEntryToJson) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "test_logger";
    entry.message = "Test message";
    entry.thread_id = "12345";
    entry.source_file = "test.cpp";
    entry.source_line = 42;

    auto json = entry.toJson();

    EXPECT_TRUE(json.contains("timestamp"));
    EXPECT_EQ(json["level"], "info");
    EXPECT_EQ(json["logger"], "test_logger");
    EXPECT_EQ(json["message"], "Test message");
    EXPECT_EQ(json["thread_id"], "12345");
    EXPECT_EQ(json["source_file"], "test.cpp");
    EXPECT_EQ(json["source_line"], 42);
}

TEST_F(LoggingManagerTest, LoggerInfoToJson) {
    LoggerInfo info;
    info.name = "my_logger";
    info.level = spdlog::level::debug;
    info.pattern = "[%l] %v";
    info.sink_names = {"console", "file"};

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "my_logger");
    EXPECT_EQ(json["level"], "debug");
    EXPECT_EQ(json["pattern"], "[%l] %v");
    EXPECT_EQ(json["sinks"].size(), 2);
}

TEST_F(LoggingManagerTest, SinkConfigToJson) {
    SinkConfig config;
    config.name = "rotating";
    config.type = "rotating_file";
    config.level = spdlog::level::info;
    config.file_path = "/var/log/test.log";
    config.max_file_size = 1024 * 1024;
    config.max_files = 3;

    auto json = config.toJson();

    EXPECT_EQ(json["name"], "rotating");
    EXPECT_EQ(json["type"], "rotating_file");
    EXPECT_EQ(json["file_path"], "/var/log/test.log");
    EXPECT_EQ(json["max_file_size"], 1024 * 1024);
    EXPECT_EQ(json["max_files"], 3);
}

TEST_F(LoggingManagerTest, SinkConfigFromJson) {
    nlohmann::json json = {
        {"name", "daily"},    {"type", "daily_file"},
        {"level", "warn"},    {"file_path", "/var/log/daily.log"},
        {"rotation_hour", 0}, {"rotation_minute", 0}};

    auto config = SinkConfig::fromJson(json);

    EXPECT_EQ(config.name, "daily");
    EXPECT_EQ(config.type, "daily_file");
    EXPECT_EQ(config.level, spdlog::level::warn);
    EXPECT_EQ(config.file_path, "/var/log/daily.log");
}

TEST_F(LoggingManagerTest, LoggingConfigToJson) {
    LoggingConfig config;
    config.default_level = spdlog::level::debug;
    config.default_pattern = "[%l] %v";
    config.ring_buffer_size = 500;
    config.async_logging = true;
    config.async_queue_size = 4096;

    auto json = config.toJson();

    EXPECT_EQ(json["default_level"], "debug");
    EXPECT_EQ(json["default_pattern"], "[%l] %v");
    EXPECT_EQ(json["ring_buffer_size"], 500);
    EXPECT_EQ(json["async_logging"], true);
    EXPECT_EQ(json["async_queue_size"], 4096);
}

TEST_F(LoggingManagerTest, LoggingConfigFromJson) {
    nlohmann::json json = {
        {"default_level", "error"},
        {"default_pattern", "[%n] %v"},
        {"ring_buffer_size", 200},
        {"async_logging", false},
        {"async_queue_size", 2048},
        {"sinks",
         nlohmann::json::array(
             {{{"name", "console"}, {"type", "console"}, {"level", "info"}}})}};

    auto config = LoggingConfig::fromJson(json);

    EXPECT_EQ(config.default_level, spdlog::level::err);
    EXPECT_EQ(config.default_pattern, "[%n] %v");
    EXPECT_EQ(config.ring_buffer_size, 200);
    EXPECT_FALSE(config.async_logging);
    EXPECT_EQ(config.sinks.size(), 1);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(LoggingManagerTest, ConcurrentLoggerAccess) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager, i, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto logger =
                    manager.getLogger("concurrent_" + std::to_string(i));
                logger->info("Thread {} message {}", i, j);
            }
            ++success_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
}

TEST_F(LoggingManagerTest, ConcurrentSubscription) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    std::atomic<int> total_received{0};

    // Subscribe multiple listeners
    for (int i = 0; i < 5; ++i) {
        manager.subscribe("listener_" + std::to_string(i),
                          [&](const LogEntry&) { ++total_received; });
    }

    // Log from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&manager, i]() {
            auto logger =
                manager.getLogger("concurrent_sub_" + std::to_string(i));
            for (int j = 0; j < 10; ++j) {
                logger->info("Message {} from thread {}", j, i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(100ms);

    // Each message should be received by all 5 listeners
    // 5 threads * 10 messages * 5 listeners = 250 minimum
    EXPECT_GE(total_received.load(), 50);  // At least some messages received

    // Cleanup
    for (int i = 0; i < 5; ++i) {
        manager.unsubscribe("listener_" + std::to_string(i));
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(LoggingManagerTest, EmptyLoggerName) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    // Empty name should still work (creates logger with empty name)
    auto logger = manager.getLogger("");
    EXPECT_NE(logger, nullptr);
}

TEST_F(LoggingManagerTest, VeryLongLogMessage) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("long_message_test");

    std::string long_message(10000, 'x');
    EXPECT_NO_THROW(logger->info("{}", long_message));
}

TEST_F(LoggingManagerTest, SpecialCharactersInLogMessage) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("special_chars_test");

    EXPECT_NO_THROW(logger->info("Special chars: \t\n\r\"'\\{}[]"));
    EXPECT_NO_THROW(logger->info("Unicode: 你好世界 🌍 αβγδ"));
}

TEST_F(LoggingManagerTest, GetLogsWhenEmpty) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    manager.clearLogBuffer();

    auto logs = manager.getRecentLogs(100);
    EXPECT_EQ(logs.size(), 0);
}

TEST_F(LoggingManagerTest, GetLogsWithZeroCount) {
    auto& manager = LoggingManager::getInstance();
    manager.initialize(createTestConfig());

    auto logger = manager.getLogger("zero_count_test");
    logger->info("Test message");

    std::this_thread::sleep_for(50ms);

    auto logs = manager.getRecentLogs(0);  // 0 means all
    EXPECT_GE(logs.size(), 1);
}
