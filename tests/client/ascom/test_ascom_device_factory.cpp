/*
 * test_ascom_device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "client/ascom/ascom_device_factory.hpp"

using namespace lithium::client::ascom;

// ==================== ASCOMDeviceFactory Tests ====================

class ASCOMDeviceFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        factory = &ASCOMDeviceFactory::getInstance();
    }

    ASCOMDeviceFactory* factory;
};

TEST_F(ASCOMDeviceFactoryTest, Singleton) {
    auto& instance1 = ASCOMDeviceFactory::getInstance();
    auto& instance2 = ASCOMDeviceFactory::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ASCOMDeviceFactoryTest, CreateCamera) {
    auto device = factory->createDevice(ASCOMDeviceType::Camera, "TestCamera", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getName(), "TestCamera");
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Camera);
}

TEST_F(ASCOMDeviceFactoryTest, CreateFocuser) {
    auto device = factory->createDevice(ASCOMDeviceType::Focuser, "TestFocuser", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Focuser);
}

TEST_F(ASCOMDeviceFactoryTest, CreateFilterWheel) {
    auto device = factory->createDevice(ASCOMDeviceType::FilterWheel, "TestFW", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::FilterWheel);
}

TEST_F(ASCOMDeviceFactoryTest, CreateTelescope) {
    auto device = factory->createDevice(ASCOMDeviceType::Telescope, "TestMount", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Telescope);
}

TEST_F(ASCOMDeviceFactoryTest, CreateRotator) {
    auto device = factory->createDevice(ASCOMDeviceType::Rotator, "TestRotator", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Rotator);
}

TEST_F(ASCOMDeviceFactoryTest, CreateDome) {
    auto device = factory->createDevice(ASCOMDeviceType::Dome, "TestDome", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Dome);
}

TEST_F(ASCOMDeviceFactoryTest, CreateObservingConditions) {
    auto device = factory->createDevice(ASCOMDeviceType::ObservingConditions, "TestWeather", 0);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::ObservingConditions);
}

TEST_F(ASCOMDeviceFactoryTest, CreateByString) {
    auto device = factory->createDevice("camera", "StringCamera", 1);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getASCOMDeviceType(), ASCOMDeviceType::Camera);
    EXPECT_EQ(device->getDeviceNumber(), 1);
}

TEST_F(ASCOMDeviceFactoryTest, CreateUnknownType) {
    auto device = factory->createDevice(ASCOMDeviceType::Unknown, "Unknown", 0);
    EXPECT_EQ(device, nullptr);
}

TEST_F(ASCOMDeviceFactoryTest, IsSupported) {
    EXPECT_TRUE(factory->isSupported(ASCOMDeviceType::Camera));
    EXPECT_TRUE(factory->isSupported(ASCOMDeviceType::Focuser));
    EXPECT_TRUE(factory->isSupported(ASCOMDeviceType::Telescope));
    EXPECT_FALSE(factory->isSupported(ASCOMDeviceType::Unknown));
}

TEST_F(ASCOMDeviceFactoryTest, GetSupportedTypes) {
    auto types = factory->getSupportedTypes();
    EXPECT_GE(types.size(), 7);  // At least 7 device types
}

TEST_F(ASCOMDeviceFactoryTest, TypedCreation) {
    auto camera = factory->createCamera("TypedCamera", 0);
    ASSERT_NE(camera, nullptr);
    EXPECT_EQ(camera->getDeviceType(), "Camera");

    auto focuser = factory->createFocuser("TypedFocuser", 0);
    ASSERT_NE(focuser, nullptr);
    EXPECT_EQ(focuser->getDeviceType(), "Focuser");
}

// ==================== ASCOMDeviceManager Tests ====================

class ASCOMDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<ASCOMDeviceManager>();
    }

    std::unique_ptr<ASCOMDeviceManager> manager;
};

TEST_F(ASCOMDeviceManagerTest, AddDevice) {
    auto camera = std::make_shared<ASCOMCamera>("Camera1", 0);
    EXPECT_TRUE(manager->addDevice(camera));
    EXPECT_TRUE(manager->hasDevice("Camera1"));
    EXPECT_EQ(manager->getDeviceCount(), 1);
}

TEST_F(ASCOMDeviceManagerTest, AddDuplicateDevice) {
    auto camera1 = std::make_shared<ASCOMCamera>("Camera1", 0);
    auto camera2 = std::make_shared<ASCOMCamera>("Camera1", 1);

    EXPECT_TRUE(manager->addDevice(camera1));
    EXPECT_FALSE(manager->addDevice(camera2));  // Duplicate name
    EXPECT_EQ(manager->getDeviceCount(), 1);
}

TEST_F(ASCOMDeviceManagerTest, RemoveDevice) {
    auto camera = std::make_shared<ASCOMCamera>("Camera1", 0);
    manager->addDevice(camera);

    EXPECT_TRUE(manager->removeDevice("Camera1"));
    EXPECT_FALSE(manager->hasDevice("Camera1"));
    EXPECT_EQ(manager->getDeviceCount(), 0);
}

TEST_F(ASCOMDeviceManagerTest, GetDevice) {
    auto camera = std::make_shared<ASCOMCamera>("Camera1", 0);
    manager->addDevice(camera);

    auto retrieved = manager->getDevice("Camera1");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getName(), "Camera1");

    auto notFound = manager->getDevice("NonExistent");
    EXPECT_EQ(notFound, nullptr);
}

TEST_F(ASCOMDeviceManagerTest, GetAllDevices) {
    manager->addDevice(std::make_shared<ASCOMCamera>("Camera1", 0));
    manager->addDevice(std::make_shared<ASCOMFocuser>("Focuser1", 0));

    auto devices = manager->getAllDevices();
    EXPECT_EQ(devices.size(), 2);
}

TEST_F(ASCOMDeviceManagerTest, GetDevicesByType) {
    manager->addDevice(std::make_shared<ASCOMCamera>("Camera1", 0));
    manager->addDevice(std::make_shared<ASCOMCamera>("Camera2", 1));
    manager->addDevice(std::make_shared<ASCOMFocuser>("Focuser1", 0));

    auto cameras = manager->getDevicesByType(ASCOMDeviceType::Camera);
    EXPECT_EQ(cameras.size(), 2);

    auto focusers = manager->getDevicesByType(ASCOMDeviceType::Focuser);
    EXPECT_EQ(focusers.size(), 1);
}

TEST_F(ASCOMDeviceManagerTest, TypedGetters) {
    manager->addDevice(std::make_shared<ASCOMCamera>("Camera1", 0));
    manager->addDevice(std::make_shared<ASCOMFocuser>("Focuser1", 0));
    manager->addDevice(std::make_shared<ASCOMTelescope>("Mount1", 0));

    EXPECT_EQ(manager->getCameras().size(), 1);
    EXPECT_EQ(manager->getFocusers().size(), 1);
    EXPECT_EQ(manager->getTelescopes().size(), 1);
    EXPECT_EQ(manager->getFilterWheels().size(), 0);
}

TEST_F(ASCOMDeviceManagerTest, Clear) {
    manager->addDevice(std::make_shared<ASCOMCamera>("Camera1", 0));
    manager->addDevice(std::make_shared<ASCOMFocuser>("Focuser1", 0));

    manager->clear();
    EXPECT_EQ(manager->getDeviceCount(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
