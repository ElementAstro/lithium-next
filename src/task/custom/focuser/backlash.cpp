#include "backlash.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace lithium::task::custom::focuser {

BacklashCompensationTask::BacklashCompensationTask(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config)
    , last_position_(0)
    , last_direction_inward_(true)
    , calibration_in_progress_(false) {
    
    setTaskName("BacklashCompensation");
    setTaskDescription("Measures and compensates for focuser backlash");
}

bool BacklashCompensationTask::validateParameters() const {
    if (!BaseFocuserTask::validateParameters()) {
        return false;
    }
    
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }
    
    if (config_.measurement_range <= 0 || config_.measurement_steps <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid measurement parameters");
        return false;
    }
    
    if (config_.max_backlash_steps <= 0 || config_.max_backlash_steps > 1000) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid maximum backlash limit");
        return false;
    }
    
    return true;
}

void BacklashCompensationTask::resetTask() {
    BaseFocuserTask::resetTask();
    
    std::lock_guard<std::mutex> meas_lock(measurement_mutex_);
    std::lock_guard<std::mutex> comp_lock(compensation_mutex_);
    
    calibration_in_progress_ = false;
    calibration_data_.clear();
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

Task::TaskResult BacklashCompensationTask::executeImpl() {
    try {
        updateProgress(0.0, "Starting backlash measurement");
        
        if (config_.auto_measurement) {
            auto result = measureBacklash();
            if (result != TaskResult::Success) {
                return result;
            }
            updateProgress(70.0, "Backlash measurement complete");
        }
        
        if (config_.auto_compensation && hasValidBacklashData()) {
            updateProgress(90.0, "Backlash compensation configured");
        }
        
        updateProgress(100.0, "Backlash task completed");
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Backlash task failed: ") + e.what());
        return TaskResult::Error;
    }
}

void BacklashCompensationTask::updateProgress() {
    if (hasValidBacklashData()) {
        std::ostringstream status;
        status << "Backlash - In: " << getCurrentInwardBacklash() 
               << ", Out: " << getCurrentOutwardBacklash()
               << " (Confidence: " << std::fixed << std::setprecision(2) 
               << getBacklashConfidence() << ")";
        setProgressMessage(status.str());
    }
}

std::string BacklashCompensationTask::getTaskInfo() const {
    std::ostringstream info;
    info << BaseFocuserTask::getTaskInfo();
    
    if (hasValidBacklashData()) {
        info << ", Backlash In/Out: " << getCurrentInwardBacklash() 
             << "/" << getCurrentOutwardBacklash();
    } else {
        info << ", Backlash: Not measured";
    }
    
    return info.str();
}

Task::TaskResult BacklashCompensationTask::measureBacklash() {
    BacklashMeasurement measurement;
    
    updateProgress(0.0, "Preparing backlash measurement");
    
    // Choose measurement method based on configuration
    TaskResult result;
    if (config_.measurement_range > 50) {
        result = performDetailedMeasurement(measurement);
    } else {
        result = performBasicMeasurement(measurement);
    }
    
    if (result != TaskResult::Success) {
        return result;
    }
    
    // Validate and save measurement
    if (isBacklashMeasurementValid(measurement)) {
        saveMeasurement(measurement);
        updateProgress(100.0, "Backlash measurement complete");
        return TaskResult::Success;
    } else {
        setLastError(Task::ErrorType::SystemError, "Backlash measurement validation failed");
        return TaskResult::Error;
    }
}

Task::TaskResult BacklashCompensationTask::performBasicMeasurement(BacklashMeasurement& measurement) {
    try {
        measurement.timestamp = std::chrono::steady_clock::now();
        measurement.measurement_method = "Basic V-curve";
        measurement.data_points.clear();
        
        int current_pos = focuser_->getPosition();
        int start_pos = current_pos - config_.measurement_range / 2;
        int end_pos = current_pos + config_.measurement_range / 2;
        
        updateProgress(10.0, "Moving to measurement start position");
        
        // Move to start position
        auto result = moveToPositionAbsolute(start_pos);
        if (result != TaskResult::Success) return result;
        
        result = waitForSettling();
        if (result != TaskResult::Success) return result;
        
        // Measure inward direction (toward telescope)
        updateProgress(20.0, "Measuring inward backlash");
        std::vector<std::pair<int, double>> inward_data;
        
        for (int pos = start_pos; pos <= end_pos; pos += config_.measurement_steps) {
            result = moveToPositionAbsolute(pos);
            if (result != TaskResult::Success) return result;
            
            result = waitForSettling();
            if (result != TaskResult::Success) return result;
            
            result = captureAndAnalyze();
            if (result != TaskResult::Success) return result;
            
            auto quality = getLastFocusQuality();
            double metric = quality.hfr; // Use HFR as quality metric
            
            inward_data.emplace_back(pos, metric);
            measurement.data_points.emplace_back(pos, metric);
            
            double progress = 20.0 + (pos - start_pos) * 30.0 / (end_pos - start_pos);
            updateProgress(progress, "Measuring inward direction");
        }
        
        // Move to end position and measure outward direction
        updateProgress(50.0, "Measuring outward backlash");
        
        result = moveToPositionAbsolute(end_pos);
        if (result != TaskResult::Success) return result;
        
        result = waitForSettling();
        if (result != TaskResult::Success) return result;
        
        std::vector<std::pair<int, double>> outward_data;
        
        for (int pos = end_pos; pos >= start_pos; pos -= config_.measurement_steps) {
            result = moveToPositionAbsolute(pos);
            if (result != TaskResult::Success) return result;
            
            result = waitForSettling();
            if (result != TaskResult::Success) return result;
            
            result = captureAndAnalyze();
            if (result != TaskResult::Success) return result;
            
            auto quality = getLastFocusQuality();
            double metric = quality.hfr;
            
            outward_data.emplace_back(pos, metric);
            measurement.data_points.emplace_back(pos, metric);
            
            double progress = 50.0 + (end_pos - pos) * 30.0 / (end_pos - start_pos);
            updateProgress(progress, "Measuring outward direction");
        }
        
        updateProgress(80.0, "Analyzing backlash data");
        
        // Analyze backlash from the data
        measurement.inward_backlash = analyzeBacklashFromData(inward_data, true);
        measurement.outward_backlash = analyzeBacklashFromData(outward_data, false);
        measurement.confidence = calculateMeasurementConfidence(measurement);
        
        updateProgress(90.0, "Backlash analysis complete");
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Backlash measurement failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult BacklashCompensationTask::performDetailedMeasurement(BacklashMeasurement& measurement) {
    // Detailed measurement using hysteresis analysis
    return performHysteresisMeasurement(measurement);
}

Task::TaskResult BacklashCompensationTask::performHysteresisMeasurement(BacklashMeasurement& measurement) {
    try {
        measurement.timestamp = std::chrono::steady_clock::now();
        measurement.measurement_method = "Hysteresis Analysis";
        measurement.data_points.clear();
        
        int current_pos = focuser_->getPosition();
        int center_pos = current_pos;
        int range = config_.measurement_range / 2;
        
        // Move well outside the measurement range to ensure consistent starting point
        updateProgress(5.0, "Moving to starting position");
        auto result = moveToPositionAbsolute(center_pos - range - config_.overshoot_steps);
        if (result != TaskResult::Success) return result;
        
        result = waitForSettling();
        if (result != TaskResult::Success) return result;
        
        // First pass: move inward through the range
        updateProgress(10.0, "First pass - inward movement");
        std::vector<std::pair<int, double>> first_pass;
        
        for (int pos = center_pos - range; pos <= center_pos + range; pos += config_.measurement_steps) {
            result = moveToPositionAbsolute(pos);
            if (result != TaskResult::Success) return result;
            
            result = waitForSettling();
            if (result != TaskResult::Success) return result;
            
            result = captureAndAnalyze();
            if (result != TaskResult::Success) return result;
            
            auto quality = getLastFocusQuality();
            first_pass.emplace_back(pos, quality.hfr);
            measurement.data_points.emplace_back(pos, quality.hfr);
            
            double progress = 10.0 + (pos - (center_pos - range)) * 35.0 / (2 * range);
            updateProgress(progress, "First pass measurement");
        }
        
        // Move well past the end to reset direction
        result = moveToPositionAbsolute(center_pos + range + config_.overshoot_steps);
        if (result != TaskResult::Success) return result;
        
        result = waitForSettling();
        if (result != TaskResult::Success) return result;
        
        // Second pass: move outward through the range
        updateProgress(45.0, "Second pass - outward movement");
        std::vector<std::pair<int, double>> second_pass;
        
        for (int pos = center_pos + range; pos >= center_pos - range; pos -= config_.measurement_steps) {
            result = moveToPositionAbsolute(pos);
            if (result != TaskResult::Success) return result;
            
            result = waitForSettling();
            if (result != TaskResult::Success) return result;
            
            result = captureAndAnalyze();
            if (result != TaskResult::Success) return result;
            
            auto quality = getLastFocusQuality();
            second_pass.emplace_back(pos, quality.hfr);
            measurement.data_points.emplace_back(pos, quality.hfr);
            
            double progress = 45.0 + ((center_pos + range) - pos) * 35.0 / (2 * range);
            updateProgress(progress, "Second pass measurement");
        }
        
        updateProgress(80.0, "Analyzing hysteresis data");
        
        // Find the minimum points in each pass
        auto min_first = std::min_element(first_pass.begin(), first_pass.end(),
                                         [](const auto& a, const auto& b) {
                                             return a.second < b.second;
                                         });
        
        auto min_second = std::min_element(second_pass.begin(), second_pass.end(),
                                          [](const auto& a, const auto& b) {
                                              return a.second < b.second;
                                          });
        
        if (min_first != first_pass.end() && min_second != second_pass.end()) {
            // Backlash is the difference between the minimum positions
            int position_difference = std::abs(min_first->first - min_second->first);
            
            // Assign backlash based on which direction gave the better minimum
            if (min_first->second < min_second->second) {
                measurement.inward_backlash = position_difference;
                measurement.outward_backlash = 0;
            } else {
                measurement.inward_backlash = 0;
                measurement.outward_backlash = position_difference;
            }
        } else {
            measurement.inward_backlash = 0;
            measurement.outward_backlash = 0;
        }
        
        measurement.confidence = calculateMeasurementConfidence(measurement);
        
        updateProgress(90.0, "Hysteresis analysis complete");
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Hysteresis measurement failed: ") + e.what());
        return TaskResult::Error;
    }
}

int BacklashCompensationTask::analyzeBacklashFromData(
    const std::vector<std::pair<int, double>>& data, bool inward_direction) {
    
    if (data.size() < MIN_MEASUREMENT_POINTS) {
        return 0;
    }
    
    // Find the minimum HFR point (best focus)
    auto min_point = std::min_element(data.begin(), data.end(),
                                     [](const auto& a, const auto& b) {
                                         return a.second < b.second;
                                     });
    
    if (min_point == data.end()) {
        return 0;
    }
    
    // For now, use a simple heuristic
    // This could be enhanced with curve fitting
    return config_.measurement_steps; // Placeholder implementation
}

double BacklashCompensationTask::calculateMeasurementConfidence(const BacklashMeasurement& measurement) {
    // Calculate confidence based on data quality and consistency
    if (measurement.data_points.size() < MIN_MEASUREMENT_POINTS) {
        return 0.0;
    }
    
    // Check if backlash values are reasonable
    if (measurement.inward_backlash > config_.max_backlash_steps ||
        measurement.outward_backlash > config_.max_backlash_steps) {
        return 0.2; // Low confidence for unreasonable values
    }
    
    // Calculate confidence based on curve quality
    double min_hfr = std::numeric_limits<double>::max();
    double max_hfr = 0.0;
    
    for (const auto& point : measurement.data_points) {
        min_hfr = std::min(min_hfr, point.second);
        max_hfr = std::max(max_hfr, point.second);
    }
    
    double dynamic_range = max_hfr - min_hfr;
    if (dynamic_range < 0.5) {
        return 0.3; // Low confidence for poor dynamic range
    }
    
    // Higher confidence for better dynamic range
    return std::min(1.0, 0.5 + dynamic_range / 10.0);
}

bool BacklashCompensationTask::isBacklashMeasurementValid(const BacklashMeasurement& measurement) {
    return measurement.confidence >= MIN_CONFIDENCE &&
           (measurement.inward_backlash <= config_.max_backlash_steps) &&
           (measurement.outward_backlash <= config_.max_backlash_steps) &&
           !measurement.data_points.empty();
}

Task::TaskResult BacklashCompensationTask::moveWithBacklashCompensation(int target_position) {
    if (!config_.auto_compensation || !hasValidBacklashData()) {
        return moveToPositionAbsolute(target_position);
    }
    
    try {
        int current_position = focuser_->getPosition();
        bool needs_compensation;
        int compensated_position = calculateCompensatedPosition(target_position, needs_compensation);
        
        if (needs_compensation) {
            // Apply compensation
            CompensationEvent event;
            event.timestamp = std::chrono::steady_clock::now();
            event.original_target = target_position;
            event.compensated_target = compensated_position;
            event.compensation_applied = compensated_position - target_position;
            event.direction_change = needsDirectionChange(current_position, target_position);
            event.reason = "Automatic backlash compensation";
            
            saveCompensationEvent(event);
            
            // Move to compensated position first
            auto result = moveToPositionAbsolute(compensated_position);
            if (result != TaskResult::Success) return result;
            
            result = waitForSettling();
            if (result != TaskResult::Success) return result;
            
            // Then move to final target position
            result = moveToPositionAbsolute(target_position);
            if (result != TaskResult::Success) return result;
            
            return waitForSettling();
        } else {
            return moveToPositionAbsolute(target_position);
        }
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Backlash compensation failed: ") + e.what());
        return TaskResult::Error;
    }
}

int BacklashCompensationTask::calculateCompensatedPosition(int target_position, bool& needs_compensation) {
    if (!hasValidBacklashData()) {
        needs_compensation = false;
        return target_position;
    }
    
    int current_position = focuser_->getPosition();
    bool direction_change = needsDirectionChange(current_position, target_position);
    
    if (!direction_change) {
        needs_compensation = false;
        return target_position;
    }
    
    needs_compensation = true;
    
    // Determine which backlash value to use
    bool moving_inward = target_position < current_position;
    int backlash_compensation = moving_inward ? getCurrentInwardBacklash() : getCurrentOutwardBacklash();
    
    // Add overshoot
    int overshoot = calculateOvershoot(backlash_compensation, target_position);
    
    return target_position + (moving_inward ? -overshoot : overshoot);
}

bool BacklashCompensationTask::needsDirectionChange(int current_position, int target_position) {
    bool moving_inward = target_position < current_position;
    return moving_inward != last_direction_inward_;
}

int BacklashCompensationTask::calculateOvershoot(int backlash_amount, int target_position) {
    return backlash_amount + config_.overshoot_steps;
}

Task::TaskResult BacklashCompensationTask::waitForSettling() {
    if (config_.settling_time.count() > 0) {
        std::this_thread::sleep_for(config_.settling_time);
    }
    return TaskResult::Success;
}

void BacklashCompensationTask::saveMeasurement(const BacklashMeasurement& measurement) {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    
    measurement_history_.push_back(measurement);
    current_measurement_ = measurement;
    
    // Maintain maximum history size
    if (measurement_history_.size() > MAX_MEASUREMENT_HISTORY) {
        measurement_history_.pop_front();
    }
    
    // Invalidate statistics cache
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

void BacklashCompensationTask::saveCompensationEvent(const CompensationEvent& event) {
    std::lock_guard<std::mutex> lock(compensation_mutex_);
    
    compensation_history_.push_back(event);
    
    // Maintain maximum history size
    if (compensation_history_.size() > MAX_COMPENSATION_HISTORY) {
        compensation_history_.pop_front();
    }
    
    // Update direction tracking
    last_direction_inward_ = event.compensated_target < focuser_->getPosition();
    last_move_time_ = event.timestamp;
    
    // Invalidate statistics cache
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

int BacklashCompensationTask::getCurrentInwardBacklash() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_measurement_ ? current_measurement_->inward_backlash : 0;
}

int BacklashCompensationTask::getCurrentOutwardBacklash() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_measurement_ ? current_measurement_->outward_backlash : 0;
}

double BacklashCompensationTask::getBacklashConfidence() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_measurement_ ? current_measurement_->confidence : 0.0;
}

bool BacklashCompensationTask::hasValidBacklashData() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_measurement_.has_value() && 
           current_measurement_->confidence >= config_.confidence_threshold;
}

std::optional<BacklashCompensationTask::BacklashMeasurement> 
BacklashCompensationTask::getLastMeasurement() const {
    std::lock_guard<std::mutex> lock(measurement_mutex_);
    return current_measurement_;
}

BacklashCompensationTask::Statistics BacklashCompensationTask::getStatistics() const {
    auto now = std::chrono::steady_clock::now();
    
    // Use cached statistics if recent
    if (now - statistics_cache_time_ < std::chrono::seconds(5)) {
        return cached_statistics_;
    }
    
    std::lock_guard<std::mutex> meas_lock(measurement_mutex_);
    std::lock_guard<std::mutex> comp_lock(compensation_mutex_);
    
    Statistics stats;
    
    stats.total_measurements = measurement_history_.size();
    stats.total_compensations = compensation_history_.size();
    
    if (!measurement_history_.empty()) {
        double sum_inward = 0.0, sum_outward = 0.0;
        for (const auto& measurement : measurement_history_) {
            sum_inward += measurement.inward_backlash;
            sum_outward += measurement.outward_backlash;
        }
        
        stats.average_inward_backlash = sum_inward / measurement_history_.size();
        stats.average_outward_backlash = sum_outward / measurement_history_.size();
        stats.last_measurement = measurement_history_.back().timestamp;
        
        // Calculate stability (inverse of standard deviation)
        stats.backlash_stability = 1.0 - calculateBacklashVariability();
    }
    
    if (!compensation_history_.empty()) {
        stats.last_compensation = compensation_history_.back().timestamp;
    }
    
    // Cache the results
    cached_statistics_ = stats;
    statistics_cache_time_ = now;
    
    return stats;
}

double BacklashCompensationTask::calculateBacklashVariability() const {
    if (measurement_history_.size() < 2) {
        return 0.0;
    }
    
    // Calculate standard deviation of backlash measurements
    double mean_inward = 0.0, mean_outward = 0.0;
    for (const auto& measurement : measurement_history_) {
        mean_inward += measurement.inward_backlash;
        mean_outward += measurement.outward_backlash;
    }
    mean_inward /= measurement_history_.size();
    mean_outward /= measurement_history_.size();
    
    double variance = 0.0;
    for (const auto& measurement : measurement_history_) {
        variance += std::pow(measurement.inward_backlash - mean_inward, 2);
        variance += std::pow(measurement.outward_backlash - mean_outward, 2);
    }
    variance /= (measurement_history_.size() * 2);
    
    return std::sqrt(variance) / std::max(mean_inward, mean_outward);
}

// BacklashDetector implementation

BacklashDetector::BacklashDetector(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config) {
    
    setTaskName("BacklashDetector");
    setTaskDescription("Quick backlash detection");
    
    last_result_.backlash_detected = false;
    last_result_.estimated_backlash = 0;
    last_result_.confidence = 0.0;
}

bool BacklashDetector::validateParameters() const {
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }
    
    if (config_.test_range <= 0 || config_.test_steps <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid test parameters");
        return false;
    }
    
    return true;
}

