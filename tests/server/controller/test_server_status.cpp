/*
 * test_server_status.cpp - Tests for Server Status Controller
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/type/json.hpp"

#include <chrono>
#include <string>
#include <thread>

using json = nlohmann::json;
using namespace std::chrono_literals;

// ============================================================================
// Server Status Structure Tests
// ============================================================================

class ServerStatusStructureTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ServerStatusStructureTest, BasicStatusStructure) {
    json status = {{"running", true},
                   {"uptime_seconds", 3600},
                   {"version", "1.0.0"},
                   {"connections", 10}};

    EXPECT_TRUE(status.contains("running"));
    EXPECT_TRUE(status.contains("uptime_seconds"));
    EXPECT_TRUE(status.contains("version"));
    EXPECT_TRUE(status.contains("connections"));
}

TEST_F(ServerStatusStructureTest, DetailedStatusStructure) {
    json status = {
        {"server",
         {{"running", true},
          {"uptime_seconds", 7200},
          {"start_time", "2024-01-01T00:00:00Z"}}},
        {"websocket",
         {{"active_connections", 5},
          {"total_messages", 1000},
          {"error_count", 2}}},
        {"tasks",
         {{"pending", 3}, {"running", 2}, {"completed", 100}, {"failed", 5}}},
        {"system",
         {{"cpu_usage", 25.5},
          {"memory_usage", 512000000},
          {"disk_free", 100000000000}}}};

    EXPECT_TRUE(status["server"]["running"].get<bool>());
    EXPECT_EQ(status["websocket"]["active_connections"], 5);
    EXPECT_EQ(status["tasks"]["completed"], 100);
}

// ============================================================================
// Uptime Calculation Tests
// ============================================================================

class UptimeCalculationTest : public ::testing::Test {
protected:
    void SetUp() override { startTime_ = std::chrono::steady_clock::now(); }

    void TearDown() override {}

    int64_t getUptimeSeconds() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now -
                                                                startTime_)
            .count();
    }

    std::string formatUptime(int64_t seconds) {
        int64_t days = seconds / 86400;
        int64_t hours = (seconds % 86400) / 3600;
        int64_t minutes = (seconds % 3600) / 60;
        int64_t secs = seconds % 60;

        std::ostringstream oss;
        if (days > 0)
            oss << days << "d ";
        if (hours > 0 || days > 0)
            oss << hours << "h ";
        if (minutes > 0 || hours > 0 || days > 0)
            oss << minutes << "m ";
        oss << secs << "s";

        return oss.str();
    }

    std::chrono::steady_clock::time_point startTime_;
};

TEST_F(UptimeCalculationTest, InitialUptime) {
    auto uptime = getUptimeSeconds();
    EXPECT_GE(uptime, 0);
    EXPECT_LT(uptime, 5);  // Should be very small
}

TEST_F(UptimeCalculationTest, UptimeAfterDelay) {
    std::this_thread::sleep_for(100ms);
    auto uptime = getUptimeSeconds();
    EXPECT_GE(uptime, 0);
}

TEST_F(UptimeCalculationTest, FormatUptimeSeconds) {
    EXPECT_EQ(formatUptime(45), "45s");
}

TEST_F(UptimeCalculationTest, FormatUptimeMinutes) {
    EXPECT_EQ(formatUptime(125), "2m 5s");
}

TEST_F(UptimeCalculationTest, FormatUptimeHours) {
    EXPECT_EQ(formatUptime(3725), "1h 2m 5s");
}

TEST_F(UptimeCalculationTest, FormatUptimeDays) {
    EXPECT_EQ(formatUptime(90125), "1d 1h 2m 5s");
}

// ============================================================================
// Connection Statistics Tests
// ============================================================================

class ConnectionStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionStatisticsTest, BasicConnectionStats) {
    json stats = {{"active_connections", 10},
                  {"total_connections", 100},
                  {"peak_connections", 25},
                  {"rejected_connections", 5}};

    EXPECT_EQ(stats["active_connections"], 10);
    EXPECT_LE(stats["active_connections"].get<int>(),
              stats["peak_connections"].get<int>());
}

TEST_F(ConnectionStatisticsTest, ConnectionRateStats) {
    json stats = {{"connections_per_minute", 5.5},
                  {"messages_per_second", 100.0},
                  {"bytes_sent", 1024000},
                  {"bytes_received", 512000}};

    EXPECT_GT(stats["connections_per_minute"].get<double>(), 0);
    EXPECT_GT(stats["messages_per_second"].get<double>(), 0);
}

// ============================================================================
// Task Statistics Tests
// ============================================================================

class TaskStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TaskStatisticsTest, BasicTaskStats) {
    json stats = {{"total_tasks", 100}, {"pending", 5}, {"running", 3},
                  {"completed", 85},    {"failed", 5},  {"cancelled", 2}};

    int sum = stats["pending"].get<int>() + stats["running"].get<int>() +
              stats["completed"].get<int>() + stats["failed"].get<int>() +
              stats["cancelled"].get<int>();

    EXPECT_EQ(sum, stats["total_tasks"].get<int>());
}

TEST_F(TaskStatisticsTest, TaskPerformanceStats) {
    json stats = {{"average_duration_ms", 150.5},
                  {"max_duration_ms", 5000},
                  {"min_duration_ms", 10},
                  {"tasks_per_minute", 20.0}};

    EXPECT_GT(stats["average_duration_ms"].get<double>(), 0);
    EXPECT_GE(stats["max_duration_ms"].get<int>(),
              stats["min_duration_ms"].get<int>());
}

// ============================================================================
// System Resource Tests
// ============================================================================

class SystemResourceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SystemResourceTest, CPUUsageRange) {
    double cpuUsage = 45.5;

    EXPECT_GE(cpuUsage, 0.0);
    EXPECT_LE(cpuUsage, 100.0);
}

TEST_F(SystemResourceTest, MemoryUsageFormat) {
    json memory = {{"total", 16000000000},
                   {"used", 8000000000},
                   {"free", 8000000000},
                   {"percent", 50.0}};

    EXPECT_EQ(memory["total"].get<int64_t>(),
              memory["used"].get<int64_t>() + memory["free"].get<int64_t>());
}

TEST_F(SystemResourceTest, DiskUsageFormat) {
    json disk = {{"total", 500000000000},
                 {"used", 250000000000},
                 {"free", 250000000000},
                 {"percent", 50.0}};

    EXPECT_EQ(disk["total"].get<int64_t>(),
              disk["used"].get<int64_t>() + disk["free"].get<int64_t>());
}

// ============================================================================
// Health Check Tests
// ============================================================================

class HealthCheckTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    std::string getHealthStatus(bool serverRunning, bool dbConnected,
                                double cpuUsage) {
        if (!serverRunning)
            return "critical";
        if (!dbConnected)
            return "degraded";
        if (cpuUsage > 90.0)
            return "warning";
        return "healthy";
    }
};

TEST_F(HealthCheckTest, HealthyStatus) {
    EXPECT_EQ(getHealthStatus(true, true, 50.0), "healthy");
}

TEST_F(HealthCheckTest, WarningStatus) {
    EXPECT_EQ(getHealthStatus(true, true, 95.0), "warning");
}

TEST_F(HealthCheckTest, DegradedStatus) {
    EXPECT_EQ(getHealthStatus(true, false, 50.0), "degraded");
}

TEST_F(HealthCheckTest, CriticalStatus) {
    EXPECT_EQ(getHealthStatus(false, true, 50.0), "critical");
}

TEST_F(HealthCheckTest, HealthCheckResponse) {
    json health = {{"status", "healthy"},
                   {"checks",
                    {{"server", {{"status", "up"}, {"latency_ms", 5}}},
                     {"database", {{"status", "up"}, {"latency_ms", 10}}},
                     {"websocket", {{"status", "up"}, {"connections", 5}}}}}};

    EXPECT_EQ(health["status"], "healthy");
    EXPECT_EQ(health["checks"]["server"]["status"], "up");
}

// ============================================================================
// Version Information Tests
// ============================================================================

class VersionInformationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool isValidSemver(const std::string& version) {
        // Simple semver validation: X.Y.Z
        int major, minor, patch;
        char dot1, dot2;
        std::istringstream iss(version);
        if (!(iss >> major >> dot1 >> minor >> dot2 >> patch))
            return false;
        return dot1 == '.' && dot2 == '.' && major >= 0 && minor >= 0 &&
               patch >= 0;
    }
};

TEST_F(VersionInformationTest, ValidSemver) {
    EXPECT_TRUE(isValidSemver("1.0.0"));
    EXPECT_TRUE(isValidSemver("2.10.5"));
    EXPECT_TRUE(isValidSemver("0.0.1"));
}

TEST_F(VersionInformationTest, InvalidSemver) {
    EXPECT_FALSE(isValidSemver("1.0"));
    EXPECT_FALSE(isValidSemver("1"));
    EXPECT_FALSE(isValidSemver("v1.0.0"));
}

TEST_F(VersionInformationTest, VersionInfoStructure) {
    json versionInfo = {{"version", "1.0.0"},
                        {"build_date", "2024-01-01"},
                        {"git_commit", "abc123"},
                        {"compiler", "gcc 12.0"}};

    EXPECT_TRUE(versionInfo.contains("version"));
    EXPECT_TRUE(versionInfo.contains("build_date"));
}

// ============================================================================
// Status Response Format Tests
// ============================================================================

class StatusResponseFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StatusResponseFormatTest, MinimalStatusResponse) {
    json response = {{"success", true},
                     {"data", {{"status", "running"}, {"uptime", 3600}}}};

    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_EQ(response["data"]["status"], "running");
}

TEST_F(StatusResponseFormatTest, FullStatusResponse) {
    json response = {
        {"success", true},
        {"request_id", "req-123"},
        {"data",
         {{"server",
           {{"status", "running"},
            {"uptime_seconds", 3600},
            {"version", "1.0.0"}}},
          {"websocket", {{"connections", 5}, {"messages", 1000}}},
          {"tasks", {{"pending", 2}, {"running", 1}, {"completed", 50}}},
          {"health",
           {{"status", "healthy"}, {"last_check", "2024-01-01T12:00:00Z"}}}}}};

    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_EQ(response["data"]["server"]["status"], "running");
    EXPECT_EQ(response["data"]["health"]["status"], "healthy");
}

// ============================================================================
// Timestamp Format Tests
// ============================================================================

class TimestampFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }
};

TEST_F(TimestampFormatTest, ISO8601Format) {
    auto timestamp = getCurrentTimestamp();

    // Should match YYYY-MM-DDTHH:MM:SSZ format
    EXPECT_EQ(timestamp.length(), 20);
    EXPECT_EQ(timestamp[4], '-');
    EXPECT_EQ(timestamp[7], '-');
    EXPECT_EQ(timestamp[10], 'T');
    EXPECT_EQ(timestamp[13], ':');
    EXPECT_EQ(timestamp[16], ':');
    EXPECT_EQ(timestamp[19], 'Z');
}

// ============================================================================
// Error Status Tests
// ============================================================================

class ErrorStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorStatusTest, ServerNotRunning) {
    json status = {{"running", false},
                   {"error", "Server failed to start"},
                   {"last_error_time", "2024-01-01T12:00:00Z"}};

    EXPECT_FALSE(status["running"].get<bool>());
    EXPECT_TRUE(status.contains("error"));
}

TEST_F(ErrorStatusTest, ServiceDegraded) {
    json status = {{"running", true},
                   {"degraded", true},
                   {"degraded_services", {"database", "external_api"}}};

    EXPECT_TRUE(status["running"].get<bool>());
    EXPECT_TRUE(status["degraded"].get<bool>());
    EXPECT_EQ(status["degraded_services"].size(), 2);
}

// ============================================================================
// Rate Limiting Status Tests
// ============================================================================

class RateLimitingStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(RateLimitingStatusTest, RateLimitStats) {
    json stats = {{"requests_per_minute", 100},
                  {"limit_per_minute", 1000},
                  {"remaining", 900},
                  {"reset_time", "2024-01-01T12:01:00Z"}};

    EXPECT_LT(stats["requests_per_minute"].get<int>(),
              stats["limit_per_minute"].get<int>());
}

TEST_F(RateLimitingStatusTest, RateLimitExceeded) {
    json stats = {{"requests_per_minute", 1000},
                  {"limit_per_minute", 1000},
                  {"remaining", 0},
                  {"exceeded", true}};

    EXPECT_TRUE(stats["exceeded"].get<bool>());
    EXPECT_EQ(stats["remaining"], 0);
}
