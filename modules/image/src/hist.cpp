#include "hist.hpp"

#include <cmath>
#include <opencv2/imgproc.hpp>
#include <vector>

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>

auto calculateHist(const cv::Mat& img, int histSize,
                   bool normalize) -> std::vector<cv::Mat> {
    LOG_F(INFO, "Starting histogram calculation for BGR image");
    LOG_F(DEBUG, "Parameters: histSize={}, normalize={}", histSize, normalize);

    if (img.empty()) {
        LOG_F(ERROR, "Input image is empty");
        THROW_INVALID_ARGUMENT("Empty input image");
    }

    if (img.channels() != 3) {
        LOG_F(ERROR, "Expected 3-channel image, got {} channels",
              img.channels());
        THROW_INVALID_ARGUMENT("Image must have 3 channels");
    }

    LOG_F(DEBUG, "Input image size: {}x{}", img.cols, img.rows);

    std::vector<cv::Mat> bgrPlanes;
    cv::split(img, bgrPlanes);
    LOG_F(DEBUG, "Split BGR image into {} planes", bgrPlanes.size());

    std::array<float, 2> range = {0, static_cast<float>(histSize)};
    const float* histRange = range.data();
    bool accumulate = false;

    std::vector<cv::Mat> histograms;
    histograms.reserve(3);

    for (int i = 0; i < 3; ++i) {
        LOG_F(DEBUG, "Processing channel {} histogram", i);
        cv::Mat hist;
        cv::calcHist(&bgrPlanes[i], 1, 0, cv::Mat(), hist, 1, &histSize,
                     &histRange, accumulate);

        double minVal, maxVal;
        cv::minMaxLoc(hist, &minVal, &maxVal);
        LOG_F(DEBUG, "Channel {} raw histogram range: [{}, {}]", i, minVal,
              maxVal);

        cv::threshold(hist, hist, 4, 0, cv::THRESH_TOZERO);
        LOG_F(DEBUG, "Applied threshold to remove noise");

        if (normalize) {
            cv::normalize(hist, hist, 0, 1, cv::NORM_MINMAX);
            LOG_F(DEBUG, "Normalized histogram for channel {}", i);
        }

        histograms.push_back(hist);
        LOG_F(INFO, "Completed histogram calculation for channel {}", i);
    }

    LOG_F(INFO, "Successfully calculated BGR histograms");
    return histograms;
}

