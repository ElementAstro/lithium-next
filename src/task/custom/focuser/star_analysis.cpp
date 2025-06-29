#include "star_analysis.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>

namespace lithium::task::custom::focuser {

StarAnalysisTask::StarAnalysisTask(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config)
    , last_image_width_(0)
    , last_image_height_(0)
    , analysis_complete_(false) {

    setTaskName("StarAnalysis");
    setTaskDescription("Advanced star detection and focus quality analysis");
}

bool StarAnalysisTask::validateParameters() const {
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }

    if (config_.detection_threshold <= 0.0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid detection threshold");
        return false;
    }

    if (config_.min_star_radius >= config_.max_star_radius) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid star radius range");
        return false;
    }

    return true;
}

void StarAnalysisTask::resetTask() {
    BaseFocuserTask::resetTask();

    std::lock_guard<std::mutex> lock(analysis_mutex_);

    analysis_complete_ = false;
    last_analysis_ = AnalysisResult{};
    last_image_data_.clear();
    last_image_width_ = 0;
    last_image_height_ = 0;
}

Task::TaskResult StarAnalysisTask::executeImpl() {
    try {
        updateProgress(0.0, "Starting star analysis");

        auto result = analyzeCurrentImage();
        if (result != TaskResult::Success) {
            return result;
        }

        if (config_.detailed_psf_analysis) {
            updateProgress(70.0, "Performing PSF analysis");
            result = performAdvancedAnalysis();
            if (result != TaskResult::Success) {
                // Don't fail for advanced analysis issues
            }
        }

        updateProgress(100.0, "Star analysis completed");
        analysis_complete_ = true;

        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError,
                    std::string("Star analysis failed: ") + e.what());
        return TaskResult::Error;
    }
}

void StarAnalysisTask::updateProgress() {
    if (analysis_complete_) {
        std::ostringstream status;
        status << "Analysis complete - " << last_analysis_.reliable_stars
               << " stars, HFR: " << std::fixed << std::setprecision(2)
               << last_analysis_.median_hfr;
        setProgressMessage(status.str());
    }
}

std::string StarAnalysisTask::getTaskInfo() const {
    std::ostringstream info;
    info << "StarAnalysis";

    std::lock_guard<std::mutex> lock(analysis_mutex_);
    if (analysis_complete_) {
        info << " - Stars: " << last_analysis_.reliable_stars
             << ", HFR: " << std::fixed << std::setprecision(2) << last_analysis_.median_hfr
             << ", Score: " << std::setprecision(3) << last_analysis_.overall_focus_score;
    }

    return info.str();
}

