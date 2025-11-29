#ifndef LITHIUM_MODULE_IMAGE_IMGIO_HPP
#define LITHIUM_MODULE_IMAGE_IMGIO_HPP

#include <string>
#include <vector>

namespace cv {
class Mat;
}

/**
 * @brief Load a single image from a file.
 *
 * @param filename The path to the image file.
 * @param flags Flags specifying the color type of a loaded image:
 *              - `cv::IMREAD_COLOR` loads the image in the BGR 8-bit format.
 *              - `cv::IMREAD_GRAYSCALE` loads the image as an 8-bit grayscale
 * image.
 *              - `cv::IMREAD_UNCHANGED` loads the image as is (including the
 * alpha channel if present).
 * @return cv::Mat The loaded image as a cv::Mat object. If the image cannot be
 * loaded, an empty cv::Mat is returned.
 */
auto loadImage(const std::string& filename, int flags = 1) -> cv::Mat;

/**
 * @brief Load multiple images from a folder.
 *
 * @param folder The path to the folder containing the images.
 * @param filenames A vector of filenames to load. If empty, all images in the
 * folder will be loaded.
 * @param flags Flags specifying the color type of a loaded image (same as in
 * loadImage).
 * @return std::vector<std::pair<std::string, cv::Mat>> A vector of pairs, each
 * containing the filename and the loaded image. If an image cannot be loaded,
 * it will not be included in the vector.
 */
auto loadImages(const std::string& folder,
                const std::vector<std::string>& filenames = {}, int flags = 1)
    -> std::vector<std::pair<std::string, cv::Mat>>;

/**
 * @brief Save an image to a file.
 *
 * @param filename The path to the output image file.
 * @param image The image to be saved.
 * @return bool True if the image is successfully saved, false otherwise.
 */
auto saveImage(const std::string& filename, const cv::Mat& image) -> bool;

/**
 * @brief Convert a cv::Mat image to an 8-bit JPG and save it to a file.
 *
 * @param image The input image to be converted.
 * @param output_path The path to the output JPG file. Default is
 * "/dev/shm/MatTo8BitJPG.jpg".
 * @return bool True if the image is successfully converted and saved, false
 * otherwise.
 */
auto saveMatTo8BitJpg(const cv::Mat& image, const std::string& output_path =
                                                "/dev/shm/MatTo8BitJPG.jpg")
    -> bool;

/**
 * @brief Convert a cv::Mat image to a 16-bit PNG and save it to a file.
 *
 * @param image The input image to be converted.
 * @param output_path The path to the output PNG file. Default is
 * "/dev/shm/MatTo16BitPNG.png".
 * @return bool True if the image is successfully converted and saved, false
 * otherwise.
 */
auto saveMatTo16BitPng(const cv::Mat& image, const std::string& output_path =
                                                 "/dev/shm/MatTo16BitPNG.png")
    -> bool;

/**
 * @brief Convert a cv::Mat image to a FITS file and save it.
 *
 * @param image The input image to be converted.
 * @param output_path The path to the output FITS file. Default is
 * "/dev/shm/MatToFITS.fits".
 * @return bool True if the image is successfully converted and saved, false
 * otherwise.
 */
auto saveMatToFits(const cv::Mat& image,
                   const std::string& output_path = "/dev/shm/MatToFITS.fits")
    -> bool;

#endif