auto calculateGrayHist(const cv::Mat& img, int histSize,
                       bool normalize) -> cv::Mat {
    LOG_F(INFO, "Starting grayscale histogram calculation");
    LOG_F(DEBUG, "Parameters: histSize={}, normalize={}", histSize, normalize);
    LOG_F(DEBUG, "Input image: {}x{}, depth={}", img.cols, img.rows,
          img.depth());

    if (img.empty()) {
        LOG_F(ERROR, "Input image is empty - cannot calculate histogram");
        THROW_INVALID_ARGUMENT("Empty input image");
    }

    if (img.channels() != 1) {
        LOG_F(ERROR, "Invalid channel count: expected 1, got {}",
              img.channels());
        THROW_INVALID_ARGUMENT("Image must be grayscale (1 channel)");
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::array<float, 2> range = {0, static_cast<float>(histSize)};
    const float* histRange = range.data();
    bool accumulate = false;

    cv::Mat grayHist;
    cv::calcHist(&img, 1, 0, cv::Mat(), grayHist, 1, &histSize, &histRange,
                 accumulate);
    LOG_F(DEBUG, "Raw histogram calculated: {}x{}", grayHist.rows,
          grayHist.cols);

    double minVal, maxVal;
    cv::minMaxLoc(grayHist, &minVal, &maxVal);
    LOG_F(DEBUG, "Histogram range: [{}, {}]", minVal, maxVal);

    cv::threshold(grayHist, grayHist, 1, 0, cv::THRESH_TOZERO);
    LOG_F(DEBUG, "Applied noise threshold");

    if (normalize) {
        cv::normalize(grayHist, grayHist, 0, 1, cv::NORM_MINMAX);
        LOG_F(DEBUG, "Histogram normalized to [0,1] range");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOG_F(INFO, "Histogram calculation completed in {}ms", duration.count());

    return grayHist;
}

auto calculateCDF(const cv::Mat& hist) -> cv::Mat {
    LOG_F(INFO, "Starting CDF calculation");
    LOG_F(DEBUG, "Input histogram size: {}x{}", hist.cols, hist.rows);

    if (hist.empty()) {
        LOG_F(ERROR, "Cannot calculate CDF - input histogram is empty");
        THROW_INVALID_ARGUMENT("Empty histogram");
    }

    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat cdf;
    hist.copyTo(cdf);
    LOG_F(DEBUG, "Histogram copied for CDF calculation");

    double sum = 0.0;
    for (int i = 1; i < hist.rows; ++i) {
        sum += cdf.at<float>(i - 1);
        cdf.at<float>(i) += sum;
    }
    LOG_F(DEBUG, "Accumulated histogram values");

    double minVal, maxVal;
    cv::minMaxLoc(cdf, &minVal, &maxVal);
    LOG_F(DEBUG, "Pre-normalization CDF range: [{}, {}]", minVal, maxVal);

    cv::normalize(cdf, cdf, 0, 1, cv::NORM_MINMAX);
    LOG_F(DEBUG, "CDF normalized to [0,1] range");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOG_F(INFO, "CDF calculation completed in {}ms", duration.count());

    return cdf;
}

auto equalizeHistogram(const cv::Mat& img) -> cv::Mat {
    LOG_F(INFO, "Starting histogram equalization");
    LOG_F(DEBUG, "Input image: {}x{}, {} channels", img.cols, img.rows,
          img.channels());

    if (img.empty()) {
        LOG_F(ERROR, "Cannot equalize histogram - input image is empty");
        THROW_INVALID_ARGUMENT("Empty input image");
    }

    auto start = std::chrono::high_resolution_clock::now();
    cv::Mat equalized;

    if (img.channels() == 1) {
        LOG_F(DEBUG, "Processing grayscale image");
        cv::equalizeHist(img, equalized);
    } else {
        LOG_F(DEBUG, "Processing multi-channel image");
        std::vector<cv::Mat> bgrPlanes;
        cv::split(img, bgrPlanes);
        LOG_F(DEBUG, "Split into {} color planes", bgrPlanes.size());

        for (int i = 0; i < bgrPlanes.size(); ++i) {
            cv::equalizeHist(bgrPlanes[i], bgrPlanes[i]);
            LOG_F(DEBUG, "Equalized channel {}", i);
        }

        cv::merge(bgrPlanes, equalized);
        LOG_F(DEBUG, "Merged equalized channels");
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOG_F(INFO, "Histogram equalization completed in {}ms", duration.count());

    return equalized;
}

auto drawHistogram(const cv::Mat& hist, int histSize, int width,
                   int height) -> cv::Mat {
    LOG_F(INFO, "Starting histogram visualization");
    LOG_F(DEBUG, "Parameters: histSize={}, width={}, height={}", histSize,
          width, height);

    if (hist.empty()) {
        LOG_F(ERROR, "Cannot draw histogram - input is empty");
        THROW_INVALID_ARGUMENT("Empty histogram");
    }

    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat histImage(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    LOG_F(DEBUG, "Created output image: {}x{}", histImage.cols, histImage.rows);

    cv::Mat histNorm;
    cv::normalize(hist, histNorm, 0, histImage.rows, cv::NORM_MINMAX);
    LOG_F(DEBUG, "Normalized histogram for visualization");

    int binWidth = cvRound(static_cast<double>(width) / histSize);
    LOG_F(DEBUG, "Bin width calculated: {} pixels", binWidth);

    for (int i = 1; i < histSize; ++i) {
        cv::Point p1(binWidth * (i - 1),
                     height - cvRound(histNorm.at<float>(i - 1)));
        cv::Point p2(binWidth * i, height - cvRound(histNorm.at<float>(i)));
        cv::line(histImage, p1, p2, cv::Scalar(DEFAULT_COLOR_VALUE, 0, 0), 2,
                 DEFAULT_LINE_TYPE, 0);
    }
    LOG_F(DEBUG, "Drew {} histogram lines", histSize - 1);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOG_F(INFO, "Histogram visualization completed in {}ms", duration.count());

    return histImage;
}