Task::TaskResult StarAnalysisTask::analyzeCurrentImage() {
    try {
        updateProgress(10.0, "Capturing image for analysis");

        // Capture image
        auto capture_result = captureAndAnalyze();
        if (capture_result != TaskResult::Success) {
            return capture_result;
        }

        // Get image data (this would need to be implemented in base class)
        // For now, we'll simulate the process
        last_image_width_ = 1024;  // Example dimensions
        last_image_height_ = 768;
        last_image_data_.resize(last_image_width_ * last_image_height_);

        // Fill with simulated data for demonstration
        std::fill(last_image_data_.begin(), last_image_data_.end(), 1000); // Background level

        updateProgress(30.0, "Detecting stars");

        std::lock_guard<std::mutex> lock(analysis_mutex_);

        last_analysis_.timestamp = std::chrono::steady_clock::now();
        last_analysis_.stars.clear();
        last_analysis_.warnings.clear();

        // Detect stars
        auto detection_result = detectStars(last_image_data_, last_image_width_,
                                          last_image_height_, last_analysis_.stars);
        if (detection_result != TaskResult::Success) {
            return detection_result;
        }

        updateProgress(50.0, "Measuring star properties");

        // Refine positions and measure properties
        auto refinement_result = refineStarPositions(last_analysis_.stars,
                                                   last_image_data_,
                                                   last_image_width_,
                                                   last_image_height_);
        if (refinement_result != TaskResult::Success) {
            return refinement_result;
        }

        updateProgress(70.0, "Calculating statistics");

        // Calculate statistics
        calculateStatistics(last_analysis_.stars, last_analysis_);

        // Assess overall focus quality
        last_analysis_.overall_focus_score = calculateOverallFocusScore(last_analysis_.stars);
        last_analysis_.focus_assessment = assessFocusQuality(last_analysis_);

        updateProgress(90.0, "Finalizing analysis");

        // Save outputs if requested
        if (config_.save_detection_overlay && !config_.output_directory.empty()) {
            saveDetectionOverlay(last_analysis_,
                                config_.output_directory + "/detection_overlay.png");
        }

        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError,
                    std::string("Image analysis failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult StarAnalysisTask::detectStars(const std::vector<uint16_t>& image_data,
                                              int width, int height,
                                              std::vector<StarData>& stars) {
    stars.clear();

    try {
        // Calculate background statistics
        double background = calculateBackgroundLevel(image_data, width, height);
        double noise = calculateBackgroundNoise(image_data, width, height, background);
        double threshold = background + config_.detection_threshold * noise;

        // Simple peak detection algorithm
        // In a real implementation, this would be more sophisticated
        for (int y = config_.max_star_radius; y < height - config_.max_star_radius; ++y) {
            for (int x = config_.max_star_radius; x < width - config_.max_star_radius; ++x) {
                double pixel_value = getPixelValue(image_data, x, y, width, height);

                if (pixel_value > threshold) {
                    // Check if this is a local maximum
                    bool is_peak = true;
                    for (int dy = -1; dy <= 1 && is_peak; ++dy) {
                        for (int dx = -1; dx <= 1 && is_peak; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            double neighbor = getPixelValue(image_data, x + dx, y + dy, width, height);
                            if (neighbor >= pixel_value) {
                                is_peak = false;
                            }
                        }
                    }

                    if (is_peak) {
                        StarData star;
                        star.x = x;
                        star.y = y;
                        star.peak_adu = pixel_value;
                        star.background = background;
                        star.snr = (pixel_value - background) / noise;

                        // Basic quality checks
                        if (star.snr >= config_.min_snr &&
                            star.peak_adu >= config_.min_peak_adu) {
                            stars.push_back(star);
                        }
                    }
                }
            }
        }

        // Sort by brightness and limit number of stars
        std::sort(stars.begin(), stars.end(),
                 [](const StarData& a, const StarData& b) {
                     return a.peak_adu > b.peak_adu;
                 });

        if (stars.size() > 100) { // Reasonable limit
            stars.resize(100);
        }

        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError,
                    std::string("Star detection failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult StarAnalysisTask::refineStarPositions(std::vector<StarData>& stars,
                                                      const std::vector<uint16_t>& image_data,
                                                      int width, int height) {
    try {
        for (auto& star : stars) {
            // Calculate centroid for better position accuracy
            double sum_x = 0.0, sum_y = 0.0, sum_weight = 0.0;

            for (int dy = -3; dy <= 3; ++dy) {
                for (int dx = -3; dx <= 3; ++dx) {
                    int px = static_cast<int>(star.x) + dx;
                    int py = static_cast<int>(star.y) + dy;

                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        double value = getPixelValue(image_data, px, py, width, height);
                        double weight = std::max(0.0, value - star.background);

                        sum_x += px * weight;
                        sum_y += py * weight;
                        sum_weight += weight;
                    }
                }
            }

            if (sum_weight > 0) {
                star.x = sum_x / sum_weight;
                star.y = sum_y / sum_weight;
            }

            // Calculate focus quality metrics
            if (config_.calculate_hfr) {
                star.hfr = calculateHFR(star, image_data, width, height);
            }

            if (config_.calculate_fwhm) {
                star.fwhm = calculateFWHM(star, image_data, width, height);
            }

            if (config_.calculate_eccentricity) {
                star.eccentricity = calculateEccentricity(star, image_data, width, height);
            }

            // Calculate HFD (Half Flux Diameter)
            star.hfd = star.hfr * 2.0;

            // Quality assessments
            star.saturated = isStarSaturated(star);
            star.edge_star = isStarNearEdge(star, width, height);
            star.reliable = isStarReliable(star);
        }

        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError,
                    std::string("Star position refinement failed: ") + e.what());
        return TaskResult::Error;
    }
}

double StarAnalysisTask::calculateHFR(const StarData& star,
                                     const std::vector<uint16_t>& image_data,
                                     int width, int height) {
    // Calculate Half Flux Radius
    std::vector<std::pair<double, double>> radial_data; // radius, flux

    double total_flux = 0.0;

    // Collect radial data
    for (int dy = -config_.max_star_radius; dy <= config_.max_star_radius; ++dy) {
        for (int dx = -config_.max_star_radius; dx <= config_.max_star_radius; ++dx) {
            int px = static_cast<int>(star.x) + dx;
            int py = static_cast<int>(star.y) + dy;

            if (px >= 0 && px < width && py >= 0 && py < height) {
                double radius = std::sqrt(dx * dx + dy * dy);
                if (radius <= config_.max_star_radius) {
                    double value = getPixelValue(image_data, px, py, width, height);
                    double flux = std::max(0.0, value - star.background);

                    radial_data.emplace_back(radius, flux);
                    total_flux += flux;
                }
            }
        }
    }

    if (radial_data.empty() || total_flux <= 0) {
        return 0.0;
    }

    // Sort by radius
    std::sort(radial_data.begin(), radial_data.end());

    // Find radius containing half the flux
    double half_flux = total_flux / 2.0;
    double cumulative_flux = 0.0;

    for (const auto& point : radial_data) {
        cumulative_flux += point.second;
        if (cumulative_flux >= half_flux) {
            return point.first;
        }
    }

    return config_.max_star_radius; // Fallback
}

double StarAnalysisTask::calculateFWHM(const StarData& star,
                                      const std::vector<uint16_t>& image_data,
                                      int width, int height) {
    // Calculate Full Width Half Maximum
    double peak_value = star.peak_adu;
    double half_max = star.background + (peak_value - star.background) / 2.0;

    // Find width at half maximum in X direction
    double left_x = star.x, right_x = star.x;

    // Search left
    for (double x = star.x - 1; x >= star.x - config_.max_star_radius; x -= 0.5) {
        double value = getInterpolatedPixelValue(image_data, x, star.y, width, height);
        if (value < half_max) {
            left_x = x;
            break;
        }
    }

    // Search right
    for (double x = star.x + 1; x <= star.x + config_.max_star_radius; x += 0.5) {
        double value = getInterpolatedPixelValue(image_data, x, star.y, width, height);
        if (value < half_max) {
            right_x = x;
            break;
        }
    }

    double width_x = right_x - left_x;

    // Find width at half maximum in Y direction
    double top_y = star.y, bottom_y = star.y;

    // Search up
    for (double y = star.y - 1; y >= star.y - config_.max_star_radius; y -= 0.5) {
        double value = getInterpolatedPixelValue(image_data, star.x, y, width, height);
        if (value < half_max) {
            top_y = y;
            break;
        }
    }

    // Search down
    for (double y = star.y + 1; y <= star.y + config_.max_star_radius; y += 0.5) {
        double value = getInterpolatedPixelValue(image_data, star.x, y, width, height);
        if (value < half_max) {
            bottom_y = y;
            break;
        }
    }

    double width_y = bottom_y - top_y;

    // Return average of X and Y FWHM
    return (width_x + width_y) / 2.0;
}

double StarAnalysisTask::calculateEccentricity(const StarData& star,
                                              const std::vector<uint16_t>& image_data,
                                              int width, int height) {
    // Calculate second moments for shape analysis
    double m20 = 0.0, m02 = 0.0, m11 = 0.0;
    double total_weight = 0.0;

    for (int dy = -config_.max_star_radius/2; dy <= config_.max_star_radius/2; ++dy) {
        for (int dx = -config_.max_star_radius/2; dx <= config_.max_star_radius/2; ++dx) {
            int px = static_cast<int>(star.x) + dx;
            int py = static_cast<int>(star.y) + dy;

            if (px >= 0 && px < width && py >= 0 && py < height) {
                double value = getPixelValue(image_data, px, py, width, height);
                double weight = std::max(0.0, value - star.background);

                if (weight > 0) {
                    double rel_x = px - star.x;
                    double rel_y = py - star.y;

                    m20 += weight * rel_x * rel_x;
                    m02 += weight * rel_y * rel_y;
                    m11 += weight * rel_x * rel_y;
                    total_weight += weight;
                }
            }
        }
    }

    if (total_weight <= 0) {
        return 0.0;
    }

    m20 /= total_weight;
    m02 /= total_weight;
    m11 /= total_weight;

    // Calculate eccentricity from second moments
    double discriminant = (m20 - m02) * (m20 - m02) + 4 * m11 * m11;
    if (discriminant < 0) {
        return 0.0;
    }

    double sqrt_disc = std::sqrt(discriminant);
    double a = std::sqrt(2 * (m20 + m02 + sqrt_disc));
    double b = std::sqrt(2 * (m20 + m02 - sqrt_disc));

    if (a <= 0) {
        return 0.0;
    }

    return std::sqrt(1.0 - (b * b) / (a * a));
}

double StarAnalysisTask::calculateBackgroundLevel(const std::vector<uint16_t>& image_data,
                                                 int width, int height) {
    // Use median of image for robust background estimation
    std::vector<uint16_t> sample_data;

    // Sample every 10th pixel to reduce computation
    for (size_t i = 0; i < image_data.size(); i += 10) {
        sample_data.push_back(image_data[i]);
    }

    if (sample_data.empty()) {
        return 1000.0; // Default background
    }

    std::sort(sample_data.begin(), sample_data.end());
    return static_cast<double>(sample_data[sample_data.size() / 2]);
}

double StarAnalysisTask::calculateBackgroundNoise(const std::vector<uint16_t>& image_data,
                                                  int width, int height, double background) {
    // Calculate standard deviation of background pixels
    double sum_sq_diff = 0.0;
    size_t count = 0;

    // Sample every 20th pixel
    for (size_t i = 0; i < image_data.size(); i += 20) {
        double value = static_cast<double>(image_data[i]);
        if (std::abs(value - background) < background * 0.1) { // Likely background pixel
            double diff = value - background;
            sum_sq_diff += diff * diff;
            ++count;
        }
    }

    if (count == 0) {
        return 10.0; // Default noise level
    }

    return std::sqrt(sum_sq_diff / count);
}

void StarAnalysisTask::calculateStatistics(std::vector<StarData>& stars, AnalysisResult& result) {
    result.total_stars_detected = static_cast<int>(stars.size());

    // Count reliable stars
    result.reliable_stars = static_cast<int>(
        std::count_if(stars.begin(), stars.end(),
                     [](const StarData& star) { return star.reliable; }));

    // Count saturated stars
    result.saturated_stars = static_cast<int>(
        std::count_if(stars.begin(), stars.end(),
                     [](const StarData& star) { return star.saturated; }));

    // Calculate HFR statistics for reliable stars only
    std::vector<double> hfr_values, fwhm_values;
    for (const auto& star : stars) {
        if (star.reliable && star.hfr > 0) {
            hfr_values.push_back(star.hfr);
        }
        if (star.reliable && star.fwhm > 0) {
            fwhm_values.push_back(star.fwhm);
        }
    }

    if (!hfr_values.empty()) {
        result.median_hfr = calculateMedian(hfr_values);
        result.mean_hfr = std::accumulate(hfr_values.begin(), hfr_values.end(), 0.0) / hfr_values.size();
        result.hfr_std_dev = calculateStandardDeviation(hfr_values, result.mean_hfr);
    }

    if (!fwhm_values.empty()) {
        result.median_fwhm = calculateMedian(fwhm_values);
        result.mean_fwhm = std::accumulate(fwhm_values.begin(), fwhm_values.end(), 0.0) / fwhm_values.size();
        result.fwhm_std_dev = calculateStandardDeviation(fwhm_values, result.mean_fwhm);
    }

    // Calculate background statistics
    result.background_level = calculateBackgroundLevel(last_image_data_, last_image_width_, last_image_height_);
    result.background_noise = calculateBackgroundNoise(last_image_data_, last_image_width_, last_image_height_, result.background_level);

    // Add warnings for common issues
    if (result.reliable_stars < 3) {
        result.warnings.push_back("Very few reliable stars detected");
    }
    if (result.saturated_stars > result.total_stars_detected / 3) {
        result.warnings.push_back("Many stars are saturated");
    }
    if (result.hfr_std_dev > result.mean_hfr * 0.3) {
        result.warnings.push_back("High HFR variation across field");
    }
}

double StarAnalysisTask::calculateMedian(const std::vector<double>& values) {
    if (values.empty()) return 0.0;

    std::vector<double> sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());

    size_t size = sorted_values.size();
    if (size % 2 == 0) {
        return (sorted_values[size/2 - 1] + sorted_values[size/2]) / 2.0;
    } else {
        return sorted_values[size/2];
    }
}

double StarAnalysisTask::calculateStandardDeviation(const std::vector<double>& values, double mean) {
    if (values.size() <= 1) return 0.0;

    double sum_sq_diff = 0.0;
    for (double value : values) {
        double diff = value - mean;
        sum_sq_diff += diff * diff;
    }

    return std::sqrt(sum_sq_diff / (values.size() - 1));
}

double StarAnalysisTask::calculateOverallFocusScore(const std::vector<StarData>& stars) const {
    // Get reliable stars only
    std::vector<StarData> reliable_stars;
    std::copy_if(stars.begin(), stars.end(), std::back_inserter(reliable_stars),
                [](const StarData& star) { return star.reliable; });

    if (reliable_stars.empty()) {
        return 0.0;
    }

    // Calculate score based on HFR quality
    std::vector<double> hfr_values;
    for (const auto& star : reliable_stars) {
        if (star.hfr > 0) {
            hfr_values.push_back(star.hfr);
        }
    }

    if (hfr_values.empty()) {
        return 0.0;
    }

    double median_hfr = calculateMedian(hfr_values);

    // Score: 1.0 for HFR <= 1.0, decreasing to 0 for HFR >= 5.0
    double hfr_score = std::max(0.0, 1.0 - (median_hfr - 1.0) / 4.0);

    // Penalty for high variation
    double mean_hfr = std::accumulate(hfr_values.begin(), hfr_values.end(), 0.0) / hfr_values.size();
    double std_dev = calculateStandardDeviation(hfr_values, mean_hfr);
    double consistency_score = std::max(0.0, 1.0 - std_dev / mean_hfr);

    // Combine scores
    return (hfr_score * 0.7 + consistency_score * 0.3);
}

std::string StarAnalysisTask::assessFocusQuality(const AnalysisResult& result) const {
    if (result.overall_focus_score >= 0.8) {
        return "Excellent focus quality";
    } else if (result.overall_focus_score >= 0.6) {
        return "Good focus quality";
    } else if (result.overall_focus_score >= 0.4) {
        return "Fair focus quality - improvement possible";
    } else if (result.overall_focus_score >= 0.2) {
        return "Poor focus quality - adjustment needed";
    } else {
        return "Very poor focus quality - significant adjustment required";
    }
}

bool StarAnalysisTask::isStarReliable(const StarData& star) const {
    return star.snr >= config_.min_snr &&
           star.hfr > 0 && star.hfr <= config_.max_star_radius &&
           star.eccentricity <= config_.max_eccentricity &&
           !star.saturated && !star.edge_star;
}

bool StarAnalysisTask::isStarSaturated(const StarData& star) const {
    return star.peak_adu >= 65535 * config_.saturation_threshold;
}

bool StarAnalysisTask::isStarNearEdge(const StarData& star, int width, int height) const {
    int margin = config_.max_star_radius * 2;
    return star.x < margin || star.x >= width - margin ||
           star.y < margin || star.y >= height - margin;
}

double StarAnalysisTask::getPixelValue(const std::vector<uint16_t>& image_data,
                                      int x, int y, int width, int height) {
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return 0.0;
    }
    return static_cast<double>(image_data[y * width + x]);
}

