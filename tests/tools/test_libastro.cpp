#include <gtest/gtest.h>
#include "tools/libastro.hpp"

namespace lithium::tools::test {

class LibAstroTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-6;
    static constexpr double J2000_JD = 2451545.0;
    
    // Helper to create date
    double getJulianDate(int year, int month, int day, 
                        int hour=0, int min=0, int sec=0) {
        std::tm time = {};
        time.tm_year = year - 1900;
        time.tm_mon = month - 1;
        time.tm_mday = day;
        time.tm_hour = hour;
        time.tm_min = min;
        time.tm_sec = sec;
        time_t epoch = std::mktime(&time);
        return (epoch / 86400.0) + 2440587.5; // Convert Unix epoch to JD
    }
};

// Basic angle conversion tests
TEST_F(LibAstroTest, AngleConversions) {
    EXPECT_NEAR(degToRad(180.0), M_PI, EPSILON);
    EXPECT_NEAR(radToDeg(M_PI), 180.0, EPSILON);
    EXPECT_NEAR(range360(370.0), 10.0, EPSILON);
    EXPECT_NEAR(range360(-10.0), 350.0, EPSILON);
}

// Nutation tests
TEST_F(LibAstroTest, Nutation) {
    double jd = J2000_JD;  // Test at J2000.0 epoch
    auto [nutLon, nutObl] = getNutation(jd);
    
    // Values from astronomical almanac for J2000.0
    EXPECT_NEAR(nutLon, -0.00337, 0.001);
    EXPECT_NEAR(nutObl, 0.00278, 0.001);
}

// Nutation application test
TEST_F(LibAstroTest, ApplyNutation) {
    EquatorialCoordinates pos{12.0, 45.0};  // 12h RA, 45° Dec
    double jd = J2000_JD;
    
    auto result = applyNutation(pos, jd);
    // Verify nutation is small but non-zero
    EXPECT_NE(result.rightAscension, pos.rightAscension);
    EXPECT_NE(result.declination, pos.declination);
    
    // Test reversibility
    auto reversed = applyNutation(result, jd, true);
    EXPECT_NEAR(reversed.rightAscension, pos.rightAscension, 0.0001);
    EXPECT_NEAR(reversed.declination, pos.declination, 0.0001);
}

// Aberration test
TEST_F(LibAstroTest, ApplyAberration) {
    EquatorialCoordinates pos{6.0, 30.0};  // 6h RA, 30° Dec
    double jd = J2000_JD;
    
    auto result = applyAberration(pos, jd);
    // Aberration should be small but measurable
    EXPECT_NE(result.rightAscension, pos.rightAscension);
    EXPECT_NE(result.declination, pos.declination);
    EXPECT_NEAR(std::abs(result.rightAscension - pos.rightAscension), 0.0, 0.1);
}

// Precession test
TEST_F(LibAstroTest, ApplyPrecession) {
    EquatorialCoordinates pos{0.0, 0.0};  // Vernal equinox
    double startJD = J2000_JD;
    double endJD = startJD + 36525.0;  // One century
    
    auto result = applyPrecession(pos, startJD, endJD);
    // Expect significant precession over a century
    EXPECT_NE(result.rightAscension, pos.rightAscension);
    EXPECT_NE(result.declination, pos.declination);
}

// Coordinate transformation tests
TEST_F(LibAstroTest, EquatorialToHorizontal) {
    EquatorialCoordinates eq{0.0, 90.0};  // North celestial pole
    GeographicCoordinates obs{0.0, 45.0, 0.0};  // 45°N latitude
    double jd = J2000_JD;
    
    auto hz = equatorialToHorizontal(eq, obs, jd);
    EXPECT_NEAR(hz.altitude, 45.0, 0.1);  // Altitude should equal latitude
}

TEST_F(LibAstroTest, HorizontalToEquatorial) {
    HorizontalCoordinates hz{0.0, 45.0};  // Due north, 45° altitude
    GeographicCoordinates obs{0.0, 45.0, 0.0};
    double jd = J2000_JD;
    
    auto eq = horizontalToEquatorial(hz, obs, jd);
    // Convert back to horizontal
    auto hz2 = equatorialToHorizontal(eq, obs, jd);
    
    EXPECT_NEAR(hz2.azimuth, hz.azimuth, 0.1);
    EXPECT_NEAR(hz2.altitude, hz.altitude, 0.1);
}

// Full coordinate chain tests
TEST_F(LibAstroTest, ObservedToJ2000) {
    EquatorialCoordinates observed{12.0, 45.0};
    double jd = getJulianDate(2020, 1, 1);
    
    auto j2000 = observedToJ2000(observed, jd);
    auto backToObserved = j2000ToObserved(j2000, jd);
    
    EXPECT_NEAR(backToObserved.rightAscension, observed.rightAscension, 0.0001);
    EXPECT_NEAR(backToObserved.declination, observed.declination, 0.0001);
}

// Edge cases
TEST_F(LibAstroTest, EdgeCases) {
    // Test pole positions
    EquatorialCoordinates pole{0.0, 90.0};
    GeographicCoordinates obs{0.0, 90.0, 0.0};
    double jd = J2000_JD;
    
    auto hz = equatorialToHorizontal(pole, obs, jd);
    EXPECT_NEAR(hz.altitude, 90.0, 0.1);
    
    // Test around meridian
    EquatorialCoordinates meridian{0.0, 0.0};
    auto hzMeridian = equatorialToHorizontal(meridian, obs, jd);
    EXPECT_NEAR(hzMeridian.azimuth, 180.0, 0.1);
}

} // namespace lithium::tools::test