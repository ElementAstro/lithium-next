#include "validation.hpp"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace lithium::task::custom::focuser {

FocusValidationTask::FocusValidationTask(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config)
    , monitoring_active_(false)
    , correction_attempts_(0) {

    setTaskName("FocusValidation");
    setTaskDescription("Validates and monitors focus quality continuously");
}

bool FocusValidationTask::validateParameters() const {
    if (!BaseFocuserTask::validateParameters()) {
        return false;
    }

    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }

    if (config_.hfr_threshold <= 0.0 || config_.fwhm_threshold <= 0.0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid quality thresholds");
        return false;
    }

    if (config_.min_star_count < 1) {
        setLastError(Task::ErrorType::InvalidParameter, "Minimum star count must be at least 1");
        return false;
    }

    return true;
}

void FocusValidationTask::resetTask() {
    BaseFocuserTask::resetTask();

    std::lock_guard<std::mutex> val_lock(validation_mutex_);
    std::lock_guard<std::mutex> alert_lock(alert_mutex_);

    monitoring_active_ = false;
    correction_attempts_ = 0;
    active_alerts_.clear();
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

Task::TaskResult FocusValidationTask::executeImpl() {
    try {
        updateProgress(0.0, "Starting focus validation");

        // Perform initial validation
        auto result = validateCurrentFocus();
        if (result != TaskResult::Success) {
            return result;
        }

        updateProgress(50.0, "Initial validation complete");

        // Start continuous monitoring if configured
        if (config_.validation_interval.count() > 0) {
            startContinuousMonitoring();
            updateProgress(75.0, "Continuous monitoring started");

            // Run monitoring loop
            result = monitoringLoop();
            if (result != TaskResult::Success) {
                return result;
            }
        }

        updateProgress(100.0, "Focus validation completed");
        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError,
                    std::string("Focus validation failed: ") + e.what());
        return TaskResult::Error;
    }
}

void FocusValidationTask::updateProgress() {
    if (monitoring_active_) {
        auto current_score = getCurrentFocusScore();
        std::ostringstream status;
        status << "Monitoring - Focus Score: " << std::fixed << std::setprecision(3)
               << current_score;

        if (!active_alerts_.empty()) {
            status << " (" << active_alerts_.size() << " alerts)";
        }

        setProgressMessage(status.str());
    }
}

std::string FocusValidationTask::getTaskInfo() const {
    std::ostringstream info;
    info << BaseFocuserTask::getTaskInfo()
         << ", Monitoring: " << (monitoring_active_ ? "Active" : "Inactive");

    std::lock_guard<std::mutex> lock(validation_mutex_);
    if (!validation_history_.empty()) {
        info << ", Last Score: " << std::fixed << std::setprecision(3)
             << validation_history_.back().quality_score;
    }

    return info.str();
}

Task::TaskResult FocusValidationTask::validateCurrentFocus() {
    ValidationResult result;
    auto task_result = performValidation(result);

    if (task_result == TaskResult::Success) {
        addValidationResult(result);
        processValidationResult(result);
    }

    return task_result;
}

Task::TaskResult FocusValidationTask::performValidation(ValidationResult& result) {
    try {
        updateProgress(0.0, "Capturing validation image");

        // Take an image for analysis
        auto capture_result = captureAndAnalyze();
        if (capture_result != TaskResult::Success) {
            return capture_result;
        }

        updateProgress(50.0, "Analyzing focus quality");

        auto quality = getLastFocusQuality();

        result.timestamp = std::chrono::steady_clock::now();
        result.quality = quality;
        result.quality_score = calculateFocusScore(quality);
        result.is_valid = isFocusAcceptable(quality);
        result.recommended_correction = calculateRecommendedCorrection(quality);

        if (!result.is_valid) {
            if (!hasMinimumStars(quality)) {
                result.reason = "Insufficient stars detected";
            } else if (quality.hfr > config_.hfr_threshold) {
                result.reason = "HFR too high: " + std::to_string(quality.hfr);
            } else if (quality.fwhm > config_.fwhm_threshold) {
                result.reason = "FWHM too high: " + std::to_string(quality.fwhm);
            } else {
                result.reason = "Overall focus quality poor";
            }
        } else {
            result.reason = "Focus quality acceptable";
        }

        updateProgress(100.0, "Validation complete");
        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError,
                    std::string("Focus validation failed: ") + e.what());
        return TaskResult::Error;
    }
}

