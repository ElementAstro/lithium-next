/*
 * test_indi_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Tests for INDI adapter implementation

*************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "device/service/indi_adapter.hpp"

using namespace lithium::device;

class INDIAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        adapter_ = INDIAdapterFactory::createDefaultAdapter();
    }

    void TearDown() override {
        if (adapter_) {
            adapter_->disconnectServer();
        }
    }

    std::shared_ptr<INDIAdapter> adapter_;
};

// ==================== DefaultINDIAdapter Tests ====================

TEST_F(INDIAdapterTest, CreateDefaultAdapter) {
    EXPECT_NE(adapter_, nullptr);
    EXPECT_FALSE(adapter_->isServerConnected());
}

TEST_F(INDIAdapterTest, ConnectServer) {
    bool result = adapter_->connectServer("localhost", 7624);
    EXPECT_TRUE(result);
    EXPECT_TRUE(adapter_->isServerConnected());
}

TEST_F(INDIAdapterTest, DisconnectServer) {
    adapter_->connectServer("localhost", 7624);
    bool result = adapter_->disconnectServer();
    EXPECT_TRUE(result);
    EXPECT_FALSE(adapter_->isServerConnected());
}

TEST_F(INDIAdapterTest, GetServerInfo) {
    adapter_->connectServer("localhost", 7624);

    auto info = adapter_->getServerInfo();

    EXPECT_EQ(info["host"], "localhost");
    EXPECT_EQ(info["port"], 7624);
    EXPECT_TRUE(info["connected"]);
}

TEST_F(INDIAdapterTest, GetDevicesEmpty) {
    adapter_->connectServer("localhost", 7624);

    auto devices = adapter_->getDevices();
    EXPECT_TRUE(devices.empty());
}

TEST_F(INDIAdapterTest, RegisterDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultINDIAdapter>(adapter_);
    ASSERT_NE(defaultAdapter, nullptr);

    adapter_->connectServer("localhost", 7624);

    INDIDeviceInfo device;
    device.name = "Test Focuser";
    device.driverName = "indi_simulator_focus";
    device.isConnected = false;

    defaultAdapter->registerDevice(device);

    auto devices = adapter_->getDevices();
    EXPECT_EQ(devices.size(), 1);
    EXPECT_EQ(devices[0].name, "Test Focuser");
}

TEST_F(INDIAdapterTest, GetDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultINDIAdapter>(adapter_);
    adapter_->connectServer("localhost", 7624);

    INDIDeviceInfo device;
    device.name = "Test Camera";
    device.driverName = "indi_simulator_ccd";
    defaultAdapter->registerDevice(device);

    auto result = adapter_->getDevice("Test Camera");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "Test Camera");

    auto notFound = adapter_->getDevice("NonExistent");
    EXPECT_FALSE(notFound.has_value());
}

TEST_F(INDIAdapterTest, ConnectDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultINDIAdapter>(adapter_);
    adapter_->connectServer("localhost", 7624);

    INDIDeviceInfo device;
    device.name = "Test Mount";
    device.isConnected = false;
    defaultAdapter->registerDevice(device);

    bool result = adapter_->connectDevice("Test Mount");
    EXPECT_TRUE(result);

    auto deviceInfo = adapter_->getDevice("Test Mount");
    EXPECT_TRUE(deviceInfo->isConnected);
}

TEST_F(INDIAdapterTest, DisconnectDevice) {
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultINDIAdapter>(adapter_);
    adapter_->connectServer("localhost", 7624);

    INDIDeviceInfo device;
    device.name = "Test Mount";
    device.isConnected = true;
    defaultAdapter->registerDevice(device);

    bool result = adapter_->disconnectDevice("Test Mount");
    EXPECT_TRUE(result);

    auto deviceInfo = adapter_->getDevice("Test Mount");
    EXPECT_FALSE(deviceInfo->isConnected);
}

// ==================== INDIPropertyValue Tests ====================

TEST_F(INDIAdapterTest, PropertyValueNumber) {
    INDIPropertyValue prop;
    prop.name = "FOCUS_ABSOLUTE_POSITION";
    prop.type = INDIPropertyType::NUMBER;
    prop.state = INDIPropertyState::OK;
    prop.numberValue = 50000.0;
    prop.numberMin = 0.0;
    prop.numberMax = 100000.0;
    prop.numberStep = 1.0;

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "number");
    EXPECT_EQ(json["name"], "FOCUS_ABSOLUTE_POSITION");
    EXPECT_DOUBLE_EQ(json["value"], 50000.0);
    EXPECT_EQ(json["state"], "Ok");
}

TEST_F(INDIAdapterTest, PropertyValueSwitch) {
    INDIPropertyValue prop;
    prop.name = "CONNECTION";
    prop.type = INDIPropertyType::SWITCH;
    prop.state = INDIPropertyState::OK;
    prop.switchValue = true;

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "switch");
    EXPECT_TRUE(json["value"]);
}

TEST_F(INDIAdapterTest, PropertyValueText) {
    INDIPropertyValue prop;
    prop.name = "DEVICE_PORT";
    prop.type = INDIPropertyType::TEXT;
    prop.state = INDIPropertyState::IDLE;
    prop.textValue = "/dev/ttyUSB0";

    auto json = prop.toJson();

    EXPECT_EQ(json["type"], "text");
    EXPECT_EQ(json["value"], "/dev/ttyUSB0");
}

// ==================== INDIDeviceInfo Tests ====================

TEST_F(INDIAdapterTest, DeviceInfoSerialization) {
    INDIDeviceInfo info;
    info.name = "Simulator Focuser";
    info.driverName = "indi_simulator_focus";
    info.driverVersion = "1.0.0";
    info.driverInterface = "16";
    info.isConnected = true;

    INDIPropertyValue prop;
    prop.name = "FOCUS_POSITION";
    prop.type = INDIPropertyType::NUMBER;
    prop.numberValue = 25000.0;
    info.properties["FOCUS_POSITION"] = prop;

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "Simulator Focuser");
    EXPECT_EQ(json["driver"], "indi_simulator_focus");
    EXPECT_TRUE(json["connected"]);
    EXPECT_TRUE(json["properties"].contains("FOCUS_POSITION"));
}

// ==================== INDIPropertyState Tests ====================

TEST_F(INDIAdapterTest, PropertyStateToString) {
    EXPECT_EQ(indiStateToString(INDIPropertyState::IDLE), "Idle");
    EXPECT_EQ(indiStateToString(INDIPropertyState::OK), "Ok");
    EXPECT_EQ(indiStateToString(INDIPropertyState::BUSY), "Busy");
    EXPECT_EQ(indiStateToString(INDIPropertyState::ALERT), "Alert");
    EXPECT_EQ(indiStateToString(INDIPropertyState::UNKNOWN), "Unknown");
}

// ==================== Event Callback Tests ====================

TEST_F(INDIAdapterTest, EventCallback) {
    bool eventReceived = false;
    INDIEventType lastEventType;

    adapter_->registerEventCallback([&](const INDIEvent& event) {
        eventReceived = true;
        lastEventType = event.type;
    });

    // Callback is registered but events would be emitted during actual
    // operations
    adapter_->unregisterEventCallback();
}

// ==================== Factory Tests ====================

TEST_F(INDIAdapterTest, FactoryCreateDefaultAdapter) {
    auto adapter = INDIAdapterFactory::createDefaultAdapter();
    EXPECT_NE(adapter, nullptr);

    // Verify it's a DefaultINDIAdapter
    auto defaultAdapter =
        std::dynamic_pointer_cast<DefaultINDIAdapter>(adapter);
    EXPECT_NE(defaultAdapter, nullptr);
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
