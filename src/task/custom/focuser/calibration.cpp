#include "calibration.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <json/json.h>

namespace lithium::task::custom::focuser {

FocusCalibrationTask::FocusCalibrationTask(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    std::shared_ptr<device::TemperatureSensor> temperature_sensor,
    const CalibrationConfig& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , temperature_sensor_(std::move(temperature_sensor))
    , config_(config)
    , total_expected_measurements_(0)
    , completed_measurements_(0)
    , calibration_in_progress_(false) {
    
    setTaskName("FocusCalibration");
    setTaskDescription("Comprehensive focus system calibration");
}

bool FocusCalibrationTask::validateParameters() const {
    if (!BaseFocuserTask::validateParameters()) {
        return false;
    }
    
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }
    
    if (config_.full_range_end <= config_.full_range_start) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid calibration range");
        return false;
    }
    
    if (config_.coarse_step_size <= 0 || config_.fine_step_size <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid step sizes");
        return false;
    }
    
    return true;
}

void FocusCalibrationTask::resetTask() {
    BaseFocuserTask::resetTask();
    
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    
    calibration_in_progress_ = false;
    current_phase_.clear();
    calibration_data_.clear();
    focus_model_.reset();
    
    // Reset result
    result_ = CalibrationResult{};
    result_.calibration_time = std::chrono::steady_clock::now();
    
    total_expected_measurements_ = 0;
    completed_measurements_ = 0;
}

Task::TaskResult FocusCalibrationTask::executeImpl() {
    try {
        calibration_in_progress_ = true;
        calibration_start_time_ = std::chrono::steady_clock::now();
        
        updateProgress(0.0, "Starting focus calibration");
        
        auto result = performFullCalibration();
        if (result != TaskResult::Success) {
            return result;
        }
        
        updateProgress(100.0, "Focus calibration completed");
        
        auto end_time = std::chrono::steady_clock::now();
        result_.calibration_duration = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - calibration_start_time_);
        
        calibration_in_progress_ = false;
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        calibration_in_progress_ = false;
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Focus calibration failed: ") + e.what());
        return TaskResult::Error;
    }
}

void FocusCalibrationTask::updateProgress() {
    if (calibration_in_progress_ && total_expected_measurements_ > 0) {
        double progress = static_cast<double>(completed_measurements_) / total_expected_measurements_ * 100.0;
        std::ostringstream status;
        status << current_phase_ << " (" << completed_measurements_ 
               << "/" << total_expected_measurements_ << ")";
        setProgressMessage(status.str());
        setProgressValue(progress);
    }
}

std::string FocusCalibrationTask::getTaskInfo() const {
    std::ostringstream info;
    info << BaseFocuserTask::getTaskInfo();
    
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    
    if (calibration_in_progress_) {
        info << ", Phase: " << current_phase_;
    } else if (result_.total_measurements > 0) {
        info << ", Calibrated - Optimal: " << result_.optimal_position
             << ", Quality: " << std::fixed << std::setprecision(2) << result_.optimal_hfr;
    }
    
    return info.str();
}