double FocusValidationTask::calculateFocusScore(const FocusQuality& quality) const {
    if (quality.star_count < config_.min_star_count) {
        return 0.0; // No score without sufficient stars
    }

    // Normalize individual metrics (higher score = better focus)
    double hfr_score = normalizeHFR(quality.hfr);
    double fwhm_score = normalizeFWHM(quality.fwhm);
    double star_score = std::min(1.0, static_cast<double>(quality.star_count) / (config_.min_star_count * 2));

    // Weight the metrics
    double combined_score = (hfr_score * 0.4 + fwhm_score * 0.4 + star_score * 0.2);

    // Apply additional factors
    if (quality.peak_value > 0) {
        double saturation_penalty = std::max(0.0, (quality.peak_value - 50000.0) / 15535.0);
        combined_score *= (1.0 - saturation_penalty * 0.2);
    }

    return std::max(0.0, std::min(1.0, combined_score));
}

bool FocusValidationTask::isFocusAcceptable(const FocusQuality& quality) const {
    if (!hasMinimumStars(quality)) {
        return false;
    }

    if (quality.hfr > config_.hfr_threshold || quality.fwhm > config_.fwhm_threshold) {
        return false;
    }

    double score = calculateFocusScore(quality);
    return score >= (1.0 - config_.focus_tolerance);
}

std::optional<int> FocusValidationTask::calculateRecommendedCorrection(
    const FocusQuality& quality) const {

    if (isFocusAcceptable(quality)) {
        return std::nullopt; // No correction needed
    }

    // Simple heuristic based on HFR
    if (quality.hfr > config_.hfr_threshold) {
        double correction_factor = (quality.hfr - config_.hfr_threshold) / config_.hfr_threshold;
        int suggested_steps = static_cast<int>(correction_factor * 20.0); // Base correction
        return std::min(suggested_steps, 100); // Limit maximum correction
    }

    return 10; // Default small correction
}

Task::TaskResult FocusValidationTask::monitoringLoop() {
    while (!shouldStop() && monitoring_active_) {
        try {
            auto result = validateCurrentFocus();
            if (result != TaskResult::Success) {
                // Log error but continue monitoring
                std::this_thread::sleep_for(config_.validation_interval);
                continue;
            }

            // Check if correction is needed
            if (config_.auto_correction && !last_validation_.is_valid) {
                auto correction_result = correctFocus();
                if (correction_result != TaskResult::Success) {
                    addAlert(Alert::CorrectionFailed,
                           "Failed to automatically correct focus", 0.8);
                }
            }

            std::this_thread::sleep_for(config_.validation_interval);

        } catch (const std::exception& e) {
            // Log error and continue
            std::this_thread::sleep_for(config_.validation_interval);
        }
    }

    return TaskResult::Success;
}

