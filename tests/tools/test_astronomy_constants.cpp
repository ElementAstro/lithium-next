/**
 * @file test_astronomy_constants.cpp
 * @brief Tests for astronomy constants module.
 */

#include <gtest/gtest.h>
#include <cmath>
#include "tools/astronomy/constants.hpp"

using namespace lithium::tools::astronomy;

class AstronomyConstantsTest : public ::testing::Test {};

TEST_F(AstronomyConstantsTest, MathematicalConstants) {
    EXPECT_DOUBLE_EQ(K_PI, std::numbers::pi);
    EXPECT_DOUBLE_EQ(K_TWO_PI, 2.0 * std::numbers::pi);
    EXPECT_DOUBLE_EQ(K_HALF_PI, std::numbers::pi / 2.0);
}

TEST_F(AstronomyConstantsTest, AngularConversions) {
    EXPECT_NEAR(DEG_TO_RAD * 180.0, std::numbers::pi, 1e-10);
    EXPECT_NEAR(RAD_TO_DEG * std::numbers::pi, 180.0, 1e-10);
    EXPECT_DOUBLE_EQ(HOURS_TO_DEG, 15.0);
    EXPECT_DOUBLE_EQ(DEG_TO_HOURS, 1.0 / 15.0);
}

TEST_F(AstronomyConstantsTest, TimeConstants) {
    EXPECT_DOUBLE_EQ(HOURS_IN_DAY, 24.0);
    EXPECT_DOUBLE_EQ(SECONDS_IN_DAY, 86400.0);
    EXPECT_DOUBLE_EQ(JULIAN_CENTURY, 36525.0);
}

TEST_F(AstronomyConstantsTest, JulianDateConstants) {
    EXPECT_DOUBLE_EQ(JD_J2000, 2451545.0);
    EXPECT_DOUBLE_EQ(MJD_OFFSET, 2400000.5);
}

TEST_F(AstronomyConstantsTest, ToRadians) {
    EXPECT_NEAR(toRadians(0.0), 0.0, 1e-10);
    EXPECT_NEAR(toRadians(90.0), K_HALF_PI, 1e-10);
    EXPECT_NEAR(toRadians(180.0), K_PI, 1e-10);
    EXPECT_NEAR(toRadians(360.0), K_TWO_PI, 1e-10);
}

TEST_F(AstronomyConstantsTest, ToDegrees) {
    EXPECT_NEAR(toDegrees(0.0), 0.0, 1e-10);
    EXPECT_NEAR(toDegrees(K_HALF_PI), 90.0, 1e-10);
    EXPECT_NEAR(toDegrees(K_PI), 180.0, 1e-10);
    EXPECT_NEAR(toDegrees(K_TWO_PI), 360.0, 1e-10);
}

TEST_F(AstronomyConstantsTest, NormalizeAngle360) {
    EXPECT_NEAR(normalizeAngle360(0.0), 0.0, 1e-10);
    EXPECT_NEAR(normalizeAngle360(360.0), 0.0, 1e-10);
    EXPECT_NEAR(normalizeAngle360(450.0), 90.0, 1e-10);
    EXPECT_NEAR(normalizeAngle360(-90.0), 270.0, 1e-10);
    EXPECT_NEAR(normalizeAngle360(-450.0), 270.0, 1e-10);
}

TEST_F(AstronomyConstantsTest, NormalizeAngle180) {
    EXPECT_NEAR(normalizeAngle180(0.0), 0.0, 1e-10);
    EXPECT_NEAR(normalizeAngle180(180.0), 180.0, 1e-10);
    EXPECT_NEAR(normalizeAngle180(270.0), -90.0, 1e-10);
    EXPECT_NEAR(normalizeAngle180(-270.0), 90.0, 1e-10);
}

TEST_F(AstronomyConstantsTest, NormalizeRA) {
    EXPECT_NEAR(normalizeRA(0.0), 0.0, 1e-10);
    EXPECT_NEAR(normalizeRA(24.0), 0.0, 1e-10);
    EXPECT_NEAR(normalizeRA(25.5), 1.5, 1e-10);
    EXPECT_NEAR(normalizeRA(-1.0), 23.0, 1e-10);
}
