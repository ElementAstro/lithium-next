#pragma once

#include "base.hpp"
#include <chrono>
#include <deque>
#include <optional>

namespace lithium::task::custom::focuser {

/**
 * @brief Task for validating and monitoring focus quality
 *
 * This task continuously monitors focus quality metrics and can
 * trigger corrective actions when focus degrades beyond acceptable
 * thresholds.
 */
class FocusValidationTask : public BaseFocuserTask {
public:
    struct Config {
        double hfr_threshold = 3.0;           // Maximum acceptable HFR
        double fwhm_threshold = 4.0;          // Maximum acceptable FWHM
        int min_star_count = 5;               // Minimum stars required for validation
        double focus_tolerance = 0.1;         // Relative tolerance for focus quality
        std::chrono::seconds validation_interval{300}; // How often to validate
        bool auto_correction = true;          // Enable automatic focus correction
        int max_correction_attempts = 3;      // Maximum correction attempts
        double quality_degradation_threshold = 0.2; // Trigger correction when quality drops by this factor
        bool enable_drift_detection = true;   // Monitor for focus drift over time
        std::chrono::minutes drift_window{30}; // Time window for drift analysis
    };

    struct ValidationResult {
        std::chrono::steady_clock::time_point timestamp;
        FocusQuality quality;
        bool is_valid;
        std::string reason;
        double quality_score;           // 0.0 to 1.0, higher is better
        std::optional<int> recommended_correction; // Steps to improve focus
    };

    struct FocusDriftInfo {
        double drift_rate;              // Focus quality change per hour
        double confidence;              // Confidence in drift detection (0-1)
        std::chrono::steady_clock::time_point analysis_time;
        bool significant_drift;         // Whether drift is significant
        std::string trend_description;  // Human-readable trend description
    };