void BacklashDetector::resetTask() {
    BaseFocuserTask::resetTask();
    last_result_.backlash_detected = false;
    last_result_.estimated_backlash = 0;
    last_result_.confidence = 0.0;
    last_result_.notes.clear();
}

Task::TaskResult BacklashDetector::executeImpl() {
    try {
        updateProgress(0.0, "Starting backlash detection");
        
        int current_pos = focuser_->getPosition();
        
        // Move outward and back inward to test for backlash
        updateProgress(20.0, "Moving outward");
        auto result = moveToPositionAbsolute(current_pos + config_.test_range);
        if (result != TaskResult::Success) return result;
        
        std::this_thread::sleep_for(config_.settling_time);
        
        updateProgress(40.0, "Capturing reference image");
        result = captureAndAnalyze();
        if (result != TaskResult::Success) return result;
        
        auto reference_quality = getLastFocusQuality();
        
        updateProgress(60.0, "Moving back to original position");
        result = moveToPositionAbsolute(current_pos);
        if (result != TaskResult::Success) return result;
        
        std::this_thread::sleep_for(config_.settling_time);
        
        updateProgress(80.0, "Capturing test image");
        result = captureAndAnalyze();
        if (result != TaskResult::Success) return result;
        
        auto test_quality = getLastFocusQuality();
        
        // Compare the qualities
        double quality_difference = std::abs(test_quality.hfr - reference_quality.hfr);
        
        if (quality_difference > 0.2) { // Threshold for backlash detection
            last_result_.backlash_detected = true;
            last_result_.estimated_backlash = static_cast<int>(quality_difference * 10); // Rough estimate
            last_result_.confidence = std::min(1.0, quality_difference / 1.0);
            last_result_.notes = "Significant HFR difference detected";
        } else {
            last_result_.backlash_detected = false;
            last_result_.estimated_backlash = 0;
            last_result_.confidence = 0.8; // High confidence in no backlash
            last_result_.notes = "No significant backlash detected";
        }
        
        updateProgress(100.0, "Backlash detection complete");
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Backlash detection failed: ") + e.what());
        return TaskResult::Error;
    }
}