double StarAnalysisTask::getInterpolatedPixelValue(const std::vector<uint16_t>& image_data,
                                                  double x, double y, int width, int height) {
    int x1 = static_cast<int>(std::floor(x));
    int y1 = static_cast<int>(std::floor(y));
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    if (x1 < 0 || x2 >= width || y1 < 0 || y2 >= height) {
        return getPixelValue(image_data, static_cast<int>(x), static_cast<int>(y), width, height);
    }

    double fx = x - x1;
    double fy = y - y1;

    double v11 = getPixelValue(image_data, x1, y1, width, height);
    double v12 = getPixelValue(image_data, x1, y2, width, height);
    double v21 = getPixelValue(image_data, x2, y1, width, height);
    double v22 = getPixelValue(image_data, x2, y2, width, height);

    // Bilinear interpolation
    double v1 = v11 * (1 - fx) + v21 * fx;
    double v2 = v12 * (1 - fx) + v22 * fx;
    return v1 * (1 - fy) + v2 * fy;
}

Task::TaskResult StarAnalysisTask::performAdvancedAnalysis() {
    // Advanced analysis implementation would go here
    return TaskResult::Success;
}

StarAnalysisTask::AnalysisResult StarAnalysisTask::getLastAnalysis() const {
    std::lock_guard<std::mutex> lock(analysis_mutex_);
    return last_analysis_;
}

