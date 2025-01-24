#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include "binning.hpp"

namespace {

class BinningTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test images of different sizes
        smallImage = cv::Mat(100, 100, CV_8UC1, cv::Scalar(128));
        largeImage = cv::Mat(3000, 3000, CV_8UC1, cv::Scalar(128));
        colorImage = cv::Mat(100, 100, CV_8UC3, cv::Scalar(64, 128, 192));
        
        // Create images with different bit depths
        image8bit = cv::Mat(100, 100, CV_8UC1, cv::Scalar(128));
        image16bit = cv::Mat(100, 100, CV_16UC1, cv::Scalar(32768));
        image32bit = cv::Mat(100, 100, CV_32SC1, cv::Scalar(2147483647));

        // Create gradient image
        gradient = cv::Mat(100, 100, CV_8UC1);
        for(int i = 0; i < gradient.rows; i++) {
            for(int j = 0; j < gradient.cols; j++) {
                gradient.at<uint8_t>(i, j) = static_cast<uint8_t>((i + j) / 2);
            }
        }
    }

    cv::Mat smallImage;
    cv::Mat largeImage;
    cv::Mat colorImage;
    cv::Mat image8bit;
    cv::Mat image16bit;
    cv::Mat image32bit;
    cv::Mat gradient;
};

TEST_F(BinningTest, MergeImageBasedOnSizeSmallImage) {
    auto result = mergeImageBasedOnSize(smallImage);
    EXPECT_EQ(result.camxbin, 1);
    EXPECT_EQ(result.camybin, 1);
}

TEST_F(BinningTest, MergeImageBasedOnSizeLargeImage) {
    auto result = mergeImageBasedOnSize(largeImage);
    EXPECT_GT(result.camxbin, 1);
    EXPECT_GT(result.camybin, 1);
    EXPECT_LE(largeImage.cols / result.camxbin, 2000);
    EXPECT_LE(largeImage.rows / result.camybin, 2000);
}

TEST_F(BinningTest, ProcessMatWithBinAvgBasic) {
    auto result = processMatWithBinAvg(smallImage, 2, 2, false, true);
    EXPECT_EQ(result.rows, smallImage.rows / 2);
    EXPECT_EQ(result.cols, smallImage.cols / 2);
    EXPECT_EQ(result.type(), smallImage.type());
}

TEST_F(BinningTest, ProcessMatWithBinAvgColor) {
    auto result = processMatWithBinAvg(colorImage, 2, 2, true, true);
    EXPECT_EQ(result.rows, colorImage.rows / 2);
    EXPECT_EQ(result.cols, colorImage.cols / 2);
    EXPECT_EQ(result.channels(), 3);
}

TEST_F(BinningTest, ProcessMatDifferentBitDepths) {
    auto result8 = processMatWithBinAvg(image8bit, 2, 2, false, true);
    auto result16 = processMatWithBinAvg(image16bit, 2, 2, false, true);
    auto result32 = processMatWithBinAvg(image32bit, 2, 2, false, true);

    EXPECT_EQ(result8.depth(), CV_8U);
    EXPECT_EQ(result16.depth(), CV_16U);
    EXPECT_EQ(result32.depth(), CV_32S);
}

TEST_F(BinningTest, BinningVsAveraging) {
    auto resultBin = processMatWithBinAvg(gradient, 2, 2, false, false);
    auto resultAvg = processMatWithBinAvg(gradient, 2, 2, false, true);

    EXPECT_NE(cv::sum(resultBin), cv::sum(resultAvg));
    EXPECT_EQ(resultBin.size(), resultAvg.size());
}

TEST_F(BinningTest, DifferentBinningFactors) {
    auto result = processMatWithBinAvg(smallImage, 4, 2, false, true);
    EXPECT_EQ(result.rows, smallImage.rows / 2);
    EXPECT_EQ(result.cols, smallImage.cols / 4);
}

TEST_F(BinningTest, InvalidInputs) {
    EXPECT_THROW(processMatWithBinAvg(cv::Mat(), 2, 2, false, true), cv::Exception);
    EXPECT_THROW(processMatWithBinAvg(smallImage, 0, 2, false, true), cv::Exception);
    EXPECT_THROW(processMatWithBinAvg(smallImage, 2, 0, false, true), cv::Exception);
}

TEST_F(BinningTest, BinningPreservesAverageIntensity) {
    cv::Scalar originalMean = cv::mean(smallImage);
    auto result = processMatWithBinAvg(smallImage, 2, 2, false, true);
    cv::Scalar binnedMean = cv::mean(result);
    
    EXPECT_NEAR(originalMean[0], binnedMean[0], 1.0);
}

TEST_F(BinningTest, ColorImagePreservesChannels) {
    cv::Mat colorGradient(100, 100, CV_8UC3);
    for(int i = 0; i < colorGradient.rows; i++) {
        for(int j = 0; j < colorGradient.cols; j++) {
            colorGradient.at<cv::Vec3b>(i, j) = cv::Vec3b(i, j, 128);
        }
    }

    auto result = processMatWithBinAvg(colorGradient, 2, 2, true, true);
    EXPECT_EQ(result.channels(), 3);
    EXPECT_EQ(result.type(), CV_8UC3);
}

TEST_F(BinningTest, LargeBinningFactors) {
    auto result = processMatWithBinAvg(smallImage, 10, 10, false, true);
    EXPECT_EQ(result.rows, smallImage.rows / 10);
    EXPECT_EQ(result.cols, smallImage.cols / 10);
    EXPECT_GT(cv::mean(result)[0], 0);
}

}  // namespace