void BacklashDetector::updateProgress() {
    // Progress updated in executeImpl
}

std::string BacklashDetector::getTaskInfo() const {
    std::ostringstream info;
    info << "BacklashDetector - " << (last_result_.backlash_detected ? "Detected" : "None")
         << ", Estimate: " << last_result_.estimated_backlash 
         << ", Confidence: " << std::fixed << std::setprecision(2) << last_result_.confidence;
    return info.str();
}

BacklashDetector::DetectionResult BacklashDetector::getLastResult() const {
    return last_result_;
}

// BacklashAdvisor implementation

BacklashAdvisor::Recommendation BacklashAdvisor::analyzeBacklashData(
    const std::vector<BacklashCompensationTask::BacklashMeasurement>& measurements) {
    
    Recommendation rec;
    rec.confidence = 0.0;
    rec.reasoning = "Insufficient data";
    
    if (measurements.empty()) {
        rec.suggested_inward_backlash = 0;
        rec.suggested_outward_backlash = 0;
        rec.suggested_overshoot = 10;
        return rec;
    }
    
    // Calculate averages and consistency
    std::vector<int> inward_values, outward_values;
    for (const auto& measurement : measurements) {
        if (measurement.confidence > 0.5) {
            inward_values.push_back(measurement.inward_backlash);
            outward_values.push_back(measurement.outward_backlash);
        }
    }
    
    if (inward_values.empty()) {
        rec.suggested_inward_backlash = 0;
        rec.suggested_outward_backlash = 0;
        rec.suggested_overshoot = 10;
        rec.warnings.push_back("No reliable measurements available");
        return rec;
    }
    
    double inward_confidence, outward_confidence;
    rec.suggested_inward_backlash = calculateOptimalBacklash(inward_values, inward_confidence);
    rec.suggested_outward_backlash = calculateOptimalBacklash(outward_values, outward_confidence);
    rec.suggested_overshoot = std::max(rec.suggested_inward_backlash, rec.suggested_outward_backlash) / 2 + 5;
    
    rec.confidence = (inward_confidence + outward_confidence) / 2.0;
    rec.reasoning = "Based on " + std::to_string(measurements.size()) + " measurements";
    
    // Add warnings for unusual values
    if (rec.suggested_inward_backlash > 100 || rec.suggested_outward_backlash > 100) {
        rec.warnings.push_back("Unusually high backlash values detected");
    }
    
    return rec;
}

int BacklashAdvisor::calculateOptimalBacklash(const std::vector<int>& values, double& confidence) {
    if (values.empty()) {
        confidence = 0.0;
        return 0;
    }
    
    // Calculate median for robustness
    std::vector<int> sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());
    
    int median = sorted_values[sorted_values.size() / 2];
    
    // Calculate consistency (inverse of variance)
    double variance = 0.0;
    for (int value : values) {
        variance += std::pow(value - median, 2);
    }
    variance /= values.size();
    
    confidence = std::max(0.0, 1.0 - variance / 100.0); // Normalize variance
    
    return median;
}

} // namespace lithium::task::custom::focuser
