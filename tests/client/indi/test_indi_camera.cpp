/*
 * test_indi_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Camera Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_camera.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDICameraTest : public ::testing::Test {
protected:
    void SetUp() override { camera_ = std::make_unique<INDICamera>("TestCamera"); }

    void TearDown() override { camera_.reset(); }

    std::unique_ptr<INDICamera> camera_;
};

// ==================== Construction Tests ====================

TEST_F(INDICameraTest, ConstructorSetsName) {
    EXPECT_EQ(camera_->getName(), "TestCamera");
}

TEST_F(INDICameraTest, GetDeviceTypeReturnsCamera) {
    EXPECT_EQ(camera_->getDeviceType(), "Camera");
}

TEST_F(INDICameraTest, InitialStateIsIdle) {
    EXPECT_EQ(camera_->getCameraState(), CameraState::Idle);
}

TEST_F(INDICameraTest, InitiallyNotExposing) {
    EXPECT_FALSE(camera_->isExposing());
}

TEST_F(INDICameraTest, InitiallyNotVideoRunning) {
    EXPECT_FALSE(camera_->isVideoRunning());
}

// ==================== Exposure Tests ====================

TEST_F(INDICameraTest, StartExposureFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->startExposure(1.0));
}

TEST_F(INDICameraTest, AbortExposureSucceedsWhenNotExposing) {
    EXPECT_TRUE(camera_->abortExposure());
}

TEST_F(INDICameraTest, GetExposureProgressReturnsNulloptWhenNotExposing) {
    auto progress = camera_->getExposureProgress();
    EXPECT_FALSE(progress.has_value());
}

TEST_F(INDICameraTest, WaitForExposureReturnsTrueWhenNotExposing) {
    EXPECT_TRUE(camera_->waitForExposure(std::chrono::milliseconds(100)));
}

// ==================== Temperature Tests ====================

TEST_F(INDICameraTest, StartCoolingFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->startCooling(-10.0));
}

TEST_F(INDICameraTest, StopCoolingFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->stopCooling());
}

TEST_F(INDICameraTest, InitiallyNoCooler) {
    EXPECT_FALSE(camera_->hasCooler());
}

TEST_F(INDICameraTest, IsCoolerOnReturnsFalseInitially) {
    EXPECT_FALSE(camera_->isCoolerOn());
}

TEST_F(INDICameraTest, GetTemperatureReturnsNulloptWhenNoCooler) {
    auto temp = camera_->getTemperature();
    EXPECT_FALSE(temp.has_value());
}

TEST_F(INDICameraTest, GetCoolerPowerReturnsNulloptWhenNoCooler) {
    auto power = camera_->getCoolerPower();
    EXPECT_FALSE(power.has_value());
}

// ==================== Gain/Offset Tests ====================

TEST_F(INDICameraTest, SetGainFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setGain(50));
}

TEST_F(INDICameraTest, SetOffsetFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setOffset(10));
}

TEST_F(INDICameraTest, GetGainReturnsValue) {
    auto gain = camera_->getGain();
    EXPECT_TRUE(gain.has_value());  // Returns default value
}

TEST_F(INDICameraTest, GetOffsetReturnsValue) {
    auto offset = camera_->getOffset();
    EXPECT_TRUE(offset.has_value());  // Returns default value
}

// ==================== Frame Settings Tests ====================

TEST_F(INDICameraTest, SetFrameFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setFrame(0, 0, 1920, 1080));
}

TEST_F(INDICameraTest, SetBinningFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setBinning(2, 2));
}

TEST_F(INDICameraTest, GetBinningReturnsDefault) {
    auto [binX, binY] = camera_->getBinning();
    EXPECT_EQ(binX, 1);
    EXPECT_EQ(binY, 1);
}

TEST_F(INDICameraTest, SetFrameTypeFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setFrameType(FrameType::Dark));
}

TEST_F(INDICameraTest, GetFrameTypeReturnsDefault) {
    EXPECT_EQ(camera_->getFrameType(), FrameType::Light);
}

TEST_F(INDICameraTest, SetUploadModeFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setUploadMode(UploadMode::Local));
}

// ==================== Video Tests ====================

TEST_F(INDICameraTest, StartVideoFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->startVideo());
}

TEST_F(INDICameraTest, StopVideoSucceedsWhenNotRunning) {
    EXPECT_TRUE(camera_->stopVideo());
}

// ==================== Image Format Tests ====================

TEST_F(INDICameraTest, SetImageFormatFailsWhenDisconnected) {
    EXPECT_FALSE(camera_->setImageFormat(ImageFormat::XISF));
}

TEST_F(INDICameraTest, GetImageFormatReturnsDefault) {
    EXPECT_EQ(camera_->getImageFormat(), ImageFormat::FITS);
}

// ==================== Status Tests ====================

TEST_F(INDICameraTest, GetStatusReturnsValidJson) {
    auto status = camera_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("cameraState"));
    EXPECT_TRUE(status.contains("isExposing"));
    EXPECT_TRUE(status.contains("isVideoRunning"));
    EXPECT_TRUE(status.contains("cooler"));
    EXPECT_TRUE(status.contains("gainOffset"));
    EXPECT_TRUE(status.contains("frame"));
    EXPECT_TRUE(status.contains("sensor"));

    EXPECT_EQ(status["type"], "Camera");
    EXPECT_FALSE(status["isExposing"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(CameraFrameTest, ToJsonProducesValidOutput) {
    CameraFrame frame;
    frame.x = 0;
    frame.y = 0;
    frame.width = 1920;
    frame.height = 1080;
    frame.binX = 2;
    frame.binY = 2;
    frame.bitDepth = 16;
    frame.pixelSizeX = 3.75;
    frame.pixelSizeY = 3.75;
    frame.frameType = FrameType::Light;

    auto j = frame.toJson();

    EXPECT_EQ(j["width"], 1920);
    EXPECT_EQ(j["height"], 1080);
    EXPECT_EQ(j["binX"], 2);
    EXPECT_EQ(j["binY"], 2);
    EXPECT_EQ(j["bitDepth"], 16);
}

TEST(SensorInfoTest, ToJsonProducesValidOutput) {
    SensorInfo info;
    info.maxWidth = 4096;
    info.maxHeight = 4096;
    info.pixelSizeX = 3.75;
    info.pixelSizeY = 3.75;
    info.bitDepth = 16;
    info.maxBinX = 4;
    info.maxBinY = 4;
    info.isColor = false;

    auto j = info.toJson();

    EXPECT_EQ(j["maxWidth"], 4096);
    EXPECT_EQ(j["maxHeight"], 4096);
    EXPECT_EQ(j["maxBinX"], 4);
    EXPECT_FALSE(j["isColor"].get<bool>());
}

TEST(CoolerInfoTest, ToJsonProducesValidOutput) {
    CoolerInfo info;
    info.hasCooler = true;
    info.coolerOn = true;
    info.currentTemp = -10.0;
    info.targetTemp = -15.0;
    info.coolerPower = 75.0;

    auto j = info.toJson();

    EXPECT_TRUE(j["hasCooler"].get<bool>());
    EXPECT_TRUE(j["coolerOn"].get<bool>());
    EXPECT_DOUBLE_EQ(j["currentTemp"].get<double>(), -10.0);
    EXPECT_DOUBLE_EQ(j["targetTemp"].get<double>(), -15.0);
}

TEST(GainOffsetInfoTest, ToJsonProducesValidOutput) {
    GainOffsetInfo info;
    info.gain = 50;
    info.minGain = 0;
    info.maxGain = 100;
    info.offset = 10;
    info.minOffset = 0;
    info.maxOffset = 50;

    auto j = info.toJson();

    EXPECT_EQ(j["gain"], 50);
    EXPECT_EQ(j["minGain"], 0);
    EXPECT_EQ(j["maxGain"], 100);
}

TEST(ExposureResultTest, ToJsonProducesValidOutput) {
    ExposureResult result;
    result.success = true;
    result.filename = "image.fits";
    result.format = ".fits";
    result.size = 8388608;
    result.duration = 30.0;

    auto j = result.toJson();

    EXPECT_TRUE(j["success"].get<bool>());
    EXPECT_EQ(j["filename"], "image.fits");
    EXPECT_EQ(j["format"], ".fits");
    EXPECT_EQ(j["size"], 8388608);
}