std::vector<StarAnalysisTask::StarData> StarAnalysisTask::getDetectedStars() const {
    std::lock_guard<std::mutex> lock(analysis_mutex_);
    return last_analysis_.stars;
}

FocusQuality StarAnalysisTask::getFocusQualityFromAnalysis() const {
    std::lock_guard<std::mutex> lock(analysis_mutex_);

    FocusQuality quality;
    quality.hfr = last_analysis_.median_hfr;
    quality.fwhm = last_analysis_.median_fwhm;
    quality.star_count = last_analysis_.reliable_stars;
    quality.peak_value = 0.0; // Would need to be calculated from stars

    return quality;
}

Task::TaskResult StarAnalysisTask::saveDetectionOverlay(const AnalysisResult& result,
                                                       const std::string& filename) {
    // Implementation for saving overlay image would go here
    return TaskResult::Success;
}

// SimpleStarDetector implementation (simplified version)

SimpleStarDetector::SimpleStarDetector(std::shared_ptr<device::Camera> camera, const Config& config)
    : BaseFocuserTask(nullptr)
    , camera_(std::move(camera))
    , config_(config) {

    setTaskName("SimpleStarDetector");
    setTaskDescription("Basic star detection");
}

bool SimpleStarDetector::validateParameters() const {
    return camera_ != nullptr;
}

