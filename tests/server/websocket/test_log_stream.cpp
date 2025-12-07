/*
 * test_log_stream.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for WebSocket Log Streaming

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/core/logging_manager.hpp"
#include "server/websocket/log_stream.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace lithium::server;
using namespace lithium::logging;
using namespace std::chrono_literals;

class LogStreamManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging manager first
        auto& logging_manager = LoggingManager::getInstance();
        if (!logging_manager.isInitialized()) {
            LoggingConfig config;
            config.default_level = spdlog::level::debug;
            config.ring_buffer_size = 100;

            SinkConfig console_sink;
            console_sink.name = "test_console";
            console_sink.type = "console";
            console_sink.level = spdlog::level::trace;
            config.sinks.push_back(console_sink);

            logging_manager.initialize(config);
        }

        // Initialize log stream manager
        auto& stream_manager = LogStreamManager::getInstance();
        if (!stream_manager.isInitialized()) {
            stream_manager.initialize();
        }
    }

    void TearDown() override {
        auto& stream_manager = LogStreamManager::getInstance();
        if (stream_manager.isInitialized()) {
            stream_manager.shutdown();
        }

        auto& logging_manager = LoggingManager::getInstance();
        if (logging_manager.isInitialized()) {
            logging_manager.shutdown();
        }
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(LogStreamManagerTest, SingletonInstance) {
    auto& instance1 = LogStreamManager::getInstance();
    auto& instance2 = LogStreamManager::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(LogStreamManagerTest, InitializeSucceeds) {
    auto& manager = LogStreamManager::getInstance();
    EXPECT_TRUE(manager.isInitialized());
}

TEST_F(LogStreamManagerTest, ShutdownSucceeds) {
    auto& manager = LogStreamManager::getInstance();
    EXPECT_TRUE(manager.isInitialized());

    manager.shutdown();
    EXPECT_FALSE(manager.isInitialized());
}

// ============================================================================
// Subscription Tests
// ============================================================================

TEST_F(LogStreamManagerTest, SubscribeAddsSubscriber) {
    auto& manager = LogStreamManager::getInstance();

    LogStreamSubscription sub;
    sub.enabled = true;

    manager.subscribe("test_conn_1", sub, [](const std::string&) {});

    EXPECT_TRUE(manager.isSubscribed("test_conn_1"));
    EXPECT_EQ(manager.getSubscriberCount(), 1);

    manager.unsubscribe("test_conn_1");
}

TEST_F(LogStreamManagerTest, UnsubscribeRemovesSubscriber) {
    auto& manager = LogStreamManager::getInstance();

    LogStreamSubscription sub;
    manager.subscribe("test_conn_2", sub, [](const std::string&) {});

    EXPECT_TRUE(manager.isSubscribed("test_conn_2"));

    manager.unsubscribe("test_conn_2");

    EXPECT_FALSE(manager.isSubscribed("test_conn_2"));
    EXPECT_EQ(manager.getSubscriberCount(), 0);
}

TEST_F(LogStreamManagerTest, GetSubscriptionReturnsCorrectData) {
    auto& manager = LogStreamManager::getInstance();

    LogStreamSubscription sub;
    sub.level_filter = spdlog::level::warn;
    sub.logger_filter = "test_logger";
    sub.include_source = true;
    sub.enabled = true;

    manager.subscribe("test_conn_3", sub, [](const std::string&) {});

    auto retrieved = manager.getSubscription("test_conn_3");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->level_filter, spdlog::level::warn);
    EXPECT_EQ(retrieved->logger_filter, "test_logger");
    EXPECT_TRUE(retrieved->include_source);
    EXPECT_TRUE(retrieved->enabled);

    manager.unsubscribe("test_conn_3");
}

TEST_F(LogStreamManagerTest, UpdateSubscriptionModifiesExisting) {
    auto& manager = LogStreamManager::getInstance();

    LogStreamSubscription sub1;
    sub1.level_filter = spdlog::level::info;
    manager.subscribe("test_conn_4", sub1, [](const std::string&) {});

    LogStreamSubscription sub2;
    sub2.level_filter = spdlog::level::error;
    manager.updateSubscription("test_conn_4", sub2);

    auto retrieved = manager.getSubscription("test_conn_4");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->level_filter, spdlog::level::error);

    manager.unsubscribe("test_conn_4");
}

// ============================================================================
// Log Delivery Tests
// ============================================================================

TEST_F(LogStreamManagerTest, SubscriberReceivesLogs) {
    auto& manager = LogStreamManager::getInstance();
    auto& logging_manager = LoggingManager::getInstance();

    std::atomic<int> received_count{0};
    std::vector<std::string> received_messages;
    std::mutex messages_mutex;

    LogStreamSubscription sub;
    sub.enabled = true;

    manager.subscribe("receiver_test", sub, [&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(messages_mutex);
        received_messages.push_back(msg);
        ++received_count;
    });

    auto logger = logging_manager.getLogger("stream_test");
    logger->info("Test message 1");
    logger->info("Test message 2");

    std::this_thread::sleep_for(100ms);

    EXPECT_GE(received_count.load(), 2);

    manager.unsubscribe("receiver_test");
}

TEST_F(LogStreamManagerTest, LevelFilterWorks) {
    auto& manager = LogStreamManager::getInstance();
    auto& logging_manager = LoggingManager::getInstance();

    std::atomic<int> received_count{0};

    LogStreamSubscription sub;
    sub.level_filter = spdlog::level::warn;  // Only warn and above
    sub.enabled = true;

    manager.subscribe("level_filter_test", sub,
                      [&](const std::string&) { ++received_count; });

    auto logger = logging_manager.getLogger("level_filter_logger");
    logger->debug("Debug - should be filtered");
    logger->info("Info - should be filtered");
    logger->warn("Warn - should pass");
    logger->error("Error - should pass");

    std::this_thread::sleep_for(100ms);

    // Should only receive warn and error
    EXPECT_GE(received_count.load(), 2);

    manager.unsubscribe("level_filter_test");
}

TEST_F(LogStreamManagerTest, LoggerFilterWorks) {
    auto& manager = LogStreamManager::getInstance();
    auto& logging_manager = LoggingManager::getInstance();

    std::atomic<int> received_count{0};

    LogStreamSubscription sub;
    sub.logger_filter = "target_logger";
    sub.enabled = true;

    manager.subscribe("logger_filter_test", sub,
                      [&](const std::string&) { ++received_count; });

    auto target_logger = logging_manager.getLogger("target_logger");
    auto other_logger = logging_manager.getLogger("other_logger");

    target_logger->info("From target - should pass");
    other_logger->info("From other - should be filtered");

    std::this_thread::sleep_for(100ms);

    // Should only receive from target_logger
    EXPECT_GE(received_count.load(), 1);

    manager.unsubscribe("logger_filter_test");
}

TEST_F(LogStreamManagerTest, DisabledSubscriptionDoesNotReceive) {
    auto& manager = LogStreamManager::getInstance();
    auto& logging_manager = LoggingManager::getInstance();

    std::atomic<int> received_count{0};

    LogStreamSubscription sub;
    sub.enabled = false;  // Disabled

    manager.subscribe("disabled_test", sub,
                      [&](const std::string&) { ++received_count; });

    auto logger = logging_manager.getLogger("disabled_test_logger");
    logger->info("Should not be received");

    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(received_count.load(), 0);

    manager.unsubscribe("disabled_test");
}

// ============================================================================
// Message Handling Tests
// ============================================================================

TEST_F(LogStreamManagerTest, HandleSubscribeMessage) {
    auto& manager = LogStreamManager::getInstance();

    std::string response;
    auto send_callback = [&response](const std::string& msg) {
        response = msg;
    };

    nlohmann::json message = {
        {"type", "subscribe"},
        {"topic", "logs"},
        {"options", {{"level", "warn"}, {"include_source", true}}}};

    bool handled = manager.handleMessage("msg_test_1", message, send_callback);

    EXPECT_TRUE(handled);
    EXPECT_TRUE(manager.isSubscribed("msg_test_1"));

    auto parsed_response = nlohmann::json::parse(response);
    EXPECT_EQ(parsed_response["type"], "subscribed");
    EXPECT_EQ(parsed_response["topic"], "logs");

    manager.unsubscribe("msg_test_1");
}

TEST_F(LogStreamManagerTest, HandleUnsubscribeMessage) {
    auto& manager = LogStreamManager::getInstance();

    // First subscribe
    LogStreamSubscription sub;
    manager.subscribe("msg_test_2", sub, [](const std::string&) {});
    EXPECT_TRUE(manager.isSubscribed("msg_test_2"));

    // Then unsubscribe via message
    std::string response;
    nlohmann::json message = {{"type", "unsubscribe"}, {"topic", "logs"}};

    bool handled = manager.handleMessage(
        "msg_test_2", message,
        [&response](const std::string& msg) { response = msg; });

    EXPECT_TRUE(handled);
    EXPECT_FALSE(manager.isSubscribed("msg_test_2"));
}

TEST_F(LogStreamManagerTest, HandleCommandSubscribe) {
    auto& manager = LogStreamManager::getInstance();

    std::string response;
    nlohmann::json message = {{"type", "command"},
                              {"command", "logs.subscribe"},
                              {"payload", {{"level", "info"}}}};

    bool handled = manager.handleMessage(
        "cmd_test_1", message,
        [&response](const std::string& msg) { response = msg; });

    EXPECT_TRUE(handled);
    EXPECT_TRUE(manager.isSubscribed("cmd_test_1"));

    manager.unsubscribe("cmd_test_1");
}

TEST_F(LogStreamManagerTest, HandleCommandStatus) {
    auto& manager = LogStreamManager::getInstance();

    // Subscribe first
    LogStreamSubscription sub;
    manager.subscribe("status_test", sub, [](const std::string&) {});

    std::string response;
    nlohmann::json message = {{"type", "command"}, {"command", "logs.status"}};

    bool handled = manager.handleMessage(
        "status_test", message,
        [&response](const std::string& msg) { response = msg; });

    EXPECT_TRUE(handled);

    auto parsed = nlohmann::json::parse(response);
    EXPECT_EQ(parsed["data"]["subscribed"], true);
    EXPECT_TRUE(parsed["data"].contains("stats"));

    manager.unsubscribe("status_test");
}

TEST_F(LogStreamManagerTest, UnrelatedMessageNotHandled) {
    auto& manager = LogStreamManager::getInstance();

    nlohmann::json message = {{"type", "other"}, {"data", "something"}};

    bool handled =
        manager.handleMessage("other_test", message, [](const std::string&) {});

    EXPECT_FALSE(handled);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(LogStreamManagerTest, GetStatsReturnsValidData) {
    auto& manager = LogStreamManager::getInstance();

    auto stats = manager.getStats();

    EXPECT_TRUE(stats.contains("subscriber_count"));
    EXPECT_TRUE(stats.contains("total_entries_sent"));
    EXPECT_TRUE(stats.contains("total_entries_filtered"));
    EXPECT_TRUE(stats.contains("initialized"));
}

// ============================================================================
// Subscription Serialization Tests
// ============================================================================

TEST_F(LogStreamManagerTest, SubscriptionToJson) {
    LogStreamSubscription sub;
    sub.level_filter = spdlog::level::warn;
    sub.logger_filter = "my_logger";
    sub.include_source = true;
    sub.enabled = true;

    auto json = sub.toJson();

    EXPECT_EQ(json["level"], "warning");
    EXPECT_EQ(json["logger"], "my_logger");
    EXPECT_EQ(json["include_source"], true);
    EXPECT_EQ(json["enabled"], true);
}

TEST_F(LogStreamManagerTest, SubscriptionFromJson) {
    nlohmann::json json = {{"level", "error"},
                           {"logger", "test_logger"},
                           {"include_source", false},
                           {"enabled", true}};

    auto sub = LogStreamSubscription::fromJson(json);

    EXPECT_EQ(sub.level_filter, spdlog::level::err);
    EXPECT_EQ(sub.logger_filter, "test_logger");
    EXPECT_FALSE(sub.include_source);
    EXPECT_TRUE(sub.enabled);
}

// ============================================================================
// Concurrent Access Tests
// ============================================================================

TEST_F(LogStreamManagerTest, ConcurrentSubscriptions) {
    auto& manager = LogStreamManager::getInstance();

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager, i, &success_count]() {
            LogStreamSubscription sub;
            sub.enabled = true;

            std::string conn_id = "concurrent_" + std::to_string(i);
            manager.subscribe(conn_id, sub, [](const std::string&) {});

            if (manager.isSubscribed(conn_id)) {
                ++success_count;
            }

            manager.unsubscribe(conn_id);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
    EXPECT_EQ(manager.getSubscriberCount(), 0);
}

TEST_F(LogStreamManagerTest, ConcurrentLogDelivery) {
    auto& manager = LogStreamManager::getInstance();
    auto& logging_manager = LoggingManager::getInstance();

    std::atomic<int> total_received{0};

    // Subscribe multiple receivers
    for (int i = 0; i < 5; ++i) {
        LogStreamSubscription sub;
        sub.enabled = true;

        manager.subscribe("concurrent_recv_" + std::to_string(i), sub,
                          [&](const std::string&) { ++total_received; });
    }

    // Log from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&logging_manager, i]() {
            auto logger = logging_manager.getLogger("concurrent_log_" +
                                                    std::to_string(i));
            for (int j = 0; j < 10; ++j) {
                logger->info("Message {} from thread {}", j, i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(200ms);

    // Should have received many messages (5 threads * 10 messages * 5
    // receivers)
    EXPECT_GE(total_received.load(), 50);

    // Cleanup
    for (int i = 0; i < 5; ++i) {
        manager.unsubscribe("concurrent_recv_" + std::to_string(i));
    }
}