void FocusValidationTask::processValidationResult(const ValidationResult& result) {
    last_validation_ = result;
    checkForAlerts(result);

    // Update statistics cache
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

void FocusValidationTask::checkForAlerts(const ValidationResult& result) {
    // Check for focus lost
    if (!result.is_valid && result.quality_score < 0.3) {
        addAlert(Alert::FocusLost, "Focus quality severely degraded", 0.9, result);
    }

    // Check for quality degradation
    if (!validation_history_.empty()) {
        const auto& prev = validation_history_.back();
        double degradation = prev.quality_score - result.quality_score;

        if (degradation > config_.quality_degradation_threshold) {
            addAlert(Alert::QualityDegraded,
                   "Focus quality degraded by " + std::to_string(degradation),
                   0.7, result);
        }
    }

    // Check for insufficient stars
    if (result.quality.star_count < config_.min_star_count) {
        addAlert(Alert::InsufficientStars,
               "Only " + std::to_string(result.quality.star_count) + " stars detected",
               0.5, result);
    }

    // Check for drift if enabled
    if (config_.enable_drift_detection) {
        auto drift_info = analyzeFocusDrift();
        if (drift_info.significant_drift) {
            addAlert(Alert::DriftDetected,
                   "Significant focus drift detected: " + drift_info.trend_description,
                   0.6);
        }
    }
}

void FocusValidationTask::addAlert(Alert::Type type, const std::string& message,
                                  double severity, const std::optional<ValidationResult>& validation) {
    std::lock_guard<std::mutex> lock(alert_mutex_);

    Alert alert;
    alert.type = type;
    alert.timestamp = std::chrono::steady_clock::now();
    alert.message = message;
    alert.severity = severity;
    alert.related_validation = validation;

    active_alerts_.push_back(alert);

    // Maintain maximum alert count
    if (active_alerts_.size() > MAX_ALERTS) {
        active_alerts_.erase(active_alerts_.begin());
    }
}

Task::TaskResult FocusValidationTask::correctFocus() {
    if (!last_validation_.recommended_correction.has_value()) {
        return TaskResult::Success; // No correction needed
    }

    auto now = std::chrono::steady_clock::now();
    if (now - last_correction_time_ < MAX_CORRECTION_INTERVAL) {
        return TaskResult::Success; // Too soon for another correction
    }

    if (correction_attempts_ >= config_.max_correction_attempts) {
        addAlert(Alert::CorrectionFailed,
               "Maximum correction attempts exceeded", 0.8);
        return TaskResult::Error;
    }

    try {
        int correction_steps = last_validation_.recommended_correction.value();

        updateProgress(0.0, "Applying focus correction");

        auto result = moveToPositionRelative(correction_steps);
        if (result != TaskResult::Success) {
            ++correction_attempts_;
            return result;
        }

        updateProgress(50.0, "Validating correction");

        // Validate the correction
        ValidationResult post_correction;
        result = performValidation(post_correction);
        if (result != TaskResult::Success) {
            ++correction_attempts_;
            return result;
        }

        if (post_correction.quality_score > last_validation_.quality_score) {
            // Correction was successful
            correction_attempts_ = 0;
            last_correction_time_ = now;
            addValidationResult(post_correction);
            updateProgress(100.0, "Focus correction successful");
            return TaskResult::Success;
        } else {
            // Correction didn't help, try opposite direction
            ++correction_attempts_;
            auto reverse_result = moveToPositionRelative(-correction_steps * 2);
            if (reverse_result == TaskResult::Success) {
                ValidationResult reverse_validation;
                if (performValidation(reverse_validation) == TaskResult::Success) {
                    addValidationResult(reverse_validation);
                    if (reverse_validation.quality_score > last_validation_.quality_score) {
                        correction_attempts_ = 0;
                        last_correction_time_ = now;
                        updateProgress(100.0, "Focus correction successful (reversed)");
                        return TaskResult::Success;
                    }
                }
            }

            return TaskResult::Error;
        }

    } catch (const std::exception& e) {
        ++correction_attempts_;
        setLastError(Task::ErrorType::DeviceError,
                    std::string("Focus correction failed: ") + e.what());
        return TaskResult::Error;
    }
}

FocusValidationTask::FocusDriftInfo FocusValidationTask::analyzeFocusDrift() const {
    FocusDriftInfo drift_info;
    drift_info.analysis_time = std::chrono::steady_clock::now();
    drift_info.drift_rate = 0.0;
    drift_info.confidence = 0.0;
    drift_info.significant_drift = false;
    drift_info.trend_description = "Insufficient data";

    std::lock_guard<std::mutex> lock(validation_mutex_);

    if (validation_history_.size() < 3) {
        return drift_info;
    }

    // Get recent validations within the drift window
    auto cutoff_time = drift_info.analysis_time - config_.drift_window;
    std::vector<ValidationResult> recent_validations;

    for (const auto& validation : validation_history_) {
        if (validation.timestamp >= cutoff_time) {
            recent_validations.push_back(validation);
        }
    }

    if (recent_validations.size() < 3) {
        return drift_info;
    }

    // Calculate drift rate
    drift_info.drift_rate = calculateDriftRate(recent_validations);

    // Calculate confidence based on data consistency
    double quality_variance = 0.0;
    double mean_quality = 0.0;
    for (const auto& val : recent_validations) {
        mean_quality += val.quality_score;
    }
    mean_quality /= recent_validations.size();

    for (const auto& val : recent_validations) {
        quality_variance += std::pow(val.quality_score - mean_quality, 2);
    }
    quality_variance /= recent_validations.size();

    drift_info.confidence = std::max(0.0, 1.0 - quality_variance * 5.0);
    drift_info.significant_drift = isSignificantDrift(drift_info.drift_rate, drift_info.confidence);

    // Create trend description
    if (std::abs(drift_info.drift_rate) < 0.01) {
        drift_info.trend_description = "Stable focus";
    } else if (drift_info.drift_rate > 0) {
        drift_info.trend_description = "Focus improving at " +
                                     std::to_string(drift_info.drift_rate) + "/hour";
    } else {
        drift_info.trend_description = "Focus degrading at " +
                                     std::to_string(-drift_info.drift_rate) + "/hour";
    }

    return drift_info;
}

double FocusValidationTask::calculateDriftRate(
    const std::vector<ValidationResult>& recent_results) const {

    if (recent_results.size() < 2) {
        return 0.0;
    }

    // Use linear regression to find trend
    std::vector<std::pair<double, double>> data; // hours_since_start, quality_score
    auto start_time = recent_results.front().timestamp;

    for (const auto& result : recent_results) {
        auto hours_since = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.timestamp - start_time).count() / 3600000.0;
        data.emplace_back(hours_since, result.quality_score);
    }

    // Simple linear regression
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    for (const auto& point : data) {
        sum_x += point.first;
        sum_y += point.second;
        sum_xy += point.first * point.second;
        sum_x2 += point.first * point.first;
    }

    double n = static_cast<double>(data.size());
    if (n * sum_x2 - sum_x * sum_x == 0) {
        return 0.0;
    }

    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    return slope;
}

