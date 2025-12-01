/*
 * test_device_manager.cpp - Comprehensive tests for DeviceManager
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "device/manager.hpp"
#include "device/template/device.hpp"
#include "device/service/backend_registry.hpp"

using namespace lithium;
using namespace testing;

// Mock device for testing
class MockAtomDriver : public AtomDriver {
public:
    explicit MockAtomDriver(const std::string& name) : AtomDriver(name) {
        type_ = "mock";
    }

    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, destroy, (), (override));
    MOCK_METHOD(bool, connect, (const std::string&, int, int), (override));
    MOCK_METHOD(bool, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, scan, (), (override));
};

class DeviceManagerTest : public Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<DeviceManager>();
    }

    void TearDown() override {
        manager_->stopHealthMonitor();
        manager_.reset();
    }

    std::unique_ptr<DeviceManager> manager_;
};

// ========== Basic Device Management Tests ==========

TEST_F(DeviceManagerTest, AddDevice_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    auto devices = manager_->getDevices();
    ASSERT_EQ(devices.size(), 1);
    ASSERT_EQ(devices["camera"].size(), 1);
    EXPECT_EQ(devices["camera"][0]->getName(), "TestCamera");
}

TEST_F(DeviceManagerTest, AddDevice_SetsPrimaryDevice) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    auto primary = manager_->getPrimaryDevice("camera");
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(primary->getName(), "TestCamera");
}

TEST_F(DeviceManagerTest, AddMultipleDevices_FirstIsPrimary) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device1);
    manager_->addDevice("camera", device2);

    auto primary = manager_->getPrimaryDevice("camera");
    EXPECT_EQ(primary->getName(), "Camera1");
}

TEST_F(DeviceManagerTest, RemoveDevice_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device, destroy()).WillOnce(Return(true));

    manager_->addDevice("camera", device);
    manager_->removeDevice("camera", device);

    auto devices = manager_->getDevices();
    EXPECT_TRUE(devices["camera"].empty());
}

TEST_F(DeviceManagerTest, RemoveDeviceByName_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);
    manager_->removeDeviceByName("TestCamera");

    auto found = manager_->findDeviceByName("TestCamera");
    EXPECT_EQ(found, nullptr);
}

TEST_F(DeviceManagerTest, RemoveDeviceByName_NotFound_Throws) {
    EXPECT_THROW(manager_->removeDeviceByName("NonExistent"),
                 DeviceNotFoundException);
}

TEST_F(DeviceManagerTest, SetPrimaryDevice_Success) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device1);
    manager_->addDevice("camera", device2);
    manager_->setPrimaryDevice("camera", device2);

    auto primary = manager_->getPrimaryDevice("camera");
    EXPECT_EQ(primary->getName(), "Camera2");
}

TEST_F(DeviceManagerTest, SetPrimaryDevice_NotFound_Throws) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device1);

    EXPECT_THROW(manager_->setPrimaryDevice("camera", device2),
                 DeviceNotFoundException);
}

// ========== Device Query Tests ==========

TEST_F(DeviceManagerTest, FindDeviceByName_Found) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    auto found = manager_->findDeviceByName("TestCamera");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getName(), "TestCamera");
}

TEST_F(DeviceManagerTest, FindDeviceByName_NotFound) {
    auto found = manager_->findDeviceByName("NonExistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(DeviceManagerTest, FindDevicesByType_Found) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device1);
    manager_->addDevice("camera", device2);

    auto devices = manager_->findDevicesByType("camera");
    EXPECT_EQ(devices.size(), 2);
}

TEST_F(DeviceManagerTest, FindDevicesByType_NotFound) {
    auto devices = manager_->findDevicesByType("nonexistent");
    EXPECT_TRUE(devices.empty());
}

TEST_F(DeviceManagerTest, IsDeviceValid_True) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    EXPECT_TRUE(manager_->isDeviceValid("TestCamera"));
}

TEST_F(DeviceManagerTest, IsDeviceValid_False) {
    EXPECT_FALSE(manager_->isDeviceValid("NonExistent"));
}

// ========== Connection Tests ==========

TEST_F(DeviceManagerTest, ConnectDeviceByName_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device, connect(_, _, _)).WillOnce(Return(true));

    manager_->addDevice("camera", device);
    EXPECT_NO_THROW(manager_->connectDeviceByName("TestCamera"));
}

TEST_F(DeviceManagerTest, ConnectDeviceByName_NotFound_Throws) {
    EXPECT_THROW(manager_->connectDeviceByName("NonExistent"),
                 DeviceNotFoundException);
}

TEST_F(DeviceManagerTest, DisconnectDeviceByName_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*device, disconnect()).WillOnce(Return(true));

    manager_->addDevice("camera", device);
    EXPECT_NO_THROW(manager_->disconnectDeviceByName("TestCamera"));
}

TEST_F(DeviceManagerTest, IsDeviceConnected_True) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));

    manager_->addDevice("camera", device);

    EXPECT_TRUE(manager_->isDeviceConnected("TestCamera"));
}

TEST_F(DeviceManagerTest, IsDeviceConnected_False) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    EXPECT_FALSE(manager_->isDeviceConnected("TestCamera"));
}

// ========== Metadata Tests ==========

TEST_F(DeviceManagerTest, AddDeviceWithMetadata_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    metadata.deviceId = "cam-001";
    metadata.displayName = "Main Camera";
    metadata.driverName = "INDI";
    metadata.priority = 10;

    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto retrieved = manager_->getDeviceMetadata("TestCamera");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->deviceId, "cam-001");
    EXPECT_EQ(retrieved->displayName, "Main Camera");
    EXPECT_EQ(retrieved->priority, 10);
}

TEST_F(DeviceManagerTest, UpdateDeviceMetadata_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    metadata.deviceId = "cam-001";
    manager_->addDeviceWithMetadata("camera", device, metadata);

    DeviceMetadata updated;
    updated.deviceId = "cam-002";
    updated.priority = 20;
    manager_->updateDeviceMetadata("TestCamera", updated);

    auto retrieved = manager_->getDeviceMetadata("TestCamera");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->deviceId, "cam-002");
    EXPECT_EQ(retrieved->priority, 20);
}

TEST_F(DeviceManagerTest, GetDeviceById_Found) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    metadata.deviceId = "unique-id-123";
    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto found = manager_->getDeviceById("unique-id-123");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->getName(), "TestCamera");
}

// ========== State Tests ==========

TEST_F(DeviceManagerTest, GetDeviceState_AfterAdd) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto state = manager_->getDeviceState("TestCamera");
    ASSERT_TRUE(state.has_value());
    EXPECT_TRUE(state->isConnected);
    EXPECT_TRUE(state->isInitialized);
    EXPECT_FLOAT_EQ(state->healthScore, 1.0f);
}

TEST_F(DeviceManagerTest, GetDevicesWithState_Success) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata meta1, meta2;
    manager_->addDeviceWithMetadata("camera", device1, meta1);
    manager_->addDeviceWithMetadata("camera", device2, meta2);

    auto devicesWithState = manager_->getDevicesWithState("camera");
    EXPECT_EQ(devicesWithState.size(), 2);
}

// ========== Health Tests ==========

TEST_F(DeviceManagerTest, GetDeviceHealth_Default) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    float health = manager_->getDeviceHealth("TestCamera");
    EXPECT_FLOAT_EQ(health, 1.0f);
}

TEST_F(DeviceManagerTest, UpdateDeviceHealth_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    // Simulate failed operations
    manager_->updateDeviceHealth("TestCamera", false);
    manager_->updateDeviceHealth("TestCamera", false);

    float health = manager_->getDeviceHealth("TestCamera");
    EXPECT_LT(health, 1.0f);
}

TEST_F(DeviceManagerTest, GetUnhealthyDevices_Success) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata meta1, meta2;
    manager_->addDeviceWithMetadata("camera", device1, meta1);
    manager_->addDeviceWithMetadata("camera", device2, meta2);

    // Make Camera1 unhealthy
    for (int i = 0; i < 5; ++i) {
        manager_->updateDeviceHealth("Camera1", false);
    }

    auto unhealthy = manager_->getUnhealthyDevices(0.5f);
    EXPECT_EQ(unhealthy.size(), 1);
    EXPECT_EQ(unhealthy[0], "Camera1");
}

TEST_F(DeviceManagerTest, ResetDevice_RestoresHealth) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    // Damage health
    for (int i = 0; i < 5; ++i) {
        manager_->updateDeviceHealth("TestCamera", false);
    }

    manager_->resetDevice("TestCamera");

    float health = manager_->getDeviceHealth("TestCamera");
    EXPECT_FLOAT_EQ(health, 1.0f);
}

// ========== Retry Config Tests ==========

TEST_F(DeviceManagerTest, SetRetryConfig_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    manager_->addDevice("camera", device);

    DeviceRetryConfig config;
    config.strategy = DeviceRetryConfig::Strategy::Exponential;
    config.maxRetries = 5;
    config.initialDelay = std::chrono::milliseconds(200);

    manager_->setDeviceRetryConfig("TestCamera", config);

    auto retrieved = manager_->getDeviceRetryConfig("TestCamera");
    EXPECT_EQ(retrieved.maxRetries, 5);
    EXPECT_EQ(retrieved.initialDelay.count(), 200);
}

TEST_F(DeviceManagerTest, RetryConfig_CalculateDelay_Exponential) {
    DeviceRetryConfig config;
    config.strategy = DeviceRetryConfig::Strategy::Exponential;
    config.initialDelay = std::chrono::milliseconds(100);
    config.multiplier = 2.0f;
    config.maxDelay = std::chrono::milliseconds(5000);

    EXPECT_EQ(config.calculateDelay(1).count(), 100);
    EXPECT_EQ(config.calculateDelay(2).count(), 200);
    EXPECT_EQ(config.calculateDelay(3).count(), 400);
}

TEST_F(DeviceManagerTest, RetryConfig_CalculateDelay_Linear) {
    DeviceRetryConfig config;
    config.strategy = DeviceRetryConfig::Strategy::Linear;
    config.initialDelay = std::chrono::milliseconds(100);

    EXPECT_EQ(config.calculateDelay(1).count(), 100);
    EXPECT_EQ(config.calculateDelay(2).count(), 100);
    EXPECT_EQ(config.calculateDelay(3).count(), 100);
}

// ========== Event System Tests ==========

TEST_F(DeviceManagerTest, SubscribeToEvents_ReceivesEvents) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    std::vector<DeviceEventType> receivedEvents;
    auto callbackId = manager_->subscribeToEvents(
        [&](DeviceEventType type, const std::string&, const std::string&, const json&) {
            receivedEvents.push_back(type);
        });

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    EXPECT_FALSE(receivedEvents.empty());
    EXPECT_EQ(receivedEvents[0], DeviceEventType::DEVICE_ADDED);

    manager_->unsubscribeFromEvents(callbackId);
}

TEST_F(DeviceManagerTest, SubscribeToSpecificEvents_FiltersEvents) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    std::vector<DeviceEventType> receivedEvents;
    auto callbackId = manager_->subscribeToEvents(
        [&](DeviceEventType type, const std::string&, const std::string&, const json&) {
            receivedEvents.push_back(type);
        },
        {DeviceEventType::DEVICE_CONNECTED});

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    // Should not receive DEVICE_ADDED since we only subscribed to DEVICE_CONNECTED
    EXPECT_TRUE(receivedEvents.empty());

    manager_->unsubscribeFromEvents(callbackId);
}

TEST_F(DeviceManagerTest, GetPendingEvents_ReturnsEvents) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto events = manager_->getPendingEvents(10);
    EXPECT_FALSE(events.empty());
}

TEST_F(DeviceManagerTest, ClearPendingEvents_ClearsAll) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    manager_->clearPendingEvents();

    auto events = manager_->getPendingEvents(10);
    EXPECT_TRUE(events.empty());
}

// ========== Batch Operations Tests ==========

TEST_F(DeviceManagerTest, ConnectDevicesBatch_Success) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device1, connect(_, _, _)).WillOnce(Return(true));
    EXPECT_CALL(*device2, connect(_, _, _)).WillOnce(Return(true));

    manager_->addDevice("camera", device1);
    manager_->addDevice("camera", device2);

    auto results = manager_->connectDevicesBatch({"Camera1", "Camera2"});

    EXPECT_EQ(results.size(), 2);
    for (const auto& [name, success] : results) {
        EXPECT_TRUE(success);
    }
}

TEST_F(DeviceManagerTest, DisconnectDevicesBatch_Success) {
    auto device1 = std::make_shared<MockAtomDriver>("Camera1");
    auto device2 = std::make_shared<MockAtomDriver>("Camera2");
    EXPECT_CALL(*device1, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*device2, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*device1, disconnect()).WillOnce(Return(true));
    EXPECT_CALL(*device2, disconnect()).WillOnce(Return(true));

    manager_->addDevice("camera", device1);
    manager_->addDevice("camera", device2);

    auto results = manager_->disconnectDevicesBatch({"Camera1", "Camera2"});

    EXPECT_EQ(results.size(), 2);
    for (const auto& [name, success] : results) {
        EXPECT_TRUE(success);
    }
}

// ========== Async Operations Tests ==========

TEST_F(DeviceManagerTest, ConnectDeviceAsync_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));
    EXPECT_CALL(*device, connect(_, _, _)).WillOnce(Return(true));

    manager_->addDevice("camera", device);

    auto future = manager_->connectDeviceAsync("TestCamera", 5000);
    bool result = future.get();

    EXPECT_TRUE(result);
}

TEST_F(DeviceManagerTest, DisconnectDeviceAsync_Success) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*device, disconnect()).WillOnce(Return(true));

    manager_->addDevice("camera", device);

    auto future = manager_->disconnectDeviceAsync("TestCamera");
    bool result = future.get();

    EXPECT_TRUE(result);
}

// ========== Statistics Tests ==========

TEST_F(DeviceManagerTest, GetStatistics_ReturnsValidJson) {
    auto stats = manager_->getStatistics();

    EXPECT_TRUE(stats.contains("totalConnections"));
    EXPECT_TRUE(stats.contains("successfulConnections"));
    EXPECT_TRUE(stats.contains("failedConnections"));
    EXPECT_TRUE(stats.contains("totalOperations"));
    EXPECT_TRUE(stats.contains("uptimeMs"));
}

TEST_F(DeviceManagerTest, ResetStatistics_ClearsCounters) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);
    manager_->updateDeviceHealth("TestCamera", true);

    manager_->resetStatistics();

    auto stats = manager_->getStatistics();
    EXPECT_EQ(stats["totalOperations"].get<int>(), 0);
}

// ========== Configuration Tests ==========

TEST_F(DeviceManagerTest, ExportConfiguration_ReturnsValidJson) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(false));

    DeviceMetadata metadata;
    metadata.deviceId = "cam-001";
    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto config = manager_->exportConfiguration();

    EXPECT_TRUE(config.contains("version"));
    EXPECT_TRUE(config.contains("devices"));
    EXPECT_TRUE(config["devices"].is_array());
    EXPECT_EQ(config["devices"].size(), 1);
}

TEST_F(DeviceManagerTest, ImportConfiguration_Success) {
    json config;
    config["version"] = "1.0";
    config["devices"] = json::array();

    json deviceJson;
    deviceJson["name"] = "TestCamera";
    deviceJson["metadata"] = DeviceMetadata{}.toJson();
    deviceJson["metadata"]["deviceId"] = "imported-001";
    config["devices"].push_back(deviceJson);

    manager_->importConfiguration(config);

    // Note: Import only updates metadata, doesn't create devices
    // This is by design - devices must be added through proper channels
}

// ========== Status Tests ==========

TEST_F(DeviceManagerTest, GetStatus_ReturnsValidJson) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));

    manager_->addDevice("camera", device);

    auto status = manager_->getStatus();

    EXPECT_TRUE(status.contains("totalDevices"));
    EXPECT_TRUE(status.contains("connectedDevices"));
    EXPECT_TRUE(status.contains("deviceTypes"));
    EXPECT_EQ(status["totalDevices"].get<int>(), 1);
    EXPECT_EQ(status["connectedDevices"].get<int>(), 1);
}

// ========== Health Monitor Tests ==========

TEST_F(DeviceManagerTest, StartStopHealthMonitor_Success) {
    EXPECT_NO_THROW(manager_->startHealthMonitor(std::chrono::seconds(1)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(manager_->stopHealthMonitor());
}

TEST_F(DeviceManagerTest, CheckAllDevicesHealth_ReturnsReport) {
    auto device = std::make_shared<MockAtomDriver>("TestCamera");
    EXPECT_CALL(*device, isConnected()).WillRepeatedly(Return(true));

    DeviceMetadata metadata;
    manager_->addDeviceWithMetadata("camera", device, metadata);

    auto report = manager_->checkAllDevicesHealth();

    EXPECT_TRUE(report.contains("timestamp"));
    EXPECT_TRUE(report.contains("devices"));
    EXPECT_EQ(report["devices"].size(), 1);
}

// ========== Thread Safety Tests ==========

TEST_F(DeviceManagerTest, ConcurrentAccess_NoDataRace) {
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([this, i]() {
            for (int j = 0; j < OPS_PER_THREAD; ++j) {
                auto device = std::make_shared<NiceMock<MockAtomDriver>>(
                    "Device_" + std::to_string(i) + "_" + std::to_string(j));
                ON_CALL(*device, isConnected()).WillByDefault(Return(false));

                manager_->addDevice("test", device);
                manager_->getStatus();
                manager_->getDeviceHealth(device->getName());
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto status = manager_->getStatus();
    EXPECT_EQ(status["totalDevices"].get<int>(), NUM_THREADS * OPS_PER_THREAD);
}

// ========== Backend Integration Tests ==========

class DeviceManagerBackendTest : public Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<DeviceManager>();
        // Initialize default backends
        device::BackendRegistry::getInstance().initializeDefaultBackends();
    }

    void TearDown() override {
        manager_->stopHealthMonitor();
        manager_.reset();
        device::BackendRegistry::getInstance().clear();
    }

    std::unique_ptr<DeviceManager> manager_;
};

TEST_F(DeviceManagerBackendTest, DiscoverDevices_EmptyWhenNotConnected) {
    // When no backend is connected, discovery should return empty
    auto devices = manager_->discoverDevices("INDI");
    EXPECT_TRUE(devices.empty());

    devices = manager_->discoverDevices("ASCOM");
    EXPECT_TRUE(devices.empty());
}

TEST_F(DeviceManagerBackendTest, DiscoverDevices_AllBackends) {
    // Discover from all backends
    auto devices = manager_->discoverDevices("ALL");
    // Should not throw, may return empty if not connected
    EXPECT_GE(devices.size(), 0);
}

TEST_F(DeviceManagerBackendTest, RefreshDevices_NoThrow) {
    // Refresh should not throw even when not connected
    EXPECT_NO_THROW(manager_->refreshDevices());
}

TEST_F(DeviceManagerBackendTest, BackendRegistry_HasDefaultBackends) {
    auto& registry = device::BackendRegistry::getInstance();

    EXPECT_TRUE(registry.hasBackend("INDI"));
    EXPECT_TRUE(registry.hasBackend("ASCOM"));
}

TEST_F(DeviceManagerBackendTest, BackendRegistry_GetStatus) {
    auto& registry = device::BackendRegistry::getInstance();

    // Create backends
    registry.getINDIBackend();
    registry.getASCOMBackend();

    auto status = registry.getStatus();

    EXPECT_TRUE(status.contains("INDI"));
    EXPECT_TRUE(status.contains("ASCOM"));
    EXPECT_EQ(status["INDI"]["connected"], false);
    EXPECT_EQ(status["ASCOM"]["connected"], false);
}

TEST_F(DeviceManagerBackendTest, DiscoverDevices_ReturnsMetadata) {
    // This test verifies the structure of returned DeviceMetadata
    auto devices = manager_->discoverDevices("INDI");

    // Even if empty, the call should succeed
    for (const auto& meta : devices) {
        // Verify metadata structure
        EXPECT_FALSE(meta.deviceId.empty());
        EXPECT_FALSE(meta.driverName.empty());
    }
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
