#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include "imgio.hpp"

namespace fs = std::filesystem;

class ImgIOTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test images
        solidBlack = cv::Mat(100, 100, CV_8UC3, cv::Scalar(0, 0, 0));
        solidWhite = cv::Mat(100, 100, CV_8UC3, cv::Scalar(255, 255, 255));
        rgbGradient = cv::Mat(100, 100, CV_8UC3);
        grayscaleImage = cv::Mat(100, 100, CV_8UC1, cv::Scalar(128));
        rgba = cv::Mat(100, 100, CV_8UC4, cv::Scalar(255, 0, 0, 128));

        // Create gradient image
        for (int i = 0; i < rgbGradient.rows; i++) {
            for (int j = 0; j < rgbGradient.cols; j++) {
                rgbGradient.at<cv::Vec3b>(i, j) =
                    cv::Vec3b(i * 2.55, j * 2.55, 128);
            }
        }

        // Create temporary test directory
        testDir = fs::temp_directory_path() / "imgio_test";
        fs::create_directories(testDir);

        // Save test images
        cv::imwrite((testDir / "black.png").string(), solidBlack);
        cv::imwrite((testDir / "white.jpg").string(), solidWhite);
        cv::imwrite((testDir / "gradient.png").string(), rgbGradient);
        cv::imwrite((testDir / "gray.png").string(), grayscaleImage);
        cv::imwrite((testDir / "rgba.png").string(), rgba);
    }

    void TearDown() override {
        // Clean up test directory
        fs::remove_all(testDir);
    }

    cv::Mat solidBlack;
    cv::Mat solidWhite;
    cv::Mat rgbGradient;
    cv::Mat grayscaleImage;
    cv::Mat rgba;
    fs::path testDir;
};

TEST_F(ImgIOTest, LoadSingleImage) {
    auto img = loadImage((testDir / "black.png").string());
    ASSERT_FALSE(img.empty());
    ASSERT_EQ(img.size(), cv::Size(100, 100));
    ASSERT_EQ(img.type(), CV_8UC3);
}

TEST_F(ImgIOTest, LoadGrayscale) {
    auto img =
        loadImage((testDir / "gradient.png").string(), cv::IMREAD_GRAYSCALE);
    ASSERT_FALSE(img.empty());
    ASSERT_EQ(img.channels(), 1);
    ASSERT_EQ(img.type(), CV_8UC1);
}

TEST_F(ImgIOTest, LoadNonexistentImage) {
    auto img = loadImage((testDir / "nonexistent.jpg").string());
    ASSERT_TRUE(img.empty());
}

TEST_F(ImgIOTest, LoadMultipleImages) {
    std::vector<std::string> filenames = {"black.png", "white.jpg",
                                          "gradient.png"};
    auto images = loadImages(testDir.string(), filenames);

    ASSERT_EQ(images.size(), 3);
    for (const auto& [filename, img] : images) {
        ASSERT_FALSE(img.empty());
        ASSERT_EQ(img.size(), cv::Size(100, 100));
    }
}

TEST_F(ImgIOTest, LoadAllImagesInFolder) {
    auto images = loadImages(testDir.string());
    ASSERT_EQ(images.size(), 5);  // All test images
}

TEST_F(ImgIOTest, SaveImage) {
    fs::path outputPath = testDir / "saved.png";
    bool success = saveImage(outputPath.string(), rgbGradient);
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists(outputPath));

    // Verify saved image
    auto loadedImg = cv::imread(outputPath.string());
    ASSERT_FALSE(loadedImg.empty());
    ASSERT_EQ(loadedImg.size(), rgbGradient.size());
}

TEST_F(ImgIOTest, SaveMatTo8BitJpg) {
    fs::path outputPath = testDir / "test8bit.jpg";
    bool success = saveMatTo8BitJpg(rgbGradient, outputPath.string());
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists(outputPath));

    auto loadedImg = cv::imread(outputPath.string());
    ASSERT_FALSE(loadedImg.empty());
    ASSERT_EQ(loadedImg.depth(), CV_8U);
}

TEST_F(ImgIOTest, SaveMatTo16BitPng) {
    cv::Mat img16bit;
    rgbGradient.convertTo(img16bit, CV_16U);

    fs::path outputPath = testDir / "test16bit.png";
    bool success = saveMatTo16BitPng(img16bit, outputPath.string());
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists(outputPath));

    auto loadedImg = cv::imread(outputPath.string(), cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(loadedImg.empty());
    ASSERT_EQ(loadedImg.depth(), CV_16U);
}

TEST_F(ImgIOTest, SaveMatToFits) {
    fs::path outputPath = testDir / "test.fits";
    bool success = saveMatToFits(rgbGradient, outputPath.string());
    ASSERT_TRUE(success);
    ASSERT_TRUE(fs::exists(outputPath));
}

TEST_F(ImgIOTest, HandleEmptyImage) {
    cv::Mat emptyImg;
    ASSERT_FALSE(saveImage((testDir / "empty.png").string(), emptyImg));
    ASSERT_FALSE(saveMatTo8BitJpg(emptyImg));
    ASSERT_FALSE(saveMatTo16BitPng(emptyImg));
    ASSERT_FALSE(saveMatToFits(emptyImg));
}

TEST_F(ImgIOTest, InvalidPath) {
    ASSERT_FALSE(saveImage("/nonexistent/path/image.png", rgbGradient));
    ASSERT_FALSE(saveMatTo8BitJpg(rgbGradient, "/nonexistent/path/image.jpg"));
    ASSERT_FALSE(saveMatTo16BitPng(rgbGradient, "/nonexistent/path/image.png"));
    ASSERT_FALSE(saveMatToFits(rgbGradient, "/nonexistent/path/image.fits"));
}

TEST_F(ImgIOTest, LoadImagesWithInvalidFlags) {
    auto img = loadImage((testDir / "black.png").string(), -1);
    ASSERT_FALSE(img.empty());  // Should use default flags
}

TEST_F(ImgIOTest, LoadMultipleImagesWithMixedValidity) {
    std::vector<std::string> filenames = {"black.png", "nonexistent.jpg",
                                          "white.jpg"};
    auto images = loadImages(testDir.string(), filenames);
    ASSERT_EQ(images.size(), 2);  // Only valid images should be loaded
}
