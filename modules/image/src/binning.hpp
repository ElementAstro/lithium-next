#ifndef LITHIUM_MODULE_IMAGE_BINNING_HPP
#define LITHIUM_MODULE_IMAGE_BINNING_HPP

#include <boost/iterator/counting_iterator.hpp>
#include <opencv2/opencv.hpp>
#include <span>

/**
 * @brief Struct to hold camera binning information.
 */
struct CamBin {
    uint32_t camxbin{1};  ///< Binning factor in the x direction.
    uint32_t camybin{1};  ///< Binning factor in the y direction.
};

/**
 * @brief Merges the image based on its size and determines the appropriate
 * binning factors.
 *
 * @param image The input image.
 * @return CamBin The binning factors for the image.
 */
CamBin mergeImageBasedOnSize(const cv::Mat& image);

/**
 * @brief Processes the image with binning or average binning.
 *
 * @param image The input image.
 * @param camxbin Binning factor in the x direction.
 * @param camybin Binning factor in the y direction.
 * @param isColor Flag indicating if the image is a color image.
 * @param isAVG Flag indicating if average binning should be used.
 * @return cv::Mat The processed image.
 */
cv::Mat processMatWithBinAvg(const cv::Mat& image, uint32_t camxbin,
                             uint32_t camybin, bool isColor = false,
                             bool isAVG = true);

/**
 * @brief Processes the image data in parallel with binning.
 *
 * @tparam T The data type of the image.
 * @param srcData The source image data.
 * @param result The result image.
 * @param width The width of the source image.
 * @param height The height of the source image.
 * @param camxbin Binning factor in the x direction.
 * @param camybin Binning factor in the y direction.
 * @param binArea The area of the bin.
 */
template <typename T>
void parallel_process_bin(std::span<const uint8_t> srcData, cv::Mat& result,
                          uint32_t width, uint32_t height, uint32_t camxbin,
                          uint32_t camybin, uint32_t binArea);

/**
 * @brief Processes the image data with monochrome binning.
 *
 * @tparam T The data type of the image.
 * @param srcData The source image data.
 * @param result The result image.
 * @param srcStride The stride of the source image.
 * @param camxbin Binning factor in the x direction.
 * @param camybin Binning factor in the y direction.
 */
template <typename T>
void process_mono_bin(std::span<const uint8_t> srcData, cv::Mat& result,
                      uint32_t srcStride, uint32_t camxbin, uint32_t camybin);

/**
 * @brief Calculates the average of the given values.
 *
 * @tparam T The data type of the values.
 * @param values The values to average.
 * @param binSize The size of the bin.
 * @return T The average value.
 */
template <typename T>
T calculateAverage(std::span<const T> values, size_t binSize);

/**
 * @brief Processes the image data with average binning.
 *
 * @param srcData The source image data.
 * @param width The width of the source image.
 * @param height The height of the source image.
 * @param depth The bit depth of the image.
 * @param newWidth The width of the result image.
 * @param newHeight The height of the result image.
 * @param camxbin Binning factor in the x direction.
 * @param camybin Binning factor in the y direction.
 * @return cv::Mat The processed image.
 */
cv::Mat processWithAverage(std::span<const uint8_t> srcData, uint32_t width,
                           uint32_t height, uint32_t depth, uint32_t newWidth,
                           uint32_t newHeight, uint32_t camxbin,
                           uint32_t camybin);

/**
 * @brief Processes the image data with binning.
 *
 * @param srcData The source image data.
 * @param width The width of the source image.
 * @param height The height of the source image.
 * @param channels The number of channels in the image.
 * @param depth The bit depth of the image.
 * @param newWidth The width of the result image.
 * @param newHeight The height of the result image.
 * @param camxbin Binning factor in the x direction.
 * @param camybin Binning factor in the y direction.
 * @param isColor Flag indicating if the image is a color image.
 * @return cv::Mat The processed image.
 */
cv::Mat processWithBinning(std::span<const uint8_t> srcData, uint32_t width,
                           uint32_t height, uint32_t channels, uint32_t depth,
                           uint32_t newWidth, uint32_t newHeight,
                           uint32_t camxbin, uint32_t camybin, bool isColor);

#endif  // LITHIUM_MODULE_IMAGE_BINNING_HPP