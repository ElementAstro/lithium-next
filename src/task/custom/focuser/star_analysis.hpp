#pragma once

#include "base.hpp"
#include <vector>
#include <optional>

namespace lithium::task::custom::focuser {

/**
 * @brief Star detection and analysis for focus quality assessment
 *
 * This task performs sophisticated star detection, measurement,
 * and analysis to provide detailed focus quality metrics.
 */
class StarAnalysisTask : public BaseFocuserTask {
public:
    struct Config {
        // Detection parameters
        double detection_threshold = 3.0;      // Sigma above background
        int min_star_radius = 2;               // Minimum star radius in pixels
        int max_star_radius = 20;              // Maximum star radius in pixels
        double saturation_threshold = 0.9;     // Fraction of max ADU for saturation

        // Analysis parameters
        bool calculate_hfr = true;              // Calculate Half Flux Radius
        bool calculate_fwhm = true;             // Calculate Full Width Half Maximum
        bool calculate_eccentricity = true;    // Calculate star shape metrics
        bool calculate_background = true;       // Calculate background statistics

        // Quality filters
        double min_snr = 5.0;                  // Minimum signal-to-noise ratio
        double max_eccentricity = 0.8;         // Maximum eccentricity for "round" stars
        int min_peak_adu = 100;                // Minimum peak brightness

        // Advanced analysis
        bool detailed_psf_analysis = false;    // Perform detailed PSF fitting
        bool star_profile_analysis = false;    // Analyze star intensity profiles
        bool focus_aberration_analysis = false; // Detect focus aberrations

        // Output options
        bool save_detection_overlay = false;   // Save image with detected stars marked
        bool save_star_profiles = false;       // Save individual star profiles
        std::string output_directory = "star_analysis";
    };

    struct StarData {
        // Position
        double x = 0.0, y = 0.0;               // Centroid position

        // Basic measurements
        double peak_adu = 0.0;                 // Peak brightness
        double total_flux = 0.0;               // Integrated flux
        double background = 0.0;               // Local background level
        double snr = 0.0;                      // Signal-to-noise ratio

        // Focus quality metrics
        double hfr = 0.0;                      // Half Flux Radius
        double fwhm = 0.0;                     // Full Width Half Maximum
        double hfd = 0.0;                      // Half Flux Diameter

        // Shape analysis
        double eccentricity = 0.0;             // 0 = perfect circle, 1 = line
        double major_axis = 0.0;               // Major axis length
        double minor_axis = 0.0;               // Minor axis length
        double position_angle = 0.0;           // Orientation angle (degrees)

        // Quality indicators
        bool saturated = false;                // Is star saturated?
        bool edge_star = false;                // Is star near image edge?
        bool reliable = true;                  // Is measurement reliable?

        // Advanced metrics (if enabled)
        std::optional<double> psf_fit_quality; // Goodness of PSF fit
        std::vector<double> radial_profile;    // Radial intensity profile
        std::optional<double> aberration_score; // Focus aberration indicator
    };

    struct AnalysisResult {
        std::chrono::steady_clock::time_point timestamp;

        // Detected stars
        std::vector<StarData> stars;
        int total_stars_detected = 0;
        int reliable_stars = 0;
        int saturated_stars = 0;

        // Overall quality metrics
        double median_hfr = 0.0;
        double mean_hfr = 0.0;
        double hfr_std_dev = 0.0;
        double median_fwhm = 0.0;
        double mean_fwhm = 0.0;
        double fwhm_std_dev = 0.0;

        // Image statistics
        double background_level = 0.0;
        double background_noise = 0.0;
        double dynamic_range = 0.0;

        // Focus quality assessment
        double overall_focus_score = 0.0;      // 0-1, higher is better
        std::string focus_assessment;          // Human-readable assessment

        // Advanced analysis (if enabled)
        std::optional<double> field_curvature;  // Field curvature measurement
        std::optional<double> astigmatism;      // Astigmatism measurement
        std::optional<double> coma;             // Coma aberration measurement

        // Warnings and notes
        std::vector<std::string> warnings;
        std::string analysis_notes;
    };

    StarAnalysisTask(std::shared_ptr<device::Focuser> focuser,
                    std::shared_ptr<device::Camera> camera,
                    const Config& config = {});

    // Task interface
    bool validateParameters() const override;
    void resetTask() override;

protected:
    TaskResult executeImpl() override;
    void updateProgress() override;
    std::string getTaskInfo() const override;

public:
    // Configuration
    void setConfig(const Config& config);
    Config getConfig() const;

    // Analysis operations
    TaskResult analyzeCurrentImage();
    TaskResult analyzeImage(const std::string& image_path);
    TaskResult performAdvancedAnalysis();

    // Data access
    AnalysisResult getLastAnalysis() const;
    std::vector<StarData> getDetectedStars() const;
    FocusQuality getFocusQualityFromAnalysis() const;

    // Star filtering and selection
    std::vector<StarData> getReliableStars() const;
    std::vector<StarData> getBrightestStars(int count) const;
    std::vector<StarData> getStarsInRegion(double x1, double y1, double x2, double y2) const;

