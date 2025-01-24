#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include "thumbhash.hpp"

namespace {

class ThumbHashTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create some test images
        solidRed =
            cv::Mat(100, 100, CV_8UC3, cv::Scalar(0, 0, 255));  // BGR format
        solidBlue = cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
        smallImage = cv::Mat(16, 16, CV_8UC3, cv::Scalar(0, 255, 0));
        largeImage = cv::Mat(1000, 1000, CV_8UC3, cv::Scalar(128, 128, 128));

        // Create a gradient image
        gradient = cv::Mat(100, 100, CV_8UC3);
        for (int i = 0; i < 100; i++) {
            for (int j = 0; j < 100; j++) {
                gradient.at<cv::Vec3b>(i, j) =
                    cv::Vec3b(i * 2.55, j * 2.55, 128);
            }
        }
    }

    cv::Mat solidRed;
    cv::Mat solidBlue;
    cv::Mat smallImage;
    cv::Mat largeImage;
    cv::Mat gradient;
};

TEST_F(ThumbHashTest, BasicEncoding) {
    auto hash = encodeThumbHash(solidRed);
    ASSERT_FALSE(hash.empty());
    ASSERT_EQ(hash.size(), 6 * 6 * 3);  // DCT_SIZE * DCT_SIZE * 3 channels
}

TEST_F(ThumbHashTest, EmptyImageHandling) {
    cv::Mat emptyImage;
    ASSERT_THROW(encodeThumbHash(emptyImage), std::invalid_argument);
}

TEST_F(ThumbHashTest, SmallImageHandling) {
    auto hash = encodeThumbHash(smallImage);
    ASSERT_FALSE(hash.empty());
    ASSERT_EQ(hash.size(), 6 * 6 * 3);
}

TEST_F(ThumbHashTest, LargeImageHandling) {
    auto hash = encodeThumbHash(largeImage);
    ASSERT_FALSE(hash.empty());
    ASSERT_EQ(hash.size(), 6 * 6 * 3);
}

TEST_F(ThumbHashTest, ConsistencyTest) {
    auto hash1 = encodeThumbHash(solidBlue);
    auto hash2 = encodeThumbHash(solidBlue);
    ASSERT_EQ(hash1.size(), hash2.size());

    for (size_t i = 0; i < hash1.size(); i++) {
        EXPECT_NEAR(hash1[i], hash2[i], 1e-6);
    }
}

TEST_F(ThumbHashTest, DifferentImagesProduceDifferentHashes) {
    auto redHash = encodeThumbHash(solidRed);
    auto blueHash = encodeThumbHash(solidBlue);

    bool isDifferent = false;
    for (size_t i = 0; i < redHash.size(); i++) {
        if (std::abs(redHash[i] - blueHash[i]) > 1e-6) {
            isDifferent = true;
            break;
        }
    }
    ASSERT_TRUE(isDifferent);
}

TEST_F(ThumbHashTest, GradientImageEncoding) {
    auto hash = encodeThumbHash(gradient);
    ASSERT_FALSE(hash.empty());
    ASSERT_EQ(hash.size(), 6 * 6 * 3);

    // Gradient image should have non-zero DCT coefficients
    bool hasNonZeroCoefficients = false;
    for (const auto& value : hash) {
        if (std::abs(value) > 1e-6) {
            hasNonZeroCoefficients = true;
            break;
        }
    }
    ASSERT_TRUE(hasNonZeroCoefficients);
}

TEST_F(ThumbHashTest, MemoryLeakTest) {
    for (int i = 0; i < 1000; i++) {
        auto hash = encodeThumbHash(gradient);
        ASSERT_FALSE(hash.empty());
    }
}

TEST_F(ThumbHashTest, InvalidImageFormat) {
    cv::Mat invalidImage(100, 100, CV_32F, cv::Scalar(1.0f));  // Wrong type
    ASSERT_THROW(encodeThumbHash(invalidImage), cv::Exception);
}

TEST_F(ThumbHashTest, ZeroSizeImageDimension) {
    cv::Mat zeroWidthImage(100, 0, CV_8UC3);
    cv::Mat zeroHeightImage(0, 100, CV_8UC3);

    ASSERT_THROW(encodeThumbHash(zeroWidthImage), std::invalid_argument);
    ASSERT_THROW(encodeThumbHash(zeroHeightImage), std::invalid_argument);
}

// Test for reasonable hash values
TEST_F(ThumbHashTest, HashValueRangeTest) {
    auto hash = encodeThumbHash(gradient);

    // DCT coefficients should be within reasonable bounds
    for (const auto& value : hash) {
        ASSERT_FALSE(std::isnan(value));
        ASSERT_FALSE(std::isinf(value));
    }
}
}  // namespace