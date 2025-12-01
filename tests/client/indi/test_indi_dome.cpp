/*
 * test_indi_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Dome Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_dome.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIDomeTest : public ::testing::Test {
protected:
    void SetUp() override { dome_ = std::make_unique<INDIDome>("TestDome"); }

    void TearDown() override { dome_.reset(); }

    std::unique_ptr<INDIDome> dome_;
};

// ==================== Construction Tests ====================

TEST_F(INDIDomeTest, ConstructorSetsName) {
    EXPECT_EQ(dome_->getName(), "TestDome");
}

TEST_F(INDIDomeTest, GetDeviceTypeReturnsDome) {
    EXPECT_EQ(dome_->getDeviceType(), "Dome");
}

TEST_F(INDIDomeTest, InitialStateIsIdle) {
    EXPECT_EQ(dome_->getDomeState(), DomeState::Idle);
}

TEST_F(INDIDomeTest, InitiallyNotMoving) {
    EXPECT_FALSE(dome_->isMoving());
}

// ==================== Azimuth Control Tests ====================

TEST_F(INDIDomeTest, SetAzimuthFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->setAzimuth(180.0));
}

TEST_F(INDIDomeTest, GetAzimuthReturnsValue) {
    auto az = dome_->getAzimuth();
    EXPECT_TRUE(az.has_value());  // Returns default value
}

TEST_F(INDIDomeTest, AbortMotionSucceeds) {
    EXPECT_TRUE(dome_->abortMotion());
}

TEST_F(INDIDomeTest, WaitForMotionReturnsTrueWhenNotMoving) {
    EXPECT_TRUE(dome_->waitForMotion(std::chrono::milliseconds(100)));
}

TEST_F(INDIDomeTest, MoveFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->move(DomeMotion::Clockwise));
}

TEST_F(INDIDomeTest, StopSucceeds) {
    EXPECT_TRUE(dome_->stop());
}

// ==================== Shutter Tests ====================

TEST_F(INDIDomeTest, OpenShutterFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->openShutter());
}

TEST_F(INDIDomeTest, CloseShutterFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->closeShutter());
}

TEST_F(INDIDomeTest, GetShutterStateReturnsUnknown) {
    EXPECT_EQ(dome_->getShutterState(), ShutterState::Unknown);
}

TEST_F(INDIDomeTest, HasShutterReturnsFalseInitially) {
    EXPECT_FALSE(dome_->hasShutter());
}

// ==================== Parking Tests ====================

TEST_F(INDIDomeTest, ParkFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->park());
}

TEST_F(INDIDomeTest, UnparkFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->unpark());
}

TEST_F(INDIDomeTest, IsParkedReturnsFalseInitially) {
    EXPECT_FALSE(dome_->isParked());
}

TEST_F(INDIDomeTest, SetParkPositionFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->setParkPosition(0.0));
}

// ==================== Telescope Sync Tests ====================

TEST_F(INDIDomeTest, EnableTelescopeSyncFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->enableTelescopeSync(true));
}

TEST_F(INDIDomeTest, IsTelescopeSyncEnabledReturnsFalseInitially) {
    EXPECT_FALSE(dome_->isTelescopeSyncEnabled());
}

TEST_F(INDIDomeTest, SyncToTelescopeFailsWhenDisconnected) {
    EXPECT_FALSE(dome_->syncToTelescope());
}

// ==================== Status Tests ====================

TEST_F(INDIDomeTest, GetStatusReturnsValidJson) {
    auto status = dome_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("domeState"));
    EXPECT_TRUE(status.contains("isMoving"));
    EXPECT_TRUE(status.contains("telescopeSyncEnabled"));
    EXPECT_TRUE(status.contains("position"));
    EXPECT_TRUE(status.contains("shutter"));
    EXPECT_TRUE(status.contains("park"));

    EXPECT_EQ(status["type"], "Dome");
    EXPECT_FALSE(status["isMoving"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(DomePositionTest, ToJsonProducesValidOutput) {
    DomePosition pos;
    pos.azimuth = 180.0;
    pos.targetAzimuth = 270.0;
    pos.minAzimuth = 0.0;
    pos.maxAzimuth = 360.0;

    auto j = pos.toJson();

    EXPECT_DOUBLE_EQ(j["azimuth"].get<double>(), 180.0);
    EXPECT_DOUBLE_EQ(j["targetAzimuth"].get<double>(), 270.0);
}

TEST(ShutterInfoTest, ToJsonProducesValidOutput) {
    ShutterInfo info;
    info.state = ShutterState::Open;
    info.hasShutter = true;

    auto j = info.toJson();

    EXPECT_EQ(j["state"], static_cast<int>(ShutterState::Open));
    EXPECT_TRUE(j["hasShutter"].get<bool>());
}

TEST(DomeParkInfoTest, ToJsonProducesValidOutput) {
    DomeParkInfo info;
    info.parked = false;
    info.parkEnabled = true;
    info.parkAzimuth = 0.0;

    auto j = info.toJson();

    EXPECT_FALSE(j["parked"].get<bool>());
    EXPECT_TRUE(j["parkEnabled"].get<bool>());
    EXPECT_DOUBLE_EQ(j["parkAzimuth"].get<double>(), 0.0);
}
