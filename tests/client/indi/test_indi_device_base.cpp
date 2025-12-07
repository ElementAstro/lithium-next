/*
 * test_indi_device_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Device Base Class

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_device_base.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIDeviceBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a concrete implementation for testing
        device_ = std::make_unique<TestDevice>("TestDevice");
    }

    void TearDown() override { device_.reset(); }

    // Concrete implementation for testing abstract base class
    class TestDevice : public INDIDeviceBase {
    public:
        explicit TestDevice(std::string name)
            : INDIDeviceBase(std::move(name)) {}

        [[nodiscard]] auto getDeviceType() const -> std::string override {
            return "TestDevice";
        }
    };

    std::unique_ptr<TestDevice> device_;
};

// ==================== Construction Tests ====================

TEST_F(INDIDeviceBaseTest, ConstructorSetsName) {
    EXPECT_EQ(device_->getName(), "TestDevice");
}

TEST_F(INDIDeviceBaseTest, InitialStateIsDisconnected) {
    EXPECT_FALSE(device_->isConnected());
    EXPECT_EQ(device_->getConnectionState(), ConnectionState::Disconnected);
}

TEST_F(INDIDeviceBaseTest, GetDeviceTypeReturnsCorrectType) {
    EXPECT_EQ(device_->getDeviceType(), "TestDevice");
}

// ==================== Lifecycle Tests ====================

TEST_F(INDIDeviceBaseTest, InitializeSucceeds) {
    EXPECT_TRUE(device_->initialize());
}

TEST_F(INDIDeviceBaseTest, InitializeTwiceSucceeds) {
    EXPECT_TRUE(device_->initialize());
    EXPECT_TRUE(
        device_->initialize());  // Should return true (already initialized)
}

TEST_F(INDIDeviceBaseTest, DestroySucceeds) {
    device_->initialize();
    EXPECT_TRUE(device_->destroy());
}

TEST_F(INDIDeviceBaseTest, DestroyWithoutInitializeSucceeds) {
    EXPECT_TRUE(device_->destroy());
}

// ==================== Connection Tests ====================

TEST_F(INDIDeviceBaseTest, ConnectSetsDeviceName) {
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);
    EXPECT_EQ(device_->getDeviceName(), "INDI_Device");
}

TEST_F(INDIDeviceBaseTest, ConnectChangesState) {
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);
    EXPECT_TRUE(device_->isConnected());
    EXPECT_EQ(device_->getConnectionState(), ConnectionState::Connected);
}

TEST_F(INDIDeviceBaseTest, DisconnectChangesState) {
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);
    device_->disconnect();
    EXPECT_FALSE(device_->isConnected());
    EXPECT_EQ(device_->getConnectionState(), ConnectionState::Disconnected);
}

TEST_F(INDIDeviceBaseTest, ConnectWhenAlreadyConnectedSucceeds) {
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);
    EXPECT_TRUE(device_->connect("INDI_Device", 5000, 3));
}

// ==================== Property Tests ====================

TEST_F(INDIDeviceBaseTest, GetPropertiesInitiallyEmpty) {
    auto props = device_->getProperties();
    EXPECT_TRUE(props.empty());
}

TEST_F(INDIDeviceBaseTest, GetPropertyReturnsNulloptForNonexistent) {
    auto prop = device_->getProperty("NONEXISTENT");
    EXPECT_FALSE(prop.has_value());
}

TEST_F(INDIDeviceBaseTest, SetNumberPropertyFailsWhenDisconnected) {
    EXPECT_FALSE(device_->setNumberProperty("PROP", "ELEM", 1.0));
}

TEST_F(INDIDeviceBaseTest, SetTextPropertyFailsWhenDisconnected) {
    EXPECT_FALSE(device_->setTextProperty("PROP", "ELEM", "value"));
}

TEST_F(INDIDeviceBaseTest, SetSwitchPropertyFailsWhenDisconnected) {
    EXPECT_FALSE(device_->setSwitchProperty("PROP", "ELEM", true));
}

// ==================== Event System Tests ====================

TEST_F(INDIDeviceBaseTest, RegisterEventCallback) {
    bool callbackCalled = false;
    device_->registerEventCallback([&](const DeviceEvent& event) {
        callbackCalled = true;
        EXPECT_EQ(event.type, DeviceEventType::Connected);
    });

    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);

    EXPECT_TRUE(callbackCalled);
}

TEST_F(INDIDeviceBaseTest, UnregisterEventCallback) {
    bool callbackCalled = false;
    device_->registerEventCallback(
        [&](const DeviceEvent&) { callbackCalled = true; });

    device_->unregisterEventCallback();
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);

    EXPECT_FALSE(callbackCalled);
}

TEST_F(INDIDeviceBaseTest, WatchPropertyRegistersCallback) {
    bool callbackCalled = false;
    device_->watchProperty("TEST_PROP",
                           [&](const INDIProperty&) { callbackCalled = true; });

    // Callback should be registered but not called yet
    EXPECT_FALSE(callbackCalled);
}

TEST_F(INDIDeviceBaseTest, UnwatchPropertyRemovesCallback) {
    device_->watchProperty("TEST_PROP", [](const INDIProperty&) {});
    device_->unwatchProperty("TEST_PROP");
    // Should not crash
}

// ==================== Status Tests ====================

TEST_F(INDIDeviceBaseTest, GetStatusReturnsValidJson) {
    auto status = device_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("deviceName"));
    EXPECT_TRUE(status.contains("connected"));
    EXPECT_TRUE(status.contains("initialized"));
    EXPECT_TRUE(status.contains("type"));

    EXPECT_EQ(status["name"], "TestDevice");
    EXPECT_EQ(status["type"], "TestDevice");
    EXPECT_FALSE(status["connected"].get<bool>());
}

TEST_F(INDIDeviceBaseTest, GetStatusReflectsConnectionState) {
    device_->initialize();
    device_->connect("INDI_Device", 5000, 3);

    auto status = device_->getStatus();
    EXPECT_TRUE(status["connected"].get<bool>());
    EXPECT_EQ(status["deviceName"], "INDI_Device");
}

// ==================== Property State Conversion Tests ====================

TEST(PropertyStateTest, PropertyStateToStringConversion) {
    EXPECT_EQ(propertyStateToString(PropertyState::Idle), "Idle");
    EXPECT_EQ(propertyStateToString(PropertyState::Ok), "Ok");
    EXPECT_EQ(propertyStateToString(PropertyState::Busy), "Busy");
    EXPECT_EQ(propertyStateToString(PropertyState::Alert), "Alert");
    EXPECT_EQ(propertyStateToString(PropertyState::Unknown), "Unknown");
}

TEST(PropertyStateTest, PropertyStateFromStringConversion) {
    EXPECT_EQ(propertyStateFromString("Idle"), PropertyState::Idle);
    EXPECT_EQ(propertyStateFromString("Ok"), PropertyState::Ok);
    EXPECT_EQ(propertyStateFromString("Busy"), PropertyState::Busy);
    EXPECT_EQ(propertyStateFromString("Alert"), PropertyState::Alert);
    EXPECT_EQ(propertyStateFromString("Unknown"), PropertyState::Unknown);
    EXPECT_EQ(propertyStateFromString("Invalid"), PropertyState::Unknown);
}

// ==================== INDIProperty Tests ====================

TEST(INDIPropertyTest, GetNumberReturnsValue) {
    INDIProperty prop;
    prop.type = PropertyType::Number;
    prop.numbers.push_back({"ELEM1", "Element 1", 42.0, 0.0, 100.0, 1.0, "%g"});

    auto value = prop.getNumber("ELEM1");
    ASSERT_TRUE(value.has_value());
    EXPECT_DOUBLE_EQ(*value, 42.0);
}

TEST(INDIPropertyTest, GetNumberReturnsNulloptForNonexistent) {
    INDIProperty prop;
    prop.type = PropertyType::Number;

    auto value = prop.getNumber("NONEXISTENT");
    EXPECT_FALSE(value.has_value());
}

TEST(INDIPropertyTest, GetTextReturnsValue) {
    INDIProperty prop;
    prop.type = PropertyType::Text;
    prop.texts.push_back({"ELEM1", "Element 1", "test_value"});

    auto value = prop.getText("ELEM1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "test_value");
}

TEST(INDIPropertyTest, GetSwitchReturnsValue) {
    INDIProperty prop;
    prop.type = PropertyType::Switch;
    prop.switches.push_back({"ELEM1", "Element 1", true});

    auto value = prop.getSwitch("ELEM1");
    ASSERT_TRUE(value.has_value());
    EXPECT_TRUE(*value);
}

TEST(INDIPropertyTest, IsWritableChecksPermission) {
    INDIProperty prop;

    prop.permission = "ro";
    EXPECT_FALSE(prop.isWritable());

    prop.permission = "wo";
    EXPECT_TRUE(prop.isWritable());

    prop.permission = "rw";
    EXPECT_TRUE(prop.isWritable());
}

TEST(INDIPropertyTest, IsReadableChecksPermission) {
    INDIProperty prop;

    prop.permission = "ro";
    EXPECT_TRUE(prop.isReadable());

    prop.permission = "wo";
    EXPECT_FALSE(prop.isReadable());

    prop.permission = "rw";
    EXPECT_TRUE(prop.isReadable());
}

TEST(INDIPropertyTest, ToJsonProducesValidOutput) {
    INDIProperty prop;
    prop.device = "TestDevice";
    prop.name = "TEST_PROP";
    prop.label = "Test Property";
    prop.group = "Main";
    prop.type = PropertyType::Number;
    prop.state = PropertyState::Ok;
    prop.permission = "rw";
    prop.numbers.push_back({"VALUE", "Value", 50.0, 0.0, 100.0, 1.0, "%g"});

    auto j = prop.toJson();

    EXPECT_EQ(j["device"], "TestDevice");
    EXPECT_EQ(j["name"], "TEST_PROP");
    EXPECT_EQ(j["type"], "number");
    EXPECT_EQ(j["state"], "Ok");
    EXPECT_TRUE(j.contains("elements"));
    EXPECT_EQ(j["elements"].size(), 1);
}

// ==================== Element ToJson Tests ====================

TEST(NumberElementTest, ToJsonProducesValidOutput) {
    NumberElement elem{"GAIN", "Gain", 50.0, 0.0, 100.0, 1.0, "%g"};
    auto j = elem.toJson();

    EXPECT_EQ(j["name"], "GAIN");
    EXPECT_EQ(j["label"], "Gain");
    EXPECT_DOUBLE_EQ(j["value"].get<double>(), 50.0);
    EXPECT_DOUBLE_EQ(j["min"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["max"].get<double>(), 100.0);
}

TEST(TextElementTest, ToJsonProducesValidOutput) {
    TextElement elem{"NAME", "Name", "Test"};
    auto j = elem.toJson();

    EXPECT_EQ(j["name"], "NAME");
    EXPECT_EQ(j["value"], "Test");
}

TEST(SwitchElementTest, ToJsonProducesValidOutput) {
    SwitchElement elem{"ENABLED", "Enabled", true};
    auto j = elem.toJson();

    EXPECT_EQ(j["name"], "ENABLED");
    EXPECT_TRUE(j["on"].get<bool>());
}

TEST(LightElementTest, ToJsonProducesValidOutput) {
    LightElement elem{"STATUS", "Status", PropertyState::Ok};
    auto j = elem.toJson();

    EXPECT_EQ(j["name"], "STATUS");
    EXPECT_EQ(j["state"], "Ok");
}

TEST(BlobElementTest, ToJsonProducesValidOutput) {
    BlobElement elem{"IMAGE", "Image", ".fits", {}, 1024};
    auto j = elem.toJson();

    EXPECT_EQ(j["name"], "IMAGE");
    EXPECT_EQ(j["format"], ".fits");
    EXPECT_EQ(j["size"], 1024);
}
