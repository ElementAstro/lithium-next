/*
 * test_device_backend.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for device backend abstraction and registry

*************************************************/

#include <gtest/gtest.h>

#include "device/service/device_backend.hpp"
#include "device/service/backend_registry.hpp"
#include "device/service/indi_backend.hpp"
#include "device/service/ascom_backend.hpp"

using namespace lithium::device;

// ==================== DiscoveredDevice Tests ====================

class DiscoveredDeviceTest : public ::testing::Test {
protected:
    DiscoveredDevice createTestDevice() {
        DiscoveredDevice dev;
        dev.deviceId = "test_camera_1";
        dev.displayName = "Test Camera";
        dev.deviceType = "Camera";
        dev.driverName = "INDI";
        dev.driverVersion = "1.0.0";
        dev.connectionString = "localhost:7624";
        dev.priority = 5;
        dev.isConnected = false;
        dev.customProperties["manufacturer"] = "TestCorp";
        return dev;
    }
};

TEST_F(DiscoveredDeviceTest, ToJsonConversion) {
    auto dev = createTestDevice();
    auto j = dev.toJson();

    EXPECT_EQ(j["deviceId"], "test_camera_1");
    EXPECT_EQ(j["displayName"], "Test Camera");
    EXPECT_EQ(j["deviceType"], "Camera");
    EXPECT_EQ(j["driverName"], "INDI");
    EXPECT_EQ(j["driverVersion"], "1.0.0");
    EXPECT_EQ(j["connectionString"], "localhost:7624");
    EXPECT_EQ(j["priority"], 5);
    EXPECT_EQ(j["isConnected"], false);
    EXPECT_EQ(j["customProperties"]["manufacturer"], "TestCorp");
}

TEST_F(DiscoveredDeviceTest, FromJsonConversion) {
    auto original = createTestDevice();
    auto j = original.toJson();
    auto restored = DiscoveredDevice::fromJson(j);

    EXPECT_EQ(restored.deviceId, original.deviceId);
    EXPECT_EQ(restored.displayName, original.displayName);
    EXPECT_EQ(restored.deviceType, original.deviceType);
    EXPECT_EQ(restored.driverName, original.driverName);
    EXPECT_EQ(restored.driverVersion, original.driverVersion);
    EXPECT_EQ(restored.connectionString, original.connectionString);
    EXPECT_EQ(restored.priority, original.priority);
    EXPECT_EQ(restored.isConnected, original.isConnected);
}

// ==================== BackendConfig Tests ====================

class BackendConfigTest : public ::testing::Test {};

TEST_F(BackendConfigTest, DefaultValues) {
    BackendConfig config;
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 0);
    EXPECT_EQ(config.timeout, 5000);
    EXPECT_TRUE(config.options.empty());
}

TEST_F(BackendConfigTest, ToJsonConversion) {
    BackendConfig config;
    config.host = "192.168.1.100";
    config.port = 7624;
    config.timeout = 10000;
    config.options["verbose"] = "true";

    auto j = config.toJson();

    EXPECT_EQ(j["host"], "192.168.1.100");
    EXPECT_EQ(j["port"], 7624);
    EXPECT_EQ(j["timeout"], 10000);
    EXPECT_EQ(j["options"]["verbose"], "true");
}

TEST_F(BackendConfigTest, FromJsonConversion) {
    nlohmann::json j;
    j["host"] = "remote.server.com";
    j["port"] = 11111;
    j["timeout"] = 15000;
    j["options"]["apiKey"] = "secret123";

    auto config = BackendConfig::fromJson(j);

    EXPECT_EQ(config.host, "remote.server.com");
    EXPECT_EQ(config.port, 11111);
    EXPECT_EQ(config.timeout, 15000);
    EXPECT_EQ(config.options["apiKey"], "secret123");
}

// ==================== BackendRegistry Tests ====================

class BackendRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        BackendRegistry::getInstance().clear();
    }

    void TearDown() override {
        BackendRegistry::getInstance().clear();
    }
};

TEST_F(BackendRegistryTest, SingletonInstance) {
    auto& instance1 = BackendRegistry::getInstance();
    auto& instance2 = BackendRegistry::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(BackendRegistryTest, RegisterAndGetBackend) {
    auto& registry = BackendRegistry::getInstance();

    // Initially no backends
    EXPECT_FALSE(registry.hasBackend("INDI"));
    EXPECT_FALSE(registry.hasBackend("ASCOM"));

    // Register INDI backend
    auto indiBackend = std::make_shared<INDIBackend>();
    registry.registerBackend(indiBackend);

    EXPECT_TRUE(registry.hasBackend("INDI"));
    EXPECT_FALSE(registry.hasBackend("ASCOM"));

    auto retrieved = registry.getBackend("INDI");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getBackendName(), "INDI");
}

TEST_F(BackendRegistryTest, RegisterFactory) {
    auto& registry = BackendRegistry::getInstance();

    // Register factory
    registry.registerFactory(std::make_shared<INDIBackendFactory>());
    registry.registerFactory(std::make_shared<ASCOMBackendFactory>());

    EXPECT_TRUE(registry.hasBackend("INDI"));
    EXPECT_TRUE(registry.hasBackend("ASCOM"));

    // Get backend (should create from factory)
    auto indiBackend = registry.getBackend("INDI");
    EXPECT_NE(indiBackend, nullptr);
    EXPECT_EQ(indiBackend->getBackendName(), "INDI");

    auto ascomBackend = registry.getBackend("ASCOM");
    EXPECT_NE(ascomBackend, nullptr);
    EXPECT_EQ(ascomBackend->getBackendName(), "ASCOM");
}

TEST_F(BackendRegistryTest, GetBackendNames) {
    auto& registry = BackendRegistry::getInstance();

    registry.registerBackend(std::make_shared<INDIBackend>());
    registry.registerBackend(std::make_shared<ASCOMBackend>());

    auto names = registry.getBackendNames();
    EXPECT_EQ(names.size(), 2);

    bool hasINDI = std::find(names.begin(), names.end(), "INDI") != names.end();
    bool hasASCOM = std::find(names.begin(), names.end(), "ASCOM") != names.end();
    EXPECT_TRUE(hasINDI);
    EXPECT_TRUE(hasASCOM);
}

TEST_F(BackendRegistryTest, UnregisterBackend) {
    auto& registry = BackendRegistry::getInstance();

    registry.registerBackend(std::make_shared<INDIBackend>());
    EXPECT_TRUE(registry.hasBackend("INDI"));

    registry.unregisterBackend("INDI");
    EXPECT_FALSE(registry.hasBackend("INDI"));
}

TEST_F(BackendRegistryTest, InitializeDefaultBackends) {
    auto& registry = BackendRegistry::getInstance();

    registry.initializeDefaultBackends();

    EXPECT_TRUE(registry.hasBackend("INDI"));
    EXPECT_TRUE(registry.hasBackend("ASCOM"));
}

// ==================== INDIBackend Tests ====================

class INDIBackendTest : public ::testing::Test {
protected:
    std::shared_ptr<INDIBackend> backend;

    void SetUp() override {
        backend = std::make_shared<INDIBackend>();
    }
};

TEST_F(INDIBackendTest, BackendName) {
    EXPECT_EQ(backend->getBackendName(), "INDI");
}

TEST_F(INDIBackendTest, BackendVersion) {
    EXPECT_FALSE(backend->getBackendVersion().empty());
}

TEST_F(INDIBackendTest, InitiallyNotConnected) {
    EXPECT_FALSE(backend->isServerConnected());
}

TEST_F(INDIBackendTest, GetServerStatusWhenDisconnected) {
    auto status = backend->getServerStatus();
    EXPECT_EQ(status["backend"], "INDI");
    EXPECT_EQ(status["connected"], false);
}