    FocusValidationTask(std::shared_ptr<device::Focuser> focuser,
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

    // Validation operations
    TaskResult validateCurrentFocus();
    TaskResult validateFocusAtPosition(int position);
    TaskResult performComprehensiveValidation();

    // Monitoring
    void startContinuousMonitoring();
    void stopContinuousMonitoring();
    bool isMonitoring() const;

    // Focus correction
    TaskResult correctFocus();
    TaskResult correctFocusWithHint(int suggested_position);

    // Data access
    std::vector<ValidationResult> getValidationHistory() const;
    ValidationResult getLastValidation() const;
    FocusDriftInfo analyzeFocusDrift() const;
    double getCurrentFocusScore() const;

    // Statistics
    struct Statistics {
        size_t total_validations = 0;
        size_t successful_validations = 0;
        size_t failed_validations = 0;
        size_t corrections_attempted = 0;
        size_t corrections_successful = 0;
        double average_focus_score = 0.0;
        double best_focus_score = 0.0;
        double worst_focus_score = 1.0;
        std::chrono::seconds monitoring_time{0};
        std::chrono::steady_clock::time_point last_good_focus;
    };
    Statistics getStatistics() const;

    // Alerts and notifications
    struct Alert {
        enum Type {
            FocusLost,
            QualityDegraded,
            DriftDetected,
            CorrectionFailed,
            InsufficientStars
        };

        Type type;
        std::chrono::steady_clock::time_point timestamp;
        std::string message;
        double severity; // 0.0 to 1.0
        std::optional<ValidationResult> related_validation;
    };

    std::vector<Alert> getActiveAlerts() const;
    void clearAlerts();

private:
    // Core validation logic
    TaskResult performValidation(ValidationResult& result);
    double calculateFocusScore(const FocusQuality& quality) const;
    bool isFocusAcceptable(const FocusQuality& quality) const;
    std::optional<int> calculateRecommendedCorrection(const FocusQuality& quality) const;

    // Monitoring implementation
    TaskResult monitoringLoop();
    void processValidationResult(const ValidationResult& result);

    // Drift analysis
    FocusDriftInfo performDriftAnalysis() const;
    double calculateDriftRate(const std::vector<ValidationResult>& recent_results) const;
    bool isSignificantDrift(double drift_rate, double confidence) const;

    // Correction logic
    TaskResult attemptFocusCorrection(const ValidationResult& validation);
    TaskResult performCoarseFocusCorrection();
    TaskResult performFineFocusCorrection(int base_position);

    // Alert management
    void checkForAlerts(const ValidationResult& result);
    void addAlert(Alert::Type type, const std::string& message, double severity,
                  const std::optional<ValidationResult>& validation = std::nullopt);
    void pruneOldAlerts();

    // Data management
    void addValidationResult(const ValidationResult& result);
    void pruneOldValidations();

    // Quality assessment helpers
    bool hasMinimumStars(const FocusQuality& quality) const;
    double normalizeHFR(double hfr) const;
    double normalizeFWHM(double fwhm) const;
    double combineQualityMetrics(const FocusQuality& quality) const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;

    // Validation data
    std::deque<ValidationResult> validation_history_;
    ValidationResult last_validation_;

    // Monitoring state
    bool monitoring_active_ = false;
    std::chrono::steady_clock::time_point monitoring_start_time_;

    // Correction state
    int correction_attempts_ = 0;
    std::chrono::steady_clock::time_point last_correction_time_;

    // Alerts
    std::vector<Alert> active_alerts_;

    // Statistics cache
    mutable Statistics cached_statistics_;
    mutable std::chrono::steady_clock::time_point statistics_cache_time_;

    // Thread safety
    mutable std::mutex validation_mutex_;
    mutable std::mutex alert_mutex_;

    // Constants
    static constexpr size_t MAX_VALIDATION_HISTORY = 1000;
    static constexpr size_t MAX_ALERTS = 100;
    static constexpr double MIN_CONFIDENCE_THRESHOLD = 0.7;
    static constexpr std::chrono::minutes MAX_CORRECTION_INTERVAL{10};
};

/**
 * @brief Simple focus quality checker for quick assessments
 */
class FocusQualityChecker : public BaseFocuserTask {
public:
    struct Config {
        int exposure_time_ms = 1000;
        bool use_binning = true;
        int binning_factor = 2;
        bool save_analysis_image = false;
        std::string analysis_image_path = "focus_check.fits";
    };

    FocusQualityChecker(std::shared_ptr<device::Focuser> focuser,
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

    FocusQuality getLastQuality() const;
    double getLastScore() const;

private:
    std::shared_ptr<device::Camera> camera_;
    Config config_;
    FocusQuality last_quality_;
    double last_score_ = 0.0;
};

/**
 * @brief Focus history tracker for long-term analysis
 */
class FocusHistoryTracker {
public:
    struct FocusEvent {
        std::chrono::steady_clock::time_point timestamp;
        int position;
        FocusQuality quality;
        std::string event_type; // "autofocus", "manual", "temperature", "validation"
        std::string notes;
    };

    void recordFocusEvent(const FocusEvent& event);
    void recordFocusEvent(int position, const FocusQuality& quality,
                         const std::string& event_type, const std::string& notes = "");

    std::vector<FocusEvent> getHistory() const;
    std::vector<FocusEvent> getHistory(std::chrono::steady_clock::time_point since) const;

    // Analysis functions
    std::optional<int> getBestFocusPosition() const;
    double getAverageFocusQuality() const;
    std::pair<int, int> getFocusRange() const; // min, max positions used

    // Export/import
    void exportToCSV(const std::string& filename) const;
    void importFromCSV(const std::string& filename);

    void clear();
    size_t size() const;

private:
    std::vector<FocusEvent> history_;
    mutable std::mutex history_mutex_;

    static constexpr size_t MAX_HISTORY_SIZE = 10000;
};

} // namespace lithium::task::custom::focuser
