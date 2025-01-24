#ifndef LITHIUM_IMAGE_UTILS_HPP
#define LITHIUM_IMAGE_UTILS_HPP

#include <opencv2/core.hpp>
#include <tuple>
#include <vector>

/**
 * @brief Check if a point is inside a circle.
 *
 * @param xCoord The x-coordinate of the point.
 * @param yCoord The y-coordinate of the point.
 * @param centerX The x-coordinate of the circle's center.
 * @param centerY The y-coordinate of the circle's center.
 * @param radius The radius of the circle.
 * @return bool True if the point is inside the circle, false otherwise.
 */
auto insideCircle(int xCoord, int yCoord, int centerX, int centerY,
                  float radius) -> bool;

/**
 * @brief Check if a rectangle is elongated.
 *
 * @param width The width of the rectangle.
 * @param height The height of the rectangle.
 * @return bool True if the rectangle is elongated, false otherwise.
 */
auto checkElongated(int width, int height) -> bool;

/**
 * @brief Check if a pixel is white.
 *
 * @param rect_contour The image containing the pixel.
 * @param x_coord The x-coordinate of the pixel.
 * @param y_coord The y-coordinate of the pixel.
 * @return int The value of the pixel (0 for black, 255 for white).
 */
auto checkWhitePixel(const cv::Mat& rect_contour, int x_coord,
                     int y_coord) -> int;

/**
 * @brief Check eight-fold symmetry of a circle.
 *
 * @param rect_contour The image containing the circle.
 * @param center The center of the circle.
 * @param x_p The x-coordinate of a point on the circle.
 * @param y_p The y-coordinate of a point on the circle.
 * @return int The symmetry score.
 */
auto checkEightSymmetryCircle(const cv::Mat& rect_contour,
                              const cv::Point& center, int x_p, int y_p) -> int;

/**
 * @brief Check four-fold symmetry of a circle.
 *
 * @param rect_contour The image containing the circle.
 * @param center The center of the circle.
 * @param radius The radius of the circle.
 * @return int The symmetry score.
 */
auto checkFourSymmetryCircle(const cv::Mat& rect_contour,
                             const cv::Point& center, float radius) -> int;

/**
 * @brief Define narrow radius for a circle.
 *
 * @param min_area The minimum area of the circle.
 * @param max_area The maximum area of the circle.
 * @param area The actual area of the circle.
 * @param scale The scale factor.
 * @return std::tuple<int, std::vector<int>, std::vector<double>> The narrow
 * radius and related parameters.
 */
auto defineNarrowRadius(int min_area, double max_area, double area,
                        double scale)
    -> std::tuple<int, std::vector<int>, std::vector<double>>;

/**
 * @brief Check if a circle follows Bresenham's algorithm.
 *
 * @param rect_contour The image containing the circle.
 * @param radius The radius of the circle.
 * @param pixel_ratio The pixel ratio.
 * @param if_debug Flag to enable debug mode.
 * @return bool True if the circle follows Bresenham's algorithm, false
 * otherwise.
 */
auto checkBresenhamCircle(const cv::Mat& rect_contour, float radius,
                          float pixel_ratio, bool if_debug = false) -> bool;

/**
 * @brief Calculate the average deviation of pixel values from a midpoint.
 *
 * @param mid The midpoint value.
 * @param norm_img The normalized image.
 * @return double The average deviation.
 */
auto calculateAverageDeviation(double mid, const cv::Mat& norm_img) -> double;

/**
 * @brief Calculate the Modulation Transfer Function (MTF) of an image.
 *
 * @param magnitude The magnitude of the MTF.
 * @param img The input image.
 * @return cv::Mat The MTF image.
 */
auto calculateMTF(double magnitude, const cv::Mat& img) -> cv::Mat;

/**
 * @brief Calculate the scale factor for resizing an image.
 *
 * @param img The input image.
 * @param resize_size The target size for resizing.
 * @return double The scale factor.
 */
auto calculateScale(const cv::Mat& img, int resize_size = 1552) -> double;

/**
 * @brief Calculate the median deviation of pixel values from a midpoint.
 *
 * @param mid The midpoint value.
 * @param img The input image.
 * @return double The median deviation.
 */
auto calculateMedianDeviation(double mid, const cv::Mat& img) -> double;

/**
 * @brief Compute parameters for a single channel of an image.
 *
 * @param img The input image.
 * @return std::tuple<double, double, double> The computed parameters.
 */
auto computeParamsOneChannel(const cv::Mat& img)
    -> std::tuple<double, double, double>;

/**
 * @brief Perform automatic white balance on an image.
 *
 * @param img The input image.
 * @return cv::Mat The white-balanced image.
 */
auto autoWhiteBalance(const cv::Mat& img) -> cv::Mat;

auto eightSymmetryCircleCheck(const cv::Mat& rect_contour,
                              const cv::Point& center, int xCoord,
                              int yCoord) -> int;

auto fourSymmetryCircleCheck(const cv::Mat& rect_contour,
                             const cv::Point& center, float radius) -> int;
#endif  // LITHIUM_IMAGE_UTILS_HPP