    // Quality assessment
    double calculateOverallFocusScore(const std::vector<StarData>& stars) const;
    std::string assessFocusQuality(const AnalysisResult& result) const;

    // Advanced analysis
    TaskResult detectFieldCurvature();
    TaskResult detectAstigmatism();
    TaskResult analyzeAberrations();

private:
    // Core detection algorithms
    TaskResult detectStars(const std::vector<uint16_t>& image_data,
                          int width, int height, std::vector<StarData>& stars);
    TaskResult refineStarPositions(std::vector<StarData>& stars,
                                  const std::vector<uint16_t>& image_data,
                                  int width, int height);

    // Measurement algorithms
    double calculateHFR(const StarData& star, const std::vector<uint16_t>& image_data,
                       int width, int height);
    double calculateFWHM(const StarData& star, const std::vector<uint16_t>& image_data,
                        int width, int height);
    double calculateEccentricity(const StarData& star, const std::vector<uint16_t>& image_data,
                                int width, int height);

    // Background analysis
    double calculateBackgroundLevel(const std::vector<uint16_t>& image_data,
                                   int width, int height);
    double calculateBackgroundNoise(const std::vector<uint16_t>& image_data,
                                   int width, int height, double background);

    // PSF analysis
    TaskResult performPSFAnalysis(StarData& star, const std::vector<uint16_t>& image_data,
                                 int width, int height);
    std::vector<double> extractRadialProfile(const StarData& star,
                                           const std::vector<uint16_t>& image_data,
                                           int width, int height);

    // Quality assessment helpers
    bool isStarReliable(const StarData& star) const;
    bool isStarSaturated(const StarData& star) const;
    bool isStarNearEdge(const StarData& star, int width, int height) const;

    // Statistical analysis
    void calculateStatistics(std::vector<StarData>& stars, AnalysisResult& result);
    double calculateMedian(const std::vector<double>& values);
    double calculateStandardDeviation(const std::vector<double>& values, double mean);

    // Advanced aberration detection
    double detectFieldCurvature(const std::vector<StarData>& stars, int width, int height);
    double detectAstigmatism(const std::vector<StarData>& stars);
    double detectComa(const std::vector<StarData>& stars);

    // Utility functions
    double getPixelValue(const std::vector<uint16_t>& image_data, int x, int y,
                        int width, int height);
    double getInterpolatedPixelValue(const std::vector<uint16_t>& image_data,
                                    double x, double y, int width, int height);

    // Output functions
    TaskResult saveDetectionOverlay(const AnalysisResult& result,
                                   const std::string& filename);
    TaskResult saveStarProfiles(const std::vector<StarData>& stars,
                               const std::string& directory);

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;

    // Analysis data
    AnalysisResult last_analysis_;
    std::vector<uint16_t> last_image_data_;
    int last_image_width_ = 0;
    int last_image_height_ = 0;

    // Processing state
    bool analysis_complete_ = false;

    // Thread safety
    mutable std::mutex analysis_mutex_;
};

/**
 * @brief Simple star detector for basic applications
 */
class SimpleStarDetector : public BaseFocuserTask {
public:
    struct Config {
        double threshold_sigma = 3.0;
        int min_star_size = 3;
        int max_stars = 100;
    };

    struct Star {
        double x, y;
        double brightness;
        double hfr;
    };

    SimpleStarDetector(std::shared_ptr<device::Camera> camera,
                      const Config& config = {});

    // Task interface
    bool validateParameters() const override;
    void resetTask() override;

protected:
    TaskResult executeImpl() override;
    void updateProgress() override;
    std::string getTaskInfo() const override;

public:
    void setConfig(const Config& config);
    Config getConfig() const;

    std::vector<Star> getDetectedStars() const;
    int getStarCount() const;
    double getMedianHFR() const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;
    std::vector<Star> detected_stars_;
};

/**
 * @brief Focus quality analyzer using star metrics
 */
class FocusQualityAnalyzer {
public:
    struct QualityMetrics {
        double hfr_quality = 0.0;          // Quality based on HFR (0-1)
        double fwhm_quality = 0.0;         // Quality based on FWHM (0-1)
        double consistency_quality = 0.0;  // Quality based on star consistency (0-1)
        double overall_quality = 0.0;      // Combined quality score (0-1)

        std::string quality_grade;         // A, B, C, D, F
        std::vector<std::string> recommendations;
    };

    static QualityMetrics analyzeQuality(const std::vector<StarAnalysisTask::StarData>& stars);
    static QualityMetrics compareQuality(const std::vector<StarAnalysisTask::StarData>& stars1,
                                        const std::vector<StarAnalysisTask::StarData>& stars2);

    static std::string getQualityGrade(double overall_quality);
    static std::vector<std::string> getRecommendations(const QualityMetrics& metrics);

private:
    static double calculateHFRQuality(const std::vector<StarAnalysisTask::StarData>& stars);
    static double calculateFWHMQuality(const std::vector<StarAnalysisTask::StarData>& stars);
    static double calculateConsistencyQuality(const std::vector<StarAnalysisTask::StarData>& stars);
};

} // namespace lithium::task::custom::focuser