bool FocusValidationTask::isSignificantDrift(double drift_rate, double confidence) const {
    return std::abs(drift_rate) > 0.05 && confidence > MIN_CONFIDENCE_THRESHOLD;
}

void FocusValidationTask::addValidationResult(const ValidationResult& result) {
    std::lock_guard<std::mutex> lock(validation_mutex_);

    validation_history_.push_back(result);

    // Maintain maximum history size
    if (validation_history_.size() > MAX_VALIDATION_HISTORY) {
        validation_history_.pop_front();
    }
}

double FocusValidationTask::normalizeHFR(double hfr) const {
    if (hfr <= 0.5) return 1.0;
    if (hfr >= config_.hfr_threshold * 2) return 0.0;
    return 1.0 - (hfr - 0.5) / (config_.hfr_threshold * 2 - 0.5);
}

double FocusValidationTask::normalizeFWHM(double fwhm) const {
    if (fwhm <= 1.0) return 1.0;
    if (fwhm >= config_.fwhm_threshold * 2) return 0.0;
    return 1.0 - (fwhm - 1.0) / (config_.fwhm_threshold * 2 - 1.0);
}

bool FocusValidationTask::hasMinimumStars(const FocusQuality& quality) const {
    return quality.star_count >= config_.min_star_count;
}

