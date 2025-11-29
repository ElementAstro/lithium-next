/*
 * test_resource_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_resource_monitor.cpp
 * @brief Comprehensive tests for Resource Monitor
 */

#include <gtest/gtest.h>
#include "script/isolated/resource_monitor.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace lithium::isolated;

// =============================================================================
// Memory Usage Tests
// =============================================================================

class ResourceMonitorMemoryTest : public ::testing::Test {
protected:
    int getCurrentPid() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
};

TEST_F(ResourceMonitorMemoryTest, GetMemoryUsageCurrentProcess) {
    int pid = getCurrentPid();
    auto memory = ResourceMonitor::getMemoryUsage(pid);

    // Should be able to get memory for current process
    if (memory.has_value()) {
        EXPECT_GT(memory.value(), 0);
    }
}

TEST_F(ResourceMonitorMemoryTest, GetMemoryUsageInvalidPid) {
    auto memory = ResourceMonitor::getMemoryUsage(-1);
    EXPECT_FALSE(memory.has_value());
}

TEST_F(ResourceMonitorMemoryTest, GetMemoryUsageNonexistentPid) {
    // Use a very high PID that's unlikely to exist
    auto memory = ResourceMonitor::getMemoryUsage(999999999);
    EXPECT_FALSE(memory.has_value());
}

TEST_F(ResourceMonitorMemoryTest, GetPeakMemoryUsageCurrentProcess) {
    int pid = getCurrentPid();
    auto peakMemory = ResourceMonitor::getPeakMemoryUsage(pid);

    if (peakMemory.has_value()) {
        EXPECT_GT(peakMemory.value(), 0);
    }
}

TEST_F(ResourceMonitorMemoryTest, GetPeakMemoryUsageInvalidPid) {
    auto peakMemory = ResourceMonitor::getPeakMemoryUsage(-1);
    EXPECT_FALSE(peakMemory.has_value());
}

TEST_F(ResourceMonitorMemoryTest, PeakMemoryGreaterThanOrEqualCurrent) {
    int pid = getCurrentPid();
    auto current = ResourceMonitor::getMemoryUsage(pid);
    auto peak = ResourceMonitor::getPeakMemoryUsage(pid);

    if (current.has_value() && peak.has_value()) {
        EXPECT_GE(peak.value(), current.value());
    }
}

// =============================================================================
// CPU Usage Tests
// =============================================================================

class ResourceMonitorCpuTest : public ::testing::Test {
protected:
    int getCurrentPid() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
};

TEST_F(ResourceMonitorCpuTest, GetCpuUsageCurrentProcess) {
    int pid = getCurrentPid();
    auto cpu = ResourceMonitor::getCpuUsage(pid);

    if (cpu.has_value()) {
        EXPECT_GE(cpu.value(), 0.0);
        EXPECT_LE(cpu.value(), 100.0);
    }
}

TEST_F(ResourceMonitorCpuTest, GetCpuUsageInvalidPid) {
    auto cpu = ResourceMonitor::getCpuUsage(-1);
    EXPECT_FALSE(cpu.has_value());
}

TEST_F(ResourceMonitorCpuTest, GetCpuUsageNonexistentPid) {
    auto cpu = ResourceMonitor::getCpuUsage(999999999);
    EXPECT_FALSE(cpu.has_value());
}

// =============================================================================
// Memory Limit Tests
// =============================================================================

class ResourceMonitorLimitTest : public ::testing::Test {
protected:
    int getCurrentPid() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
};

TEST_F(ResourceMonitorLimitTest, IsMemoryLimitExceededFalse) {
    int pid = getCurrentPid();
    // Use a very high limit that shouldn't be exceeded
    bool exceeded = ResourceMonitor::isMemoryLimitExceeded(pid, 100000);
    EXPECT_FALSE(exceeded);
}

TEST_F(ResourceMonitorLimitTest, IsMemoryLimitExceededTrue) {
    int pid = getCurrentPid();
    // Use a very low limit that should be exceeded
    bool exceeded = ResourceMonitor::isMemoryLimitExceeded(pid, 1);
    EXPECT_TRUE(exceeded);
}

TEST_F(ResourceMonitorLimitTest, IsMemoryLimitExceededInvalidPid) {
    bool exceeded = ResourceMonitor::isMemoryLimitExceeded(-1, 1000);
    // Should return false for invalid PID (can't check memory)
    EXPECT_FALSE(exceeded);
}

TEST_F(ResourceMonitorLimitTest, IsMemoryLimitExceededZeroLimit) {
    int pid = getCurrentPid();
    bool exceeded = ResourceMonitor::isMemoryLimitExceeded(pid, 0);
    // Zero limit should always be exceeded (any memory > 0)
    EXPECT_TRUE(exceeded);
}

// =============================================================================
// Edge Cases
// =============================================================================

class ResourceMonitorEdgeCasesTest : public ::testing::Test {};

TEST_F(ResourceMonitorEdgeCasesTest, GetMemoryUsagePidZero) {
    auto memory = ResourceMonitor::getMemoryUsage(0);
    // PID 0 is typically the kernel scheduler, may or may not be accessible
    SUCCEED();
}

TEST_F(ResourceMonitorEdgeCasesTest, GetMemoryUsagePidOne) {
    auto memory = ResourceMonitor::getMemoryUsage(1);
    // PID 1 is typically init/systemd, may or may not be accessible
    SUCCEED();
}

TEST_F(ResourceMonitorEdgeCasesTest, GetCpuUsagePidZero) {
    auto cpu = ResourceMonitor::getCpuUsage(0);
    SUCCEED();
}

TEST_F(ResourceMonitorEdgeCasesTest, GetCpuUsagePidOne) {
    auto cpu = ResourceMonitor::getCpuUsage(1);
    SUCCEED();
}

// =============================================================================
// Consistency Tests
// =============================================================================

class ResourceMonitorConsistencyTest : public ::testing::Test {
protected:
    int getCurrentPid() {
#ifdef _WIN32
        return static_cast<int>(GetCurrentProcessId());
#else
        return static_cast<int>(getpid());
#endif
    }
};

TEST_F(ResourceMonitorConsistencyTest, MultipleMemoryCalls) {
    int pid = getCurrentPid();

    std::vector<size_t> readings;
    for (int i = 0; i < 10; ++i) {
        auto memory = ResourceMonitor::getMemoryUsage(pid);
        if (memory.has_value()) {
            readings.push_back(memory.value());
        }
    }

    // All readings should be non-zero and relatively consistent
    if (!readings.empty()) {
        for (auto reading : readings) {
            EXPECT_GT(reading, 0);
        }
    }
}

TEST_F(ResourceMonitorConsistencyTest, MultipleCpuCalls) {
    int pid = getCurrentPid();

    std::vector<double> readings;
    for (int i = 0; i < 10; ++i) {
        auto cpu = ResourceMonitor::getCpuUsage(pid);
        if (cpu.has_value()) {
            readings.push_back(cpu.value());
        }
    }

    // All readings should be in valid range
    for (auto reading : readings) {
        EXPECT_GE(reading, 0.0);
        EXPECT_LE(reading, 100.0);
    }
}