void SimpleStarDetector::resetTask() {
    BaseFocuserTask::resetTask();
    detected_stars_.clear();
}

Task::TaskResult SimpleStarDetector::executeImpl() {
    // Simplified implementation
    detected_stars_.clear();

    // Simulate detecting some stars
    for (int i = 0; i < 10; ++i) {
        Star star;
        star.x = 100 + i * 50;
        star.y = 100 + i * 30;
        star.brightness = 1000 + i * 100;
        star.hfr = 2.0 + i * 0.1;
        detected_stars_.push_back(star);
    }

    return TaskResult::Success;
}

void SimpleStarDetector::updateProgress() {
    // Simple progress update
}

std::string SimpleStarDetector::getTaskInfo() const {
    return "SimpleStarDetector - " + std::to_string(detected_stars_.size()) + " stars";
}

std::vector<SimpleStarDetector::Star> SimpleStarDetector::getDetectedStars() const {
    return detected_stars_;
}

int SimpleStarDetector::getStarCount() const {
    return static_cast<int>(detected_stars_.size());
}

double SimpleStarDetector::getMedianHFR() const {
    if (detected_stars_.empty()) return 0.0;

    std::vector<double> hfr_values;
    for (const auto& star : detected_stars_) {
        hfr_values.push_back(star.hfr);
    }

    std::sort(hfr_values.begin(), hfr_values.end());
    return hfr_values[hfr_values.size() / 2];
}

