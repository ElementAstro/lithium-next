/*
 * test_indi_device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Device Factory and Manager

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_device_factory.hpp"

using namespace lithium::client::indi;

// ==================== Device Type Conversion Tests ====================

TEST(DeviceTypeTest, DeviceTypeToStringConversion) {
    EXPECT_EQ(deviceTypeToString(DeviceType::Camera), "Camera");
    EXPECT_EQ(deviceTypeToString(DeviceType::Focuser), "Focuser");
    EXPECT_EQ(deviceTypeToString(DeviceType::FilterWheel), "FilterWheel");
    EXPECT_EQ(deviceTypeToString(DeviceType::Telescope), "Telescope");
    EXPECT_EQ(deviceTypeToString(DeviceType::Unknown), "Unknown");
}

TEST(DeviceTypeTest, DeviceTypeFromStringConversion) {
    EXPECT_EQ(deviceTypeFromString("Camera"), DeviceType::Camera);
    EXPECT_EQ(deviceTypeFromString("CCD"), DeviceType::Camera);
    EXPECT_EQ(deviceTypeFromString("Focuser"), DeviceType::Focuser);
    EXPECT_EQ(deviceTypeFromString("FilterWheel"), DeviceType::FilterWheel);
    EXPECT_EQ(deviceTypeFromString("Filter Wheel"), DeviceType::FilterWheel);
    EXPECT_EQ(deviceTypeFromString("Telescope"), DeviceType::Telescope);
    EXPECT_EQ(deviceTypeFromString("Mount"), DeviceType::Telescope);
    EXPECT_EQ(deviceTypeFromString("Invalid"), DeviceType::Unknown);
}

// ==================== INDIDeviceFactory Tests ====================

class INDIDeviceFactoryTest : public ::testing::Test {
protected:
    INDIDeviceFactory& factory_ = INDIDeviceFactory::getInstance();
};

TEST_F(INDIDeviceFactoryTest, SingletonInstance) {
    auto& instance1 = INDIDeviceFactory::getInstance();
    auto& instance2 = INDIDeviceFactory::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(INDIDeviceFactoryTest, CreateCameraByType) {
    auto device = factory_.createDevice(DeviceType::Camera, "TestCamera");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "Camera");
    EXPECT_EQ(device->getName(), "TestCamera");
}

TEST_F(INDIDeviceFactoryTest, CreateFocuserByType) {
    auto device = factory_.createDevice(DeviceType::Focuser, "TestFocuser");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "Focuser");
    EXPECT_EQ(device->getName(), "TestFocuser");
}

TEST_F(INDIDeviceFactoryTest, CreateFilterWheelByType) {
    auto device = factory_.createDevice(DeviceType::FilterWheel, "TestFW");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "FilterWheel");
    EXPECT_EQ(device->getName(), "TestFW");
}

TEST_F(INDIDeviceFactoryTest, CreateTelescopeByType) {
    auto device = factory_.createDevice(DeviceType::Telescope, "TestScope");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "Telescope");
    EXPECT_EQ(device->getName(), "TestScope");
}

TEST_F(INDIDeviceFactoryTest, CreateDeviceByTypeString) {
    auto device = factory_.createDevice("Camera", "TestCamera");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "Camera");
}

TEST_F(INDIDeviceFactoryTest, CreateDeviceByAlternateTypeString) {
    auto device = factory_.createDevice("CCD", "TestCCD");
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), "Camera");
}

TEST_F(INDIDeviceFactoryTest, CreateDeviceUnknownTypeReturnsNull) {
    auto device = factory_.createDevice("InvalidType", "Test");
    EXPECT_EQ(device, nullptr);
}

TEST_F(INDIDeviceFactoryTest, CreateCameraDirectly) {
    auto camera = factory_.createCamera("DirectCamera");
    ASSERT_NE(camera, nullptr);
    EXPECT_EQ(camera->getDeviceType(), "Camera");
}

TEST_F(INDIDeviceFactoryTest, CreateFocuserDirectly) {
    auto focuser = factory_.createFocuser("DirectFocuser");
    ASSERT_NE(focuser, nullptr);
    EXPECT_EQ(focuser->getDeviceType(), "Focuser");
}

TEST_F(INDIDeviceFactoryTest, CreateFilterWheelDirectly) {
    auto fw = factory_.createFilterWheel("DirectFW");
    ASSERT_NE(fw, nullptr);
    EXPECT_EQ(fw->getDeviceType(), "FilterWheel");
}

TEST_F(INDIDeviceFactoryTest, CreateTelescopeDirectly) {
    auto telescope = factory_.createTelescope("DirectScope");
    ASSERT_NE(telescope, nullptr);
    EXPECT_EQ(telescope->getDeviceType(), "Telescope");
}

TEST_F(INDIDeviceFactoryTest, IsSupportedReturnsTrue) {
    EXPECT_TRUE(factory_.isSupported(DeviceType::Camera));
    EXPECT_TRUE(factory_.isSupported(DeviceType::Focuser));
    EXPECT_TRUE(factory_.isSupported(DeviceType::FilterWheel));
    EXPECT_TRUE(factory_.isSupported(DeviceType::Telescope));
}

TEST_F(INDIDeviceFactoryTest, GetSupportedTypesReturnsAll) {
    auto types = factory_.getSupportedTypes();
    EXPECT_GE(types.size(), 4);  // At least 4 default types
}

// ==================== INDIDeviceManager Tests ====================

class INDIDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override { manager_ = std::make_unique<INDIDeviceManager>(); }

    void TearDown() override { manager_.reset(); }

    std::unique_ptr<INDIDeviceManager> manager_;
    INDIDeviceFactory& factory_ = INDIDeviceFactory::getInstance();
};

TEST_F(INDIDeviceManagerTest, InitiallyEmpty) {
    EXPECT_EQ(manager_->getDeviceCount(), 0);
}

TEST_F(INDIDeviceManagerTest, AddDevice) {
    auto camera = factory_.createCamera("Camera1");
    EXPECT_TRUE(manager_->addDevice(camera));
    EXPECT_EQ(manager_->getDeviceCount(), 1);
}

TEST_F(INDIDeviceManagerTest, AddNullDeviceFails) {
    EXPECT_FALSE(manager_->addDevice(nullptr));
}

TEST_F(INDIDeviceManagerTest, AddDuplicateDeviceFails) {
    auto camera1 = factory_.createCamera("Camera1");
    auto camera2 = factory_.createCamera("Camera1");  // Same name

    EXPECT_TRUE(manager_->addDevice(camera1));
    EXPECT_FALSE(manager_->addDevice(camera2));
    EXPECT_EQ(manager_->getDeviceCount(), 1);
}

TEST_F(INDIDeviceManagerTest, RemoveDevice) {
    auto camera = factory_.createCamera("Camera1");
    manager_->addDevice(camera);

    EXPECT_TRUE(manager_->removeDevice("Camera1"));
    EXPECT_EQ(manager_->getDeviceCount(), 0);
}

TEST_F(INDIDeviceManagerTest, RemoveNonexistentDeviceFails) {
    EXPECT_FALSE(manager_->removeDevice("NonexistentDevice"));
}

TEST_F(INDIDeviceManagerTest, GetDevice) {
    auto camera = factory_.createCamera("Camera1");
    manager_->addDevice(camera);

    auto retrieved = manager_->getDevice("Camera1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getName(), "Camera1");
}

TEST_F(INDIDeviceManagerTest, GetNonexistentDeviceReturnsNull) {
    auto device = manager_->getDevice("NonexistentDevice");
    EXPECT_EQ(device, nullptr);
}

TEST_F(INDIDeviceManagerTest, GetDeviceWithType) {
    auto camera = factory_.createCamera("Camera1");
    manager_->addDevice(camera);

    auto retrieved = manager_->getDevice<INDICamera>("Camera1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getDeviceType(), "Camera");
}

TEST_F(INDIDeviceManagerTest, GetDeviceWithWrongTypeReturnsNull) {
    auto camera = factory_.createCamera("Camera1");
    manager_->addDevice(camera);

    auto retrieved = manager_->getDevice<INDIFocuser>("Camera1");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(INDIDeviceManagerTest, GetDevices) {
    manager_->addDevice(factory_.createCamera("Camera1"));
    manager_->addDevice(factory_.createFocuser("Focuser1"));

    auto devices = manager_->getDevices();
    EXPECT_EQ(devices.size(), 2);
}

TEST_F(INDIDeviceManagerTest, GetDevicesByType) {
    manager_->addDevice(factory_.createCamera("Camera1"));
    manager_->addDevice(factory_.createCamera("Camera2"));
    manager_->addDevice(factory_.createFocuser("Focuser1"));

    auto cameras = manager_->getDevicesByType(DeviceType::Camera);
    EXPECT_EQ(cameras.size(), 2);

    auto focusers = manager_->getDevicesByType(DeviceType::Focuser);
    EXPECT_EQ(focusers.size(), 1);
}

TEST_F(INDIDeviceManagerTest, GetCameras) {
    manager_->addDevice(factory_.createCamera("Camera1"));
    manager_->addDevice(factory_.createCamera("Camera2"));
    manager_->addDevice(factory_.createFocuser("Focuser1"));

    auto cameras = manager_->getCameras();
    EXPECT_EQ(cameras.size(), 2);
}

TEST_F(INDIDeviceManagerTest, GetFocusers) {
    manager_->addDevice(factory_.createFocuser("Focuser1"));
    manager_->addDevice(factory_.createCamera("Camera1"));

    auto focusers = manager_->getFocusers();
    EXPECT_EQ(focusers.size(), 1);
}

TEST_F(INDIDeviceManagerTest, GetFilterWheels) {
    manager_->addDevice(factory_.createFilterWheel("FW1"));
    manager_->addDevice(factory_.createCamera("Camera1"));

    auto fws = manager_->getFilterWheels();
    EXPECT_EQ(fws.size(), 1);
}

TEST_F(INDIDeviceManagerTest, GetTelescopes) {
    manager_->addDevice(factory_.createTelescope("Scope1"));
    manager_->addDevice(factory_.createCamera("Camera1"));

    auto scopes = manager_->getTelescopes();
    EXPECT_EQ(scopes.size(), 1);
}

TEST_F(INDIDeviceManagerTest, HasDevice) {
    manager_->addDevice(factory_.createCamera("Camera1"));

    EXPECT_TRUE(manager_->hasDevice("Camera1"));
    EXPECT_FALSE(manager_->hasDevice("Camera2"));
}

TEST_F(INDIDeviceManagerTest, InitializeAll) {
    manager_->addDevice(factory_.createCamera("Camera1"));
    manager_->addDevice(factory_.createFocuser("Focuser1"));

    int count = manager_->initializeAll();
    EXPECT_EQ(count, 2);
}

TEST_F(INDIDeviceManagerTest, Clear) {
    manager_->addDevice(factory_.createCamera("Camera1"));
    manager_->addDevice(factory_.createFocuser("Focuser1"));

    manager_->clear();
    EXPECT_EQ(manager_->getDeviceCount(), 0);
}