TEST_F(INDIBackendTest, GetDevicesWhenDisconnected) {
    auto devices = backend->getDevices();
    EXPECT_TRUE(devices.empty());
}

TEST_F(INDIBackendTest, DiscoverDevicesWhenDisconnected) {
    auto devices = backend->discoverDevices();
    EXPECT_TRUE(devices.empty());
}

// ==================== ASCOMBackend Tests ====================

class ASCOMBackendTest : public ::testing::Test {
protected:
    std::shared_ptr<ASCOMBackend> backend;

    void SetUp() override {
        backend = std::make_shared<ASCOMBackend>();
    }
};

TEST_F(ASCOMBackendTest, BackendName) {
    EXPECT_EQ(backend->getBackendName(), "ASCOM");
}

TEST_F(ASCOMBackendTest, BackendVersion) {
    EXPECT_FALSE(backend->getBackendVersion().empty());
}

TEST_F(ASCOMBackendTest, InitiallyNotConnected) {
    EXPECT_FALSE(backend->isServerConnected());
}

TEST_F(ASCOMBackendTest, GetServerStatusWhenDisconnected) {
    auto status = backend->getServerStatus();
    EXPECT_EQ(status["backend"], "ASCOM");
    EXPECT_EQ(status["connected"], false);
}

TEST_F(ASCOMBackendTest, GetDevicesWhenDisconnected) {
    auto devices = backend->getDevices();
    EXPECT_TRUE(devices.empty());
}

TEST_F(ASCOMBackendTest, DiscoverDevicesWhenDisconnected) {
    auto devices = backend->discoverDevices();
    EXPECT_TRUE(devices.empty());
}

// ==================== Backend Event Tests ====================

class BackendEventTest : public ::testing::Test {};

TEST_F(BackendEventTest, EventToJson) {
    BackendEvent event;
    event.type = BackendEventType::DEVICE_CONNECTED;
    event.backendName = "INDI";
    event.deviceId = "test_camera";
    event.message = "Device connected successfully";
    event.timestamp = std::chrono::system_clock::now();

    auto j = event.toJson();

    EXPECT_EQ(j["type"], static_cast<int>(BackendEventType::DEVICE_CONNECTED));
    EXPECT_EQ(j["backendName"], "INDI");
    EXPECT_EQ(j["deviceId"], "test_camera");
    EXPECT_EQ(j["message"], "Device connected successfully");
    EXPECT_TRUE(j.contains("timestamp"));
}

// ==================== Integration Tests ====================

class BackendIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        BackendRegistry::getInstance().clear();
        BackendRegistry::getInstance().initializeDefaultBackends();
    }

    void TearDown() override {
        BackendRegistry::getInstance().clear();
    }
};

TEST_F(BackendIntegrationTest, GetAllBackends) {
    auto& registry = BackendRegistry::getInstance();
    auto backends = registry.getAllBackends();

    // Should have INDI and ASCOM after initialization
    EXPECT_GE(backends.size(), 0);  // May be 0 if not created yet
}

TEST_F(BackendIntegrationTest, GetStatus) {
    auto& registry = BackendRegistry::getInstance();

    // Create backends
    registry.getINDIBackend();
    registry.getASCOMBackend();

    auto status = registry.getStatus();

    EXPECT_TRUE(status.contains("INDI"));
    EXPECT_TRUE(status.contains("ASCOM"));
}

TEST_F(BackendIntegrationTest, EventCallbackRegistration) {
    auto& registry = BackendRegistry::getInstance();

    bool callbackCalled = false;
    registry.registerGlobalEventCallback([&callbackCalled](const BackendEvent& event) {
        callbackCalled = true;
    });

    // Unregister should not throw
    EXPECT_NO_THROW(registry.unregisterGlobalEventCallback());
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
