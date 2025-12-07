/*
 * test_phd2_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/common/guider_client.hpp"
#include "client/phd2/phd2_client.hpp"

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

    void TearDown() override { client->destroy(); }
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
// GuiderState Tests
// ============================================================================

class GuiderStateTest : public ::testing::Test {};

TEST_F(GuiderStateTest, AllStatesExist) {
    // Verify all guider states can be used
    GuiderState state1 = GuiderState::Stopped;
    GuiderState state2 = GuiderState::Selected;
    GuiderState state3 = GuiderState::Calibrating;
    GuiderState state4 = GuiderState::Guiding;
    GuiderState state5 = GuiderState::LostLock;
    GuiderState state6 = GuiderState::Paused;
    GuiderState state7 = GuiderState::Looping;

    EXPECT_NE(state1, state4);
    EXPECT_NE(state2, state3);
}

// ============================================================================
// PHD2Client Connection Edge Cases
// ============================================================================

class PHD2ClientConnectionTest : public ::testing::Test {
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

TEST_F(PHD2ClientConnectionTest, ConnectWithInvalidHost) {
    // Should not crash when connecting to invalid host
    bool result = client->connect("invalid.host.that.does.not.exist", 4400, 1000);
    EXPECT_FALSE(result);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(PHD2ClientConnectionTest, ConnectWithInvalidPort) {
    // Should not crash when connecting to invalid port
    bool result = client->connect("localhost", 99999, 1000);
    EXPECT_FALSE(result);
    EXPECT_FALSE(client->isConnected());
}

TEST_F(PHD2ClientConnectionTest, DisconnectWhenNotConnected) {
    // Should not crash when disconnecting without connection
    EXPECT_FALSE(client->isConnected());
    bool result = client->disconnect();
    EXPECT_TRUE(result);  // Disconnect should succeed even if not connected
}

TEST_F(PHD2ClientConnectionTest, DoubleInitialize) {
    // Double initialize should not cause issues
    EXPECT_TRUE(client->initialize());
    EXPECT_TRUE(client->initialize());
}

TEST_F(PHD2ClientConnectionTest, DoubleDestroy) {
    // Double destroy should not cause issues
    EXPECT_TRUE(client->destroy());
    EXPECT_TRUE(client->destroy());
}

// ============================================================================
// PHD2Client Guiding Operations (Not Connected)
// ============================================================================

class PHD2ClientGuidingTest : public ::testing::Test {
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

TEST_F(PHD2ClientGuidingTest, StartGuidingNotConnected) {
    // Should not crash when not connected
    client->startGuiding();
    EXPECT_FALSE(client->isGuiding());
}

TEST_F(PHD2ClientGuidingTest, StopGuidingNotConnected) {
    // Should not crash when not connected
    client->stopGuiding();
    EXPECT_FALSE(client->isGuiding());
}

TEST_F(PHD2ClientGuidingTest, DitherNotConnected) {
    DitherParams params;
    params.amount = 5.0;
    params.raOnly = false;

    // Should not crash when not connected
    client->dither(params);
    EXPECT_FALSE(client->isGuiding());
}

TEST_F(PHD2ClientGuidingTest, SetExposureNotConnected) {
    // Should not crash when not connected
    client->setExposure(1000);
    EXPECT_EQ(client->getExposure(), 0);
}

TEST_F(PHD2ClientGuidingTest, SetLockPositionNotConnected) {
    // Should not crash when not connected
    client->setLockPosition(512.0, 384.0);
    auto pos = client->getLockPosition();
    EXPECT_FALSE(pos.has_value());
}

TEST_F(PHD2ClientGuidingTest, ClearCalibrationNotConnected) {
    // Should not crash when not connected
    client->clearCalibration();
    EXPECT_FALSE(client->isCalibrated());
}

TEST_F(PHD2ClientGuidingTest, FlipCalibrationNotConnected) {
    // Should not crash when not connected
    client->flipCalibration();
    EXPECT_FALSE(client->isCalibrated());
}

// ============================================================================
// PHD2Client Stats (Not Connected)
// ============================================================================

class PHD2ClientStatsTest : public ::testing::Test {
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

TEST_F(PHD2ClientStatsTest, GetStatsNotConnected) {
    auto stats = client->getStats();

    // Should return default/zero stats when not connected
    EXPECT_DOUBLE_EQ(stats.rmsRA, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsDec, 0.0);
    EXPECT_DOUBLE_EQ(stats.rmsTotal, 0.0);
}

TEST_F(PHD2ClientStatsTest, GetAppStateNotConnected) {
    auto state = client->getAppState();
    EXPECT_TRUE(state.empty());
}

// ============================================================================
// SettleParams Edge Cases
// ============================================================================

class SettleParamsEdgeCaseTest : public ::testing::Test {};

TEST_F(SettleParamsEdgeCaseTest, ZeroValues) {
    SettleParams params;
    params.pixels = 0.0;
    params.time = 0.0;
    params.timeout = 0.0;

    EXPECT_DOUBLE_EQ(params.pixels, 0.0);
    EXPECT_DOUBLE_EQ(params.time, 0.0);
    EXPECT_DOUBLE_EQ(params.timeout, 0.0);
}

TEST_F(SettleParamsEdgeCaseTest, NegativeValues) {
    SettleParams params;
    params.pixels = -1.0;
    params.time = -10.0;
    params.timeout = -60.0;

    // Should accept negative values (validation is elsewhere)
    EXPECT_DOUBLE_EQ(params.pixels, -1.0);
    EXPECT_DOUBLE_EQ(params.time, -10.0);
    EXPECT_DOUBLE_EQ(params.timeout, -60.0);
}

// ============================================================================
// DitherParams Edge Cases
// ============================================================================

class DitherParamsEdgeCaseTest : public ::testing::Test {};

TEST_F(DitherParamsEdgeCaseTest, ZeroAmount) {
    DitherParams params;
    params.amount = 0.0;

    EXPECT_DOUBLE_EQ(params.amount, 0.0);
}

TEST_F(DitherParamsEdgeCaseTest, LargeAmount) {
    DitherParams params;
    params.amount = 100.0;

    EXPECT_DOUBLE_EQ(params.amount, 100.0);
}

TEST_F(DitherParamsEdgeCaseTest, RAOnlyTrue) {
    DitherParams params;
    params.raOnly = true;

    EXPECT_TRUE(params.raOnly);
}

// ============================================================================
// CalibrationData Edge Cases
// ============================================================================

class CalibrationDataEdgeCaseTest : public ::testing::Test {};

TEST_F(CalibrationDataEdgeCaseTest, AllFieldsSet) {
    CalibrationData cal;
    cal.calibrated = true;
    cal.raRate = 15.0;
    cal.decRate = 14.5;
    cal.raAngle = 90.0;
    cal.decAngle = 0.0;
    cal.decFlipped = true;
    cal.timestamp = "2024-12-07T10:00:00Z";

    EXPECT_TRUE(cal.calibrated);
    EXPECT_DOUBLE_EQ(cal.raRate, 15.0);
    EXPECT_DOUBLE_EQ(cal.decRate, 14.5);
    EXPECT_DOUBLE_EQ(cal.raAngle, 90.0);
    EXPECT_DOUBLE_EQ(cal.decAngle, 0.0);
    EXPECT_TRUE(cal.decFlipped);
    EXPECT_EQ(cal.timestamp, "2024-12-07T10:00:00Z");
}

TEST_F(CalibrationDataEdgeCaseTest, NegativeRates) {
    CalibrationData cal;
    cal.raRate = -15.0;
    cal.decRate = -14.5;

    EXPECT_DOUBLE_EQ(cal.raRate, -15.0);
    EXPECT_DOUBLE_EQ(cal.decRate, -14.5);
}

// ============================================================================
// GuideStar Edge Cases
// ============================================================================

class GuideStarEdgeCaseTest : public ::testing::Test {};

TEST_F(GuideStarEdgeCaseTest, NegativeCoordinates) {
    GuideStar star;
    star.x = -100.0;
    star.y = -200.0;

    EXPECT_DOUBLE_EQ(star.x, -100.0);
    EXPECT_DOUBLE_EQ(star.y, -200.0);
}

TEST_F(GuideStarEdgeCaseTest, LargeCoordinates) {
    GuideStar star;
    star.x = 10000.0;
    star.y = 10000.0;

    EXPECT_DOUBLE_EQ(star.x, 10000.0);
    EXPECT_DOUBLE_EQ(star.y, 10000.0);
}

TEST_F(GuideStarEdgeCaseTest, ZeroSNR) {
    GuideStar star;
    star.snr = 0.0;
    star.valid = false;

    EXPECT_DOUBLE_EQ(star.snr, 0.0);
    EXPECT_FALSE(star.valid);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
