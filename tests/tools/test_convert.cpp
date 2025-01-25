#include <gtest/gtest.h>
#include "tools/convert.hpp"
#include "tools/constant.hpp"

namespace lithium::tools::test {

class ConvertTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-6;
};

// Basic angle conversion tests
TEST_F(ConvertTest, DegreeToRadian) {
    EXPECT_NEAR(degreeToRad(0.0), 0.0, EPSILON);
    EXPECT_NEAR(degreeToRad(180.0), M_PI, EPSILON);
    EXPECT_NEAR(degreeToRad(360.0), 2 * M_PI, EPSILON);
    EXPECT_NEAR(degreeToRad(-90.0), -M_PI/2, EPSILON);
}

TEST_F(ConvertTest, RadianToDegree) {
    EXPECT_NEAR(radToDegree(0.0), 0.0, EPSILON);
    EXPECT_NEAR(radToDegree(M_PI), 180.0, EPSILON);
    EXPECT_NEAR(radToDegree(2 * M_PI), 360.0, EPSILON);
    EXPECT_NEAR(radToDegree(-M_PI/2), -90.0, EPSILON);
}

TEST_F(ConvertTest, HourConversions) {
    // Hour to Degree
    EXPECT_NEAR(hourToDegree(0.0), 0.0, EPSILON);
    EXPECT_NEAR(hourToDegree(12.0), 180.0, EPSILON);
    EXPECT_NEAR(hourToDegree(24.0), 360.0, EPSILON);

    // Hour to Radian
    EXPECT_NEAR(hourToRad(0.0), 0.0, EPSILON);
    EXPECT_NEAR(hourToRad(12.0), M_PI, EPSILON);

    // Degree to Hour
    EXPECT_NEAR(degreeToHour(180.0), 12.0, EPSILON);
    EXPECT_NEAR(degreeToHour(360.0), 24.0, EPSILON);

    // Radian to Hour
    EXPECT_NEAR(radToHour(M_PI), 12.0, EPSILON);
    EXPECT_NEAR(radToHour(2 * M_PI), 24.0, EPSILON);
}

// Range adjustment tests
TEST_F(ConvertTest, RangeTo) {
    // Test normal range [0, 360]
    EXPECT_NEAR(rangeTo(400.0, 360.0, 0.0), 40.0, EPSILON);
    EXPECT_NEAR(rangeTo(-30.0, 360.0, 0.0), 330.0, EPSILON);

    // Test hour range [0, 24]
    EXPECT_NEAR(rangeTo(25.0, 24.0, 0.0), 1.0, EPSILON);
    EXPECT_NEAR(rangeTo(-1.0, 24.0, 0.0), 23.0, EPSILON);

    // Test multiple wraps
    EXPECT_NEAR(rangeTo(720.0, 360.0, 0.0), 0.0, EPSILON);
    EXPECT_NEAR(rangeTo(-720.0, 360.0, 0.0), 0.0, EPSILON);
}

// Coordinate conversion tests
TEST_F(ConvertTest, EquatorialToCartesian) {
    CartesianCoordinates result = convertEquatorialToCartesian(0.0, 0.0, 1.0);
    EXPECT_NEAR(result.x, 1.0, EPSILON);
    EXPECT_NEAR(result.y, 0.0, EPSILON);
    EXPECT_NEAR(result.z, 0.0, EPSILON);

    result = convertEquatorialToCartesian(90.0, 0.0, 1.0);
    EXPECT_NEAR(result.x, 0.0, EPSILON);
    EXPECT_NEAR(result.y, 1.0, EPSILON);
    EXPECT_NEAR(result.z, 0.0, EPSILON);
}

TEST_F(ConvertTest, CartesianToSpherical) {
    CartesianCoordinates cart{1.0, 0.0, 0.0};
    auto result = convertToSphericalCoordinates(cart);
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->rightAscension, 0.0, EPSILON);
    EXPECT_NEAR(result->declination, 0.0, EPSILON);

    // Test null case
    CartesianCoordinates zero{0.0, 0.0, 0.0};
    EXPECT_FALSE(convertToSphericalCoordinates(zero).has_value());
}

TEST_F(ConvertTest, RaDecToAltAz) {
    // Object at zenith
    auto result = raDecToAltAz(0.0, M_PI/2, M_PI/2);
    EXPECT_NEAR(result[0], M_PI/2, EPSILON);  // altitude = 90°

    // Object at horizon
    result = raDecToAltAz(0.0, 0.0, 0.0);
    EXPECT_NEAR(result[0], 0.0, EPSILON);     // altitude = 0°
}

TEST_F(ConvertTest, AltAzToRaDec) {
    double hr, dec;
    // Test zenith case
    altAzToRaDec(M_PI/2, 0.0, hr, dec, M_PI/4);  // 45° latitude
    EXPECT_NEAR(dec, M_PI/4, EPSILON);

    // Test horizon case
    altAzToRaDec(0.0, 0.0, hr, dec, 0.0);  // equator
    EXPECT_NEAR(dec, 0.0, EPSILON);
}

// String format conversion tests
TEST_F(ConvertTest, DMSConversion) {
    // Positive angle
    EXPECT_NEAR(dmsToDegree(30, 30, 30.0), 30.508333, 0.000001);

    // Negative angle
    EXPECT_NEAR(dmsToDegree(-30, 30, 30.0), -30.508333, 0.000001);

    // Zero case
    EXPECT_NEAR(dmsToDegree(0, 0, 0.0), 0.0, EPSILON);
}

TEST_F(ConvertTest, RadianToDMSString) {
    EXPECT_EQ(radToDmsStr(0.0), "+00°00'0.0\"");
    EXPECT_EQ(radToDmsStr(M_PI/2), "+90°00'0.0\"");
    EXPECT_EQ(radToDmsStr(-M_PI/4), "-45°00'0.0\"");
}

TEST_F(ConvertTest, RadianToHMSString) {
    EXPECT_EQ(radToHmsStr(0.0), "00:00:0.0");
    EXPECT_EQ(radToHmsStr(M_PI), "12:00:0.0");
    EXPECT_EQ(radToHmsStr(2 * M_PI), "00:00:0.0");  // Full circle
}

// Edge cases
TEST_F(ConvertTest, EdgeCases) {
    // Very large angles
    EXPECT_NO_THROW(rangeTo(1e6, 360.0, 0.0));
    EXPECT_NO_THROW(radToDmsStr(1e6));

    // Very small angles
    EXPECT_NEAR(degreeToRad(1e-6), 1e-6 * M_PI / 180.0, EPSILON);

    // NaN handling in string conversions
    EXPECT_NO_THROW(radToDmsStr(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_NO_THROW(radToHmsStr(std::numeric_limits<double>::quiet_NaN()));
}

} // namespace lithium::tools::test
