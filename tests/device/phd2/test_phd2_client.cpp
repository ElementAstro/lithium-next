/*
 * test_phd2_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "client/phd2/phd2_client.hpp"
#include "client/common/guider_client.hpp"

using namespace lithium::client;
using namespace testing;

// ============================================================================
// GuideStar Tests
// ============================================================================

class GuideStarTest : public ::testing::Test {
protected:
    GuideStar star;
};

TEST_F(GuideStarTest, DefaultConstruction) {
    EXPECT_DOUBLE_EQ(star.x, 0.0);
    EXPECT_DOUBLE_EQ(star.y, 0.0);
    EXPECT_DOUBLE_EQ(star.snr, 0.0);
    EXPECT_DOUBLE_EQ(star.mass, 0.0);
    EXPECT_FALSE(star.valid);
}

TEST_F(GuideStarTest, SetValues) {
    star.x = 512.5;
    star.y = 384.2;
    star.snr = 25.5;
    star.mass = 1500.0;
    star.valid = true;

    EXPECT_DOUBLE_EQ(star.x, 512.5);
    EXPECT_DOUBLE_EQ(star.y, 384.2);
    EXPECT_DOUBLE_EQ(star.snr, 25.5);
    EXPECT_DOUBLE_EQ(star.mass, 1500.0);
    EXPECT_TRUE(star.valid);
}

// ============================================================================
// GuideStats Tests
// ============================================================================

class GuideStatsTest : public ::testing::Test {
protected:
    GuideStats stats;
};

TEST_F(GuideStatsTest, DefaultConstruction) {
    EXPECT_DOUBLE_EQ(stats.rmsRA, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsDec, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsTotal, 0.0);
    EXPECT_DOUBLE_EQ(stats.peakRA, 0.0);
    EXPECT_DOUBLE_EQ(stats.peakDec, 0.0);
    EXPECT_EQ(stats.sampleCount, 0);
    EXPECT_DOUBLE_EQ(stats.snr, 0.0);
}

TEST_F(GuideStatsTest, SetValues) {
    stats.rmsRA = 0.45;
    stats.rmsDec = 0.52;
    stats.rmsTotal = 0.69;
    stats.peakRA = 1.2;
    stats.peakDec = 1.5;
    stats.sampleCount = 100;
    stats.snr = 30.0;

    EXPECT_DOUBLE_EQ(stats.rmsRA, 0.45);
    EXPECT_DOUBLE_EQ(stats.rmsDec, 0.52);
    EXPECT_DOUBLE_EQ(stats.rmsTotal, 0.69);
    EXPECT_DOUBLE_EQ(stats.peakRA, 1.2);
    EXPECT_DOUBLE_EQ(stats.peakDec, 1.5);
    EXPECT_EQ(stats.sampleCount, 100);
    EXPECT_DOUBLE_EQ(stats.snr, 30.0);
}

// ============================================================================
// SettleParams Tests
// ============================================================================

class SettleParamsTest : public ::testing::Test {
protected:
    SettleParams params;
};

TEST_F(SettleParamsTest, DefaultValues) {
    EXPECT_DOUBLE_EQ(params.pixels, 1.5);
    EXPECT_DOUBLE_EQ(params.time, 10.0);
    EXPECT_DOUBLE_EQ(params.timeout, 60.0);
}

TEST_F(SettleParamsTest, CustomValues) {
    params.pixels = 2.0;
    params.time = 15.0;
    params.timeout = 120.0;

    EXPECT_DOUBLE_EQ(params.pixels, 2.0);
    EXPECT_DOUBLE_EQ(params.time, 15.0);
    EXPECT_DOUBLE_EQ(params.timeout, 120.0);
}

// ============================================================================
// DitherParams Tests
// ============================================================================

class DitherParamsTest : public ::testing::Test {
protected:
    DitherParams params;
};

TEST_F(DitherParamsTest, DefaultValues) {
    EXPECT_DOUBLE_EQ(params.amount, 5.0);
    EXPECT_FALSE(params.raOnly);
    EXPECT_DOUBLE_EQ(params.settle.pixels, 1.5);
}

TEST_F(DitherParamsTest, CustomValues) {
    params.amount = 10.0;
    params.raOnly = true;
    params.settle.pixels = 2.5;
    params.settle.time = 20.0;

    EXPECT_DOUBLE_EQ(params.amount, 10.0);
    EXPECT_TRUE(params.raOnly);
    EXPECT_DOUBLE_EQ(params.settle.pixels, 2.5);
    EXPECT_DOUBLE_EQ(params.settle.time, 20.0);
}

// ============================================================================
// CalibrationData Tests
// ============================================================================

class CalibrationDataTest : public ::testing::Test {
protected:
    CalibrationData calData;
};

TEST_F(CalibrationDataTest, DefaultValues) {
    EXPECT_FALSE(calData.calibrated);
    EXPECT_DOUBLE_EQ(calData.raRate, 0.0);
    EXPECT_DOUBLE_EQ(calData.decRate, 0.0);
    EXPECT_DOUBLE_EQ(calData.raAngle, 0.0);
    EXPECT_DOUBLE_EQ(calData.decAngle, 0.0);
    EXPECT_FALSE(calData.decFlipped);
    EXPECT_TRUE(calData.timestamp.empty());
}

TEST_F(CalibrationDataTest, SetValues) {
    calData.calibrated = true;
    calData.raRate = 15.5;
    calData.decRate = 14.2;
    calData.raAngle = 90.0;
    calData.decAngle = 0.0;
    calData.decFlipped = true;
    calData.timestamp = "2024-11-28T12:00:00Z";

    EXPECT_TRUE(calData.calibrated);
    EXPECT_DOUBLE_EQ(calData.raRate, 15.5);
    EXPECT_DOUBLE_EQ(calData.decRate, 14.2);
    EXPECT_DOUBLE_EQ(calData.raAngle, 90.0);
    EXPECT_DOUBLE_EQ(calData.decAngle, 0.0);
    EXPECT_TRUE(calData.decFlipped);
    EXPECT_EQ(calData.timestamp, "2024-11-28T12:00:00Z");
}

// ============================================================================
// PHD2Client Construction Tests
// ============================================================================

class PHD2ClientConstructionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PHD2ClientConstructionTest, DefaultConstruction) {
    PHD2Client client("TestClient");

    EXPECT_EQ(client.getName(), "TestClient");
    EXPECT_FALSE(client.isConnected());
    EXPECT_EQ(client.getGuiderState(), GuiderState::Stopped);
}

TEST_F(PHD2ClientConstructionTest, Initialize) {
    PHD2Client client("TestClient");

    EXPECT_TRUE(client.initialize());
}

TEST_F(PHD2ClientConstructionTest, Destroy) {
    PHD2Client client("TestClient");
    client.initialize();

    EXPECT_TRUE(client.destroy());
    EXPECT_FALSE(client.isConnected());
}

// ============================================================================
// PHD2Client State Tests (without actual PHD2 connection)
// ============================================================================

class PHD2ClientStateTest : public ::testing::Test {
protected:
    std::unique_ptr<PHD2Client> client;

    void SetUp() override {
        client = std::make_unique<PHD2Client>("TestClient");
        client->initialize();
    }

    void TearDown() override {
        client->destroy();
    }
};

TEST_F(PHD2ClientStateTest, InitialState) {
    EXPECT_EQ(client->getGuiderState(), GuiderState::Stopped);
    EXPECT_FALSE(client->isGuiding());
    EXPECT_FALSE(client->isPaused());
}

TEST_F(PHD2ClientStateTest, NotConnectedOperations) {
    // Operations should not crash when not connected
    client->stopGuiding();
    client->pause();
    client->resume();
    client->loop();
    EXPECT_FALSE(client->isCalibrated());
}

TEST_F(PHD2ClientStateTest, GetExposureNotConnected) {
    EXPECT_EQ(client->getExposure(), 0);
}

TEST_F(PHD2ClientStateTest, GetExposureDurationsNotConnected) {
    auto durations = client->getExposureDurations();
    EXPECT_TRUE(durations.empty());
}

TEST_F(PHD2ClientStateTest, GetPixelScaleNotConnected) {
    EXPECT_DOUBLE_EQ(client->getPixelScale(), 0.0);
}

TEST_F(PHD2ClientStateTest, GetCurrentStarNotConnected) {
    auto star = client->getCurrentStar();
    EXPECT_FALSE(star.valid);
}

TEST_F(PHD2ClientStateTest, GetCalibrationDataNotConnected) {
    auto calData = client->getCalibrationData();
    EXPECT_FALSE(calData.calibrated);
}

TEST_F(PHD2ClientStateTest, FindStarNotConnected) {
    auto star = client->findStar();
    EXPECT_FALSE(star.valid);
}

TEST_F(PHD2ClientStateTest, GetLockPositionNotConnected) {
    auto pos = client->getLockPosition();
    EXPECT_FALSE(pos.has_value());
}

TEST_F(PHD2ClientStateTest, ScanDevices) {
    auto devices = client->scan();
    EXPECT_EQ(devices.size(), 1);
    EXPECT_EQ(devices[0], "localhost:4400");
}

// ============================================================================
// PHD2Config Tests
// ============================================================================

class PHD2ConfigTest : public ::testing::Test {
protected:
    PHD2Config config;
};

TEST_F(PHD2ConfigTest, DefaultValues) {
    EXPECT_EQ(config.host, "localhost");
    EXPECT_EQ(config.port, 4400);
    EXPECT_EQ(config.reconnectAttempts, 3);
    EXPECT_EQ(config.reconnectDelayMs, 1000);
}

TEST_F(PHD2ConfigTest, CustomValues) {
    config.host = "192.168.1.100";
    config.port = 4401;
    config.reconnectAttempts = 5;
    config.reconnectDelayMs = 2000;

    EXPECT_EQ(config.host, "192.168.1.100");
    EXPECT_EQ(config.port, 4401);
    EXPECT_EQ(config.reconnectAttempts, 5);
    EXPECT_EQ(config.reconnectDelayMs, 2000);
}

TEST_F(PHD2ConfigTest, ConfigureClient) {
    PHD2Client client("TestClient");
    
    PHD2Config customConfig;
    customConfig.host = "192.168.1.50";
    customConfig.port = 4402;
    
    client.configurePHD2(customConfig);
    
    auto retrievedConfig = client.getPHD2Config();
    EXPECT_EQ(retrievedConfig.host, "192.168.1.50");
    EXPECT_EQ(retrievedConfig.port, 4402);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
