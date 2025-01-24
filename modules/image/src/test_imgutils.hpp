#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include "imgutils.hpp"

class ImgUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test images
        testImage = cv::Mat(100, 100, CV_8UC1, cv::Scalar(0));
        colorImage = cv::Mat(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
        doubleImage = cv::Mat(100, 100, CV_64FC1, cv::Scalar(0.5));

        // Draw a white circle
        cv::circle(testImage, cv::Point(50, 50), 30, cv::Scalar(255), 1);
    }

    cv::Mat testImage;
    cv::Mat colorImage;
    cv::Mat doubleImage;
};

// Geometry Tests
TEST_F(ImgUtilsTest, InsideCircle) {
    EXPECT_TRUE(insideCircle(50, 50, 50, 50, 10));   // Center
    EXPECT_TRUE(insideCircle(55, 50, 50, 50, 10));   // Inside
    EXPECT_FALSE(insideCircle(70, 70, 50, 50, 10));  // Outside
}

TEST_F(ImgUtilsTest, CheckElongated) {
    EXPECT_TRUE(checkElongated(100, 50));    // 2:1 ratio
    EXPECT_TRUE(checkElongated(50, 100));    // 1:2 ratio
    EXPECT_FALSE(checkElongated(100, 100));  // 1:1 ratio
}

// Pixel Operation Tests
TEST_F(ImgUtilsTest, CheckWhitePixel) {
    EXPECT_EQ(checkWhitePixel(testImage, 50, 20), 1);  // On circle
    EXPECT_EQ(checkWhitePixel(testImage, 50, 50), 0);  // Inside circle
    EXPECT_EQ(checkWhitePixel(testImage, 0, 0), 0);    // Corner
}

// Circle Symmetry Tests
TEST_F(ImgUtilsTest, EightSymmetryCircleCheck) {
    cv::Point center(50, 50);
    int result = eightSymmetryCircleCheck(testImage, center, 30, 0);
    EXPECT_GT(result, 0);
}

TEST_F(ImgUtilsTest, FourSymmetryCircleCheck) {
    cv::Point center(50, 50);
    int result = fourSymmetryCircleCheck(testImage, center, 30);
    EXPECT_GT(result, 0);
}

// Area Tests
TEST_F(ImgUtilsTest, DefineNarrowRadius) {
    auto [checkNum, checklist, thresholdList] =
        defineNarrowRadius(100, 2000, 600, 1.0);
    EXPECT_EQ(checkNum, 3);
    EXPECT_FALSE(checklist.empty());
    EXPECT_FALSE(thresholdList.empty());
}

// Bresenham Circle Tests
TEST_F(ImgUtilsTest, CheckBresenhamCircle) {
    bool result = checkBresenhamCircle(testImage, 30, 0.5, false);
    EXPECT_TRUE(result);
}

// Average Deviation Tests
TEST_F(ImgUtilsTest, CalculateAverageDeviation) {
    double result = calculateAverageDeviation(0.5, doubleImage);
    EXPECT_NEAR(result, 0.0, 0.001);
}

// MTF Tests
TEST_F(ImgUtilsTest, CalculateMTF) {
    cv::Mat result = calculateMTF(0.5, doubleImage);
    EXPECT_EQ(result.size(), doubleImage.size());
}

// Scale Tests
TEST_F(ImgUtilsTest, CalculateScale) {
    double scale = calculateScale(testImage, 1552);
    EXPECT_GT(scale, 0.0);
}

// Median Deviation Tests
TEST_F(ImgUtilsTest, CalculateMedianDeviation) {
    double result = calculateMedianDeviation(0.5, doubleImage);
    EXPECT_NEAR(result, 0.0, 0.001);
}

// Parameter Computation Tests
TEST_F(ImgUtilsTest, ComputeParamsOneChannel) {
    auto [shadows, midtones, highlights] = computeParamsOneChannel(testImage);
    EXPECT_GE(shadows, 0.0);
    EXPECT_GE(midtones, 0.0);
    EXPECT_LE(highlights, 1.0);
}

// White Balance Tests
TEST_F(ImgUtilsTest, AutoWhiteBalance) {
    cv::Mat result = autoWhiteBalance(colorImage);
    EXPECT_EQ(result.size(), colorImage.size());
    EXPECT_EQ(result.channels(), 3);
}

// Error Cases
TEST_F(ImgUtilsTest, EmptyImageErrors) {
    cv::Mat emptyImg;
    EXPECT_THROW(calculateAverageDeviation(0.5, emptyImg),
                 std::invalid_argument);
    EXPECT_THROW(calculateMTF(0.5, emptyImg), std::invalid_argument);
    EXPECT_THROW(calculateScale(emptyImg), std::invalid_argument);
    EXPECT_THROW(calculateMedianDeviation(0.5, emptyImg),
                 std::invalid_argument);
    EXPECT_THROW(computeParamsOneChannel(emptyImg), std::invalid_argument);
    EXPECT_THROW(autoWhiteBalance(emptyImg), std::invalid_argument);
}

TEST_F(ImgUtilsTest, InvalidChannelCount) {
    EXPECT_THROW(autoWhiteBalance(testImage), std::invalid_argument);
}

// Edge Cases
TEST_F(ImgUtilsTest, ExtremeDimensions) {
    cv::Mat tinyImg(1, 1, CV_8UC3, cv::Scalar(128, 128, 128));
    EXPECT_NO_THROW(autoWhiteBalance(tinyImg));

    cv::Mat wideImg(10, 1000, CV_8UC3, cv::Scalar(128, 128, 128));
    EXPECT_NO_THROW(autoWhiteBalance(wideImg));
}