// FocusQualityAnalyzer implementation

FocusQualityAnalyzer::QualityMetrics FocusQualityAnalyzer::analyzeQuality(
    const std::vector<StarAnalysisTask::StarData>& stars) {

    QualityMetrics metrics;

    std::vector<StarAnalysisTask::StarData> reliable_stars;
    std::copy_if(stars.begin(), stars.end(), std::back_inserter(reliable_stars),
                [](const StarAnalysisTask::StarData& star) { return star.reliable; });

    if (reliable_stars.empty()) {
        metrics.quality_grade = "F";
        metrics.recommendations.push_back("No reliable stars detected");
        return metrics;
    }

    metrics.hfr_quality = calculateHFRQuality(reliable_stars);
    metrics.fwhm_quality = calculateFWHMQuality(reliable_stars);
    metrics.consistency_quality = calculateConsistencyQuality(reliable_stars);

    metrics.overall_quality = (metrics.hfr_quality * 0.5 +
                              metrics.fwhm_quality * 0.3 +
                              metrics.consistency_quality * 0.2);

    metrics.quality_grade = getQualityGrade(metrics.overall_quality);
    metrics.recommendations = getRecommendations(metrics);

    return metrics;
}

double FocusQualityAnalyzer::calculateHFRQuality(const std::vector<StarAnalysisTask::StarData>& stars) {
    std::vector<double> hfr_values;
    for (const auto& star : stars) {
        if (star.hfr > 0) {
            hfr_values.push_back(star.hfr);
        }
    }

    if (hfr_values.empty()) return 0.0;

    std::sort(hfr_values.begin(), hfr_values.end());
    double median_hfr = hfr_values[hfr_values.size() / 2];

    // Quality score: 1.0 for HFR <= 1.5, decreasing to 0 for HFR >= 5.0
    return std::max(0.0, std::min(1.0, (5.0 - median_hfr) / 3.5));
}

std::string FocusQualityAnalyzer::getQualityGrade(double overall_quality) {
    if (overall_quality >= 0.9) return "A";
    if (overall_quality >= 0.8) return "B";
    if (overall_quality >= 0.6) return "C";
    if (overall_quality >= 0.4) return "D";
    return "F";
}

} // namespace lithium::task::custom::focuser
