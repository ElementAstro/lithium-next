#pragma once

#include "base.hpp"
#include <deque>
#include <optional>

namespace lithium::task::custom::focuser {

/**
 * @brief Task for measuring and compensating focuser backlash
 * 
 * Backlash occurs when changing direction due to mechanical play
 * in gears. This task measures backlash and compensates for it
 * during focusing operations.
 */
class BacklashCompensationTask : public BaseFocuserTask {
public:
    struct Config {
        int measurement_range = 100;        // Range for backlash measurement
        int measurement_steps = 10;         // Steps per measurement point
        int overshoot_steps = 20;           // Extra steps to overcome backlash
        bool auto_measurement = true;       // Automatically measure backlash
        bool auto_compensation = true;      // Automatically apply compensation
        double confidence_threshold = 0.8;  // Minimum confidence for backlash value
        int max_backlash_steps = 200;      // Maximum expected backlash
        std::chrono::seconds settling_time{500}; // Time to wait after movement
    };

    struct BacklashMeasurement {
        std::chrono::steady_clock::time_point timestamp;
        int inward_backlash;               // Steps of backlash moving inward
        int outward_backlash;              // Steps of backlash moving outward
        double confidence;                 // Confidence in measurement (0-1)
        std::string measurement_method;    // How the measurement was taken
        std::vector<std::pair<int, double>> data_points; // Position, quality pairs
    };

    struct CompensationEvent {
        std::chrono::steady_clock::time_point timestamp;
        int original_target;
        int compensated_target;
        int compensation_applied;
        bool direction_change;
        std::string reason;
    };

    BacklashCompensationTask(std::shared_ptr<device::Focuser> focuser,
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

    // Backlash measurement
    TaskResult measureBacklash();
    TaskResult measureBacklashDetailed();
    TaskResult calibrateBacklash();

    // Compensation
    TaskResult moveWithBacklashCompensation(int target_position);
    TaskResult compensateLastMove();
    int calculateCompensatedPosition(int target_position, bool& needs_compensation);

    // Backlash data
    std::optional<BacklashMeasurement> getLastMeasurement() const;
    std::vector<BacklashMeasurement> getMeasurementHistory() const;
    std::vector<CompensationEvent> getCompensationHistory() const;

    // Current backlash values
    int getCurrentInwardBacklash() const;
    int getCurrentOutwardBacklash() const;
    double getBacklashConfidence() const;
    bool hasValidBacklashData() const;

    // Statistics and analysis
    struct Statistics {
        size_t total_measurements = 0;
        size_t total_compensations = 0;
        double average_inward_backlash = 0.0;
        double average_outward_backlash = 0.0;
        double backlash_stability = 0.0;    // How consistent backlash is
        double compensation_accuracy = 0.0; // How well compensation works
        std::chrono::steady_clock::time_point last_measurement;
        std::chrono::steady_clock::time_point last_compensation;
    };
    Statistics getStatistics() const;

    // Advanced features
    TaskResult analyzeBacklashStability();
    TaskResult optimizeCompensationParameters();
    bool shouldRemeasureBacklash() const;

private:
    // Core measurement logic
    TaskResult performBasicMeasurement(BacklashMeasurement& measurement);
    TaskResult performDetailedMeasurement(BacklashMeasurement& measurement);
    TaskResult performHysteresisMeasurement(BacklashMeasurement& measurement);
    
    // Analysis helpers
    int analyzeBacklashFromData(const std::vector<std::pair<int, double>>& data, 
                               bool inward_direction);
    double calculateMeasurementConfidence(const BacklashMeasurement& measurement);
    bool isBacklashMeasurementValid(const BacklashMeasurement& measurement);
    
    // Compensation logic
    TaskResult applyBacklashCompensation(int target_position, int current_position);
    bool needsDirectionChange(int current_position, int target_position);
    int calculateOvershoot(int backlash_amount, int target_position);
    
    // Movement helpers
    TaskResult moveAndSettle(int position);
    TaskResult moveInDirection(int steps, bool inward);
    TaskResult waitForSettling();
    
    // Data management
    void saveMeasurement(const BacklashMeasurement& measurement);
    void saveCompensationEvent(const CompensationEvent& event);
    void pruneOldMeasurements();
    void pruneOldEvents();
    
    // Analysis and optimization
    BacklashMeasurement calculateAverageMeasurement() const;
    double calculateBacklashVariability() const;
    Config optimizeConfigFromHistory() const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;
    
    // Backlash data
    std::deque<BacklashMeasurement> measurement_history_;
    std::deque<CompensationEvent> compensation_history_;
    std::optional<BacklashMeasurement> current_measurement_;
    
    // Movement tracking
    int last_position_ = 0;
    bool last_direction_inward_ = true;
    std::chrono::steady_clock::time_point last_move_time_;
    
    // Calibration state
    bool calibration_in_progress_ = false;
    std::vector<std::pair<int, double>> calibration_data_;
    
    // Statistics cache
    mutable Statistics cached_statistics_;
    mutable std::chrono::steady_clock::time_point statistics_cache_time_;
    
    // Thread safety
    mutable std::mutex measurement_mutex_;
    mutable std::mutex compensation_mutex_;
    
    // Constants
    static constexpr size_t MAX_MEASUREMENT_HISTORY = 100;
    static constexpr size_t MAX_COMPENSATION_HISTORY = 1000;
    static constexpr double MIN_CONFIDENCE = 0.5;
    static constexpr int MIN_MEASUREMENT_POINTS = 5;
};

/**
 * @brief Simple backlash detector for quick assessment
 */
class BacklashDetector : public BaseFocuserTask {
public:
    struct Config {
        int test_range = 50;              // Range for quick test
        int test_steps = 5;               // Steps per test point
        std::chrono::seconds settling_time{200}; // Settling time
    };

    struct DetectionResult {
        bool backlash_detected;
        int estimated_backlash;
        double confidence;
        std::string notes;
    };

    BacklashDetector(std::shared_ptr<device::Focuser> focuser,
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
    
    DetectionResult getLastResult() const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;
    DetectionResult last_result_;
};

/**
 * @brief Backlash compensation advisor for optimization
 */
class BacklashAdvisor {
public:
    struct Recommendation {
        int suggested_inward_backlash;
        int suggested_outward_backlash;
        int suggested_overshoot;
        double confidence;
        std::string reasoning;
        std::vector<std::string> warnings;
    };

    static Recommendation analyzeBacklashData(
        const std::vector<BacklashCompensationTask::BacklashMeasurement>& measurements);
    
    static Recommendation optimizeForFocuser(
        const std::string& focuser_model,
        const std::vector<BacklashCompensationTask::BacklashMeasurement>& measurements);
    
    static bool shouldRecalibrate(
        const std::vector<BacklashCompensationTask::BacklashMeasurement>& measurements,
        std::chrono::steady_clock::time_point last_calibration);

private:
    static double calculateConsistency(
        const std::vector<BacklashCompensationTask::BacklashMeasurement>& measurements);
    static int calculateOptimalBacklash(
        const std::vector<int>& values, double& confidence);
};

} // namespace lithium::task::custom::focuser
