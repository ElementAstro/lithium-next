#include <gtest/gtest.h>
#include "tools/solverutils.hpp"

namespace lithium::tools::test {

class SolverUtilsTest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-6;

    // Helper to create sample WCS parameters
    WCSParams createSampleWCS() {
        WCSParams wcs;
        wcs.crpix0 = 1024.5;    // Center X pixel
        wcs.crpix1 = 768.5;     // Center Y pixel
        wcs.crval0 = 180.0;     // RA at center (deg)
        wcs.crval1 = 45.0;      // Dec at center (deg)
        wcs.cd11 = -0.000277778; // Plate scale matrix
        wcs.cd12 = 0.0;
        wcs.cd21 = 0.0;
        wcs.cd22 = 0.000277778;
        return wcs;
    }

    // Helper to create sample WCS info string
    std::string createSampleWCSInfo() {
        return "crpix0 1024.5\n"
               "crpix1 768.5\n"
               "crval0 180.0\n"
               "crval1 45.0\n"
               "cd11 -0.000277778\n"
               "cd12 0.0\n"
               "cd21 0.0\n"
               "cd22 0.000277778\n";
    }
};

TEST_F(SolverUtilsTest, ExtractWCSParamsValidInput) {
    std::string wcsInfo = createSampleWCSInfo();
    auto wcs = extractWCSParams(wcsInfo);

    EXPECT_NEAR(wcs.crpix0, 1024.5, EPSILON);
    EXPECT_NEAR(wcs.crpix1, 768.5, EPSILON);
    EXPECT_NEAR(wcs.crval0, 180.0, EPSILON);
    EXPECT_NEAR(wcs.crval1, 45.0, EPSILON);
    EXPECT_NEAR(wcs.cd11, -0.000277778, EPSILON);
    EXPECT_NEAR(wcs.cd12, 0.0, EPSILON);
    EXPECT_NEAR(wcs.cd21, 0.0, EPSILON);
    EXPECT_NEAR(wcs.cd22, 0.000277778, EPSILON);
}

TEST_F(SolverUtilsTest, ExtractWCSParamsInvalidInput) {
    std::string malformedWCS = "invalid wcs info";
    EXPECT_THROW(extractWCSParams(malformedWCS), std::exception);
}

TEST_F(SolverUtilsTest, ExtractWCSParamsEmptyInput) {
    std::string emptyWCS = "";
    EXPECT_THROW(extractWCSParams(emptyWCS), std::exception);
}

TEST_F(SolverUtilsTest, PixelToRaDecCenterPixel) {
    WCSParams wcs = createSampleWCS();
    auto coords = pixelToRaDec(1024.5, 768.5, wcs);

    EXPECT_NEAR(coords.rightAscension, 180.0, EPSILON);
    EXPECT_NEAR(coords.declination, 45.0, EPSILON);
}

TEST_F(SolverUtilsTest, PixelToRaDecCornerPixel) {
    WCSParams wcs = createSampleWCS();
    auto coords = pixelToRaDec(0.0, 0.0, wcs);

    // Expected values calculated based on WCS transformation
    double expectedRA = 180.0 + wcs.cd11 * (-1024.5) + wcs.cd12 * (-768.5);
    double expectedDec = 45.0 + wcs.cd21 * (-1024.5) + wcs.cd22 * (-768.5);

    EXPECT_NEAR(coords.rightAscension, expectedRA, EPSILON);
    EXPECT_NEAR(coords.declination, expectedDec, EPSILON);
}

TEST_F(SolverUtilsTest, PixelToRaDecArbitraryPixel) {
    WCSParams wcs = createSampleWCS();
    auto coords = pixelToRaDec(512.0, 384.0, wcs);

    // Test point halfway between center and origin
    double dx = 512.0 - wcs.crpix0;
    double dy = 384.0 - wcs.crpix1;
    double expectedRA = wcs.crval0 + wcs.cd11 * dx + wcs.cd12 * dy;
    double expectedDec = wcs.crval1 + wcs.cd21 * dx + wcs.cd22 * dy;

    EXPECT_NEAR(coords.rightAscension, expectedRA, EPSILON);
    EXPECT_NEAR(coords.declination, expectedDec, EPSILON);
}

TEST_F(SolverUtilsTest, GetFOVCornersStandardImage) {
    WCSParams wcs = createSampleWCS();
    int width = 2048;
    int height = 1536;

    auto corners = getFOVCorners(wcs, width, height);

    ASSERT_EQ(corners.size(), 4);

    // Verify corners are in correct order
    auto bottomLeft = corners[0];
    auto bottomRight = corners[1];
    auto topRight = corners[2];
    auto topLeft = corners[3];

    // Verify bottom-left is different from bottom-right
    EXPECT_NE(bottomLeft.rightAscension, bottomRight.rightAscension);
    EXPECT_NE(bottomLeft.declination, bottomRight.declination);

    // Verify top-right is different from bottom-right
    EXPECT_NE(topRight.rightAscension, bottomRight.rightAscension);
    EXPECT_NE(topRight.declination, bottomRight.declination);
}

TEST_F(SolverUtilsTest, GetFOVCornersSquareImage) {
    WCSParams wcs = createSampleWCS();
    int size = 1000;

    auto corners = getFOVCorners(wcs, size, size);

    ASSERT_EQ(corners.size(), 4);

    // For square image with symmetric WCS, diagonal corners should be symmetric
    auto bottomLeft = corners[0];
    auto topRight = corners[2];

    double centerRA = wcs.crval0;
    double centerDec = wcs.crval1;

    // Verify symmetry around center point
    EXPECT_NEAR(std::abs(bottomLeft.rightAscension - centerRA),
                std::abs(topRight.rightAscension - centerRA),
                EPSILON);
    EXPECT_NEAR(std::abs(bottomLeft.declination - centerDec),
                std::abs(topRight.declination - centerDec),
                EPSILON);
}

TEST_F(SolverUtilsTest, GetFOVCornersZeroDimensions) {
    WCSParams wcs = createSampleWCS();
    auto corners = getFOVCorners(wcs, 0, 0);

    ASSERT_EQ(corners.size(), 4);
    // All corners should be the same when dimensions are zero
    for (int i = 1; i < 4; ++i) {
        EXPECT_NEAR(corners[i].rightAscension, corners[0].rightAscension, EPSILON);
        EXPECT_NEAR(corners[i].declination, corners[0].declination, EPSILON);
    }
}

} // namespace lithium::tools::test
