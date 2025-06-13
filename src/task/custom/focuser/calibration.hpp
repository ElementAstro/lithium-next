#pragma once

#include "base.hpp"
#include <map>
#include <vector>
#include <optional>

namespace lithium::task::custom::focuser {

/**
 * @brief Task for calibrating focuser parameters and creating focus models
 * 
 * This task performs comprehensive calibration to establish optimal
 * focusing parameters, temperature coefficients, and focus models
 * for different conditions.
 */
class FocusCalibrationTask : public BaseFocuserTask {
public:
    struct CalibrationConfig {
        // Focus range calibration
        int full_range_start = -1000;      // Start of full range test
        int full_range_end = 1000;         // End of full range test
        int coarse_step_size = 100;        // Large steps for initial sweep
        int fine_step_size = 10;           // Fine steps around optimal region
        int ultra_fine_step_size = 2;      // Ultra-fine steps for precision
        
        // Temperature calibration
        bool calibrate_temperature = true;
        double min_temp_range = 5.0;       // Minimum temperature range for calibration
        int temp_focus_samples = 10;       // Samples per temperature point
        
        // Multi-point calibration
        bool multi_point_calibration = true;
        std::vector<int> calibration_positions; // Specific positions to test
        
        // Quality thresholds
        double min_star_count = 5;
        double max_acceptable_hfr = 5.0;
        
        // Timing
        std::chrono::seconds settling_time{1};
        std::chrono::seconds image_interval{2};
        
        // Advanced options
        bool create_focus_model = true;
        bool validate_backlash = true;
        bool optimize_step_size = true;
        bool save_calibration_images = false;
        std::string calibration_data_path = "focus_calibration.json";
    };

    struct CalibrationPoint {
        int position;
        FocusQuality quality;
        double temperature;
        std::chrono::steady_clock::time_point timestamp;
        std::string notes;
    };

    struct CalibrationResult {
        // Optimal focus parameters
        int optimal_position = 0;
        double optimal_hfr = 0.0;
        double optimal_fwhm = 0.0;
        int focus_range_min = 0;
        int focus_range_max = 0;
        
        // Temperature compensation
        double temperature_coefficient = 0.0;
        double temp_coeff_confidence = 0.0;
        std::pair<double, double> temperature_range; // min, max
        
        // Step size optimization
        int recommended_coarse_steps = 50;
        int recommended_fine_steps = 5;
        int recommended_ultra_fine_steps = 1;
        
        // Backlash measurements
        int inward_backlash = 0;
        int outward_backlash = 0;
        double backlash_confidence = 0.0;
        
        // Quality metrics
        double calibration_confidence = 0.0;
        std::chrono::steady_clock::time_point calibration_time;
        size_t total_measurements = 0;
        std::chrono::seconds calibration_duration{0};
        
        // Curve analysis
        struct CurveAnalysis {
            double curve_sharpness = 0.0;      // How sharp the focus curve is
            double asymmetry_factor = 0.0;     // Asymmetry of the curve
            int critical_focus_zone = 0;       // Size of critical focus region
            double repeatability = 0.0;        // Focus repeatability
        } curve_analysis;
        
        std::vector<CalibrationPoint> data_points;
    };

    struct FocusModel {
        // Polynomial coefficients for focus curve
        std::vector<double> curve_coefficients;
        
        // Temperature model
        double base_temperature = 20.0;
        double temp_coefficient = 0.0;
        
        // Confidence intervals
        double position_uncertainty = 0.0;
        double temperature_uncertainty = 0.0;
        
        // Model validity
        std::pair<int, int> valid_position_range;
        std::pair<double, double> valid_temperature_range;
        std::chrono::steady_clock::time_point model_creation_time;
        
        // Model quality
        double r_squared = 0.0;             // Goodness of fit
        double mean_absolute_error = 0.0;   // Average prediction error
    };

    FocusCalibrationTask(std::shared_ptr<device::Focuser> focuser,
                        std::shared_ptr<device::Camera> camera,
                        std::shared_ptr<device::TemperatureSensor> temperature_sensor = nullptr,
                        const CalibrationConfig& config = {});

    // Task interface
    bool validateParameters() const override;
    void resetTask() override;

protected:
    TaskResult executeImpl() override;
    void updateProgress() override;
    std::string getTaskInfo() const override;

public:
    // Configuration
    void setConfig(const CalibrationConfig& config);
    CalibrationConfig getConfig() const;

    // Calibration operations
    TaskResult performFullCalibration();
    TaskResult performQuickCalibration();
    TaskResult performTemperatureCalibration();
    TaskResult performBacklashCalibration();
    TaskResult performStepSizeOptimization();

    // Model creation
    TaskResult createFocusModel();
    TaskResult validateFocusModel();
    std::optional<FocusModel> getFocusModel() const;

