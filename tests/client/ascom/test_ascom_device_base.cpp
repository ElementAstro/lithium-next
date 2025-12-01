/*
 * test_ascom_device_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "client/ascom/ascom_device_base.hpp"

using namespace lithium::client::ascom;

// ==================== Test Fixture ====================

class ASCOMDeviceBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a concrete device for testing (using Camera as example)
    }

    void TearDown() override {}
};

// ==================== Device State Tests ====================

TEST(DeviceStateTest, StateToString) {
    EXPECT_EQ(deviceStateToString(DeviceState::Disconnected), "Disconnected");
    EXPECT_EQ(deviceStateToString(DeviceState::Connecting), "Connecting");
    EXPECT_EQ(deviceStateToString(DeviceState::Connected), "Connected");
    EXPECT_EQ(deviceStateToString(DeviceState::Disconnecting), "Disconnecting");
    EXPECT_EQ(deviceStateToString(DeviceState::Error), "Error");
}

// ==================== Device Event Tests ====================

TEST(DeviceEventTest, EventToJson) {
    DeviceEvent event;
    event.type = DeviceEventType::Connected;
    event.deviceName = "TestCamera";
    event.propertyName = "";
    event.message = "Device connected";
    event.data = {};

    auto json = event.toJson();
    EXPECT_EQ(json["type"], static_cast<int>(DeviceEventType::Connected));
    EXPECT_EQ(json["deviceName"], "TestCamera");
    EXPECT_EQ(json["message"], "Device connected");
}

// ==================== ASCOMDeviceType Tests ====================

TEST(ASCOMDeviceTypeTest, TypeToString) {
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Camera), "camera");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Focuser), "focuser");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::FilterWheel), "filterwheel");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Telescope), "telescope");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Rotator), "rotator");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Dome), "dome");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::ObservingConditions), "observingconditions");
    EXPECT_EQ(deviceTypeToString(ASCOMDeviceType::Unknown), "unknown");
}

TEST(ASCOMDeviceTypeTest, StringToType) {
    EXPECT_EQ(stringToDeviceType("camera"), ASCOMDeviceType::Camera);
    EXPECT_EQ(stringToDeviceType("focuser"), ASCOMDeviceType::Focuser);
    EXPECT_EQ(stringToDeviceType("filterwheel"), ASCOMDeviceType::FilterWheel);
    EXPECT_EQ(stringToDeviceType("telescope"), ASCOMDeviceType::Telescope);
    EXPECT_EQ(stringToDeviceType("rotator"), ASCOMDeviceType::Rotator);
    EXPECT_EQ(stringToDeviceType("dome"), ASCOMDeviceType::Dome);
    EXPECT_EQ(stringToDeviceType("observingconditions"), ASCOMDeviceType::ObservingConditions);
    EXPECT_EQ(stringToDeviceType("invalid"), ASCOMDeviceType::Unknown);
}

// ==================== AlpacaResponse Tests ====================

TEST(AlpacaResponseTest, IsSuccess) {
    AlpacaResponse success;
    success.errorNumber = 0;
    EXPECT_TRUE(success.isSuccess());

    AlpacaResponse failure;
    failure.errorNumber = ASCOMErrorCode::NotConnected;
    EXPECT_FALSE(failure.isSuccess());
}

TEST(AlpacaResponseTest, ToJson) {
    AlpacaResponse resp;
    resp.clientTransactionId = 1;
    resp.serverTransactionId = 100;
    resp.errorNumber = 0;
    resp.errorMessage = "";
    resp.value = 42;

    auto json = resp.toJson();
    EXPECT_EQ(json["ClientTransactionID"], 1);
    EXPECT_EQ(json["ServerTransactionID"], 100);
    EXPECT_EQ(json["ErrorNumber"], 0);
    EXPECT_EQ(json["Value"], 42);
}

TEST(AlpacaResponseTest, FromJson) {
    nlohmann::json j = {
        {"ClientTransactionID", 5},
        {"ServerTransactionID", 200},
        {"ErrorNumber", 0},
        {"ErrorMessage", ""},
        {"Value", "test"}
    };

    auto resp = AlpacaResponse::fromJson(j);
    EXPECT_EQ(resp.clientTransactionId, 5);
    EXPECT_EQ(resp.serverTransactionId, 200);
    EXPECT_EQ(resp.errorNumber, 0);
    EXPECT_EQ(resp.value, "test");
}

// ==================== ASCOMDeviceDescription Tests ====================

TEST(ASCOMDeviceDescriptionTest, ToJson) {
    ASCOMDeviceDescription desc;
    desc.deviceName = "Test Camera";
    desc.deviceType = ASCOMDeviceType::Camera;
    desc.deviceNumber = 0;
    desc.uniqueId = "abc123";

    auto json = desc.toJson();
    EXPECT_EQ(json["DeviceName"], "Test Camera");
    EXPECT_EQ(json["DeviceType"], "camera");
    EXPECT_EQ(json["DeviceNumber"], 0);
    EXPECT_EQ(json["UniqueID"], "abc123");
}

TEST(ASCOMDeviceDescriptionTest, FromJson) {
    nlohmann::json j = {
        {"DeviceName", "Test Focuser"},
        {"DeviceType", "focuser"},
        {"DeviceNumber", 1},
        {"UniqueID", "xyz789"}
    };

    auto desc = ASCOMDeviceDescription::fromJson(j);
    EXPECT_EQ(desc.deviceName, "Test Focuser");
    EXPECT_EQ(desc.deviceType, ASCOMDeviceType::Focuser);
    EXPECT_EQ(desc.deviceNumber, 1);
    EXPECT_EQ(desc.uniqueId, "xyz789");
}

// ==================== AlpacaServerInfo Tests ====================

TEST(AlpacaServerInfoTest, ToJson) {
    AlpacaServerInfo info;
    info.serverName = "Test Server";
    info.manufacturer = "Test Mfg";
    info.manufacturerVersion = "1.0";
    info.location = "Test Location";

    ASCOMDeviceDescription dev;
    dev.deviceName = "Camera1";
    dev.deviceType = ASCOMDeviceType::Camera;
    info.devices.push_back(dev);

    auto json = info.toJson();
    EXPECT_EQ(json["ServerName"], "Test Server");
    EXPECT_EQ(json["Manufacturer"], "Test Mfg");
    EXPECT_EQ(json["Devices"].size(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
