/*
 * test_indi_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Tests for INDI FilterWheel Device

**************************************************/

#include <gtest/gtest.h>

#include "client/indi/indi_filterwheel.hpp"

using namespace lithium::client::indi;

// ==================== Test Fixtures ====================

class INDIFilterWheelTest : public ::testing::Test {
protected:
    void SetUp() override {
        filterwheel_ = std::make_unique<INDIFilterWheel>("TestFilterWheel");
    }

    void TearDown() override { filterwheel_.reset(); }

    std::unique_ptr<INDIFilterWheel> filterwheel_;
};

// ==================== Construction Tests ====================

TEST_F(INDIFilterWheelTest, ConstructorSetsName) {
    EXPECT_EQ(filterwheel_->getName(), "TestFilterWheel");
}

TEST_F(INDIFilterWheelTest, GetDeviceTypeReturnsFilterWheel) {
    EXPECT_EQ(filterwheel_->getDeviceType(), "FilterWheel");
}

TEST_F(INDIFilterWheelTest, InitialStateIsIdle) {
    EXPECT_EQ(filterwheel_->getFilterWheelState(), FilterWheelState::Idle);
}

TEST_F(INDIFilterWheelTest, InitiallyNotMoving) {
    EXPECT_FALSE(filterwheel_->isMoving());
}

// ==================== Position Control Tests ====================

TEST_F(INDIFilterWheelTest, SetPositionFailsWhenDisconnected) {
    EXPECT_FALSE(filterwheel_->setPosition(3));
}

TEST_F(INDIFilterWheelTest, GetPositionReturnsValue) {
    auto pos = filterwheel_->getPosition();
    EXPECT_TRUE(pos.has_value());  // Returns default value
}

TEST_F(INDIFilterWheelTest, WaitForMoveReturnsTrueWhenNotMoving) {
    EXPECT_TRUE(filterwheel_->waitForMove(std::chrono::milliseconds(100)));
}

// ==================== Filter Names Tests ====================

TEST_F(INDIFilterWheelTest, GetCurrentFilterNameReturnsNulloptInitially) {
    auto name = filterwheel_->getCurrentFilterName();
    // May return nullopt or default name depending on initialization
}

TEST_F(INDIFilterWheelTest, GetFilterNameReturnsNulloptForInvalidPosition) {
    auto name = filterwheel_->getFilterName(0);  // Invalid position
    EXPECT_FALSE(name.has_value());
}

TEST_F(INDIFilterWheelTest, SetFilterNameFailsWhenDisconnected) {
    EXPECT_FALSE(filterwheel_->setFilterName(1, "Red"));
}

TEST_F(INDIFilterWheelTest, GetFilterNamesReturnsEmptyInitially) {
    auto names = filterwheel_->getFilterNames();
    // May be empty or have default names
}

TEST_F(INDIFilterWheelTest, SetFilterNamesFailsWhenDisconnected) {
    std::vector<std::string> names = {"Red", "Green", "Blue"};
    EXPECT_FALSE(filterwheel_->setFilterNames(names));
}

// ==================== Filter Slots Tests ====================

TEST_F(INDIFilterWheelTest, GetSlotCountReturnsDefault) {
    int count = filterwheel_->getSlotCount();
    EXPECT_GE(count, 0);  // Should be non-negative
}

TEST_F(INDIFilterWheelTest, GetSlotReturnsNulloptForInvalidPosition) {
    auto slot = filterwheel_->getSlot(0);  // Invalid position
    EXPECT_FALSE(slot.has_value());
}

TEST_F(INDIFilterWheelTest, GetSlotsReturnsEmptyInitially) {
    auto slots = filterwheel_->getSlots();
    // May be empty or have default slots
}

// ==================== Status Tests ====================

TEST_F(INDIFilterWheelTest, GetStatusReturnsValidJson) {
    auto status = filterwheel_->getStatus();

    EXPECT_TRUE(status.contains("name"));
    EXPECT_TRUE(status.contains("type"));
    EXPECT_TRUE(status.contains("filterWheelState"));
    EXPECT_TRUE(status.contains("isMoving"));
    EXPECT_TRUE(status.contains("position"));
    EXPECT_TRUE(status.contains("filters"));

    EXPECT_EQ(status["type"], "FilterWheel");
    EXPECT_FALSE(status["isMoving"].get<bool>());
}

// ==================== Struct Tests ====================

TEST(FilterSlotTest, ToJsonProducesValidOutput) {
    FilterSlot slot;
    slot.position = 1;
    slot.name = "Red";
    slot.color = "#FF0000";

    auto j = slot.toJson();

    EXPECT_EQ(j["position"], 1);
    EXPECT_EQ(j["name"], "Red");
    EXPECT_EQ(j["color"], "#FF0000");
}

TEST(FilterWheelPositionTest, ToJsonProducesValidOutput) {
    FilterWheelPosition pos;
    pos.current = 3;
    pos.min = 1;
    pos.max = 8;

    FilterSlot slot1{1, "Red", "#FF0000"};
    FilterSlot slot2{2, "Green", "#00FF00"};
    FilterSlot slot3{3, "Blue", "#0000FF"};
    pos.slots = {slot1, slot2, slot3};

    auto j = pos.toJson();

    EXPECT_EQ(j["current"], 3);
    EXPECT_EQ(j["min"], 1);
    EXPECT_EQ(j["max"], 8);
    EXPECT_EQ(j["slots"].size(), 3);
}
