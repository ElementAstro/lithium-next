/*
 * test_indi_rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI Rotator Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_rotator.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIRotatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        rotator_ = std::make_unique<INDIRotator>("TestRotator");
    }

    void TearDown() override { rotator_.reset(); }

    std::unique_ptr<INDIRotator> rotator_;
};

// ==================== Construction Tests ====================

TEST_F(INDIRotatorTest, ConstructorSetsName) {
    EXPECT_EQ(rotator_->getName(), "TestRotator");
}

TEST_F(INDIRotatorTest, GetDeviceTypeReturnsRotator) {
    EXPECT_EQ(rotator_->getDeviceType(), "Rotator");
}

TEST_F(INDIRotatorTest, InitialStateIsIdle) {
    EXPECT_EQ(rotator_->getRotatorState(), RotatorState::Idle);
}

TEST_F(INDIRotatorTest, InitiallyNotRotating) {
    EXPECT_FALSE(rotator_->isRotating());
}

// ==================== Angle Control Tests ====================

TEST_F(INDIRotatorTest, SetAngleFailsWhenDisconnected) {
    EXPECT_FALSE(rotator_->setAngle(90.0));
}

TEST_F(INDIRotatorTest, GetAngleReturnsValue) {
    auto angle = rotator_->getAngle();
    EXPECT_TRUE(angle.has_value());  // Returns default value
}

TEST_F(INDIRotatorTest, AbortRotationSucceedsWhenNotRotating) {
    EXPECT_TRUE(rotator_->abortRotation());
}

TEST_F(INDIRotatorTest, WaitForRotationReturnsTrueWhenNotRotating) {
    EXPECT_TRUE(rotator_->waitForRotation(std::chrono::milliseconds(100)));
}

// ==================== Sync Tests ====================

TEST_F(INDIRotatorTest, SyncAngleFailsWhenDisconnected) {
    EXPECT_FALSE(rotator_->syncAngle(45.0));
}

// ==================== Reverse Tests ====================

TEST_F(INDIRotatorTest, SetReversedFailsWhenDisconnected) {
    EXPECT_FALSE(rotator_->setReversed(true));
}

TEST_F(INDIRotatorTest, IsReversedReturnsValue) {
    auto reversed = rotator_->isReversed();
    EXPECT_TRUE(reversed.has_value());
}

// ==================== Status Tests ====================

TEST_F(INDIRotatorTest, GetStatusReturnsValidJson) {
    auto status = rotator_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("rotatorState"));
    EXPECT_TRUE(status.contains("isRotating"));
    EXPECT_TRUE(status.contains("isReversed"));
    EXPECT_TRUE(status.contains("position"));

    EXPECT_EQ(status["type"], "Rotator");
    EXPECT_FALSE(status["isRotating"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(RotatorPositionTest, ToJsonProducesValidOutput) {
    RotatorPosition pos;
    pos.angle = 90.0;
    pos.targetAngle = 180.0;
    pos.minAngle = 0.0;
    pos.maxAngle = 360.0;

    auto j = pos.toJson();

    EXPECT_DOUBLE_EQ(j["angle"].get<double>(), 90.0);
    EXPECT_DOUBLE_EQ(j["targetAngle"].get<double>(), 180.0);
    EXPECT_DOUBLE_EQ(j["minAngle"].get<double>(), 0.0);
    EXPECT_DOUBLE_EQ(j["maxAngle"].get<double>(), 360.0);
}
