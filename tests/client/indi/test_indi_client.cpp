/*
 * test_indi_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for INDI client implementation

*************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "client/indi/indi_client.hpp"
#include "client/common/server_client.hpp"

using namespace lithium::client;

class INDIClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<INDIClient>("test_indi");
    }

    void TearDown() override {
        if (client_) {
            client_->destroy();
        }
    }

    std::shared_ptr<INDIClient> client_;
};

// ==================== Basic Lifecycle Tests ====================

TEST_F(INDIClientTest, CreateClient) {
    EXPECT_NE(client_, nullptr);
    EXPECT_EQ(client_->getName(), "test_indi");
    EXPECT_EQ(client_->getBackendName(), "INDI");
}

TEST_F(INDIClientTest, InitializeClient) {
    // Note: This may fail if INDI is not installed
    // In that case, the test verifies error handling
    bool result = client_->initialize();
    
    if (result) {
        EXPECT_EQ(client_->getState(), ClientState::Initialized);
    } else {
        EXPECT_TRUE(client_->getLastError().hasError());
    }
}

TEST_F(INDIClientTest, DestroyClient) {
    client_->initialize();
    bool result = client_->destroy();
    EXPECT_TRUE(result);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

// ==================== Connection Tests ====================

TEST_F(INDIClientTest, ConnectWithTarget) {
    client_->initialize();
    
    // This will create a connector but may not actually connect
    // if no server is running
    bool result = client_->connect("localhost:7624");
    
    if (result) {
        EXPECT_EQ(client_->getState(), ClientState::Connected);
        EXPECT_TRUE(client_->isConnected());
    }
}

TEST_F(INDIClientTest, DisconnectClient) {
    client_->initialize();
    client_->connect("localhost:7624");
    
    bool result = client_->disconnect();
    EXPECT_TRUE(result);
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(INDIClientTest, ScanForServers) {
    client_->initialize();
    
    auto servers = client_->scan();
    EXPECT_FALSE(servers.empty());
    EXPECT_EQ(servers[0], "localhost:7624");
}

// ==================== Configuration Tests ====================

TEST_F(INDIClientTest, ConfigureINDI) {
    client_->configureINDI("192.168.1.100", 7625, "/config", "/data", "/fifo");
    
    auto config = client_->getServerConfig();
    EXPECT_EQ(config.host, "192.168.1.100");
    EXPECT_EQ(config.port, 7625);
}

// ==================== DeviceInfo Tests ====================

TEST_F(INDIClientTest, DeviceInfoSerialization) {
    DeviceInfo info;
    info.id = "test_device_1";
    info.name = "Test Device";
    info.displayName = "Test Device Display";
    info.driver = "test_driver";
    info.driverVersion = "1.0.0";
    info.backend = "INDI";
    info.interfaces = DeviceInterface::Focuser | DeviceInterface::Telescope;
    info.connected = true;
    info.initialized = true;
    info.health = DeviceHealth::Good;
    
    auto json = info.toJson();
    
    EXPECT_EQ(json["id"], "test_device_1");
    EXPECT_EQ(json["name"], "Test Device");
    EXPECT_EQ(json["backend"], "INDI");
    EXPECT_TRUE(json["connected"]);
    
    // Test deserialization
    auto restored = DeviceInfo::fromJson(json);
    EXPECT_EQ(restored.id, info.id);
    EXPECT_EQ(restored.name, info.name);
    EXPECT_EQ(restored.backend, info.backend);
}

TEST_F(INDIClientTest, DeviceInterfaceFlags) {
    DeviceInterface flags = DeviceInterface::Focuser | DeviceInterface::CCD;
    
    EXPECT_TRUE(hasInterface(flags, DeviceInterface::Focuser));
    EXPECT_TRUE(hasInterface(flags, DeviceInterface::CCD));
    EXPECT_FALSE(hasInterface(flags, DeviceInterface::Telescope));
}

// ==================== DriverInfo Tests ====================

TEST_F(INDIClientTest, DriverInfoSerialization) {
    DriverInfo info;
    info.id = "driver_1";
    info.name = "indi_simulator_telescope";
    info.label = "Telescope Simulator";
    info.version = "1.0.0";
    info.binary = "indi_simulator_telescope";
    info.backend = "INDI";
    info.running = true;
    
    auto json = info.toJson();
    
    EXPECT_EQ(json["name"], "indi_simulator_telescope");
    EXPECT_EQ(json["label"], "Telescope Simulator");
    EXPECT_TRUE(json["running"]);
    
    auto restored = DriverInfo::fromJson(json);
    EXPECT_EQ(restored.name, info.name);
    EXPECT_EQ(restored.running, info.running);
}

// ==================== INDIDriverInfo Tests ====================

TEST_F(INDIClientTest, INDIDriverInfoFromContainer) {
    INDIDeviceContainer container(
        "sim_telescope", "Telescope Simulator", "1.0.0",
        "indi_simulator_telescope", "Simulators", "", false
    );
    
    auto info = INDIDriverInfo::fromContainer(container);
    
    EXPECT_EQ(info.name, "sim_telescope");
    EXPECT_EQ(info.label, "Telescope Simulator");
    EXPECT_EQ(info.binary, "indi_simulator_telescope");
    EXPECT_EQ(info.backend, "INDI");
}

// ==================== ServerConfig Tests ====================

TEST_F(INDIClientTest, ServerConfigSerialization) {
    ServerConfig config;
    config.host = "192.168.1.50";
    config.port = 7625;
    config.protocol = "tcp";
    config.connectionTimeout = 10000;
    config.verbose = true;
    
    auto json = config.toJson();
    
    EXPECT_EQ(json["host"], "192.168.1.50");
    EXPECT_EQ(json["port"], 7625);
    EXPECT_TRUE(json["verbose"]);
    
    auto restored = ServerConfig::fromJson(json);
    EXPECT_EQ(restored.host, config.host);
    EXPECT_EQ(restored.port, config.port);
    EXPECT_EQ(restored.connectionTimeout, config.connectionTimeout);
}

// ==================== PropertyValue Tests ====================

TEST_F(INDIClientTest, PropertyValueNumber) {
    PropertyValue prop;
    prop.type = PropertyValue::Type::Number;
    prop.name = "FOCUS_POSITION";
    prop.label = "Focus Position";
    prop.numberValue = 12345.0;
    prop.numberMin = 0.0;
    prop.numberMax = 100000.0;
    prop.numberStep = 1.0;
    
    auto json = prop.toJson();
    
    EXPECT_EQ(json["type"], "number");
    EXPECT_EQ(json["name"], "FOCUS_POSITION");
    EXPECT_DOUBLE_EQ(json["value"], 12345.0);
}

TEST_F(INDIClientTest, PropertyValueSwitch) {
    PropertyValue prop;
    prop.type = PropertyValue::Type::Switch;
    prop.name = "CONNECTION";
    prop.switchValue = true;
    
    auto json = prop.toJson();
    
    EXPECT_EQ(json["type"], "switch");
    EXPECT_TRUE(json["value"]);
}

TEST_F(INDIClientTest, PropertyValueText) {
    PropertyValue prop;
    prop.type = PropertyValue::Type::Text;
    prop.name = "DEVICE_PORT";
    prop.textValue = "/dev/ttyUSB0";
    
    auto json = prop.toJson();
    
    EXPECT_EQ(json["type"], "text");
    EXPECT_EQ(json["value"], "/dev/ttyUSB0");
}

// ==================== Event Callback Tests ====================

TEST_F(INDIClientTest, EventCallback) {
    bool eventReceived = false;
    std::string lastEvent;
    
    client_->setEventCallback([&](const std::string& event, const std::string& data) {
        eventReceived = true;
        lastEvent = event;
        (void)data;
    });
    
    client_->initialize();
    
    // Event should be emitted on initialize
    // Note: This depends on implementation details
}

TEST_F(INDIClientTest, ServerEventCallback) {
    bool eventReceived = false;
    ServerEventType lastEventType;
    
    client_->registerServerEventCallback([&](const ServerEvent& event) {
        eventReceived = true;
        lastEventType = event.type;
    });
    
    // Events would be emitted during actual server operations
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