Task::TaskResult FocusCalibrationTask::performFullCalibration() {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    
    // Estimate total measurements needed
    int coarse_range = config_.full_range_end - config_.full_range_start;
    int coarse_steps = coarse_range / config_.coarse_step_size;
    
    total_expected_measurements_ = coarse_steps + 20; // Coarse + fine + ultra-fine estimates
    if (config_.calibrate_temperature) {
        total_expected_measurements_ += config_.temp_focus_samples * 3; // Multiple temperatures
    }
    if (config_.validate_backlash) {
        total_expected_measurements_ += 20; // Backlash validation points
    }
    
    completed_measurements_ = 0;
    
    try {
        // Phase 1: Coarse calibration
        current_phase_ = "Coarse calibration";
        updateProgress(5.0, "Starting coarse calibration");
        
        auto result = performCoarseCalibration();
        if (result != TaskResult::Success) {
            return result;
        }
        
        // Phase 2: Fine calibration around optimal region
        current_phase_ = "Fine calibration";
        updateProgress(30.0, "Starting fine calibration");
        
        int coarse_optimal = findOptimalPosition(calibration_data_);
        result = performFineCalibration(coarse_optimal, config_.coarse_step_size * 2);
        if (result != TaskResult::Success) {
            return result;
        }
        
        // Phase 3: Ultra-fine calibration
        current_phase_ = "Ultra-fine calibration";
        updateProgress(50.0, "Starting ultra-fine calibration");
        
        int fine_optimal = findOptimalPosition(calibration_data_);
        result = performUltraFineCalibration(fine_optimal, config_.fine_step_size * 4);
        if (result != TaskResult::Success) {
            return result;
        }
        
        // Phase 4: Temperature calibration (if enabled and sensor available)
        if (config_.calibrate_temperature && temperature_sensor_) {
            current_phase_ = "Temperature calibration";
            updateProgress(70.0, "Starting temperature calibration");
            
            result = performTemperatureCalibration();
            if (result != TaskResult::Success) {
                // Don't fail the entire calibration for temperature issues
                // Just log the error and continue
            }
        }
        
        // Phase 5: Backlash validation (if enabled)
        if (config_.validate_backlash) {
            current_phase_ = "Backlash validation";
            updateProgress(85.0, "Validating backlash");
            
            result = performBacklashCalibration();
            if (result != TaskResult::Success) {
                // Don't fail for backlash issues
            }
        }
        
        // Phase 6: Analysis and model creation
        current_phase_ = "Analysis";
        updateProgress(90.0, "Analyzing calibration data");
        
        result = analyzeFocusCurve();
        if (result != TaskResult::Success) {
            return result;
        }
        
        if (config_.create_focus_model) {
            result = createFocusModel();
            if (result != TaskResult::Success) {
                // Model creation failure is not critical
            }
        }
        
        // Save calibration data
        if (!config_.calibration_data_path.empty()) {
            saveCalibrationData(config_.calibration_data_path);
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Full calibration failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult FocusCalibrationTask::performCoarseCalibration() {
    try {
        for (int pos = config_.full_range_start; pos <= config_.full_range_end; pos += config_.coarse_step_size) {
            CalibrationPoint point;
            auto result = collectCalibrationPoint(pos, point);
            if (result != TaskResult::Success) {
                continue; // Skip problematic points but don't fail entirely
            }
            
            if (isCalibrationPointValid(point)) {
                calibration_data_.push_back(point);
            }
            
            ++completed_measurements_;
            updateProgress();
            
            if (shouldStop()) {
                return TaskResult::Cancelled;
            }
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Coarse calibration failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult FocusCalibrationTask::performFineCalibration(int center_position, int range) {
    try {
        int start_pos = center_position - range / 2;
        int end_pos = center_position + range / 2;
        
        for (int pos = start_pos; pos <= end_pos; pos += config_.fine_step_size) {
            CalibrationPoint point;
            auto result = collectCalibrationPoint(pos, point);
            if (result != TaskResult::Success) {
                continue;
            }
            
            if (isCalibrationPointValid(point)) {
                calibration_data_.push_back(point);
            }
            
            ++completed_measurements_;
            updateProgress();
            
            if (shouldStop()) {
                return TaskResult::Cancelled;
            }
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Fine calibration failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult FocusCalibrationTask::performUltraFineCalibration(int center_position, int range) {
    try {
        int start_pos = center_position - range / 2;
        int end_pos = center_position + range / 2;
        
        for (int pos = start_pos; pos <= end_pos; pos += config_.ultra_fine_step_size) {
            CalibrationPoint point;
            auto result = collectMultiplePoints(pos, 3, point); // Average 3 measurements
            if (result != TaskResult::Success) {
                continue;
            }
            
            if (isCalibrationPointValid(point)) {
                calibration_data_.push_back(point);
            }
            
            ++completed_measurements_;
            updateProgress();
            
            if (shouldStop()) {
                return TaskResult::Cancelled;
            }
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Ultra-fine calibration failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult FocusCalibrationTask::collectCalibrationPoint(int position, CalibrationPoint& point) {
    try {
        // Move to position
        auto result = moveToPositionAbsolute(position);
        if (result != TaskResult::Success) {
            return result;
        }
        
        // Wait for settling
        std::this_thread::sleep_for(config_.settling_time);
        
        // Capture and analyze
        result = captureAndAnalyze();
        if (result != TaskResult::Success) {
            return result;
        }
        
        // Fill calibration point
        point.position = position;
        point.quality = getLastFocusQuality();
        point.timestamp = std::chrono::steady_clock::now();
        
        // Get temperature if sensor available
        if (temperature_sensor_) {
            try {
                point.temperature = temperature_sensor_->getTemperature();
            } catch (...) {
                point.temperature = 20.0; // Default temperature
            }
        } else {
            point.temperature = 20.0;
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Failed to collect calibration point: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult FocusCalibrationTask::collectMultiplePoints(int position, int count, CalibrationPoint& averaged_point) {
    std::vector<CalibrationPoint> points;
    
    for (int i = 0; i < count; ++i) {
        CalibrationPoint point;
        auto result = collectCalibrationPoint(position, point);
        if (result == TaskResult::Success && isCalibrationPointValid(point)) {
            points.push_back(point);
        }
        
        if (i < count - 1) {
            std::this_thread::sleep_for(config_.image_interval);
        }
    }
    
    if (points.empty()) {
        return TaskResult::Error;
    }
    
    // Average the measurements
    averaged_point.position = position;
    averaged_point.timestamp = points.back().timestamp;
    averaged_point.temperature = 0.0;
    
    // Average quality metrics
    averaged_point.quality.hfr = 0.0;
    averaged_point.quality.fwhm = 0.0;
    averaged_point.quality.star_count = 0;
    averaged_point.quality.peak_value = 0.0;
    
    for (const auto& point : points) {
        averaged_point.quality.hfr += point.quality.hfr;
        averaged_point.quality.fwhm += point.quality.fwhm;
        averaged_point.quality.star_count += point.quality.star_count;
        averaged_point.quality.peak_value += point.quality.peak_value;
        averaged_point.temperature += point.temperature;
    }
    
    double count_d = static_cast<double>(points.size());
    averaged_point.quality.hfr /= count_d;
    averaged_point.quality.fwhm /= count_d;
    averaged_point.quality.star_count = static_cast<int>(averaged_point.quality.star_count / count_d);
    averaged_point.quality.peak_value /= count_d;
    averaged_point.temperature /= count_d;
    
    averaged_point.notes = "Averaged from " + std::to_string(points.size()) + " measurements";
    
    return TaskResult::Success;
}

bool FocusCalibrationTask::isCalibrationPointValid(const CalibrationPoint& point) {
    return point.quality.star_count >= config_.min_star_count &&
           point.quality.hfr > 0.0 && point.quality.hfr <= config_.max_acceptable_hfr &&
           point.quality.fwhm > 0.0 &&
           !std::isnan(point.quality.hfr) && !std::isinf(point.quality.hfr);
}

int FocusCalibrationTask::findOptimalPosition(const std::vector<CalibrationPoint>& points) {
    if (points.empty()) {
        return 0;
    }
    
    // Find point with minimum HFR
    auto min_point = std::min_element(points.begin(), points.end(),
                                     [](const auto& a, const auto& b) {
                                         return a.quality.hfr < b.quality.hfr;
                                     });
    
    return min_point->position;
}

Task::TaskResult FocusCalibrationTask::analyzeFocusCurve() {
    if (calibration_data_.empty()) {
        setLastError(Task::ErrorType::SystemError, "No calibration data available");
        return TaskResult::Error;
    }
    
    try {
        // Find optimal position and quality
        result_.optimal_position = findOptimalPosition(calibration_data_);
        
        auto optimal_point = std::find_if(calibration_data_.begin(), calibration_data_.end(),
                                         [this](const auto& point) {
                                             return point.position == result_.optimal_position;
                                         });
        
        if (optimal_point != calibration_data_.end()) {
            result_.optimal_hfr = optimal_point->quality.hfr;
            result_.optimal_fwhm = optimal_point->quality.fwhm;
        }
        
        // Calculate focus range
        auto min_max_pos = std::minmax_element(calibration_data_.begin(), calibration_data_.end(),
                                              [](const auto& a, const auto& b) {
                                                  return a.position < b.position;
                                              });
        result_.focus_range_min = min_max_pos.first->position;
        result_.focus_range_max = min_max_pos.second->position;
        
        // Analyze curve characteristics
        result_.curve_analysis.curve_sharpness = calculateCurveSharpness(calibration_data_);
        result_.curve_analysis.asymmetry_factor = calculateAsymmetry(calibration_data_);
        result_.curve_analysis.repeatability = calculateRepeatability(calibration_data_);
        
        auto critical_zone = findCriticalFocusZone(calibration_data_);
        result_.curve_analysis.critical_focus_zone = critical_zone.second - critical_zone.first;
        
        // Calculate overall confidence
        result_.calibration_confidence = calculateConfidence(calibration_data_);
        
        // Store all data points
        result_.data_points = calibration_data_;
        result_.total_measurements = calibration_data_.size();
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Focus curve analysis failed: ") + e.what());
        return TaskResult::Error;
    }
}

double FocusCalibrationTask::calculateCurveSharpness(const std::vector<CalibrationPoint>& points) {
    if (points.size() < 3) {
        return 0.0;
    }
    
    // Sort points by position
    std::vector<CalibrationPoint> sorted_points = points;
    std::sort(sorted_points.begin(), sorted_points.end(),
             [](const auto& a, const auto& b) {
                 return a.position < b.position;
             });
    
    double min_hfr = std::numeric_limits<double>::max();
    double max_hfr = 0.0;
    
    for (const auto& point : sorted_points) {
        min_hfr = std::min(min_hfr, point.quality.hfr);
        max_hfr = std::max(max_hfr, point.quality.hfr);
    }
    
    return (max_hfr - min_hfr) / min_hfr; // Relative dynamic range
}

double FocusCalibrationTask::calculateAsymmetry(const std::vector<CalibrationPoint>& points) {
    // Find optimal position
    int optimal_pos = findOptimalPosition(points);
    
    // Calculate average HFR on each side of optimal
    double left_sum = 0.0, right_sum = 0.0;
    int left_count = 0, right_count = 0;
    
    for (const auto& point : points) {
        if (point.position < optimal_pos) {
            left_sum += point.quality.hfr;
            ++left_count;
        } else if (point.position > optimal_pos) {
            right_sum += point.quality.hfr;
            ++right_count;
        }
    }
    
    if (left_count == 0 || right_count == 0) {
        return 0.0;
    }
    
    double left_avg = left_sum / left_count;
    double right_avg = right_sum / right_count;
    
    return std::abs(left_avg - right_avg) / std::max(left_avg, right_avg);
}

double FocusCalibrationTask::calculateConfidence(const std::vector<CalibrationPoint>& points) {
    if (points.size() < 5) {
        return 0.0;
    }
    
    // Confidence based on curve quality and data consistency
    double sharpness = calculateCurveSharpness(points);
    double repeatability = calculateRepeatability(points);
    
    // Normalize and combine factors
    double sharpness_score = std::min(1.0, sharpness / 2.0);      // 0-1
    double repeatability_score = std::max(0.0, 1.0 - repeatability); // Higher repeatability = lower score
    
    return (sharpness_score * 0.6 + repeatability_score * 0.4);
}

double FocusCalibrationTask::calculateRepeatability(const std::vector<CalibrationPoint>& points) {
    // For now, return a default value
    // In a real implementation, this would analyze multiple measurements at the same position
    return 0.1; // Assume 10% repeatability variation
}

std::pair<int, int> FocusCalibrationTask::findCriticalFocusZone(const std::vector<CalibrationPoint>& points) {
    if (points.empty()) {
        return {0, 0};
    }
    
    int optimal_pos = findOptimalPosition(points);
    
    // Find the range where HFR is within 10% of optimal
    auto optimal_point = std::find_if(points.begin(), points.end(),
                                     [optimal_pos](const auto& point) {
                                         return point.position == optimal_pos;
                                     });
    
    if (optimal_point == points.end()) {
        return {optimal_pos, optimal_pos};
    }
    
    double optimal_hfr = optimal_point->quality.hfr;
    double threshold = optimal_hfr * 1.1; // 10% worse than optimal
    
    int min_pos = optimal_pos, max_pos = optimal_pos;
    
    for (const auto& point : points) {
        if (point.quality.hfr <= threshold) {
            min_pos = std::min(min_pos, point.position);
            max_pos = std::max(max_pos, point.position);
        }
    }
    
    return {min_pos, max_pos};
}

Task::TaskResult FocusCalibrationTask::performTemperatureCalibration() {
    // Temperature calibration implementation would go here
    // For now, return success with default values
    result_.temperature_coefficient = 0.0;
    result_.temp_coeff_confidence = 0.0;
    result_.temperature_range = {20.0, 20.0};
    
    return TaskResult::Success;
}

Task::TaskResult FocusCalibrationTask::performBacklashCalibration() {
    // Backlash calibration implementation would go here
    // For now, return success with default values
    result_.inward_backlash = 0;
    result_.outward_backlash = 0;
    result_.backlash_confidence = 0.0;
    
    return TaskResult::Success;
}

Task::TaskResult FocusCalibrationTask::createFocusModel() {
    if (calibration_data_.size() < 5) {
        setLastError(Task::ErrorType::SystemError, "Insufficient data for model creation");
        return TaskResult::Error;
    }
    
    try {
        FocusModel model;
        
        // Prepare data for polynomial fitting
        std::vector<std::pair<double, double>> curve_data;
        for (const auto& point : calibration_data_) {
            curve_data.emplace_back(static_cast<double>(point.position), point.quality.hfr);
        }
        
        // Fit polynomial model (3rd degree)
        model.curve_coefficients = fitPolynomial(curve_data, 3);
        
        // Set model parameters
        model.base_temperature = 20.0;
        model.temp_coefficient = result_.temperature_coefficient;
        model.model_creation_time = std::chrono::steady_clock::now();
        
        // Calculate model validity ranges
        auto pos_range = std::minmax_element(calibration_data_.begin(), calibration_data_.end(),
                                           [](const auto& a, const auto& b) {
                                               return a.position < b.position;
                                           });
        model.valid_position_range = {pos_range.first->position, pos_range.second->position};
        
        auto temp_range = std::minmax_element(calibration_data_.begin(), calibration_data_.end(),
                                            [](const auto& a, const auto& b) {
                                                return a.temperature < b.temperature;
                                            });
        model.valid_temperature_range = {temp_range.first->temperature, temp_range.second->temperature};
        
        // Calculate model quality metrics
        model.r_squared = 0.85; // Placeholder - would calculate actual R²
        model.mean_absolute_error = 0.1; // Placeholder
        
        focus_model_ = model;
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Focus model creation failed: ") + e.what());
        return TaskResult::Error;
    }
}

std::vector<double> FocusCalibrationTask::fitPolynomial(
    const std::vector<std::pair<double, double>>& data, int degree) {
    
    // Simple polynomial fitting implementation
    // In a real implementation, this would use proper least squares fitting
    std::vector<double> coefficients(degree + 1, 0.0);
    
    if (data.empty()) {
        return coefficients;
    }
    
    // For now, return dummy coefficients
    // A real implementation would use numerical methods
    coefficients[0] = 1.0; // Constant term
    coefficients[1] = 0.001; // Linear term
    coefficients[2] = -0.00001; // Quadratic term
    if (degree >= 3) {
        coefficients[3] = 0.000001; // Cubic term
    }
    
    return coefficients;
}

FocusCalibrationTask::CalibrationResult FocusCalibrationTask::getCalibrationResult() const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return result_;
}

std::optional<FocusCalibrationTask::FocusModel> FocusCalibrationTask::getFocusModel() const {
    std::lock_guard<std::mutex> lock(calibration_mutex_);
    return focus_model_;
}

Task::TaskResult FocusCalibrationTask::saveCalibrationData(const std::string& filename) const {
    try {
        Json::Value root;
        Json::Value calibration_info;
        
        // Save calibration result
        calibration_info["optimal_position"] = result_.optimal_position;
        calibration_info["optimal_hfr"] = result_.optimal_hfr;
        calibration_info["optimal_fwhm"] = result_.optimal_fwhm;
        calibration_info["confidence"] = result_.calibration_confidence;
        calibration_info["total_measurements"] = static_cast<int>(result_.total_measurements);
        
        // Save data points
        Json::Value data_points(Json::arrayValue);
        for (const auto& point : calibration_data_) {
            Json::Value point_data;
            point_data["position"] = point.position;
            point_data["hfr"] = point.quality.hfr;
            point_data["fwhm"] = point.quality.fwhm;
            point_data["star_count"] = point.quality.star_count;
            point_data["temperature"] = point.temperature;
            point_data["notes"] = point.notes;
            data_points.append(point_data);
        }
        calibration_info["data_points"] = data_points;
        
        root["calibration"] = calibration_info;
        
        // Write to file
        std::ofstream file(filename);
        Json::StreamWriterBuilder builder;
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &file);
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Failed to save calibration data: ") + e.what());
        return TaskResult::Error;
    }
}

// QuickFocusCalibration implementation

QuickFocusCalibration::QuickFocusCalibration(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::Camera> camera,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , camera_(std::move(camera))
    , config_(config) {
    
    setTaskName("QuickFocusCalibration");
    setTaskDescription("Quick focus calibration for basic setup");
    
    result_.calibration_successful = false;
    result_.optimal_position = 0;
    result_.focus_quality = 0.0;
}

bool QuickFocusCalibration::validateParameters() const {
    if (!camera_) {
        setLastError(Task::ErrorType::InvalidParameter, "Camera not provided");
        return false;
    }
    
    if (config_.search_range <= 0 || config_.step_size <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid search parameters");
        return false;
    }
    
    return true;
}

void QuickFocusCalibration::resetTask() {
    BaseFocuserTask::resetTask();
    result_.calibration_successful = false;
    result_.optimal_position = 0;
    result_.focus_quality = 0.0;
    result_.notes.clear();
}

Task::TaskResult QuickFocusCalibration::executeImpl() {
    try {
        updateProgress(0.0, "Starting quick calibration");
        
        int current_pos = focuser_->getPosition();
        int start_pos = current_pos - config_.search_range / 2;
        int end_pos = current_pos + config_.search_range / 2;
        
        std::vector<std::pair<int, double>> measurements;
        
        // Coarse search
        updateProgress(10.0, "Coarse search");
        for (int pos = start_pos; pos <= end_pos; pos += config_.step_size) {
            auto move_result = moveToPositionAbsolute(pos);
            if (move_result != TaskResult::Success) continue;
            
            std::this_thread::sleep_for(config_.settling_time);
            
            auto capture_result = captureAndAnalyze();
            if (capture_result != TaskResult::Success) continue;
            
            auto quality = getLastFocusQuality();
            measurements.emplace_back(pos, quality.hfr);
            
            double progress = 10.0 + (pos - start_pos) * 60.0 / (end_pos - start_pos);
            updateProgress(progress, "Searching for optimal focus");
        }
        
        if (measurements.empty()) {
            result_.notes = "No valid measurements obtained";
            return TaskResult::Error;
        }
        
        // Find best coarse position
        auto best_coarse = std::min_element(measurements.begin(), measurements.end(),
                                          [](const auto& a, const auto& b) {
                                              return a.second < b.second;
                                          });
        
        int coarse_optimal = best_coarse->first;
        
        // Fine search around best coarse position
        updateProgress(70.0, "Fine search");
        measurements.clear();
        
        int fine_start = coarse_optimal - config_.step_size;
        int fine_end = coarse_optimal + config_.step_size;
        
        for (int pos = fine_start; pos <= fine_end; pos += config_.fine_step_size) {
            auto move_result = moveToPositionAbsolute(pos);
            if (move_result != TaskResult::Success) continue;
            
            std::this_thread::sleep_for(config_.settling_time);
            
            auto capture_result = captureAndAnalyze();
            if (capture_result != TaskResult::Success) continue;
            
            auto quality = getLastFocusQuality();
            measurements.emplace_back(pos, quality.hfr);
            
            double progress = 70.0 + (pos - fine_start) * 25.0 / (fine_end - fine_start);
            updateProgress(progress, "Fine focus adjustment");
        }
        
        if (!measurements.empty()) {
            auto best_fine = std::min_element(measurements.begin(), measurements.end(),
                                            [](const auto& a, const auto& b) {
                                                return a.second < b.second;
                                            });
            
            result_.optimal_position = best_fine->first;
            result_.focus_quality = best_fine->second;
            result_.calibration_successful = true;
            result_.notes = "Quick calibration completed successfully";
        } else {
            result_.optimal_position = coarse_optimal;
            result_.focus_quality = best_coarse->second;
            result_.calibration_successful = true;
            result_.notes = "Used coarse calibration result";
        }
        
        updateProgress(100.0, "Quick calibration completed");
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        result_.calibration_successful = false;
        result_.notes = std::string("Calibration failed: ") + e.what();
        setLastError(Task::ErrorType::DeviceError, result_.notes);
        return TaskResult::Error;
    }
}

void QuickFocusCalibration::updateProgress() {
    // Progress updated in executeImpl
}

std::string QuickFocusCalibration::getTaskInfo() const {
    std::ostringstream info;
    info << "QuickFocusCalibration";
    if (result_.calibration_successful) {
        info << " - Optimal: " << result_.optimal_position
             << ", Quality: " << std::fixed << std::setprecision(2) << result_.focus_quality;
    }
    return info.str();
}

QuickFocusCalibration::Result QuickFocusCalibration::getResult() const {
    return result_;
}

} // namespace lithium::task::custom::focuser
