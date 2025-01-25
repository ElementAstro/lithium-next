#include "thumbhash.hpp"
#include <filesystem>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

// Creates a test image with gradient pattern
cv::Mat createTestImage(int width = 512, int height = 512) {
    cv::Mat testImage(height, width, CV_8UC3);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            testImage.at<cv::Vec3b>(y, x) =
                cv::Vec3b(static_cast<uchar>(x * 255 / width),
                          static_cast<uchar>(y * 255 / height),
                          static_cast<uchar>((x + y) * 255 / (width + height)));
        }
    }
    return testImage;
}

void displayImage(const std::string& windowName, const cv::Mat& image) {
    cv::imshow(windowName, image);
    cv::waitKey(0);
}

// Ensures test directory exists and is clean
void setupTestEnvironment(const std::string& testDir) {
    if (fs::exists(testDir)) {
        fs::remove_all(testDir);
    }
    fs::create_directory(testDir);
}

int main() {
    try {
        const std::string testDir = "test_output";
        setupTestEnvironment(testDir);

        // 1. Generate and save test image
        cv::Mat originalImage = createTestImage();
        std::string imagePath = testDir + "/test_image.jpg";
        cv::imwrite(imagePath, originalImage);

        // Display original image
        displayImage("Original Image", originalImage);

        // 2. Encode to ThumbHash
        std::vector<double> thumbHash = encodeThumbHash(originalImage);
        std::cout << "ThumbHash size: " << thumbHash.size() << std::endl;

        // 3. Convert to Base64 string
        std::string base64String = base64Encode(thumbHash);
        std::cout << "Base64 encoded result: " << base64String << std::endl;

        // 4. Decode ThumbHash
        int width = 100;  // desired output dimensions
        int height = 100;
        cv::Mat decodedImage = decodeThumbHash(thumbHash, width, height);
        displayImage("Decoded ThumbHash", decodedImage);
        cv::imwrite(testDir + "/decoded_image.jpg", decodedImage);

        // 5. Test RGB to YCbCr conversion
        cv::Vec3b testRGB(255, 128, 64);  // test RGB value
        YCbCr yCbCr = rgbToYCbCr(testRGB);
        std::cout << "YCbCr conversion results:" << std::endl;
        std::cout << "Y: " << yCbCr.y << std::endl;
        std::cout << "Cb: " << yCbCr.cb << std::endl;
        std::cout << "Cr: " << yCbCr.cr << std::endl;

        // 6. Test DCT transformation
        cv::Mat testMat = cv::Mat::eye(8, 8, CV_64F);
        cv::Mat dctOutput;
        dct(testMat, dctOutput);
        std::cout << "DCT transformation result (8x8 matrix partial):"
                  << std::endl;
        std::cout << dctOutput(cv::Rect(0, 0, 3, 3)) << std::endl;

        // 7. Error handling test
        try {
            cv::Mat emptyMat;
            std::vector<double> emptyHash = encodeThumbHash(emptyMat);
        } catch (const std::exception& e) {
            std::cout << "Expected error caught: " << e.what() << std::endl;
        }

        cv::destroyAllWindows();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error occurred: " << e.what() << std::endl;
        return -1;
    }
}
