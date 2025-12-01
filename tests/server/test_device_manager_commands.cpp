/*
 * test_device_manager_commands.cpp - Tests for Device Manager Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "atom/type/json.hpp"
#include "server/command.hpp"
#include "server/command/device_manager.hpp"

using namespace lithium::app;
using namespace testing;
using json = nlohmann::json;

class DeviceManagerCommandsTest : public Test {
protected:
    void SetUp() override {
        dispatcher_ = std::make_shared<CommandDispatcher>();
        registerDeviceManager(dispatcher_);
    }

    void TearDown() override {
        dispatcher_.reset();
    }

    json executeCommand(const std::string& command, json payload = json::object()) {
        dispatcher_->dispatch(command, payload);
        return payload;
    }

    std::shared_ptr<CommandDispatcher> dispatcher_;
};

// ========== Device List Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceList_ReturnsArray) {
    auto result = executeCommand("device.list");

    EXPECT_TRUE(result.contains("status"));
    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result.contains("data"));
    EXPECT_TRUE(result["data"].is_array());
}

// ========== Device Status Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceStatus_ReturnsValidJson) {
    auto result = executeCommand("device.status");

    EXPECT_TRUE(result.contains("status"));
    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result.contains("data"));
    EXPECT_TRUE(result["data"].contains("totalDevices"));
    EXPECT_TRUE(result["data"].contains("connectedDevices"));
}

// ========== Device Connect Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceConnect_MissingName_ReturnsError) {
    auto result = executeCommand("device.connect");

    EXPECT_TRUE(result.contains("status"));
    EXPECT_EQ(result["status"], "error");
    EXPECT_TRUE(result.contains("error"));
}

TEST_F(DeviceManagerCommandsTest, DeviceConnect_EmptyName_ReturnsError) {
    json payload;
    payload["name"] = "";
    auto result = executeCommand("device.connect", payload);

    EXPECT_EQ(result["status"], "error");
}

// ========== Device Disconnect Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceDisconnect_MissingName_ReturnsError) {
    auto result = executeCommand("device.disconnect");

    EXPECT_EQ(result["status"], "error");
}

// ========== Device Connect Batch Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceConnectBatch_MissingNames_ReturnsError) {
    auto result = executeCommand("device.connect_batch");

    EXPECT_EQ(result["status"], "error");
}

TEST_F(DeviceManagerCommandsTest, DeviceConnectBatch_EmptyArray_ReturnsError) {
    json payload;
    payload["names"] = json::array();
    auto result = executeCommand("device.connect_batch", payload);

    EXPECT_EQ(result["status"], "error");
}

// ========== Device Health Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceHealth_AllDevices_ReturnsReport) {
    auto result = executeCommand("device.health");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("timestamp"));
    EXPECT_TRUE(result["data"].contains("devices"));
}

TEST_F(DeviceManagerCommandsTest, DeviceHealth_SpecificDevice_ReturnsHealth) {
    json payload;
    payload["name"] = "NonExistentDevice";
    auto result = executeCommand("device.health", payload);

    // Should still succeed but with default health
    EXPECT_EQ(result["status"], "success");
}

// ========== Device Unhealthy Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceUnhealthy_ReturnsDeviceList) {
    auto result = executeCommand("device.unhealthy");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("threshold"));
    EXPECT_TRUE(result["data"].contains("devices"));
    EXPECT_TRUE(result["data"].contains("count"));
}

TEST_F(DeviceManagerCommandsTest, DeviceUnhealthy_CustomThreshold) {
    json payload;
    payload["threshold"] = 0.8f;
    auto result = executeCommand("device.unhealthy", payload);

    EXPECT_EQ(result["status"], "success");
    EXPECT_FLOAT_EQ(result["data"]["threshold"].get<float>(), 0.8f);
}

// ========== Device Statistics Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceStatistics_ReturnsStats) {
    auto result = executeCommand("device.statistics");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("totalConnections"));
    EXPECT_TRUE(result["data"].contains("totalOperations"));
    EXPECT_TRUE(result["data"].contains("uptimeMs"));
}

TEST_F(DeviceManagerCommandsTest, DeviceResetStatistics_Success) {
    auto result = executeCommand("device.reset_statistics");

    EXPECT_EQ(result["status"], "success");
}

// ========== Device Retry Config Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceSetRetryConfig_MissingName_ReturnsError) {
    auto result = executeCommand("device.set_retry_config");

    EXPECT_EQ(result["status"], "error");
}

TEST_F(DeviceManagerCommandsTest, DeviceSetRetryConfig_ValidParams) {
    json payload;
    payload["name"] = "TestDevice";
    payload["strategy"] = 2;  // Exponential
    payload["maxRetries"] = 5;
    payload["initialDelayMs"] = 200;

    auto result = executeCommand("device.set_retry_config", payload);

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("config"));
}

TEST_F(DeviceManagerCommandsTest, DeviceGetRetryConfig_MissingName_ReturnsError) {
    auto result = executeCommand("device.get_retry_config");

    EXPECT_EQ(result["status"], "error");
}

// ========== Device Reset Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceReset_MissingName_ReturnsError) {
    auto result = executeCommand("device.reset");

    EXPECT_EQ(result["status"], "error");
}

// ========== Health Monitor Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceStartHealthMonitor_Success) {
    json payload;
    payload["interval"] = 60;
    auto result = executeCommand("device.start_health_monitor", payload);

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("interval"));

    // Clean up
    executeCommand("device.stop_health_monitor");
}

TEST_F(DeviceManagerCommandsTest, DeviceStopHealthMonitor_Success) {
    auto result = executeCommand("device.stop_health_monitor");

    EXPECT_EQ(result["status"], "success");
}

// ========== Device Events Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceGetEvents_ReturnsArray) {
    auto result = executeCommand("device.get_events");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].is_array());
}

TEST_F(DeviceManagerCommandsTest, DeviceGetEvents_WithMaxEvents) {
    json payload;
    payload["maxEvents"] = 10;
    auto result = executeCommand("device.get_events", payload);

    EXPECT_EQ(result["status"], "success");
}

TEST_F(DeviceManagerCommandsTest, DeviceClearEvents_Success) {
    auto result = executeCommand("device.clear_events");

    EXPECT_EQ(result["status"], "success");
}

// ========== Device Configuration Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceExportConfig_ReturnsConfig) {
    auto result = executeCommand("device.export_config");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("version"));
    EXPECT_TRUE(result["data"].contains("devices"));
}

TEST_F(DeviceManagerCommandsTest, DeviceImportConfig_MissingConfig_ReturnsError) {
    auto result = executeCommand("device.import_config");

    EXPECT_EQ(result["status"], "error");
}

TEST_F(DeviceManagerCommandsTest, DeviceImportConfig_ValidConfig) {
    json payload;
    payload["config"] = {
        {"version", "1.0"},
        {"devices", json::array()}
    };
    auto result = executeCommand("device.import_config", payload);

    EXPECT_EQ(result["status"], "success");
}

// ========== Device Refresh Command Tests ==========

TEST_F(DeviceManagerCommandsTest, DeviceRefresh_ReturnsStatus) {
    auto result = executeCommand("device.refresh");

    EXPECT_EQ(result["status"], "success");
    EXPECT_TRUE(result["data"].contains("totalDevices"));
}

// ========== Command Registration Tests ==========

TEST_F(DeviceManagerCommandsTest, AllCommandsRegistered) {
    std::vector<std::string> expectedCommands = {
        "device.list",
        "device.status",
        "device.connect",
        "device.disconnect",
        "device.connect_batch",
        "device.disconnect_batch",
        "device.health",
        "device.unhealthy",
        "device.statistics",
        "device.reset_statistics",
        "device.set_retry_config",
        "device.get_retry_config",
        "device.reset",
        "device.start_health_monitor",
        "device.stop_health_monitor",
        "device.get_events",
        "device.clear_events",
        "device.export_config",
        "device.import_config",
        "device.refresh"
    };

    for (const auto& cmd : expectedCommands) {
        EXPECT_TRUE(dispatcher_->hasCommand(cmd))
            << "Command not registered: " << cmd;
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
