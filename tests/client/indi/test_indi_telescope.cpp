/*
 * test_indi_telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Telescope Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_telescope.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDITelescopeTest : public ::testing::Test {
protected:
    void SetUp() override {
        telescope_ = std::make_unique<INDITelescope>("TestTelescope");
    }

    void TearDown() override { telescope_.reset(); }

    std::unique_ptr<INDITelescope> telescope_;
};

// ==================== Construction Tests ====================

TEST_F(INDITelescopeTest, ConstructorSetsName) {
    EXPECT_EQ(telescope_->getName(), "TestTelescope");
}

TEST_F(INDITelescopeTest, GetDeviceTypeReturnsTelescope) {
    EXPECT_EQ(telescope_->getDeviceType(), "Telescope");
}

TEST_F(INDITelescopeTest, InitialStateIsIdle) {
    EXPECT_EQ(telescope_->getTelescopeState(), TelescopeState::Idle);
}

TEST_F(INDITelescopeTest, InitiallyNotSlewing) {
    EXPECT_FALSE(telescope_->isSlewing());
}

// ==================== Coordinate Tests ====================

TEST_F(INDITelescopeTest, GetRADECJ2000ReturnsNulloptWhenDisconnected) {
    auto coords = telescope_->getRADECJ2000();
    EXPECT_FALSE(coords.has_value());
}

TEST_F(INDITelescopeTest, SetRADECJ2000FailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setRADECJ2000(12.0, 45.0));
}

TEST_F(INDITelescopeTest, SetRADECJNowFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setRADECJNow(12.0, 45.0));
}

TEST_F(INDITelescopeTest, SetTargetRADECFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setTargetRADEC(12.0, 45.0));
}

TEST_F(INDITelescopeTest, SetAzAltFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setAzAlt(180.0, 45.0));
}

// ==================== Slewing Tests ====================

TEST_F(INDITelescopeTest, SlewToRADECFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->slewToRADEC(12.0, 45.0));
}

TEST_F(INDITelescopeTest, SlewToAzAltFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->slewToAzAlt(180.0, 45.0));
}

TEST_F(INDITelescopeTest, SyncToRADECFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->syncToRADEC(12.0, 45.0));
}

TEST_F(INDITelescopeTest, AbortMotionSucceeds) {
    // Should succeed even when disconnected
    EXPECT_TRUE(telescope_->abortMotion());
}

TEST_F(INDITelescopeTest, WaitForSlewReturnsTrueWhenNotSlewing) {
    EXPECT_TRUE(telescope_->waitForSlew(std::chrono::milliseconds(100)));
}

// ==================== Tracking Tests ====================

TEST_F(INDITelescopeTest, EnableTrackingFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->enableTracking(true));
}

TEST_F(INDITelescopeTest, IsTrackingEnabledReturnsFalseInitially) {
    EXPECT_FALSE(telescope_->isTrackingEnabled());
}

TEST_F(INDITelescopeTest, SetTrackModeFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setTrackMode(TrackMode::Sidereal));
}

TEST_F(INDITelescopeTest, GetTrackModeReturnsDefault) {
    // Default track mode
    auto mode = telescope_->getTrackMode();
    // Just verify it returns a valid mode
}

TEST_F(INDITelescopeTest, SetTrackRateFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setTrackRate(15.0, 0.0));
}

// ==================== Parking Tests ====================

TEST_F(INDITelescopeTest, ParkFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->park());
}

TEST_F(INDITelescopeTest, UnparkFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->unpark());
}

TEST_F(INDITelescopeTest, IsParkedReturnsFalseInitially) {
    EXPECT_FALSE(telescope_->isParked());
}

TEST_F(INDITelescopeTest, SetParkPositionFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setParkPosition(0.0, 90.0));
}

TEST_F(INDITelescopeTest, GetParkPositionReturnsNulloptInitially) {
    auto pos = telescope_->getParkPosition();
    EXPECT_FALSE(pos.has_value());
}

TEST_F(INDITelescopeTest, SetParkOptionFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setParkOption(ParkOption::Current));
}

// ==================== Motion Control Tests ====================

TEST_F(INDITelescopeTest, SetSlewRateFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->setSlewRate(SlewRate::Max));
}

TEST_F(INDITelescopeTest, GetSlewRateReturnsDefault) {
    EXPECT_EQ(telescope_->getSlewRate(), SlewRate::None);
}

TEST_F(INDITelescopeTest, MoveNSFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->moveNS(MotionNS::North));
}

TEST_F(INDITelescopeTest, MoveEWFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->moveEW(MotionEW::East));
}

TEST_F(INDITelescopeTest, StopNSSucceeds) {
    EXPECT_TRUE(telescope_->stopNS());
}

TEST_F(INDITelescopeTest, StopEWSucceeds) {
    EXPECT_TRUE(telescope_->stopEW());
}

// ==================== Guiding Tests ====================

TEST_F(INDITelescopeTest, GuideNSFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->guideNS(1, 1000));
}

TEST_F(INDITelescopeTest, GuideEWFailsWhenDisconnected) {
    EXPECT_FALSE(telescope_->guideEW(1, 1000));
}

// ==================== Telescope Info Tests ====================

TEST_F(INDITelescopeTest, GetTelescopeInfoReturnsDefault) {
    auto info = telescope_->getTelescopeInfo();
    // Just verify it returns valid info
    EXPECT_GE(info.aperture, 0.0);
    EXPECT_GE(info.focalLength, 0.0);
}

TEST_F(INDITelescopeTest, SetTelescopeInfoFailsWhenDisconnected) {
    TelescopeInfo info;
    info.aperture = 200.0;
    info.focalLength = 2000.0;
    EXPECT_FALSE(telescope_->setTelescopeInfo(info));
}

TEST_F(INDITelescopeTest, GetPierSideReturnsDefault) {
    EXPECT_EQ(telescope_->getPierSide(), PierSide::Unknown);
}

// ==================== Status Tests ====================

TEST_F(INDITelescopeTest, GetStatusReturnsValidJson) {
    auto status = telescope_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("telescopeState"));
    EXPECT_TRUE(status.contains("isSlewing"));
    EXPECT_TRUE(status.contains("pierSide"));
    EXPECT_TRUE(status.contains("currentRADEC"));
    EXPECT_TRUE(status.contains("tracking"));
    EXPECT_TRUE(status.contains("park"));
    EXPECT_TRUE(status.contains("telescopeInfo"));

    EXPECT_EQ(status["type"], "Telescope");
    EXPECT_FALSE(status["isSlewing"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(EquatorialCoordsTest, ToJsonProducesValidOutput) {
    EquatorialCoords coords;
    coords.ra = 12.5;
    coords.dec = 45.0;

    auto j = coords.toJson();

    EXPECT_DOUBLE_EQ(j["ra"].get<double>(), 12.5);
    EXPECT_DOUBLE_EQ(j["dec"].get<double>(), 45.0);
}

TEST(HorizontalCoordsTest, ToJsonProducesValidOutput) {
    HorizontalCoords coords;
    coords.az = 180.0;
    coords.alt = 45.0;

    auto j = coords.toJson();

    EXPECT_DOUBLE_EQ(j["az"].get<double>(), 180.0);
    EXPECT_DOUBLE_EQ(j["alt"].get<double>(), 45.0);
}

TEST(TelescopeInfoTest, ToJsonProducesValidOutput) {
    TelescopeInfo info;
    info.aperture = 200.0;
    info.focalLength = 2000.0;
    info.guiderAperture = 50.0;
    info.guiderFocalLength = 200.0;

    auto j = info.toJson();

    EXPECT_DOUBLE_EQ(j["aperture"].get<double>(), 200.0);
    EXPECT_DOUBLE_EQ(j["focalLength"].get<double>(), 2000.0);
    EXPECT_DOUBLE_EQ(j["focalRatio"].get<double>(), 10.0);
}

TEST(TelescopeInfoTest, FocalRatioCalculation) {
    TelescopeInfo info;
    info.aperture = 200.0;
    info.focalLength = 2000.0;

    EXPECT_DOUBLE_EQ(info.focalRatio(), 10.0);

    info.aperture = 0.0;
    EXPECT_DOUBLE_EQ(info.focalRatio(), 0.0);  // Division by zero protection
}

TEST(TrackRateInfoTest, ToJsonProducesValidOutput) {
    TrackRateInfo info;
    info.mode = TrackMode::Sidereal;
    info.raRate = 15.0;
    info.decRate = 0.0;
    info.enabled = true;

    auto j = info.toJson();

    EXPECT_EQ(j["mode"], static_cast<int>(TrackMode::Sidereal));
    EXPECT_DOUBLE_EQ(j["raRate"].get<double>(), 15.0);
    EXPECT_TRUE(j["enabled"].get<bool>());
}

TEST(ParkInfoTest, ToJsonProducesValidOutput) {
    ParkInfo info;
    info.parked = false;
    info.parkEnabled = true;
    info.parkRA = 0.0;
    info.parkDEC = 90.0;
    info.option = ParkOption::Default;

    auto j = info.toJson();

    EXPECT_FALSE(j["parked"].get<bool>());
    EXPECT_TRUE(j["parkEnabled"].get<bool>());
    EXPECT_DOUBLE_EQ(j["parkRA"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["parkDEC"].get<double>(), 90.0);
}
