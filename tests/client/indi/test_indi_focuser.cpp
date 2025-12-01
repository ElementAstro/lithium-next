/*
 * test_indi_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Focuser Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_focuser.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIFocuserTest : public ::testing::Test {
protected:
    void SetUp() override {
        focuser_ = std::make_unique<INDIFocuser>("TestFocuser");
    }

    void TearDown() override { focuser_.reset(); }

    std::unique_ptr<INDIFocuser> focuser_;
};

// ==================== Construction Tests ====================

TEST_F(INDIFocuserTest, ConstructorSetsName) {
    EXPECT_EQ(focuser_->getName(), "TestFocuser");
}

TEST_F(INDIFocuserTest, GetDeviceTypeReturnsFocuser) {
    EXPECT_EQ(focuser_->getDeviceType(), "Focuser");
}

TEST_F(INDIFocuserTest, InitialStateIsIdle) {
    EXPECT_EQ(focuser_->getFocuserState(), FocuserState::Idle);
}

TEST_F(INDIFocuserTest, InitiallyNotMoving) {
    EXPECT_FALSE(focuser_->isMoving());
}

// ==================== Position Control Tests ====================

TEST_F(INDIFocuserTest, MoveToPositionFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->moveToPosition(5000));
}

TEST_F(INDIFocuserTest, MoveStepsFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->moveSteps(100));
}

TEST_F(INDIFocuserTest, MoveForDurationFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->moveForDuration(1000));
}

TEST_F(INDIFocuserTest, AbortMoveSucceedsWhenNotMoving) {
    EXPECT_TRUE(focuser_->abortMove());
}

TEST_F(INDIFocuserTest, SyncPositionFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->syncPosition(5000));
}

TEST_F(INDIFocuserTest, GetPositionReturnsValue) {
    auto pos = focuser_->getPosition();
    EXPECT_TRUE(pos.has_value());  // Returns default value
}

TEST_F(INDIFocuserTest, WaitForMoveReturnsTrueWhenNotMoving) {
    EXPECT_TRUE(focuser_->waitForMove(std::chrono::milliseconds(100)));
}

// ==================== Speed Control Tests ====================

TEST_F(INDIFocuserTest, SetSpeedFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setSpeed(50.0));
}

TEST_F(INDIFocuserTest, GetSpeedReturnsValue) {
    auto speed = focuser_->getSpeed();
    EXPECT_TRUE(speed.has_value());  // Returns default value
}

// ==================== Direction Control Tests ====================

TEST_F(INDIFocuserTest, SetDirectionFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setDirection(FocusDirection::In));
}

TEST_F(INDIFocuserTest, GetDirectionReturnsDefault) {
    EXPECT_EQ(focuser_->getDirection(), FocusDirection::None);
}

TEST_F(INDIFocuserTest, SetReversedFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setReversed(true));
}

TEST_F(INDIFocuserTest, IsReversedReturnsValue) {
    auto reversed = focuser_->isReversed();
    EXPECT_TRUE(reversed.has_value());
}

// ==================== Limits Tests ====================

TEST_F(INDIFocuserTest, SetMaxLimitFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setMaxLimit(100000));
}

TEST_F(INDIFocuserTest, GetMaxLimitReturnsValue) {
    auto maxLimit = focuser_->getMaxLimit();
    EXPECT_TRUE(maxLimit.has_value());
}

// ==================== Temperature Tests ====================

TEST_F(INDIFocuserTest, GetExternalTemperatureReturnsNulloptInitially) {
    auto temp = focuser_->getExternalTemperature();
    EXPECT_FALSE(temp.has_value());
}

TEST_F(INDIFocuserTest, GetChipTemperatureReturnsNulloptInitially) {
    auto temp = focuser_->getChipTemperature();
    EXPECT_FALSE(temp.has_value());
}

// ==================== Backlash Tests ====================

TEST_F(INDIFocuserTest, SetBacklashEnabledFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setBacklashEnabled(true));
}

TEST_F(INDIFocuserTest, SetBacklashStepsFailsWhenDisconnected) {
    EXPECT_FALSE(focuser_->setBacklashSteps(50));
}

TEST_F(INDIFocuserTest, GetBacklashInfoReturnsDefault) {
    auto info = focuser_->getBacklashInfo();
    EXPECT_FALSE(info.enabled);
    EXPECT_EQ(info.steps, 0);
}

// ==================== Mode Tests ====================

TEST_F(INDIFocuserTest, GetFocusModeReturnsDefault) {
    EXPECT_EQ(focuser_->getFocusMode(), FocusMode::All);
}

// ==================== Status Tests ====================

TEST_F(INDIFocuserTest, GetStatusReturnsValidJson) {
    auto status = focuser_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("focuserState"));
    EXPECT_TRUE(status.contains("isMoving"));
    EXPECT_TRUE(status.contains("direction"));
    EXPECT_TRUE(status.contains("position"));
    EXPECT_TRUE(status.contains("speed"));
    EXPECT_TRUE(status.contains("temperature"));
    EXPECT_TRUE(status.contains("backlash"));

    EXPECT_EQ(status["type"], "Focuser");
    EXPECT_FALSE(status["isMoving"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(FocuserPositionTest, ToJsonProducesValidOutput) {
    FocuserPosition pos;
    pos.absolute = 5000;
    pos.relative = 100;
    pos.maxPosition = 100000;
    pos.minPosition = 0;

    auto j = pos.toJson();

    EXPECT_EQ(j["absolute"], 5000);
    EXPECT_EQ(j["relative"], 100);
    EXPECT_EQ(j["maxPosition"], 100000);
    EXPECT_EQ(j["minPosition"], 0);
}

TEST(FocuserSpeedTest, ToJsonProducesValidOutput) {
    FocuserSpeed speed;
    speed.current = 50.0;
    speed.min = 0.0;
    speed.max = 100.0;

    auto j = speed.toJson();

    EXPECT_DOUBLE_EQ(j["current"].get<double>(), 50.0);
    EXPECT_DOUBLE_EQ(j["min"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["max"].get<double>(), 100.0);
}

TEST(FocuserTemperatureTest, ToJsonProducesValidOutput) {
    FocuserTemperature temp;
    temp.external = 20.5;
    temp.chip = 25.0;
    temp.hasExternal = true;
    temp.hasChip = true;

    auto j = temp.toJson();

    EXPECT_DOUBLE_EQ(j["external"].get<double>(), 20.5);
    EXPECT_DOUBLE_EQ(j["chip"].get<double>(), 25.0);
    EXPECT_TRUE(j["hasExternal"].get<bool>());
    EXPECT_TRUE(j["hasChip"].get<bool>());
}

TEST(BacklashInfoTest, ToJsonProducesValidOutput) {
    BacklashInfo info;
    info.enabled = true;
    info.steps = 50;

    auto j = info.toJson();

    EXPECT_TRUE(j["enabled"].get<bool>());
    EXPECT_EQ(j["steps"], 50);
}