double FocusValidationTask::getCurrentFocusScore() const {
    std::lock_guard<std::mutex> lock(validation_mutex_);
    return validation_history_.empty() ? 0.0 : validation_history_.back().quality_score;
}

std::vector<FocusValidationTask::ValidationResult>
FocusValidationTask::getValidationHistory() const {
    std::lock_guard<std::mutex> lock(validation_mutex_);
    return std::vector<ValidationResult>(validation_history_.begin(), validation_history_.end());
}

std::vector<FocusValidationTask::Alert> FocusValidationTask::getActiveAlerts() const {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    return active_alerts_;
}

void FocusValidationTask::clearAlerts() {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    active_alerts_.clear();
}

// FocusQualityChecker implementation

FocusQualityChecker::FocusQualityChecker(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config)
    , last_score_(0.0) {

    setTaskName("FocusQualityChecker");
    setTaskDescription("Quick focus quality assessment");
}

bool FocusQualityChecker::validateParameters() const {
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }

    if (config_.exposure_time_ms <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid exposure time");
        return false;
    }

    return true;
}

void FocusQualityChecker::resetTask() {
    BaseFocuserTask::resetTask();
    last_score_ = 0.0;
}

Task::TaskResult FocusQualityChecker::executeImpl() {
    try {
        updateProgress(0.0, "Capturing test image");

        // Configure camera for quick capture
        if (config_.use_binning) {
            // Set binning if supported
        }

        // Capture and analyze
        auto result = captureAndAnalyze();
        if (result != TaskResult::Success) {
            return result;
        }

        last_quality_ = getLastFocusQuality();

        // Calculate simple score
        if (last_quality_.star_count > 0) {
            last_score_ = std::max(0.0, 1.0 - (last_quality_.hfr - 1.0) / 5.0);
        } else {
            last_score_ = 0.0;
        }

        updateProgress(100.0, "Focus quality check complete");
        return TaskResult::Success;

    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError,
                    std::string("Focus quality check failed: ") + e.what());
        return TaskResult::Error;
    }
}

void FocusQualityChecker::updateProgress() {
    // Progress updated in executeImpl
}

std::string FocusQualityChecker::getTaskInfo() const {
    std::ostringstream info;
    info << "FocusQualityChecker - Score: " << std::fixed << std::setprecision(3)
         << last_score_ << ", Stars: " << last_quality_.star_count;
    return info.str();
}

FocusQuality FocusQualityChecker::getLastQuality() const {
    return last_quality_;
}

double FocusQualityChecker::getLastScore() const {
    return last_score_;
}

// FocusHistoryTracker implementation

void FocusHistoryTracker::recordFocusEvent(const FocusEvent& event) {
    std::lock_guard<std::mutex> lock(history_mutex_);

    history_.push_back(event);

    // Maintain maximum history size
    if (history_.size() > MAX_HISTORY_SIZE) {
        history_.erase(history_.begin());
    }
}

void FocusHistoryTracker::recordFocusEvent(int position, const FocusQuality& quality,
                                          const std::string& event_type, const std::string& notes) {
    FocusEvent event;
    event.timestamp = std::chrono::steady_clock::now();
    event.position = position;
    event.quality = quality;
    event.event_type = event_type;
    event.notes = notes;

    recordFocusEvent(event);
}

std::vector<FocusHistoryTracker::FocusEvent> FocusHistoryTracker::getHistory() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return history_;
}

std::optional<int> FocusHistoryTracker::getBestFocusPosition() const {
    std::lock_guard<std::mutex> lock(history_mutex_);

    if (history_.empty()) {
        return std::nullopt;
    }

    auto best = std::min_element(history_.begin(), history_.end(),
                                [](const auto& a, const auto& b) {
                                    return a.quality.hfr < b.quality.hfr;
                                });

    return best->position;
}

void FocusHistoryTracker::clear() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

size_t FocusHistoryTracker::size() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return history_.size();
}

} // namespace lithium::task::custom::focuser
