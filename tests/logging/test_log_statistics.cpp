/*
 * test_log_statistics.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for LogStatistics

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/log_statistics.hpp"
#include "logging/types.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace lithium::logging;
using namespace std::chrono_literals;

class LogStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset statistics before each test
        LogStatistics::getInstance().reset();
    }

    void TearDown() override {
        // Clean up after each test
        LogStatistics::getInstance().reset();
    }
};

// ============================================================================
// Singleton Tests
// ============================================================================

TEST_F(LogStatisticsTest, SingletonInstance) {
    auto& instance1 = LogStatistics::getInstance();
    auto& instance2 = LogStatistics::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

// ============================================================================
// Basic Recording Tests
// ============================================================================

TEST_F(LogStatisticsTest, RecordSingleMessage) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "test_logger", 100);

    EXPECT_EQ(stats.getTotalMessages(), 1);
    EXPECT_EQ(stats.getTotalBytes(), 100);
}

TEST_F(LogStatisticsTest, RecordMultipleMessages) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger1", 50);
    stats.recordMessage(spdlog::level::debug, "logger2", 75);
    stats.recordMessage(spdlog::level::warn, "logger1", 100);

    EXPECT_EQ(stats.getTotalMessages(), 3);
    EXPECT_EQ(stats.getTotalBytes(), 225);
}

TEST_F(LogStatisticsTest, RecordAllLevels) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::trace, "logger", 10);
    stats.recordMessage(spdlog::level::debug, "logger", 20);
    stats.recordMessage(spdlog::level::info, "logger", 30);
    stats.recordMessage(spdlog::level::warn, "logger", 40);
    stats.recordMessage(spdlog::level::err, "logger", 50);
    stats.recordMessage(spdlog::level::critical, "logger", 60);

    EXPECT_EQ(stats.getTotalMessages(), 6);
    EXPECT_EQ(stats.getTotalBytes(), 210);
}

TEST_F(LogStatisticsTest, RecordZeroSizeMessage) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger", 0);

    EXPECT_EQ(stats.getTotalMessages(), 1);
    EXPECT_EQ(stats.getTotalBytes(), 0);
}

TEST_F(LogStatisticsTest, RecordLargeMessage) {
    auto& stats = LogStatistics::getInstance();

    size_t large_size = 1024 * 1024;  // 1MB
    stats.recordMessage(spdlog::level::info, "logger", large_size);

    EXPECT_EQ(stats.getTotalMessages(), 1);
    EXPECT_EQ(stats.getTotalBytes(), large_size);
}

// ============================================================================
// Level Statistics Tests
// ============================================================================

TEST_F(LogStatisticsTest, GetLevelStatsEmpty) {
    auto& stats = LogStatistics::getInstance();
    auto level_stats = stats.getLevelStats();

    EXPECT_TRUE(level_stats.contains("trace"));
    EXPECT_TRUE(level_stats.contains("debug"));
    EXPECT_TRUE(level_stats.contains("info"));
    EXPECT_TRUE(level_stats.contains("warn"));
    EXPECT_TRUE(level_stats.contains("error"));
    EXPECT_TRUE(level_stats.contains("critical"));
    EXPECT_TRUE(level_stats.contains("off"));
}

TEST_F(LogStatisticsTest, GetLevelStatsAfterRecording) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger", 100);
    stats.recordMessage(spdlog::level::info, "logger", 150);
    stats.recordMessage(spdlog::level::err, "logger", 200);

    auto level_stats = stats.getLevelStats();

    EXPECT_EQ(level_stats["info"]["count"].get<uint64_t>(), 2);
    EXPECT_EQ(level_stats["info"]["total_bytes"].get<uint64_t>(), 250);
    EXPECT_EQ(level_stats["error"]["count"].get<uint64_t>(), 1);
    EXPECT_EQ(level_stats["error"]["total_bytes"].get<uint64_t>(), 200);
}

TEST_F(LogStatisticsTest, LevelStatsLastOccurrence) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::warn, "logger", 50);

    auto level_stats = stats.getLevelStats();
    std::string last_occurrence =
        level_stats["warn"]["last_occurrence"].get<std::string>();

    // Should have a valid timestamp
    EXPECT_FALSE(last_occurrence.empty());
    EXPECT_TRUE(last_occurrence.find('T') != std::string::npos);
}

// ============================================================================
// Logger Statistics Tests
// ============================================================================

TEST_F(LogStatisticsTest, GetLoggerStatsEmpty) {
    auto& stats = LogStatistics::getInstance();
    auto logger_stats = stats.getLoggerStats();

    EXPECT_TRUE(logger_stats.is_array());
    EXPECT_TRUE(logger_stats.empty());
}

TEST_F(LogStatisticsTest, GetLoggerStatsAfterRecording) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger_a", 100);
    stats.recordMessage(spdlog::level::debug, "logger_a", 50);
    stats.recordMessage(spdlog::level::warn, "logger_b", 200);

    auto logger_stats = stats.getLoggerStats();

    EXPECT_EQ(logger_stats.size(), 2);

    // Find logger_a stats
    bool found_a = false;
    for (const auto& ls : logger_stats) {
        if (ls["name"] == "logger_a") {
            found_a = true;
            EXPECT_EQ(ls["total_messages"].get<uint64_t>(), 2);
            EXPECT_EQ(ls["total_bytes"].get<uint64_t>(), 150);
        }
    }
    EXPECT_TRUE(found_a);
}

TEST_F(LogStatisticsTest, LoggerStatsLevelCounts) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "test_logger", 10);
    stats.recordMessage(spdlog::level::info, "test_logger", 20);
    stats.recordMessage(spdlog::level::err, "test_logger", 30);

    auto logger_stats = stats.getLoggerStats();

    for (const auto& ls : logger_stats) {
        if (ls["name"] == "test_logger") {
            auto level_counts = ls["level_counts"];
            EXPECT_EQ(level_counts["info"].get<uint64_t>(), 2);
            EXPECT_EQ(level_counts["error"].get<uint64_t>(), 1);
        }
    }
}

// ============================================================================
// Summary Statistics Tests
// ============================================================================

TEST_F(LogStatisticsTest, GetSummaryEmpty) {
    auto& stats = LogStatistics::getInstance();
    auto summary = stats.getSummary();

    EXPECT_TRUE(summary.contains("uptime_seconds"));
    EXPECT_TRUE(summary.contains("total_messages"));
    EXPECT_TRUE(summary.contains("total_bytes"));
    EXPECT_TRUE(summary.contains("message_rate_per_second"));
    EXPECT_TRUE(summary.contains("error_rate_per_minute"));
    EXPECT_TRUE(summary.contains("error_count"));
    EXPECT_TRUE(summary.contains("warning_count"));
    EXPECT_TRUE(summary.contains("critical_count"));
    EXPECT_TRUE(summary.contains("logger_count"));
}

TEST_F(LogStatisticsTest, GetSummaryAfterRecording) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger1", 100);
    stats.recordMessage(spdlog::level::warn, "logger2", 200);
    stats.recordMessage(spdlog::level::err, "logger1", 300);
    stats.recordMessage(spdlog::level::critical, "logger3", 400);

    auto summary = stats.getSummary();

    EXPECT_EQ(summary["total_messages"].get<uint64_t>(), 4);
    EXPECT_EQ(summary["total_bytes"].get<uint64_t>(), 1000);
    EXPECT_EQ(summary["warning_count"].get<uint64_t>(), 1);
    EXPECT_EQ(summary["error_count"].get<uint64_t>(), 1);
    EXPECT_EQ(summary["critical_count"].get<uint64_t>(), 1);
    EXPECT_EQ(summary["logger_count"].get<size_t>(), 3);
}

TEST_F(LogStatisticsTest, SummaryUptimeIncreases) {
    auto& stats = LogStatistics::getInstance();

    auto summary1 = stats.getSummary();
    auto uptime1 = summary1["uptime_seconds"].get<int64_t>();

    std::this_thread::sleep_for(100ms);

    auto summary2 = stats.getSummary();
    auto uptime2 = summary2["uptime_seconds"].get<int64_t>();

    EXPECT_GE(uptime2, uptime1);
}

// ============================================================================
// Rate Calculation Tests
// ============================================================================

TEST_F(LogStatisticsTest, GetMessageRateEmpty) {
    auto& stats = LogStatistics::getInstance();
    double rate = stats.getMessageRate();

    EXPECT_EQ(rate, 0.0);
}

TEST_F(LogStatisticsTest, GetMessageRateAfterRecording) {
    auto& stats = LogStatistics::getInstance();

    // Record some messages
    for (int i = 0; i < 10; ++i) {
        stats.recordMessage(spdlog::level::info, "logger", 50);
    }

    double rate = stats.getMessageRate(60);

    // Rate should be positive
    EXPECT_GT(rate, 0.0);
}

TEST_F(LogStatisticsTest, GetErrorRateEmpty) {
    auto& stats = LogStatistics::getInstance();

    // Need some uptime for rate calculation
    std::this_thread::sleep_for(10ms);

    double rate = stats.getErrorRate();
    EXPECT_GE(rate, 0.0);
}

TEST_F(LogStatisticsTest, GetErrorRateAfterErrors) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::err, "logger", 100);
    stats.recordMessage(spdlog::level::critical, "logger", 100);

    // Need some uptime
    std::this_thread::sleep_for(10ms);

    double rate = stats.getErrorRate();
    EXPECT_GT(rate, 0.0);
}

// ============================================================================
// Reset Tests
// ============================================================================

TEST_F(LogStatisticsTest, ResetClearsMessages) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger", 100);
    stats.recordMessage(spdlog::level::err, "logger", 200);

    EXPECT_EQ(stats.getTotalMessages(), 2);

    stats.reset();

    EXPECT_EQ(stats.getTotalMessages(), 0);
    EXPECT_EQ(stats.getTotalBytes(), 0);
}

TEST_F(LogStatisticsTest, ResetClearsLoggerStats) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "logger1", 100);
    stats.recordMessage(spdlog::level::info, "logger2", 100);

    stats.reset();

    auto logger_stats = stats.getLoggerStats();
    EXPECT_TRUE(logger_stats.empty());
}

TEST_F(LogStatisticsTest, ResetResetsUptime) {
    auto& stats = LogStatistics::getInstance();

    std::this_thread::sleep_for(100ms);
    auto uptime_before = stats.getUptime();

    stats.reset();

    auto uptime_after = stats.getUptime();
    EXPECT_LT(uptime_after.count(), uptime_before.count());
}

// ============================================================================
// Uptime Tests
// ============================================================================

TEST_F(LogStatisticsTest, GetUptimeInitial) {
    auto& stats = LogStatistics::getInstance();
    auto uptime = stats.getUptime();

    // Should be very small initially
    EXPECT_GE(uptime.count(), 0);
}

TEST_F(LogStatisticsTest, GetUptimeIncreases) {
    auto& stats = LogStatistics::getInstance();

    auto uptime1 = stats.getUptime();
    std::this_thread::sleep_for(100ms);
    auto uptime2 = stats.getUptime();

    EXPECT_GT(uptime2.count(), uptime1.count());
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(LogStatisticsTest, ConcurrentRecording) {
    auto& stats = LogStatistics::getInstance();
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&stats, i, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                auto level = static_cast<spdlog::level::level_enum>(j % 6);
                stats.recordMessage(level, "logger_" + std::to_string(i),
                                    50 + j);
            }
            ++success_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
    EXPECT_EQ(stats.getTotalMessages(), 1000);
}

TEST_F(LogStatisticsTest, ConcurrentReading) {
    auto& stats = LogStatistics::getInstance();

    // Pre-populate some data
    for (int i = 0; i < 100; ++i) {
        stats.recordMessage(spdlog::level::info, "logger", 50);
    }

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&stats, &success_count]() {
            for (int j = 0; j < 100; ++j) {
                stats.getSummary();
                stats.getLevelStats();
                stats.getLoggerStats();
                stats.getMessageRate();
                stats.getErrorRate();
            }
            ++success_count;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 10);
}

TEST_F(LogStatisticsTest, ConcurrentReadWrite) {
    auto& stats = LogStatistics::getInstance();
    std::vector<std::thread> threads;
    std::atomic<int> operation_count{0};

    // Writer threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&stats, &operation_count]() {
            for (int j = 0; j < 100; ++j) {
                stats.recordMessage(spdlog::level::info, "writer", 50);
                ++operation_count;
            }
        });
    }

    // Reader threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&stats, &operation_count]() {
            for (int j = 0; j < 100; ++j) {
                stats.getSummary();
                stats.getTotalMessages();
                ++operation_count;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(operation_count.load(), 1000);
}

// ============================================================================
// LogSearchQuery Tests
// ============================================================================

class LogSearchQueryTest : public ::testing::Test {};

TEST_F(LogSearchQueryTest, DefaultConstruction) {
    LogSearchQuery query;

    EXPECT_FALSE(query.text_pattern.has_value());
    EXPECT_FALSE(query.regex_pattern.has_value());
    EXPECT_FALSE(query.min_level.has_value());
    EXPECT_FALSE(query.max_level.has_value());
    EXPECT_FALSE(query.logger_name.has_value());
    EXPECT_FALSE(query.start_time.has_value());
    EXPECT_FALSE(query.end_time.has_value());
    EXPECT_EQ(query.limit, 100);
    EXPECT_EQ(query.offset, 0);
    EXPECT_FALSE(query.case_sensitive);
}

TEST_F(LogSearchQueryTest, FromJsonBasic) {
    nlohmann::json json = {{"text", "error"},
                           {"min_level", "warn"},
                           {"logger", "my_logger"},
                           {"limit", 50},
                           {"offset", 10},
                           {"case_sensitive", true}};

    auto query = LogSearchQuery::fromJson(json);

    EXPECT_TRUE(query.text_pattern.has_value());
    EXPECT_EQ(*query.text_pattern, "error");
    EXPECT_TRUE(query.min_level.has_value());
    EXPECT_EQ(*query.min_level, spdlog::level::warn);
    EXPECT_TRUE(query.logger_name.has_value());
    EXPECT_EQ(*query.logger_name, "my_logger");
    EXPECT_EQ(query.limit, 50);
    EXPECT_EQ(query.offset, 10);
    EXPECT_TRUE(query.case_sensitive);
}

TEST_F(LogSearchQueryTest, FromJsonWithRegex) {
    nlohmann::json json = {{"regex", "error.*failed"}};

    auto query = LogSearchQuery::fromJson(json);

    EXPECT_TRUE(query.regex_pattern.has_value());
    EXPECT_EQ(*query.regex_pattern, "error.*failed");
}

TEST_F(LogSearchQueryTest, FromJsonMissingFields) {
    nlohmann::json json = nlohmann::json::object();

    auto query = LogSearchQuery::fromJson(json);

    EXPECT_FALSE(query.text_pattern.has_value());
    EXPECT_EQ(query.limit, 100);  // Default
    EXPECT_EQ(query.offset, 0);   // Default
}

TEST_F(LogSearchQueryTest, ToJsonBasic) {
    LogSearchQuery query;
    query.text_pattern = "search_text";
    query.min_level = spdlog::level::info;
    query.limit = 200;
    query.case_sensitive = true;

    auto json = query.toJson();

    EXPECT_EQ(json["text"], "search_text");
    EXPECT_EQ(json["min_level"], "info");
    EXPECT_EQ(json["limit"], 200);
    EXPECT_TRUE(json["case_sensitive"].get<bool>());
}

TEST_F(LogSearchQueryTest, ToJsonOptionalFields) {
    LogSearchQuery query;
    // Leave optional fields empty

    auto json = query.toJson();

    EXPECT_FALSE(json.contains("text"));
    EXPECT_FALSE(json.contains("regex"));
    EXPECT_FALSE(json.contains("min_level"));
    EXPECT_TRUE(json.contains("limit"));
    EXPECT_TRUE(json.contains("offset"));
}

TEST_F(LogSearchQueryTest, RoundTripConversion) {
    LogSearchQuery original;
    original.text_pattern = "test";
    original.min_level = spdlog::level::debug;
    original.max_level = spdlog::level::err;
    original.logger_name = "my_logger";
    original.limit = 50;
    original.offset = 25;
    original.case_sensitive = true;

    auto json = original.toJson();
    auto restored = LogSearchQuery::fromJson(json);

    EXPECT_EQ(*restored.text_pattern, *original.text_pattern);
    EXPECT_EQ(*restored.min_level, *original.min_level);
    EXPECT_EQ(*restored.max_level, *original.max_level);
    EXPECT_EQ(*restored.logger_name, *original.logger_name);
    EXPECT_EQ(restored.limit, original.limit);
    EXPECT_EQ(restored.offset, original.offset);
    EXPECT_EQ(restored.case_sensitive, original.case_sensitive);
}

// ============================================================================
// LogSearchResult Tests
// ============================================================================

class LogSearchResultTest : public ::testing::Test {};

TEST_F(LogSearchResultTest, DefaultConstruction) {
    LogSearchResult result;

    EXPECT_TRUE(result.entries.empty());
    EXPECT_EQ(result.total_matches, 0);
    EXPECT_EQ(result.returned_count, 0);
    EXPECT_FALSE(result.has_more);
    EXPECT_EQ(result.search_time.count(), 0);
}

TEST_F(LogSearchResultTest, ToJsonEmpty) {
    LogSearchResult result;
    auto json = result.toJson();

    EXPECT_TRUE(json.contains("entries"));
    EXPECT_TRUE(json["entries"].is_array());
    EXPECT_TRUE(json["entries"].empty());
    EXPECT_EQ(json["total_matches"], 0);
    EXPECT_EQ(json["returned_count"], 0);
    EXPECT_FALSE(json["has_more"].get<bool>());
    EXPECT_EQ(json["search_time_ms"], 0);
}

TEST_F(LogSearchResultTest, ToJsonWithEntries) {
    LogSearchResult result;

    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = spdlog::level::info;
    entry.logger_name = "test";
    entry.message = "Test message";

    result.entries.push_back(entry);
    result.total_matches = 10;
    result.returned_count = 1;
    result.has_more = true;
    result.search_time = std::chrono::milliseconds(50);

    auto json = result.toJson();

    EXPECT_EQ(json["entries"].size(), 1);
    EXPECT_EQ(json["total_matches"], 10);
    EXPECT_EQ(json["returned_count"], 1);
    EXPECT_TRUE(json["has_more"].get<bool>());
    EXPECT_EQ(json["search_time_ms"], 50);
}

// ============================================================================
// LoggerStats Tests
// ============================================================================

class LoggerStatsTest : public ::testing::Test {};

TEST_F(LoggerStatsTest, ToJsonBasic) {
    LoggerStats stats;
    stats.name = "test_logger";
    stats.total_messages = 100;
    stats.total_bytes = 5000;
    stats.level_counts[spdlog::level::info] = 80;
    stats.level_counts[spdlog::level::err] = 20;
    stats.first_message = std::chrono::system_clock::now() - 1h;
    stats.last_message = std::chrono::system_clock::now();

    auto json = stats.toJson();

    EXPECT_EQ(json["name"], "test_logger");
    EXPECT_EQ(json["total_messages"], 100);
    EXPECT_EQ(json["total_bytes"], 5000);
    EXPECT_TRUE(json.contains("level_counts"));
    EXPECT_TRUE(json.contains("first_message"));
    EXPECT_TRUE(json.contains("last_message"));
}

TEST_F(LoggerStatsTest, ToJsonLevelCounts) {
    LoggerStats stats;
    stats.name = "level_test";
    stats.level_counts[spdlog::level::trace] = 10;
    stats.level_counts[spdlog::level::debug] = 20;
    stats.level_counts[spdlog::level::info] = 30;
    stats.level_counts[spdlog::level::warn] = 40;
    stats.level_counts[spdlog::level::err] = 50;
    stats.level_counts[spdlog::level::critical] = 60;

    auto json = stats.toJson();
    auto level_counts = json["level_counts"];

    EXPECT_EQ(level_counts["trace"], 10);
    EXPECT_EQ(level_counts["debug"], 20);
    EXPECT_EQ(level_counts["info"], 30);
    EXPECT_EQ(level_counts["warning"], 40);
    EXPECT_EQ(level_counts["error"], 50);
    EXPECT_EQ(level_counts["critical"], 60);
}

TEST_F(LoggerStatsTest, ToJsonEmptyTimestamps) {
    LoggerStats stats;
    stats.name = "empty_time";
    // Leave timestamps at default (epoch)

    auto json = stats.toJson();

    // Empty timestamps should result in empty strings
    EXPECT_TRUE(json["first_message"].get<std::string>().empty());
    EXPECT_TRUE(json["last_message"].get<std::string>().empty());
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(LogStatisticsTest, RecordEmptyLoggerName) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "", 100);

    EXPECT_EQ(stats.getTotalMessages(), 1);

    auto logger_stats = stats.getLoggerStats();
    EXPECT_EQ(logger_stats.size(), 1);
}

TEST_F(LogStatisticsTest, RecordVeryLongLoggerName) {
    auto& stats = LogStatistics::getInstance();

    std::string long_name(1000, 'x');
    stats.recordMessage(spdlog::level::info, long_name, 100);

    EXPECT_EQ(stats.getTotalMessages(), 1);
}

TEST_F(LogStatisticsTest, RecordUnicodeLoggerName) {
    auto& stats = LogStatistics::getInstance();

    stats.recordMessage(spdlog::level::info, "日志记录器", 100);

    EXPECT_EQ(stats.getTotalMessages(), 1);

    auto logger_stats = stats.getLoggerStats();
    bool found = false;
    for (const auto& ls : logger_stats) {
        if (ls["name"] == "日志记录器") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LogStatisticsTest, HighVolumeRecording) {
    auto& stats = LogStatistics::getInstance();

    const int count = 10000;
    for (int i = 0; i < count; ++i) {
        stats.recordMessage(spdlog::level::info, "high_volume", 50);
    }

    EXPECT_EQ(stats.getTotalMessages(), count);
    EXPECT_EQ(stats.getTotalBytes(), count * 50);
}
