#include <gtest/gtest.h>
#include "tools/calculate.hpp"
#include "tools/constant.hpp"

namespace lithium::tools::test {

class CalculateTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-6;  // For floating point comparisons
};

TEST_F(CalculateTest, CalculateVector) {
    CartesianCoordinates pointA{1.0, 2.0, 3.0};
    CartesianCoordinates pointB{4.0, 6.0, 8.0};
    
    auto result = calculateVector(pointA, pointB);
    
    EXPECT_NEAR(result.x, 3.0, EPSILON);
    EXPECT_NEAR(result.y, 4.0, EPSILON);
    EXPECT_NEAR(result.z, 5.0, EPSILON);
}

TEST_F(CalculateTest, CalculatePointC) {
    CartesianCoordinates pointA{1.0, 2.0, 3.0};
    CartesianCoordinates vector{2.0, 3.0, 4.0};
    
    auto result = calculatePointC(pointA, vector);
    
    EXPECT_NEAR(result.x, 3.0, EPSILON);
    EXPECT_NEAR(result.y, 5.0, EPSILON);
    EXPECT_NEAR(result.z, 7.0, EPSILON);
}

TEST_F(CalculateTest, CalculateFOVStandardCamera) {
    int focalLength = 1000;  // 1000mm
    double sensorWidth = 36.0;  // Full frame sensor width
    double sensorHeight = 24.0; // Full frame sensor height
    
    auto result = calculateFOV(focalLength, sensorWidth, sensorHeight);
    
    // Expected values calculated using the formula: 2 * atan(size/(2*focal)) * 180/pi
    EXPECT_NEAR(result.minFOV, 1.37, 0.01);  // Height angle
    EXPECT_NEAR(result.maxFOV, 2.06, 0.01);  // Diagonal angle
}

TEST_F(CalculateTest, CalculateFOVEdgeCases) {
    // Test with very large focal length
    auto resultLarge = calculateFOV(10000, 36.0, 24.0);
    EXPECT_NEAR(resultLarge.minFOV, 0.137, 0.001);
    
    // Test with very small focal length
    auto resultSmall = calculateFOV(10, 36.0, 24.0);
    EXPECT_NEAR(resultSmall.maxFOV, 127.0, 1.0);
}

TEST_F(CalculateTest, CalculateGSTKnownDate) {
    std::tm date = {};
    date.tm_year = 100;  // 2000
    date.tm_mon = 0;    // January
    date.tm_mday = 1;   // 1st
    date.tm_hour = 12;  // 12:00:00
    date.tm_min = 0;
    date.tm_sec = 0;
    
    double gst = calculateGST(date);
    EXPECT_NEAR(gst, 280.46062, 0.0001);
}

TEST_F(CalculateTest, CalculateAltAzKnownPosition) {
    // Test case: Polaris observation from mid-northern latitude
    double ra = 2.530;        // Polaris RA (hours)
    double dec = 89.264;      // Polaris Dec (degrees)
    double lat = 45.0;        // Observer latitude
    double lon = -75.0;       // Observer longitude
    
    std::tm date = {};
    date.tm_year = 121;    // 2021
    date.tm_mon = 6;       // July
    date.tm_mday = 1;      // 1st
    date.tm_hour = 0;      // 00:00:00
    date.tm_min = 0;
    date.tm_sec = 0;
    
    auto result = calculateAltAz(ra, dec, lat, lon, date);
    
    // Polaris should be visible at about the observer's latitude
    EXPECT_NEAR(result.altitude, lat, 1.0);
    // Azimuth should be close to true north (0 or 360 degrees)
    EXPECT_TRUE(result.azimuth < 5.0 || result.azimuth > 355.0);
}

TEST_F(CalculateTest, CalculateAltAzEquator) {
    // Test observation from equator
    double ra = 12.0;     // Object at local meridian
    double dec = 0.0;     // Object on celestial equator
    double lat = 0.0;     // Observer at equator
    double lon = 0.0;     // At prime meridian
    
    std::tm date = {};
    date.tm_year = 121;
    date.tm_mon = 6;
    date.tm_mday = 1;
    date.tm_hour = 12;    // Local noon
    date.tm_min = 0;
    date.tm_sec = 0;
    
    auto result = calculateAltAz(ra, dec, lat, lon, date);
    
    // Object should be near zenith
    EXPECT_NEAR(result.altitude, 90.0, 1.0);
}

TEST_F(CalculateTest, CalculateAltAzPoles) {
    // Test observation from north pole
    double ra = 0.0;
    double dec = 0.0;
    double lat = 90.0;    // North pole
    double lon = 0.0;
    
    std::tm date = {};
    date.tm_year = 121;
    date.tm_mon = 6;
    date.tm_mday = 1;
    date.tm_hour = 0;
    date.tm_min = 0;
    date.tm_sec = 0;
    
    auto result = calculateAltAz(ra, dec, lat, lon, date);
    
    // Object should be on horizon
    EXPECT_NEAR(result.altitude, 0.0, 1.0);
}

TEST_F(CalculateTest, CalculateAltAzBelowHorizon) {
    // Test object below horizon
    double ra = 12.0;     // Opposite to local meridian
    double dec = 0.0;
    double lat = 45.0;
    double lon = 0.0;
    
    std::tm date = {};
    date.tm_year = 121;
    date.tm_mon = 6;
    date.tm_mday = 1;
    date.tm_hour = 12;    // Local noon
    date.tm_min = 0;
    date.tm_sec = 0;
    
    auto result = calculateAltAz(ra, dec, lat, lon, date);
    
    // Object should be below horizon
    EXPECT_LT(result.altitude, 0.0);
}

} // namespace lithium::tools::test