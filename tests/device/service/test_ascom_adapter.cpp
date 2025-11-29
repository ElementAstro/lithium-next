/*
 * test_ascom_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for ASCOM adapter implementation

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "device/service/ascom_adapter.hpp"

using namespace lithium::device;

class ASCOMAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        adapter_ = ASCOMAdapterFactory::createDefaultAdapter();
    }

    void TearDown() override {
        if (adapter_) {
            adapter_->disconnectServer();
        }
    }

    std::shared_ptr<ASCOMAdapter> adapter_;
};

// ==================== DefaultASCOMAdapter Tests ====================

TEST_F(ASCOMAdapterTest, CreateDefaultAdapter) {
    EXPECT_NE(adapter_, nullptr);
    EXPECT_FALSE(adapter_->isServerConnected());
}

TEST_F(ASCOMAdapterTest, ConnectServer) {
    bool result = adapter_->connectServer("localhost", 11111);
    EXPECT_TRUE(result);
    EXPECT_TRUE(adapter_->isServerConnected());
}

TEST_F(ASCOMAdapterTest, DisconnectServer) {
    adapter_->connectServer("localhost", 11111);
    bool result = adapter_->disconnectServer();
    EXPECT_TRUE(result);
    EXPECT_FALSE(adapter_->isServerConnected());
}

TEST_F(ASCOMAdapterTest, GetServerInfo) {
    adapter_->connectServer("localhost", 11111);

    auto info = adapter_->getServerInfo();

    EXPECT_EQ(info["host"], "localhost");
    EXPECT_EQ(info["port"], 11111);
    EXPECT_TRUE(info["connected"]);
}

TEST_F(ASCOMAdapterTest, GetDevicesEmpty) {
    adapter_->connectServer("localhost", 11111);

    auto devices = adapter_->getDevices();
    EXPECT_TRUE(devices.empty());
}

TEST_F(ASCOMAdapterTest, RegisterDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultASCOMAdapter>(adapter_);
    ASSERT_NE(defaultAdapter, nullptr);

    adapter_->connectServer("localhost", 11111);

    ASCOMDeviceInfo device;
    device.name = "Simulator Focuser";
    device.deviceType = "focuser";
    device.deviceNumber = 0;
    device.isConnected = false;

    defaultAdapter->registerDevice(device);

    auto devices = adapter_->getDevices();
    EXPECT_EQ(devices.size(), 1);
    EXPECT_EQ(devices[0].name, "Simulator Focuser");
}

TEST_F(ASCOMAdapterTest, GetDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultASCOMAdapter>(adapter_);
    adapter_->connectServer("localhost", 11111);

    ASCOMDeviceInfo device;
    device.name = "Simulator Camera";
    device.deviceType = "camera";
    defaultAdapter->registerDevice(device);

    auto result = adapter_->getDevice("Simulator Camera");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "Simulator Camera");

    auto notFound = adapter_->getDevice("NonExistent");
    EXPECT_FALSE(notFound.has_value());
}

TEST_F(ASCOMAdapterTest, ConnectDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultASCOMAdapter>(adapter_);
    adapter_->connectServer("localhost", 11111);

    ASCOMDeviceInfo device;
    device.name = "Simulator Telescope";
    device.isConnected = false;
    defaultAdapter->registerDevice(device);

    bool result = adapter_->connectDevice("Simulator Telescope");
    EXPECT_TRUE(result);

    auto deviceInfo = adapter_->getDevice("Simulator Telescope");
    EXPECT_TRUE(deviceInfo->isConnected);
}

TEST_F(ASCOMAdapterTest, DisconnectDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultASCOMAdapter>(adapter_);
    adapter_->connectServer("localhost", 11111);

    ASCOMDeviceInfo device;
    device.name = "Simulator Telescope";
    device.isConnected = true;
    defaultAdapter->registerDevice(device);

    bool result = adapter_->disconnectDevice("Simulator Telescope");
    EXPECT_TRUE(result);

    auto deviceInfo = adapter_->getDevice("Simulator Telescope");
    EXPECT_FALSE(deviceInfo->isConnected);
}

// ==================== ASCOMPropertyValue Tests ====================

TEST_F(ASCOMAdapterTest, PropertyValueNumber) {
    ASCOMPropertyValue prop;
    prop.name = "Position";
    prop.type = ASCOMPropertyType::NUMBER;
    prop.numberValue = 50000.0;

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "number");
    EXPECT_EQ(json["name"], "Position");
    EXPECT_DOUBLE_EQ(json["value"], 50000.0);
}

TEST_F(ASCOMAdapterTest, PropertyValueString) {
    ASCOMPropertyValue prop;
    prop.name = "Description";
    prop.type = ASCOMPropertyType::STRING;
    prop.stringValue = "Simulator Focuser";

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "string");
    EXPECT_EQ(json["value"], "Simulator Focuser");
}

TEST_F(ASCOMAdapterTest, PropertyValueBoolean) {
    ASCOMPropertyValue prop;
    prop.name = "Connected";
    prop.type = ASCOMPropertyType::BOOLEAN;
    prop.boolValue = true;

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "boolean");
    EXPECT_TRUE(json["value"]);
}

TEST_F(ASCOMAdapterTest, PropertyValueArray) {
    ASCOMPropertyValue prop;
    prop.name = "FilterOffsets";
    prop.type = ASCOMPropertyType::ARRAY;
    prop.arrayValue = {0.0, 100.0, 200.0, 300.0};

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "array");
    EXPECT_EQ(json["value"].size(), 4);
}

// ==================== ASCOMDeviceInfo Tests ====================

TEST_F(ASCOMAdapterTest, DeviceInfoSerialization) {
    ASCOMDeviceInfo info;
    info.name = "Simulator Focuser";
    info.deviceType = "focuser";
    info.deviceNumber = 0;
    info.uniqueId = "focuser-001";
    info.driverInfo = "ASCOM Focuser Simulator";
    info.driverVersion = "1.0.0";
    info.isConnected = true;

    ASCOMPropertyValue prop;
    prop.name = "Position";
    prop.type = ASCOMPropertyType::NUMBER;
    prop.numberValue = 25000.0;
    info.properties["Position"] = prop;

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "Simulator Focuser");
    EXPECT_EQ(json["deviceType"], "focuser");
    EXPECT_EQ(json["deviceNumber"], 0);
    EXPECT_TRUE(json["connected"]);
    EXPECT_TRUE(json["properties"].contains("Position"));
}

// ==================== Event Callback Tests ====================

TEST_F(ASCOMAdapterTest, EventCallback) {
    bool eventReceived = false;
    ASCOMEventType lastEventType;

    adapter_->registerEventCallback([&](const ASCOMEvent& event) {
        eventReceived = true;
        lastEventType = event.type;
    });

    // Callback is registered but events would be emitted during actual
    // operations
    adapter_->unregisterEventCallback();
}

// ==================== Factory Tests ====================

TEST_F(ASCOMAdapterTest, FactoryCreateDefaultAdapter) {
    auto adapter = ASCOMAdapterFactory::createDefaultAdapter();
    EXPECT_NE(adapter, nullptr);

    // Verify it's a DefaultASCOMAdapter
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultASCOMAdapter>(adapter);
    EXPECT_NE(defaultAdapter, nullptr);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
