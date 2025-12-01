// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Test suite for CelestialObject model
 */

#include <gtest/gtest.h>
#include "target/model/celestial_object.hpp"
#include "atom/type/json.hpp"

using namespace lithium::target::model;

class CelestialObjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a sample celestial object for testing
        testObject_.identifier = "M31";
        testObject_.mIdentifier = "M31";
        testObject_.extensionName = "Andromeda Galaxy";
        testObject_.chineseName = "仙女座星系";
        testObject_.type = "Galaxy";
        testObject_.morphology = "Sb";
        testObject_.constellationEn = "Andromeda";
        testObject_.constellationZh = "仙女座";
        testObject_.radJ2000 = 10.6847;
        testObject_.decDJ2000 = 41.2689;
        testObject_.visualMagnitudeV = 3.44;
        testObject_.majorAxis = 190.0;
        testObject_.minorAxis = 60.0;
    }

    CelestialObject testObject_;
};

TEST_F(CelestialObjectTest, DefaultConstruction) {
    CelestialObject obj;
    EXPECT_TRUE(obj.identifier.empty());
    EXPECT_EQ(obj.radJ2000, 0.0);
    EXPECT_EQ(obj.decDJ2000, 0.0);
}

TEST_F(CelestialObjectTest, GettersReturnCorrectValues) {
    EXPECT_EQ(testObject_.identifier, "M31");
    EXPECT_EQ(testObject_.type, "Galaxy");
    EXPECT_DOUBLE_EQ(testObject_.visualMagnitudeV, 3.44);
}

TEST_F(CelestialObjectTest, CoordinatesAreValid) {
    EXPECT_GE(testObject_.radJ2000, 0.0);
    EXPECT_LE(testObject_.radJ2000, 360.0);
    EXPECT_GE(testObject_.decDJ2000, -90.0);
    EXPECT_LE(testObject_.decDJ2000, 90.0);
}

TEST_F(CelestialObjectTest, JsonSerialization) {
    nlohmann::json j = testObject_.toJson();
    EXPECT_EQ(j["identifier"], "M31");
    EXPECT_EQ(j["type"], "Galaxy");
    EXPECT_DOUBLE_EQ(j["radJ2000"].get<double>(), 10.6847);
}

TEST_F(CelestialObjectTest, JsonDeserialization) {
    nlohmann::json j = {
        {"identifier", "NGC224"},
        {"type", "Galaxy"},
        {"radJ2000", 10.6847},
        {"decDJ2000", 41.2689},
        {"visualMagnitudeV", 3.44}
    };
    
    auto obj = CelestialObject::fromJson(j);
    EXPECT_EQ(obj.identifier, "NGC224");
    EXPECT_DOUBLE_EQ(obj.radJ2000, 10.6847);
}

TEST_F(CelestialObjectTest, AxisDimensions) {
    EXPECT_GT(testObject_.majorAxis, 0.0);
    EXPECT_GT(testObject_.minorAxis, 0.0);
    EXPECT_GE(testObject_.majorAxis, testObject_.minorAxis);
}

TEST_F(CelestialObjectTest, MagnitudeRange) {
    // Typical visual magnitude range for observable objects
    EXPECT_GE(testObject_.visualMagnitudeV, -30.0);
    EXPECT_LE(testObject_.visualMagnitudeV, 30.0);
}