    // Data access
    CalibrationResult getCalibrationResult() const;
    std::vector<CalibrationPoint> getCalibrationData() const;
    
    // Prediction using model
    std::optional<int> predictOptimalPosition(double temperature) const;
    std::optional<double> predictFocusQuality(int position, double temperature) const;

    // Calibration analysis
    TaskResult analyzeCalibrationQuality();
    std::vector<std::string> getCalibrationRecommendations() const;

    // Import/Export
    TaskResult saveCalibrationData(const std::string& filename) const;
    TaskResult loadCalibrationData(const std::string& filename);
    TaskResult exportFocusModel(const std::string& filename) const;
    TaskResult importFocusModel(const std::string& filename);

private:
    // Core calibration methods
    TaskResult performCoarseCalibration();
    TaskResult performFineCalibration(int center_position, int range);
    TaskResult performUltraFineCalibration(int center_position, int range);
    
    // Temperature-specific methods
    TaskResult collectTemperatureFocusData();
    TaskResult analyzeTemperatureRelationship();
    
    // Analysis methods
    TaskResult analyzeFocusCurve();
    int findOptimalPosition(const std::vector<CalibrationPoint>& points);
    double calculateCurveSharpness(const std::vector<CalibrationPoint>& points);
    double calculateAsymmetry(const std::vector<CalibrationPoint>& points);
    
    // Model building
    TaskResult buildPolynomialModel();
    TaskResult validateModelAccuracy();
    std::vector<double> fitPolynomial(const std::vector<std::pair<double, double>>& data, int degree);
    
    // Optimization methods
    TaskResult optimizeStepSizes();
    int calculateOptimalStepSize(const std::vector<CalibrationPoint>& data, double quality_threshold);
    
    // Data collection helpers
    TaskResult collectCalibrationPoint(int position, CalibrationPoint& point);
    TaskResult collectMultiplePoints(int position, int count, CalibrationPoint& averaged_point);
    bool isCalibrationPointValid(const CalibrationPoint& point);
    
    // Analysis helpers
    double calculateConfidence(const std::vector<CalibrationPoint>& points);
    double calculateRepeatability(const std::vector<CalibrationPoint>& points);
    std::pair<int, int> findCriticalFocusZone(const std::vector<CalibrationPoint>& points);
    
    // Validation methods
    bool validateCalibrationRange();
    bool validateTemperatureRange();
    TaskResult performValidationTest();

private:
    std::shared_ptr<device::Camera> camera_;
    std::shared_ptr<device::TemperatureSensor> temperature_sensor_;
    CalibrationConfig config_;
    
    // Calibration data
    CalibrationResult result_;
    std::vector<CalibrationPoint> calibration_data_;
    std::optional<FocusModel> focus_model_;
    
    // Progress tracking
    size_t total_expected_measurements_ = 0;
    size_t completed_measurements_ = 0;
    std::chrono::steady_clock::time_point calibration_start_time_;
    
    // State management
    bool calibration_in_progress_ = false;
    std::string current_phase_;
    
    // Thread safety
    mutable std::mutex calibration_mutex_;
};

/**
 * @brief Quick focus calibration for basic setups
 */
class QuickFocusCalibration : public BaseFocuserTask {
public:
    struct Config {
        int search_range = 200;
        int step_size = 20;
        int fine_step_size = 5;
        std::chrono::seconds settling_time{500};
    };

    struct Result {
        int optimal_position;
        double focus_quality;
        bool calibration_successful;
        std::string notes;
    };

    QuickFocusCalibration(std::shared_ptr<device::Focuser> focuser,
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
    void setConfig(const Config& config);
    Config getConfig() const;
    Result getResult() const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;
    Result result_;
};

/**
 * @brief Focus model validator for testing existing models
 */
class FocusModelValidator {
public:
    struct ValidationResult {
        bool model_valid;
        double accuracy_score;              // 0-1, higher is better
        double mean_error;                  // Average prediction error
        double max_error;                   // Maximum prediction error
        size_t test_points;                 // Number of validation points
        std::vector<std::pair<int, double>> error_data; // Position, error pairs
        std::string validation_notes;
    };

    static ValidationResult validateModel(
        const FocusCalibrationTask::FocusModel& model,
        const std::vector<FocusCalibrationTask::CalibrationPoint>& test_data);
    
    static ValidationResult crossValidateModel(
        const std::vector<FocusCalibrationTask::CalibrationPoint>& all_data,
        int polynomial_degree = 3);
    
    static bool isModelReliable(const ValidationResult& result);
    static std::vector<std::string> getValidationRecommendations(const ValidationResult& result);

private:
    static double calculatePredictionError(
        const FocusCalibrationTask::FocusModel& model,
        const FocusCalibrationTask::CalibrationPoint& point);
    
    static double evaluatePolynomial(const std::vector<double>& coefficients, double x);
};

} // namespace lithium::task::custom::focuser
