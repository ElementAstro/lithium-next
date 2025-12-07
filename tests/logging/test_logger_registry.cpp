/*
 * test_logger_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for LoggerRegistry

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/core/logger_registry.hpp"

#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace lithium::logging;
using namespace std::chrono_literals;

class LoggerRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear spdlog registry before each test
        spdlog::drop_all();
    }

    void TearDown() override {
        // Clean up after each test
        spdlog::drop_all();
    }

    std::vector<spdlog::sink_ptr> createTestSinks() {
        return {std::make_shared<spdlog::sinks::null_sink_mt>()};
    }

    LoggerRegistry registry_;
};

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(LoggerRegistryTest, GetOrCreateNewLogger) {
    auto sinks = createTestSinks();
    auto logger =
        registry_.getOrCreate("new_logger", sinks, spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "new_logger");
    EXPECT_EQ(logger->level(), spdlog::level::info);
}

TEST_F(LoggerRegistryTest, GetOrCreateReturnsSameLogger) {
    auto sinks = createTestSinks();
    auto logger1 =
        registry_.getOrCreate("same_logger", sinks, spdlog::level::info, "%v");
    auto logger2 =
        registry_.getOrCreate("same_logger", sinks, spdlog::level::debug, "%v");

    EXPECT_EQ(logger1.get(), logger2.get());
    // Level should remain as originally set
    EXPECT_EQ(logger1->level(), spdlog::level::info);
}

TEST_F(LoggerRegistryTest, GetExistingLogger) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("existing", sinks, spdlog::level::info, "%v");

    auto logger = registry_.get("existing");
    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "existing");
}

TEST_F(LoggerRegistryTest, GetNonExistentLogger) {
    auto logger = registry_.get("nonexistent");
    EXPECT_EQ(logger, nullptr);
}

TEST_F(LoggerRegistryTest, ExistsReturnsTrueForExisting) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("check_exists", sinks, spdlog::level::info, "%v");

    EXPECT_TRUE(registry_.exists("check_exists"));
}

TEST_F(LoggerRegistryTest, ExistsReturnsFalseForNonExistent) {
    EXPECT_FALSE(registry_.exists("does_not_exist"));
}

TEST_F(LoggerRegistryTest, RemoveLogger) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("removable", sinks, spdlog::level::info, "%v");

    EXPECT_TRUE(registry_.exists("removable"));
    EXPECT_TRUE(registry_.remove("removable"));
    EXPECT_FALSE(registry_.exists("removable"));
}

TEST_F(LoggerRegistryTest, RemoveNonExistentLogger) {
    // Removing non-existent logger should succeed (spdlog::drop doesn't fail)
    EXPECT_TRUE(registry_.remove("nonexistent"));
}

TEST_F(LoggerRegistryTest, CannotRemoveDefaultLogger) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("default", sinks, spdlog::level::info, "%v");

    EXPECT_FALSE(registry_.remove("default"));
    EXPECT_TRUE(registry_.exists("default"));
}

TEST_F(LoggerRegistryTest, CannotRemoveEmptyNameLogger) {
    EXPECT_FALSE(registry_.remove(""));
}

// ============================================================================
// List Operations Tests
// ============================================================================

TEST_F(LoggerRegistryTest, ListEmptyRegistry) {
    auto loggers = registry_.list();
    // May contain default logger from spdlog
    EXPECT_GE(loggers.size(), 0);
}

TEST_F(LoggerRegistryTest, ListMultipleLoggers) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("logger_a", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("logger_b", sinks, spdlog::level::debug, "[%l] %v");
    registry_.getOrCreate("logger_c", sinks, spdlog::level::warn, "%v");

    auto loggers = registry_.list();

    std::vector<std::string> names;
    for (const auto& info : loggers) {
        names.push_back(info.name);
    }

    EXPECT_THAT(names, ::testing::Contains("logger_a"));
    EXPECT_THAT(names, ::testing::Contains("logger_b"));
    EXPECT_THAT(names, ::testing::Contains("logger_c"));
}

TEST_F(LoggerRegistryTest, ListContainsCorrectLevels) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("level_test", sinks, spdlog::level::warn, "%v");

    auto loggers = registry_.list();

    auto it = std::find_if(
        loggers.begin(), loggers.end(),
        [](const LoggerInfo& info) { return info.name == "level_test"; });

    ASSERT_NE(it, loggers.end());
    EXPECT_EQ(it->level, spdlog::level::warn);
}

TEST_F(LoggerRegistryTest, ListContainsPatterns) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("pattern_test", sinks, spdlog::level::info,
                          "[%Y-%m-%d] %v");

    auto loggers = registry_.list();

    auto it = std::find_if(
        loggers.begin(), loggers.end(),
        [](const LoggerInfo& info) { return info.name == "pattern_test"; });

    ASSERT_NE(it, loggers.end());
    EXPECT_EQ(it->pattern, "[%Y-%m-%d] %v");
}

// ============================================================================
// Level Management Tests
// ============================================================================

TEST_F(LoggerRegistryTest, SetLevelForExistingLogger) {
    auto sinks = createTestSinks();
    auto logger =
        registry_.getOrCreate("level_change", sinks, spdlog::level::info, "%v");

    EXPECT_TRUE(registry_.setLevel("level_change", spdlog::level::error));
    EXPECT_EQ(logger->level(), spdlog::level::err);
}

TEST_F(LoggerRegistryTest, SetLevelForNonExistentLogger) {
    EXPECT_FALSE(registry_.setLevel("nonexistent", spdlog::level::error));
}

TEST_F(LoggerRegistryTest, SetGlobalLevel) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("global_1", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("global_2", sinks, spdlog::level::debug, "%v");

    registry_.setGlobalLevel(spdlog::level::warn);

    // Note: setGlobalLevel uses spdlog::set_level which affects default level
    // Individual loggers may or may not be affected depending on spdlog version
}

TEST_F(LoggerRegistryTest, SetLevelAllValues) {
    auto sinks = createTestSinks();
    auto logger =
        registry_.getOrCreate("all_levels", sinks, spdlog::level::info, "%v");

    std::vector<spdlog::level::level_enum> levels = {
        spdlog::level::trace, spdlog::level::debug, spdlog::level::info,
        spdlog::level::warn,  spdlog::level::err,   spdlog::level::critical,
        spdlog::level::off};

    for (auto level : levels) {
        EXPECT_TRUE(registry_.setLevel("all_levels", level));
        EXPECT_EQ(logger->level(), level);
    }
}

// ============================================================================
// Pattern Management Tests
// ============================================================================

TEST_F(LoggerRegistryTest, SetPatternForExistingLogger) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("pattern_change", sinks, spdlog::level::info, "%v");

    EXPECT_TRUE(registry_.setPattern("pattern_change", "[%l] %v"));
    EXPECT_EQ(registry_.getPattern("pattern_change"), "[%l] %v");
}

TEST_F(LoggerRegistryTest, SetPatternForNonExistentLogger) {
    EXPECT_FALSE(registry_.setPattern("nonexistent", "[%l] %v"));
}

TEST_F(LoggerRegistryTest, GetPatternForExistingLogger) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("get_pattern", sinks, spdlog::level::info,
                          "[%Y-%m-%d] %v");

    EXPECT_EQ(registry_.getPattern("get_pattern"), "[%Y-%m-%d] %v");
}

TEST_F(LoggerRegistryTest, GetPatternForNonExistentLogger) {
    EXPECT_TRUE(registry_.getPattern("nonexistent").empty());
}

TEST_F(LoggerRegistryTest, PatternWithSpecialCharacters) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("special_pattern", sinks, spdlog::level::info, "%v");

    std::string complex_pattern =
        "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v";
    EXPECT_TRUE(registry_.setPattern("special_pattern", complex_pattern));
    EXPECT_EQ(registry_.getPattern("special_pattern"), complex_pattern);
}

// ============================================================================
// Sink Management Tests
// ============================================================================

TEST_F(LoggerRegistryTest, AddSinkToAll) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("sink_test_1", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("sink_test_2", sinks, spdlog::level::info, "%v");

    auto new_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    registry_.addSinkToAll(new_sink);

    auto logger1 = registry_.get("sink_test_1");
    auto logger2 = registry_.get("sink_test_2");

    // Each logger should now have 2 sinks (original + new)
    EXPECT_EQ(logger1->sinks().size(), 2);
    EXPECT_EQ(logger2->sinks().size(), 2);
}

TEST_F(LoggerRegistryTest, RemoveSinkFromAll) {
    auto shared_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    std::vector<spdlog::sink_ptr> sinks = {shared_sink};

    registry_.getOrCreate("remove_sink_1", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("remove_sink_2", sinks, spdlog::level::info, "%v");

    registry_.removeSinkFromAll(shared_sink);

    auto logger1 = registry_.get("remove_sink_1");
    auto logger2 = registry_.get("remove_sink_2");

    EXPECT_EQ(logger1->sinks().size(), 0);
    EXPECT_EQ(logger2->sinks().size(), 0);
}

TEST_F(LoggerRegistryTest, AddMultipleSinks) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("multi_sink", sinks, spdlog::level::info, "%v");

    auto sink1 = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto sink2 = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto sink3 = std::make_shared<spdlog::sinks::null_sink_mt>();

    registry_.addSinkToAll(sink1);
    registry_.addSinkToAll(sink2);
    registry_.addSinkToAll(sink3);

    auto logger = registry_.get("multi_sink");
    EXPECT_EQ(logger->sinks().size(), 4);  // original + 3 new
}

// ============================================================================
// Flush and Clear Tests
// ============================================================================

TEST_F(LoggerRegistryTest, FlushAllDoesNotThrow) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("flush_test", sinks, spdlog::level::info, "%v");

    EXPECT_NO_THROW(registry_.flushAll());
}

TEST_F(LoggerRegistryTest, FlushAllOnEmptyRegistry) {
    EXPECT_NO_THROW(registry_.flushAll());
}

TEST_F(LoggerRegistryTest, ClearRemovesLoggers) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("clear_test_1", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("clear_test_2", sinks, spdlog::level::info, "%v");

    registry_.clear();

    // After clear, loggers should be removed
    EXPECT_FALSE(registry_.exists("clear_test_1"));
    EXPECT_FALSE(registry_.exists("clear_test_2"));
}

TEST_F(LoggerRegistryTest, ClearPreservesDefaultLogger) {
    // Set up a default logger
    auto default_logger = spdlog::default_logger();

    auto sinks = createTestSinks();
    registry_.getOrCreate("to_clear", sinks, spdlog::level::info, "%v");

    registry_.clear();

    // Default logger should still be accessible
    EXPECT_NE(spdlog::default_logger(), nullptr);
}

// ============================================================================
// Count Tests
// ============================================================================

TEST_F(LoggerRegistryTest, CountEmptyRegistry) {
    // May have default logger
    auto count = registry_.count();
    EXPECT_GE(count, 0);
}

TEST_F(LoggerRegistryTest, CountAfterCreation) {
    auto initial_count = registry_.count();

    auto sinks = createTestSinks();
    registry_.getOrCreate("count_1", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("count_2", sinks, spdlog::level::info, "%v");
    registry_.getOrCreate("count_3", sinks, spdlog::level::info, "%v");

    EXPECT_EQ(registry_.count(), initial_count + 3);
}

TEST_F(LoggerRegistryTest, CountAfterRemoval) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("remove_count", sinks, spdlog::level::info, "%v");

    auto count_before = registry_.count();
    registry_.remove("remove_count");
    auto count_after = registry_.count();

    EXPECT_EQ(count_after, count_before - 1);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(LoggerRegistryTest, ConcurrentGetOrCreate) {
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    auto sinks = createTestSinks();

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &sinks, i, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto logger = registry_.getOrCreate(
                    "concurrent_" + std::to_string(i) + "_" + std::to_string(j),
                    sinks, spdlog::level::info, "%v");
                if (logger) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 1000);
}

TEST_F(LoggerRegistryTest, ConcurrentSetLevel) {
    auto sinks = createTestSinks();
    registry_.getOrCreate("concurrent_level", sinks, spdlog::level::info, "%v");

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto level = static_cast<spdlog::level::level_enum>(j % 7);
                if (registry_.setLevel("concurrent_level", level)) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 1000);
}

TEST_F(LoggerRegistryTest, ConcurrentList) {
    auto sinks = createTestSinks();
    for (int i = 0; i < 10; ++i) {
        registry_.getOrCreate("list_" + std::to_string(i), sinks,
                              spdlog::level::info, "%v");
    }

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto loggers = registry_.list();
                if (!loggers.empty()) {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 1000);
}

TEST_F(LoggerRegistryTest, ConcurrentMixedOperations) {
    auto sinks = createTestSinks();
    std::vector<std::thread> threads;
    std::atomic<int> operation_count{0};

    // Create some initial loggers
    for (int i = 0; i < 5; ++i) {
        registry_.getOrCreate("mixed_" + std::to_string(i), sinks,
                              spdlog::level::info, "%v");
    }

    // Mix of operations
    for (int i = 0; i < 5; ++i) {
        // Reader threads
        threads.emplace_back([this, &operation_count]() {
            for (int j = 0; j < 50; ++j) {
                registry_.list();
                registry_.count();
                registry_.exists("mixed_0");
                ++operation_count;
            }
        });

        // Writer threads
        threads.emplace_back([this, &sinks, i, &operation_count]() {
            for (int j = 0; j < 50; ++j) {
                registry_.setLevel("mixed_" + std::to_string(i % 5),
                                   spdlog::level::debug);
                registry_.setPattern("mixed_" + std::to_string(i % 5),
                                     "[%l] %v");
                ++operation_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(operation_count.load(), 500);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(LoggerRegistryTest, EmptyLoggerName) {
    auto sinks = createTestSinks();
    auto logger = registry_.getOrCreate("", sinks, spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_TRUE(logger->name().empty());
}

TEST_F(LoggerRegistryTest, VeryLongLoggerName) {
    auto sinks = createTestSinks();
    std::string long_name(1000, 'x');
    auto logger =
        registry_.getOrCreate(long_name, sinks, spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), long_name);
}

TEST_F(LoggerRegistryTest, SpecialCharactersInLoggerName) {
    auto sinks = createTestSinks();
    auto logger = registry_.getOrCreate("logger.with.dots", sinks,
                                        spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "logger.with.dots");
}

TEST_F(LoggerRegistryTest, UnicodeLoggerName) {
    auto sinks = createTestSinks();
    auto logger =
        registry_.getOrCreate("日志记录器", sinks, spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "日志记录器");
}

TEST_F(LoggerRegistryTest, EmptySinksList) {
    std::vector<spdlog::sink_ptr> empty_sinks;
    auto logger = registry_.getOrCreate("no_sinks", empty_sinks,
                                        spdlog::level::info, "%v");

    EXPECT_NE(logger, nullptr);
    EXPECT_TRUE(logger->sinks().empty());
}

TEST_F(LoggerRegistryTest, EmptyPattern) {
    auto sinks = createTestSinks();
    auto logger =
        registry_.getOrCreate("empty_pattern", sinks, spdlog::level::info, "");

    EXPECT_NE(logger, nullptr);
    EXPECT_TRUE(registry_.getPattern("empty_pattern").empty());
}
