/*
 * test_ascom_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for ASCOM client implementation

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/ascom/alpaca_client.hpp"
#include "client/ascom/ascom_client.hpp"
#include "client/ascom/ascom_types.hpp"

using namespace lithium::client;
using namespace lithium::client::ascom;

class ASCOMClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<ASCOMClient>("test_ascom");
    }

    void TearDown() override {
        if (client_) {
            client_->destroy();
        }
    }

    std::shared_ptr<ASCOMClient> client_;
};

// ==================== Basic Lifecycle Tests ====================

TEST_F(ASCOMClientTest, CreateClient) {
    EXPECT_NE(client_, nullptr);
    EXPECT_EQ(client_->getName(), "test_ascom");
    EXPECT_EQ(client_->getBackendName(), "ASCOM");
}

TEST_F(ASCOMClientTest, InitializeClient) {
    bool result = client_->initialize();
    EXPECT_TRUE(result);
    EXPECT_EQ(client_->getState(), ClientState::Initialized);
}

TEST_F(ASCOMClientTest, DestroyClient) {
    client_->initialize();
    bool result = client_->destroy();
    EXPECT_TRUE(result);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

// ==================== Connection Tests ====================

TEST_F(ASCOMClientTest, ConnectWithTarget) {
    client_->initialize();

    // This will attempt to connect to an Alpaca server
    // May fail if no server is running
    bool result = client_->connect("localhost:11111");

    if (result) {
        EXPECT_EQ(client_->getState(), ClientState::Connected);
        EXPECT_TRUE(client_->isConnected());
    }
}

TEST_F(ASCOMClientTest, DisconnectClient) {
    client_->initialize();
    client_->connect("localhost:11111");

    bool result = client_->disconnect();
    EXPECT_TRUE(result);
    EXPECT_FALSE(client_->isConnected());
}

// ==================== Configuration Tests ====================

TEST_F(ASCOMClientTest, ConfigureASCOM) {
    client_->configureASCOM("192.168.1.100", 11112);

    auto config = client_->getServerConfig();
    EXPECT_EQ(config.host, "192.168.1.100");
    EXPECT_EQ(config.port, 11112);
}

// ==================== ASCOM Types Tests ====================

TEST_F(ASCOMClientTest, DeviceTypeConversion) {
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Camera), "camera");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Telescope), "telescope");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Focuser), "focuser");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::FilterWheel), "filterwheel");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Dome), "dome");

    EXPECT_EQ(stringToDeviceType("camera"), ASCOMDeviceType::Camera);
    EXPECT_EQ(stringToDeviceType("telescope"), ASCOMDeviceType::Telescope);
    EXPECT_EQ(stringToDeviceType("unknown"), ASCOMDeviceType::Unknown);
}

TEST_F(ASCOMClientTest, AlpacaResponseSerialization) {
    AlpacaResponse response;
    response.clientTransactionId = 1;
    response.serverTransactionId = 100;
    response.errorNumber = 0;
    response.errorMessage = "";
    response.value = 42.5;

    auto json = response.toJson();

    EXPECT_EQ(json["ClientTransactionID"], 1);
    EXPECT_EQ(json["ServerTransactionID"], 100);
    EXPECT_EQ(json["ErrorNumber"], 0);
    EXPECT_DOUBLE_EQ(json["Value"], 42.5);

    auto restored = AlpacaResponse::fromJson(json);
    EXPECT_EQ(restored.clientTransactionId, 1);
    EXPECT_TRUE(restored.isSuccess());
}

TEST_F(ASCOMClientTest, AlpacaResponseError) {
    AlpacaResponse response;
    response.errorNumber = ASCOMErrorCode::NotConnected;
    response.errorMessage = "Device not connected";

    EXPECT_FALSE(response.isSuccess());
    EXPECT_EQ(response.errorNumber, 0x407);
}

TEST_F(ASCOMClientTest, DeviceDescriptionSerialization) {
    ASCOMDeviceDescription desc;
    desc.deviceName = "Simulator Focuser";
    desc.deviceType = ASCOMDeviceType::Focuser;
    desc.deviceNumber = 0;
    desc.uniqueId = "12345-abcde";

    auto json = desc.toJson();

    EXPECT_EQ(json["DeviceName"], "Simulator Focuser");
    EXPECT_EQ(json["DeviceType"], "focuser");
    EXPECT_EQ(json["DeviceNumber"], 0);

    auto restored = ASCOMDeviceDescription::fromJson(json);
    EXPECT_EQ(restored.deviceName, desc.deviceName);
    EXPECT_EQ(restored.deviceType, ASCOMDeviceType::Focuser);
}

TEST_F(ASCOMClientTest, AlpacaServerInfoSerialization) {
    AlpacaServerInfo info;
    info.serverName = "Test Alpaca Server";
    info.manufacturer = "Test Manufacturer";
    info.manufacturerVersion = "1.0.0";
    info.location = "Test Location";

    ASCOMDeviceDescription dev1;
    dev1.deviceName = "Camera 1";
    dev1.deviceType = ASCOMDeviceType::Camera;
    dev1.deviceNumber = 0;
    info.devices.push_back(dev1);

    auto json = info.toJson();

    EXPECT_EQ(json["ServerName"], "Test Alpaca Server");
    EXPECT_EQ(json["Devices"].size(), 1);

    auto restored = AlpacaServerInfo::fromJson(json);
    EXPECT_EQ(restored.serverName, info.serverName);
    EXPECT_EQ(restored.devices.size(), 1);
}

// ==================== ASCOMDriverInfo Tests ====================

TEST_F(ASCOMClientTest, ASCOMDriverInfoFromDescription) {
    ASCOMDeviceDescription desc;
    desc.deviceName = "Simulator Camera";
    desc.deviceType = ASCOMDeviceType::Camera;
    desc.deviceNumber = 0;
    desc.uniqueId = "cam-001";

    auto info = ASCOMDriverInfo::fromDescription(desc);

    EXPECT_EQ(info.name, "Simulator Camera");
    EXPECT_EQ(info.deviceType, ASCOMDeviceType::Camera);
    EXPECT_EQ(info.backend, "ASCOM");
}

// ==================== AlpacaClient Tests ====================

class AlpacaClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        alpacaClient_ = std::make_unique<AlpacaClient>("localhost", 11111);
    }

    std::unique_ptr<AlpacaClient> alpacaClient_;
};

TEST_F(AlpacaClientTest, CreateClient) {
    EXPECT_NE(alpacaClient_, nullptr);
    EXPECT_EQ(alpacaClient_->getHost(), "localhost");
    EXPECT_EQ(alpacaClient_->getPort(), 11111);
}

TEST_F(AlpacaClientTest, SetServer) {
    alpacaClient_->setServer("192.168.1.100", 11112);
    EXPECT_EQ(alpacaClient_->getHost(), "192.168.1.100");
    EXPECT_EQ(alpacaClient_->getPort(), 11112);
}

TEST_F(AlpacaClientTest, SetTimeout) {
    alpacaClient_->setTimeout(10000);
    EXPECT_EQ(alpacaClient_->getTimeout(), 10000);
}

TEST_F(AlpacaClientTest, TransactionIdIncrement) {
    int id1 = alpacaClient_->getNextTransactionId();
    int id2 = alpacaClient_->getNextTransactionId();
    EXPECT_EQ(id2, id1 + 1);
}

TEST_F(AlpacaClientTest, DiscoverServers) {
    // This is a static method that performs network discovery
    auto servers = AlpacaClient::discoverServers(1000);

    // Should at least return localhost as default
    EXPECT_FALSE(servers.empty());
}

// ==================== Enum Tests ====================

TEST_F(ASCOMClientTest, CameraStateValues) {
    EXPECT_EQ(static_cast<int>(CameraState::Idle), 0);
    EXPECT_EQ(static_cast<int>(CameraState::Exposing), 2);
    EXPECT_EQ(static_cast<int>(CameraState::Error), 5);
}

TEST_F(ASCOMClientTest, GuideDirectionValues) {
    EXPECT_EQ(static_cast<int>(GuideDirection::North), 0);
    EXPECT_EQ(static_cast<int>(GuideDirection::South), 1);
    EXPECT_EQ(static_cast<int>(GuideDirection::East), 2);
    EXPECT_EQ(static_cast<int>(GuideDirection::West), 3);
}

TEST_F(ASCOMClientTest, ShutterStateValues) {
    EXPECT_EQ(static_cast<int>(ShutterState::Open), 0);
    EXPECT_EQ(static_cast<int>(ShutterState::Closed), 1);
    EXPECT_EQ(static_cast<int>(ShutterState::Opening), 2);
    EXPECT_EQ(static_cast<int>(ShutterState::Closing), 3);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
