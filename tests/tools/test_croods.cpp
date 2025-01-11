#include <gtest/gtest.h>
#include "tools/croods.hpp"

namespace lithium::tools::test {

class CroodsTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-6;

    // Known epoch time for testing
    std::chrono::system_clock::time_point getTestTime() {
        std::tm tm = {};
        tm.tm_year = 120;  // 2020
        tm.tm_mon = 0;     // January
        tm.tm_mday = 1;    // 1st
        tm.tm_hour = 12;   // 12:00:00
        tm.tm_min = 0;
        tm.tm_sec = 0;
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
};

TEST_F(CroodsTest, TimeToJD) {
    auto testTime = getTestTime();
    double jd = timeToJD(testTime);
    // 2020-01-01 12:00:00 corresponds to JD 2458850.0
    EXPECT_NEAR(jd, 2458850.0, 0.1);
}

TEST_F(CroodsTest, JDConversions) {
    double testJD = 2458850.0;
    double mjd = jdToMJD(testJD);
    EXPECT_NEAR(mjd, testJD - MJD_OFFSET, EPSILON);
    EXPECT_NEAR(mjdToJD(mjd), testJD, EPSILON);
}

TEST_F(CroodsTest, CalculateBJD) {
    double jd = 2458850.0;
    double ra = 15.0;     // 1 hour RA
    double dec = 45.0;    // 45° Dec
    double lon = -75.0;   // 75° W
    double lat = 45.0;    // 45° N
    double elev = 100.0;  // 100m elevation

    double bjd = calculateBJD(jd, ra, dec, lon, lat, elev);
    EXPECT_GT(bjd, jd);                // BJD should be slightly larger than JD
    EXPECT_NEAR(bjd - jd, 0.0, 0.01);  // Difference should be small
}

TEST_F(CroodsTest, PeriodBelongs) {
    // Test within period
    EXPECT_TRUE(periodBelongs(10.0, 0.0, 360.0, 360.0, true, true));

    // Test at boundaries
    EXPECT_TRUE(periodBelongs(0.0, 0.0, 360.0, 360.0, true, true));
    EXPECT_TRUE(periodBelongs(360.0, 0.0, 360.0, 360.0, true, true));

    // Test outside period
    EXPECT_FALSE(periodBelongs(361.0, 0.0, 360.0, 360.0, false, false));

    // Test with different inclusivity
    EXPECT_FALSE(periodBelongs(0.0, 0.0, 360.0, 360.0, false, true));
    EXPECT_FALSE(periodBelongs(360.0, 0.0, 360.0, 360.0, true, false));
}

TEST_F(CroodsTest, FormatTime) {
    auto testTime = getTestTime();

    // Test local time format
    std::string localTime = formatTime(testTime, true);
    EXPECT_TRUE(localTime.find("(Local)") != std::string::npos);

    // Test UTC format
    std::string utcTime = formatTime(testTime, false);
    EXPECT_TRUE(utcTime.find("(UTC)") != std::string::npos);

    // Test custom format
    std::string customFormat = formatTime(testTime, true, "%Y-%m-%d");
    EXPECT_TRUE(customFormat.find("2020-01-01") != std::string::npos);
}

TEST_F(CroodsTest, GetInfoTextA) {
    auto testTime = getTestTime();
    double ra = 15.0;   // 1 hour RA
    double dec = 45.0;  // 45° Dec
    double dRa = 0.5;   // 0.5° RA error
    double dDec = 0.3;  // 0.3° Dec error
    std::string mountStatus = "TRACKING";
    std::string guideStatus = "GUIDING";

    std::string info =
        getInfoTextA(testTime, ra, dec, dRa, dDec, mountStatus, guideStatus);

    EXPECT_FALSE(info.empty());
    EXPECT_TRUE(info.find("RA/DEC") != std::string::npos);
    EXPECT_TRUE(info.find("TRACKING") != std::string::npos);
    EXPECT_TRUE(info.find("GUIDING") != std::string::npos);
}

TEST_F(CroodsTest, GetInfoTextB) {
    auto testTime = getTestTime();
    double az = 1.0;   // 1 radian azimuth
    double alt = 0.5;  // 0.5 radian altitude
    std::string camStatus = "EXPOSING";
    double camTemp = -10.0;
    double targetTemp = -15.0;
    int camX = 1920;
    int camY = 1080;
    int cfwPos = 1;
    std::string cfwName = "LRGB-L";
    std::string cfwStatus = "READY";

    std::string info =
        getInfoTextB(testTime, az, alt, camStatus, camTemp, targetTemp, camX,
                     camY, cfwPos, cfwName, cfwStatus);

    EXPECT_FALSE(info.empty());
    EXPECT_TRUE(info.find("AZ/ALT") != std::string::npos);
    EXPECT_TRUE(info.find("EXPOSING") != std::string::npos);
    EXPECT_TRUE(info.find("LRGB-L") != std::string::npos);
}

TEST_F(CroodsTest, GetInfoTextC) {
    int cpuTemp = 45;
    int cpuLoad = 30;
    double diskFree = 500.0;
    double lon = -1.0;      // -1 radian longitude
    double lat = 0.8;       // 0.8 radian latitude
    double raJ2000 = 2.0;   // 2 radians RA
    double decJ2000 = 0.5;  // 0.5 radians Dec
    double az = 3.0;        // 3 radians azimuth
    double alt = 1.0;       // 1 radian altitude
    std::string objName = "M31";

    std::string info = getInfoTextC(cpuTemp, cpuLoad, diskFree, lon, lat,
                                    raJ2000, decJ2000, az, alt, objName);

    EXPECT_FALSE(info.empty());
    EXPECT_TRUE(info.find("CPU") != std::string::npos);
    EXPECT_TRUE(info.find("Site") != std::string::npos);
    EXPECT_TRUE(info.find("M31") != std::string::npos);
}

TEST_F(CroodsTest, EdgeCases) {
    // Test period belongs with zero period
    EXPECT_FALSE(periodBelongs(1.0, 0.0, 1.0, 0.0, true, true));

    // Test BJD calculation at poles
    double bjd = calculateBJD(2458850.0, 0.0, 90.0, 0.0, 90.0, 0.0);
    EXPECT_GT(bjd, 2458850.0);

    // Test info text with empty strings
    std::string emptyInfo =
        getInfoTextA(getTestTime(), 0.0, 0.0, 0.0, 0.0, "", "");
    EXPECT_FALSE(emptyInfo.empty());
}

}  // namespace lithium::tools::test