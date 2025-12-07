/*
 * test_sink_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Comprehensive tests for SinkFactory

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "logging/core/types.hpp"
#include "logging/sinks/sink_factory.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace lithium::logging;

class SinkFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir_ =
            std::filesystem::temp_directory_path() / "sink_factory_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
};

// ============================================================================
// Console Sink Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateConsoleSink) {
    SinkConfig config;
    config.name = "console";
    config.type = "console";
    config.level = spdlog::level::info;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateConsoleSinkWithStdout) {
    SinkConfig config;
    config.name = "stdout";
    config.type = "stdout";
    config.level = spdlog::level::debug;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateConsoleSinkWithPattern) {
    SinkConfig config;
    config.name = "console";
    config.type = "console";
    config.level = spdlog::level::info;
    config.pattern = "[%l] %v";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateConsoleSinkAllLevels) {
    std::vector<spdlog::level::level_enum> levels = {
        spdlog::level::trace, spdlog::level::debug, spdlog::level::info,
        spdlog::level::warn,  spdlog::level::err,   spdlog::level::critical,
        spdlog::level::off};

    for (auto level : levels) {
        SinkConfig config;
        config.name = "console";
        config.type = "console";
        config.level = level;

        auto sink = SinkFactory::createSink(config);
        EXPECT_NE(sink, nullptr);
    }
}

TEST_F(SinkFactoryTest, CreateConsoleSinkDirect) {
    auto sink = SinkFactory::createConsoleSink(spdlog::level::info, "[%l] %v");

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateConsoleSinkDirectDefaultPattern) {
    auto sink = SinkFactory::createConsoleSink(spdlog::level::debug);

    EXPECT_NE(sink, nullptr);
}

// ============================================================================
// File Sink Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateFileSink) {
    auto file_path = test_dir_ / "test.log";

    SinkConfig config;
    config.name = "file";
    config.type = "file";
    config.level = spdlog::level::debug;
    config.file_path = file_path.string();

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
    EXPECT_TRUE(std::filesystem::exists(file_path));
}

TEST_F(SinkFactoryTest, CreateFileSinkWithBasicFileType) {
    auto file_path = test_dir_ / "basic.log";

    SinkConfig config;
    config.name = "basic";
    config.type = "basic_file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateFileSinkWithPattern) {
    auto file_path = test_dir_ / "pattern.log";

    SinkConfig config;
    config.name = "file";
    config.type = "file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();
    config.pattern = "[%Y-%m-%d %H:%M:%S] [%l] %v";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateFileSinkCreatesDirectory) {
    auto file_path = test_dir_ / "subdir" / "nested" / "test.log";

    SinkConfig config;
    config.name = "file";
    config.type = "file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
    EXPECT_TRUE(std::filesystem::exists(file_path.parent_path()));
}

TEST_F(SinkFactoryTest, CreateFileSinkDirect) {
    auto file_path = test_dir_ / "direct.log";

    auto sink = SinkFactory::createFileSink(file_path.string(),
                                            spdlog::level::debug, "[%l] %v");

    EXPECT_NE(sink, nullptr);
    EXPECT_TRUE(std::filesystem::exists(file_path));
}

TEST_F(SinkFactoryTest, CreateFileSinkDirectTruncate) {
    auto file_path = test_dir_ / "truncate.log";

    // Create file with content
    {
        std::ofstream file(file_path);
        file << "Existing content";
    }

    auto sink = SinkFactory::createFileSink(file_path.string(),
                                            spdlog::level::info, "", true);

    EXPECT_NE(sink, nullptr);

    // File should be truncated (empty or very small)
    auto size = std::filesystem::file_size(file_path);
    EXPECT_EQ(size, 0);
}

// ============================================================================
// Rotating File Sink Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateRotatingFileSink) {
    auto file_path = test_dir_ / "rotating.log";

    SinkConfig config;
    config.name = "rotating";
    config.type = "rotating_file";
    config.level = spdlog::level::debug;
    config.file_path = file_path.string();
    config.max_file_size = 1024 * 1024;  // 1MB
    config.max_files = 3;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateRotatingFileSinkWithPattern) {
    auto file_path = test_dir_ / "rotating_pattern.log";

    SinkConfig config;
    config.name = "rotating";
    config.type = "rotating_file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();
    config.max_file_size = 512 * 1024;
    config.max_files = 5;
    config.pattern = "[%Y-%m-%d] %v";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateRotatingFileSinkDirect) {
    auto file_path = test_dir_ / "rotating_direct.log";

    auto sink = SinkFactory::createRotatingFileSink(
        file_path.string(), 1024 * 1024, 3, spdlog::level::debug, "[%l] %v");

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateRotatingFileSinkSmallSize) {
    auto file_path = test_dir_ / "small_rotating.log";

    auto sink = SinkFactory::createRotatingFileSink(file_path.string(), 1024, 2,
                                                    spdlog::level::info);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateRotatingFileSinkCreatesDirectory) {
    auto file_path = test_dir_ / "nested" / "rotating.log";

    SinkConfig config;
    config.name = "rotating";
    config.type = "rotating_file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();
    config.max_file_size = 1024 * 1024;
    config.max_files = 3;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
    EXPECT_TRUE(std::filesystem::exists(file_path.parent_path()));
}

// ============================================================================
// Daily File Sink Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateDailyFileSink) {
    auto file_path = test_dir_ / "daily.log";

    SinkConfig config;
    config.name = "daily";
    config.type = "daily_file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();
    config.rotation_hour = 0;
    config.rotation_minute = 0;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateDailyFileSinkWithPattern) {
    auto file_path = test_dir_ / "daily_pattern.log";

    SinkConfig config;
    config.name = "daily";
    config.type = "daily_file";
    config.level = spdlog::level::debug;
    config.file_path = file_path.string();
    config.rotation_hour = 2;
    config.rotation_minute = 30;
    config.pattern = "[%H:%M:%S] %v";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateDailyFileSinkDirect) {
    auto file_path = test_dir_ / "daily_direct.log";

    auto sink = SinkFactory::createDailyFileSink(
        file_path.string(), 0, 0, spdlog::level::info, "[%l] %v");

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateDailyFileSinkVariousRotationTimes) {
    std::vector<std::pair<int, int>> times = {
        {0, 0}, {12, 0}, {23, 59}, {6, 30}, {18, 45}};

    for (const auto& [hour, minute] : times) {
        auto file_path = test_dir_ / ("daily_" + std::to_string(hour) + "_" +
                                      std::to_string(minute) + ".log");

        auto sink = SinkFactory::createDailyFileSink(
            file_path.string(), hour, minute, spdlog::level::info);

        EXPECT_NE(sink, nullptr)
            << "Failed for hour=" << hour << ", minute=" << minute;
    }
}

TEST_F(SinkFactoryTest, CreateDailyFileSinkCreatesDirectory) {
    auto file_path = test_dir_ / "nested" / "daily.log";

    SinkConfig config;
    config.name = "daily";
    config.type = "daily_file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();
    config.rotation_hour = 0;
    config.rotation_minute = 0;

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
    EXPECT_TRUE(std::filesystem::exists(file_path.parent_path()));
}

// ============================================================================
// Unknown Sink Type Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateUnknownSinkType) {
    SinkConfig config;
    config.name = "unknown";
    config.type = "unknown_type";
    config.level = spdlog::level::info;

    auto sink = SinkFactory::createSink(config);

    EXPECT_EQ(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateEmptySinkType) {
    SinkConfig config;
    config.name = "empty";
    config.type = "";
    config.level = spdlog::level::info;

    auto sink = SinkFactory::createSink(config);

    EXPECT_EQ(sink, nullptr);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateFileSinkInvalidPath) {
    SinkConfig config;
    config.name = "invalid";
    config.type = "file";
    config.level = spdlog::level::info;
    // Use an invalid path that should fail
#ifdef _WIN32
    config.file_path = "Z:\\nonexistent\\path\\that\\should\\fail\\test.log";
#else
    config.file_path = "/nonexistent/path/that/should/fail/test.log";
#endif

    // This may or may not fail depending on permissions
    // Just ensure it doesn't crash
    EXPECT_NO_THROW(SinkFactory::createSink(config));
}

// ============================================================================
// Configuration Variations Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateSinkWithEmptyPattern) {
    SinkConfig config;
    config.name = "console";
    config.type = "console";
    config.level = spdlog::level::info;
    config.pattern = "";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateSinkWithComplexPattern) {
    SinkConfig config;
    config.name = "console";
    config.type = "console";
    config.level = spdlog::level::info;
    config.pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%t] %v";

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateFileSinkWithUnicodePath) {
    auto file_path = test_dir_ / "日志文件.log";

    SinkConfig config;
    config.name = "unicode";
    config.type = "file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();

    auto sink = SinkFactory::createSink(config);

    // May or may not work depending on filesystem support
    // Just ensure no crash
    EXPECT_NO_THROW(SinkFactory::createSink(config));
}

TEST_F(SinkFactoryTest, CreateFileSinkWithSpacesInPath) {
    auto file_path = test_dir_ / "path with spaces" / "test file.log";

    SinkConfig config;
    config.name = "spaces";
    config.type = "file";
    config.level = spdlog::level::info;
    config.file_path = file_path.string();

    auto sink = SinkFactory::createSink(config);

    EXPECT_NE(sink, nullptr);
}

// ============================================================================
// Multiple Sink Creation Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateMultipleSinksOfSameType) {
    std::vector<spdlog::sink_ptr> sinks;

    for (int i = 0; i < 5; ++i) {
        SinkConfig config;
        config.name = "console_" + std::to_string(i);
        config.type = "console";
        config.level = spdlog::level::info;

        auto sink = SinkFactory::createSink(config);
        EXPECT_NE(sink, nullptr);
        sinks.push_back(sink);
    }

    EXPECT_EQ(sinks.size(), 5);
}

TEST_F(SinkFactoryTest, CreateMultipleFileSinks) {
    std::vector<spdlog::sink_ptr> sinks;

    for (int i = 0; i < 5; ++i) {
        auto file_path = test_dir_ / ("file_" + std::to_string(i) + ".log");

        SinkConfig config;
        config.name = "file_" + std::to_string(i);
        config.type = "file";
        config.level = spdlog::level::info;
        config.file_path = file_path.string();

        auto sink = SinkFactory::createSink(config);
        EXPECT_NE(sink, nullptr);
        sinks.push_back(sink);
    }

    EXPECT_EQ(sinks.size(), 5);
}

TEST_F(SinkFactoryTest, CreateMixedSinkTypes) {
    auto file_path = test_dir_ / "mixed.log";
    auto rotating_path = test_dir_ / "rotating_mixed.log";
    auto daily_path = test_dir_ / "daily_mixed.log";

    // Console sink
    SinkConfig console_config;
    console_config.name = "console";
    console_config.type = "console";
    console_config.level = spdlog::level::info;
    auto console_sink = SinkFactory::createSink(console_config);
    EXPECT_NE(console_sink, nullptr);

    // File sink
    SinkConfig file_config;
    file_config.name = "file";
    file_config.type = "file";
    file_config.level = spdlog::level::debug;
    file_config.file_path = file_path.string();
    auto file_sink = SinkFactory::createSink(file_config);
    EXPECT_NE(file_sink, nullptr);

    // Rotating file sink
    SinkConfig rotating_config;
    rotating_config.name = "rotating";
    rotating_config.type = "rotating_file";
    rotating_config.level = spdlog::level::info;
    rotating_config.file_path = rotating_path.string();
    rotating_config.max_file_size = 1024 * 1024;
    rotating_config.max_files = 3;
    auto rotating_sink = SinkFactory::createSink(rotating_config);
    EXPECT_NE(rotating_sink, nullptr);

    // Daily file sink
    SinkConfig daily_config;
    daily_config.name = "daily";
    daily_config.type = "daily_file";
    daily_config.level = spdlog::level::warn;
    daily_config.file_path = daily_path.string();
    daily_config.rotation_hour = 0;
    daily_config.rotation_minute = 0;
    auto daily_sink = SinkFactory::createSink(daily_config);
    EXPECT_NE(daily_sink, nullptr);
}

// ============================================================================
// Default Values Tests
// ============================================================================

TEST_F(SinkFactoryTest, CreateConsoleSinkDefaultLevel) {
    auto sink = SinkFactory::createConsoleSink();

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateFileSinkDefaultValues) {
    auto file_path = test_dir_ / "default.log";

    auto sink = SinkFactory::createFileSink(file_path.string());

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateRotatingFileSinkDefaultPattern) {
    auto file_path = test_dir_ / "rotating_default.log";

    auto sink =
        SinkFactory::createRotatingFileSink(file_path.string(), 1024 * 1024, 3);

    EXPECT_NE(sink, nullptr);
}

TEST_F(SinkFactoryTest, CreateDailyFileSinkDefaultPattern) {
    auto file_path = test_dir_ / "daily_default.log";

    auto sink = SinkFactory::createDailyFileSink(file_path.string(), 0, 0);

    EXPECT_NE(sink, nullptr);
}

// ============================================================================
// Sink Level Tests
// ============================================================================

TEST_F(SinkFactoryTest, SinkLevelIsSet) {
    auto sink = SinkFactory::createConsoleSink(spdlog::level::warn);

    EXPECT_NE(sink, nullptr);
    EXPECT_EQ(sink->level(), spdlog::level::warn);
}

TEST_F(SinkFactoryTest, FileSinkLevelIsSet) {
    auto file_path = test_dir_ / "level_test.log";

    auto sink =
        SinkFactory::createFileSink(file_path.string(), spdlog::level::err);

    EXPECT_NE(sink, nullptr);
    EXPECT_EQ(sink->level(), spdlog::level::err);
}

TEST_F(SinkFactoryTest, RotatingFileSinkLevelIsSet) {
    auto file_path = test_dir_ / "rotating_level.log";

    auto sink = SinkFactory::createRotatingFileSink(
        file_path.string(), 1024 * 1024, 3, spdlog::level::critical);

    EXPECT_NE(sink, nullptr);
    EXPECT_EQ(sink->level(), spdlog::level::critical);
}

TEST_F(SinkFactoryTest, DailyFileSinkLevelIsSet) {
    auto file_path = test_dir_ / "daily_level.log";

    auto sink = SinkFactory::createDailyFileSink(file_path.string(), 0, 0,
                                                 spdlog::level::debug);

    EXPECT_NE(sink, nullptr);
    EXPECT_EQ(sink->level(), spdlog::level::debug);
}
