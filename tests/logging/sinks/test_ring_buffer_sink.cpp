/*
 * test_ring_buffer_sink.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for RingBufferSink

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/sinks/ring_buffer_sink.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace lithium::logging;
using namespace std::chrono_literals;

class RingBufferSinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a fresh sink for each test
        sink_ = std::make_shared<RingBufferSink>(100);
    }

    void TearDown() override { sink_.reset(); }

    void logMessage(spdlog::level::level_enum level, const std::string& logger,
                    const std::string& message) {
        spdlog::details::log_msg msg(logger, level, message);
        sink_->log(msg);
    }

    std::shared_ptr<RingBufferSink> sink_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(RingBufferSinkTest, ConstructWithCapacity) {
    auto sink = std::make_shared<RingBufferSink>(50);
    EXPECT_EQ(sink->capacity(), 50);
    EXPECT_EQ(sink->size(), 0);
}

TEST_F(RingBufferSinkTest, ConstructWithSmallCapacity) {
    auto sink = std::make_shared<RingBufferSink>(1);
    EXPECT_EQ(sink->capacity(), 1);
}

TEST_F(RingBufferSinkTest, ConstructWithLargeCapacity) {
    auto sink = std::make_shared<RingBufferSink>(10000);
    EXPECT_EQ(sink->capacity(), 10000);
}

// ============================================================================
// Basic Logging Tests
// ============================================================================

TEST_F(RingBufferSinkTest, LogSingleMessage) {
    logMessage(spdlog::level::info, "test_logger", "Test message");

    EXPECT_EQ(sink_->size(), 1);

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].message, "Test message");
    EXPECT_EQ(entries[0].logger_name, "test_logger");
    EXPECT_EQ(entries[0].level, spdlog::level::info);
}

TEST_F(RingBufferSinkTest, LogMultipleMessages) {
    logMessage(spdlog::level::info, "logger1", "Message 1");
    logMessage(spdlog::level::debug, "logger2", "Message 2");
    logMessage(spdlog::level::warn, "logger3", "Message 3");

    EXPECT_EQ(sink_->size(), 3);

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries.size(), 3);
}

TEST_F(RingBufferSinkTest, LogAllLevels) {
    logMessage(spdlog::level::trace, "logger", "Trace");
    logMessage(spdlog::level::debug, "logger", "Debug");
    logMessage(spdlog::level::info, "logger", "Info");
    logMessage(spdlog::level::warn, "logger", "Warn");
    logMessage(spdlog::level::err, "logger", "Error");
    logMessage(spdlog::level::critical, "logger", "Critical");

    EXPECT_EQ(sink_->size(), 6);

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].level, spdlog::level::trace);
    EXPECT_EQ(entries[1].level, spdlog::level::debug);
    EXPECT_EQ(entries[2].level, spdlog::level::info);
    EXPECT_EQ(entries[3].level, spdlog::level::warn);
    EXPECT_EQ(entries[4].level, spdlog::level::err);
    EXPECT_EQ(entries[5].level, spdlog::level::critical);
}

TEST_F(RingBufferSinkTest, LogEmptyMessage) {
    logMessage(spdlog::level::info, "logger", "");

    EXPECT_EQ(sink_->size(), 1);

    auto entries = sink_->getEntries();
    EXPECT_TRUE(entries[0].message.empty());
}

TEST_F(RingBufferSinkTest, LogLongMessage) {
    std::string long_message(10000, 'x');
    logMessage(spdlog::level::info, "logger", long_message);

    EXPECT_EQ(sink_->size(), 1);

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].message, long_message);
}

TEST_F(RingBufferSinkTest, LogUnicodeMessage) {
    logMessage(spdlog::level::info, "logger", "Unicode: 你好世界 🌍 αβγδ");

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].message, "Unicode: 你好世界 🌍 αβγδ");
}

// ============================================================================
// Ring Buffer Behavior Tests
// ============================================================================

TEST_F(RingBufferSinkTest, RingBufferOverflow) {
    auto small_sink = std::make_shared<RingBufferSink>(5);

    for (int i = 0; i < 10; ++i) {
        spdlog::details::log_msg msg("logger", spdlog::level::info,
                                     "Message " + std::to_string(i));
        small_sink->log(msg);
    }

    // Should only keep last 5 messages
    EXPECT_EQ(small_sink->size(), 5);

    auto entries = small_sink->getEntries();
    EXPECT_EQ(entries.size(), 5);

    // Should have messages 5-9 (oldest ones dropped)
    EXPECT_EQ(entries[0].message, "Message 5");
    EXPECT_EQ(entries[4].message, "Message 9");
}

TEST_F(RingBufferSinkTest, RingBufferExactCapacity) {
    auto sink = std::make_shared<RingBufferSink>(5);

    for (int i = 0; i < 5; ++i) {
        spdlog::details::log_msg msg("logger", spdlog::level::info,
                                     "Message " + std::to_string(i));
        sink->log(msg);
    }

    EXPECT_EQ(sink->size(), 5);

    auto entries = sink->getEntries();
    EXPECT_EQ(entries[0].message, "Message 0");
    EXPECT_EQ(entries[4].message, "Message 4");
}

TEST_F(RingBufferSinkTest, RingBufferWrapAround) {
    auto sink = std::make_shared<RingBufferSink>(3);

    // Fill buffer
    for (int i = 0; i < 3; ++i) {
        spdlog::details::log_msg msg("logger", spdlog::level::info,
                                     "First " + std::to_string(i));
        sink->log(msg);
    }

    // Overwrite with new messages
    for (int i = 0; i < 3; ++i) {
        spdlog::details::log_msg msg("logger", spdlog::level::info,
                                     "Second " + std::to_string(i));
        sink->log(msg);
    }

    auto entries = sink->getEntries();
    EXPECT_EQ(entries[0].message, "Second 0");
    EXPECT_EQ(entries[1].message, "Second 1");
    EXPECT_EQ(entries[2].message, "Second 2");
}

// ============================================================================
// GetEntries Tests
// ============================================================================

TEST_F(RingBufferSinkTest, GetEntriesEmpty) {
    auto entries = sink_->getEntries();
    EXPECT_TRUE(entries.empty());
}

TEST_F(RingBufferSinkTest, GetEntriesAll) {
    for (int i = 0; i < 10; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto entries = sink_->getEntries(0);  // 0 means all
    EXPECT_EQ(entries.size(), 10);
}

TEST_F(RingBufferSinkTest, GetEntriesLimited) {
    for (int i = 0; i < 10; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto entries = sink_->getEntries(5);
    EXPECT_EQ(entries.size(), 5);

    // Should get the most recent 5
    EXPECT_EQ(entries[0].message, "Message 5");
    EXPECT_EQ(entries[4].message, "Message 9");
}

TEST_F(RingBufferSinkTest, GetEntriesMoreThanAvailable) {
    for (int i = 0; i < 3; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto entries = sink_->getEntries(100);
    EXPECT_EQ(entries.size(), 3);
}

// ============================================================================
// GetEntriesSince Tests
// ============================================================================

TEST_F(RingBufferSinkTest, GetEntriesSinceEmpty) {
    auto since = std::chrono::system_clock::now() - 1h;
    auto entries = sink_->getEntriesSince(since);
    EXPECT_TRUE(entries.empty());
}

TEST_F(RingBufferSinkTest, GetEntriesSinceAll) {
    auto before = std::chrono::system_clock::now() - 1h;

    for (int i = 0; i < 5; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto entries = sink_->getEntriesSince(before);
    EXPECT_EQ(entries.size(), 5);
}

TEST_F(RingBufferSinkTest, GetEntriesSinceNone) {
    for (int i = 0; i < 5; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto future = std::chrono::system_clock::now() + 1h;
    auto entries = sink_->getEntriesSince(future);
    EXPECT_TRUE(entries.empty());
}

TEST_F(RingBufferSinkTest, GetEntriesSincePartial) {
    for (int i = 0; i < 3; ++i) {
        logMessage(spdlog::level::info, "logger", "Old " + std::to_string(i));
    }

    auto middle = std::chrono::system_clock::now();
    std::this_thread::sleep_for(10ms);

    for (int i = 0; i < 3; ++i) {
        logMessage(spdlog::level::info, "logger", "New " + std::to_string(i));
    }

    auto entries = sink_->getEntriesSince(middle);
    EXPECT_GE(entries.size(), 3);  // At least the new ones
}

// ============================================================================
// GetEntriesFiltered Tests
// ============================================================================

TEST_F(RingBufferSinkTest, GetEntriesFilteredByLevel) {
    logMessage(spdlog::level::debug, "logger", "Debug");
    logMessage(spdlog::level::info, "logger", "Info");
    logMessage(spdlog::level::warn, "logger", "Warn");
    logMessage(spdlog::level::err, "logger", "Error");

    auto entries =
        sink_->getEntriesFiltered(spdlog::level::warn, std::nullopt, 100);

    // Should only get warn and above
    for (const auto& entry : entries) {
        EXPECT_GE(entry.level, spdlog::level::warn);
    }
}

TEST_F(RingBufferSinkTest, GetEntriesFilteredByLogger) {
    logMessage(spdlog::level::info, "logger_a", "Message A1");
    logMessage(spdlog::level::info, "logger_b", "Message B1");
    logMessage(spdlog::level::info, "logger_a", "Message A2");
    logMessage(spdlog::level::info, "logger_c", "Message C1");

    auto entries =
        sink_->getEntriesFiltered(std::nullopt, std::string("logger_a"), 100);

    EXPECT_EQ(entries.size(), 2);
    for (const auto& entry : entries) {
        EXPECT_TRUE(entry.logger_name.find("logger_a") != std::string::npos);
    }
}

TEST_F(RingBufferSinkTest, GetEntriesFilteredByBoth) {
    logMessage(spdlog::level::debug, "target", "Debug target");
    logMessage(spdlog::level::info, "target", "Info target");
    logMessage(spdlog::level::warn, "target", "Warn target");
    logMessage(spdlog::level::warn, "other", "Warn other");

    auto entries = sink_->getEntriesFiltered(spdlog::level::warn,
                                             std::string("target"), 100);

    EXPECT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].message, "Warn target");
}

TEST_F(RingBufferSinkTest, GetEntriesFilteredWithLimit) {
    for (int i = 0; i < 10; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    auto entries = sink_->getEntriesFiltered(std::nullopt, std::nullopt, 5);

    EXPECT_EQ(entries.size(), 5);
}

TEST_F(RingBufferSinkTest, GetEntriesFilteredNoMatch) {
    logMessage(spdlog::level::info, "logger", "Info message");

    auto entries =
        sink_->getEntriesFiltered(spdlog::level::err, std::nullopt, 100);

    EXPECT_TRUE(entries.empty());
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(RingBufferSinkTest, ClearEmpty) {
    EXPECT_NO_THROW(sink_->clear());
    EXPECT_EQ(sink_->size(), 0);
}

TEST_F(RingBufferSinkTest, ClearWithEntries) {
    for (int i = 0; i < 10; ++i) {
        logMessage(spdlog::level::info, "logger", "Message");
    }

    EXPECT_EQ(sink_->size(), 10);

    sink_->clear();

    EXPECT_EQ(sink_->size(), 0);
    EXPECT_TRUE(sink_->getEntries().empty());
}

TEST_F(RingBufferSinkTest, ClearAndReuse) {
    for (int i = 0; i < 5; ++i) {
        logMessage(spdlog::level::info, "logger", "Old " + std::to_string(i));
    }

    sink_->clear();

    for (int i = 0; i < 3; ++i) {
        logMessage(spdlog::level::info, "logger", "New " + std::to_string(i));
    }

    EXPECT_EQ(sink_->size(), 3);

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].message, "New 0");
}

// ============================================================================
// Size and Capacity Tests
// ============================================================================

TEST_F(RingBufferSinkTest, SizeEmpty) {
    EXPECT_EQ(sink_->size(), 0);
}

TEST_F(RingBufferSinkTest, SizeAfterLogging) {
    for (int i = 0; i < 50; ++i) {
        logMessage(spdlog::level::info, "logger", "Message");
        EXPECT_EQ(sink_->size(), i + 1);
    }
}

TEST_F(RingBufferSinkTest, SizeAtCapacity) {
    for (int i = 0; i < 150; ++i) {
        logMessage(spdlog::level::info, "logger", "Message");
    }

    EXPECT_EQ(sink_->size(), 100);  // Capped at capacity
}

TEST_F(RingBufferSinkTest, CapacityConstant) {
    EXPECT_EQ(sink_->capacity(), 100);

    for (int i = 0; i < 200; ++i) {
        logMessage(spdlog::level::info, "logger", "Message");
    }

    EXPECT_EQ(sink_->capacity(), 100);  // Unchanged
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(RingBufferSinkTest, AddCallback) {
    std::atomic<int> call_count{0};

    sink_->addCallback("test_callback", [&call_count](const LogEntry&) {
        ++call_count;
    });

    logMessage(spdlog::level::info, "logger", "Test");

    EXPECT_EQ(call_count.load(), 1);
}

TEST_F(RingBufferSinkTest, AddMultipleCallbacks) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    sink_->addCallback("callback1", [&count1](const LogEntry&) { ++count1; });
    sink_->addCallback("callback2", [&count2](const LogEntry&) { ++count2; });

    logMessage(spdlog::level::info, "logger", "Test");

    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);
}

TEST_F(RingBufferSinkTest, RemoveCallback) {
    std::atomic<int> call_count{0};

    sink_->addCallback("removable", [&call_count](const LogEntry&) {
        ++call_count;
    });

    logMessage(spdlog::level::info, "logger", "Before remove");
    EXPECT_EQ(call_count.load(), 1);

    sink_->removeCallback("removable");

    logMessage(spdlog::level::info, "logger", "After remove");
    EXPECT_EQ(call_count.load(), 1);  // No increase
}

TEST_F(RingBufferSinkTest, RemoveNonExistentCallback) {
    EXPECT_NO_THROW(sink_->removeCallback("nonexistent"));
}

TEST_F(RingBufferSinkTest, HasCallback) {
    EXPECT_FALSE(sink_->hasCallback("test"));

    sink_->addCallback("test", [](const LogEntry&) {});

    EXPECT_TRUE(sink_->hasCallback("test"));

    sink_->removeCallback("test");

    EXPECT_FALSE(sink_->hasCallback("test"));
}

TEST_F(RingBufferSinkTest, CallbackCount) {
    EXPECT_EQ(sink_->callbackCount(), 0);

    sink_->addCallback("cb1", [](const LogEntry&) {});
    EXPECT_EQ(sink_->callbackCount(), 1);

    sink_->addCallback("cb2", [](const LogEntry&) {});
    EXPECT_EQ(sink_->callbackCount(), 2);

    sink_->removeCallback("cb1");
    EXPECT_EQ(sink_->callbackCount(), 1);
}

TEST_F(RingBufferSinkTest, CallbackReceivesCorrectData) {
    LogEntry received_entry;
    std::mutex mtx;

    sink_->addCallback("data_check", [&received_entry, &mtx](const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(mtx);
        received_entry = entry;
    });

    logMessage(spdlog::level::warn, "my_logger", "Test message content");

    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_EQ(received_entry.level, spdlog::level::warn);
    EXPECT_EQ(received_entry.logger_name, "my_logger");
    EXPECT_EQ(received_entry.message, "Test message content");
}

TEST_F(RingBufferSinkTest, CallbackExceptionHandled) {
    sink_->addCallback("throwing", [](const LogEntry&) {
        throw std::runtime_error("Test exception");
    });

    // Should not throw
    EXPECT_NO_THROW(logMessage(spdlog::level::info, "logger", "Test"));

    // Buffer should still work
    EXPECT_EQ(sink_->size(), 1);
}

TEST_F(RingBufferSinkTest, ReplaceCallback) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    sink_->addCallback("same_id", [&count1](const LogEntry&) { ++count1; });

    logMessage(spdlog::level::info, "logger", "First");
    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 0);

    // Replace with new callback
    sink_->addCallback("same_id", [&count2](const LogEntry&) { ++count2; });

    logMessage(spdlog::level::info, "logger", "Second");
    EXPECT_EQ(count1.load(), 1);  // Old callback not called
    EXPECT_EQ(count2.load(), 1);  // New callback called
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(RingBufferSinkTest, ConcurrentLogging) {
    std::vector<std::thread> threads;
    std::atomic<int> logged_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, i, &logged_count]() {
            for (int j = 0; j < 100; ++j) {
                logMessage(spdlog::level::info, "thread_" + std::to_string(i),
                           "Message " + std::to_string(j));
                ++logged_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(logged_count.load(), 1000);
    EXPECT_EQ(sink_->size(), 100);  // Capped at capacity
}

TEST_F(RingBufferSinkTest, ConcurrentCallbacks) {
    std::atomic<int> callback_count{0};

    sink_->addCallback("concurrent", [&callback_count](const LogEntry&) {
        ++callback_count;
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                logMessage(spdlog::level::info, "logger", "Message");
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(callback_count.load(), 1000);
}

TEST_F(RingBufferSinkTest, ConcurrentReading) {
    // Pre-populate
    for (int i = 0; i < 50; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
    }

    std::vector<std::thread> threads;
    std::atomic<int> read_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &read_count]() {
            for (int j = 0; j < 100; ++j) {
                auto entries = sink_->getEntries();
                if (!entries.empty()) {
                    ++read_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(read_count.load(), 1000);
}

TEST_F(RingBufferSinkTest, ConcurrentReadWrite) {
    std::vector<std::thread> threads;
    std::atomic<bool> stop{false};

    // Writer threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &stop]() {
            int count = 0;
            while (!stop && count < 200) {
                logMessage(spdlog::level::info, "writer", "Message");
                ++count;
            }
        });
    }

    // Reader threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &stop]() {
            while (!stop) {
                sink_->getEntries();
                sink_->size();
            }
        });
    }

    std::this_thread::sleep_for(100ms);
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash and buffer should be valid
    EXPECT_LE(sink_->size(), sink_->capacity());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(RingBufferSinkTest, EmptyLoggerName) {
    logMessage(spdlog::level::info, "", "Message");

    auto entries = sink_->getEntries();
    EXPECT_TRUE(entries[0].logger_name.empty());
}

TEST_F(RingBufferSinkTest, SpecialCharactersInLoggerName) {
    logMessage(spdlog::level::info, "logger.with.dots", "Message");

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].logger_name, "logger.with.dots");
}

TEST_F(RingBufferSinkTest, UnicodeLoggerName) {
    logMessage(spdlog::level::info, "日志记录器", "Message");

    auto entries = sink_->getEntries();
    EXPECT_EQ(entries[0].logger_name, "日志记录器");
}

TEST_F(RingBufferSinkTest, TimestampOrdering) {
    for (int i = 0; i < 5; ++i) {
        logMessage(spdlog::level::info, "logger", "Message " + std::to_string(i));
        std::this_thread::sleep_for(1ms);
    }

    auto entries = sink_->getEntries();

    for (size_t i = 1; i < entries.size(); ++i) {
        EXPECT_GE(entries[i].timestamp, entries[i - 1].timestamp);
    }
}

TEST_F(RingBufferSinkTest, FlushDoesNothing) {
    logMessage(spdlog::level::info, "logger", "Message");

    EXPECT_NO_THROW(sink_->flush());

    // Data should still be there
    EXPECT_EQ(sink_->size(), 1);
}
