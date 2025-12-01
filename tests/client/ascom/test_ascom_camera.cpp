/*
 * test_ascom_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "client/ascom/ascom_camera.hpp"

using namespace lithium::client::ascom;

// ==================== CameraCapabilities Tests ====================

TEST(CameraCapabilitiesTest, ToJson) {
    CameraCapabilities caps;
    caps.canAbortExposure = true;
    caps.canSetCCDTemperature = true;
    caps.hasShutter = true;

    auto json = caps.toJson();
    EXPECT_TRUE(json["canAbortExposure"]);
    EXPECT_TRUE(json["canSetCCDTemperature"]);
    EXPECT_TRUE(json["hasShutter"]);
    EXPECT_FALSE(json["canPulseGuide"]);
}

// ==================== SensorInfo Tests ====================

TEST(SensorInfoTest, ToJson) {
    SensorInfo info;
    info.cameraXSize = 4656;
    info.cameraYSize = 3520;
    info.pixelSizeX = 3.76;
    info.pixelSizeY = 3.76;
    info.maxBinX = 4;
    info.maxBinY = 4;
    info.sensorName = "IMX294";

    auto json = info.toJson();
    EXPECT_EQ(json["cameraXSize"], 4656);
    EXPECT_EQ(json["cameraYSize"], 3520);
    EXPECT_DOUBLE_EQ(json["pixelSizeX"], 3.76);
    EXPECT_EQ(json["sensorName"], "IMX294");
}

// ==================== ExposureSettings Tests ====================

TEST(ExposureSettingsTest, ToJson) {
    ExposureSettings settings;
    settings.duration = 30.0;
    settings.light = true;
    settings.binX = 2;
    settings.binY = 2;

    auto json = settings.toJson();
    EXPECT_DOUBLE_EQ(json["duration"], 30.0);
    EXPECT_TRUE(json["light"]);
    EXPECT_EQ(json["binX"], 2);
}

// ==================== TemperatureInfo Tests ====================

TEST(TemperatureInfoTest, ToJson) {
    TemperatureInfo info;
    info.ccdTemperature = -10.0;
    info.setPoint = -15.0;
    info.coolerPower = 75.0;
    info.coolerOn = true;

    auto json = info.toJson();
    EXPECT_DOUBLE_EQ(json["ccdTemperature"], -10.0);
    EXPECT_DOUBLE_EQ(json["setPoint"], -15.0);
    EXPECT_DOUBLE_EQ(json["coolerPower"], 75.0);
    EXPECT_TRUE(json["coolerOn"]);
}

// ==================== GainSettings Tests ====================

TEST(GainSettingsTest, ToJson) {
    GainSettings settings;
    settings.gain = 100;
    settings.gainMin = 0;
    settings.gainMax = 300;
    settings.offset = 10;

    auto json = settings.toJson();
    EXPECT_EQ(json["gain"], 100);
    EXPECT_EQ(json["gainMin"], 0);
    EXPECT_EQ(json["gainMax"], 300);
    EXPECT_EQ(json["offset"], 10);
}

// ==================== ASCOMCamera Tests ====================

class ASCOMCameraTest : public ::testing::Test {
protected:
    void SetUp() override {
        camera = std::make_unique<ASCOMCamera>("TestCamera", 0);
    }

    std::unique_ptr<ASCOMCamera> camera;
};

TEST_F(ASCOMCameraTest, Construction) {
    EXPECT_EQ(camera->getName(), "TestCamera");
    EXPECT_EQ(camera->getDeviceType(), "Camera");
    EXPECT_EQ(camera->getDeviceNumber(), 0);
    EXPECT_EQ(camera->getASCOMDeviceType(), ASCOMDeviceType::Camera);
}

TEST_F(ASCOMCameraTest, InitialState) {
    EXPECT_EQ(camera->getState(), DeviceState::Disconnected);
    EXPECT_FALSE(camera->isConnected());
    EXPECT_FALSE(camera->isExposing());
}

TEST_F(ASCOMCameraTest, CameraState) {
    EXPECT_EQ(camera->getCameraState(), CameraState::Idle);
}

TEST_F(ASCOMCameraTest, StatusJson) {
    auto status = camera->getStatus();
    EXPECT_EQ(status["name"], "TestCamera");
    EXPECT_EQ(status["type"], "Camera");
    EXPECT_FALSE(status["connected"]);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
