#include "temperature.hpp"
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace lithium::task::custom::focuser {

TemperatureCompensationTask::TemperatureCompensationTask(
    std::shared_ptr<device::Focuser> focuser,
    std::shared_ptr<device::TemperatureSensor> sensor,
    const Config& config)
    : BaseFocuserTask(std::move(focuser))
    , temperature_sensor_(std::move(sensor))
    , config_(config)
    , last_compensation_temperature_(0.0)
    , monitoring_active_(false)
    , calibration_in_progress_(false) {
    
    setTaskName("TemperatureCompensation");
    setTaskDescription("Compensates focus position based on temperature changes");
}

bool TemperatureCompensationTask::validateParameters() const {
    if (!BaseFocuserTask::validateParameters()) {
        return false;
    }
    
    if (!temperature_sensor_) {
        setLastError(Task::ErrorType::InvalidParameter, "Temperature sensor not provided");
        return false;
    }
    
    if (config_.temperature_coefficient < -MAX_REASONABLE_COEFFICIENT ||
        config_.temperature_coefficient > MAX_REASONABLE_COEFFICIENT) {
        setLastError(Task::ErrorType::InvalidParameter, 
                    "Temperature coefficient out of reasonable range");
        return false;
    }
    
    if (config_.min_temperature_change <= 0.0) {
        setLastError(Task::ErrorType::InvalidParameter, 
                    "Minimum temperature change must be positive");
        return false;
    }
    
    return true;
}

void TemperatureCompensationTask::resetTask() {
    BaseFocuserTask::resetTask();
    
    std::lock_guard<std::mutex> temp_lock(temperature_mutex_);
    std::lock_guard<std::mutex> comp_lock(compensation_mutex_);
    
    monitoring_active_ = false;
    calibration_in_progress_ = false;
    last_compensation_temperature_ = 0.0;
    
    // Clear caches but keep historical data for analysis
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

Task::TaskResult TemperatureCompensationTask::executeImpl() {
    try {
        updateProgress(0.0, "Starting temperature compensation");
        
        if (config_.auto_compensation) {
            startMonitoring();
            updateProgress(50.0, "Temperature monitoring active");
            
            // Perform initial temperature check
            auto result = performTemperatureCheck();
            if (result != TaskResult::Success) {
                return result;
            }
        }
        
        updateProgress(100.0, "Temperature compensation configured");
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::SystemError, 
                    std::string("Temperature compensation failed: ") + e.what());
        return TaskResult::Error;
    }
}

void TemperatureCompensationTask::updateProgress() {
    if (monitoring_active_) {
        auto current_temp = getCurrentTemperature();
        auto avg_temp = getAverageTemperature();
        
        std::ostringstream status;
        status << "Monitoring - Current: " << std::fixed << std::setprecision(1) 
               << current_temp << "°C, Average: " << avg_temp << "°C";
        
        setProgressMessage(status.str());
    }
}

std::string TemperatureCompensationTask::getTaskInfo() const {
    std::ostringstream info;
    info << BaseFocuserTask::getTaskInfo()
         << ", Coefficient: " << config_.temperature_coefficient << " steps/°C"
         << ", Monitoring: " << (monitoring_active_ ? "Active" : "Inactive");
    
    if (!temperature_history_.empty()) {
        info << ", Current Temp: " << std::fixed << std::setprecision(1) 
             << getCurrentTemperature() << "°C";
    }
    
    return info.str();
}

void TemperatureCompensationTask::setConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    config_ = config;
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

TemperatureCompensationTask::Config TemperatureCompensationTask::getConfig() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    return config_;
}

void TemperatureCompensationTask::startMonitoring() {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    if (!monitoring_active_) {
        monitoring_active_ = true;
        monitoring_start_time_ = std::chrono::steady_clock::now();
        
        // Get initial temperature reading
        try {
            double initial_temp = temperature_sensor_->getTemperature();
            if (isTemperatureReadingValid(initial_temp)) {
                int current_position = focuser_->getPosition();
                addTemperatureReading(initial_temp, current_position);
                last_compensation_temperature_ = initial_temp;
            }
        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }
    }
}

void TemperatureCompensationTask::stopMonitoring() {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    monitoring_active_ = false;
}

bool TemperatureCompensationTask::isMonitoring() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    return monitoring_active_;
}

Task::TaskResult TemperatureCompensationTask::performTemperatureCheck() {
    try {
        double current_temp = temperature_sensor_->getTemperature();
        
        if (!isTemperatureReadingValid(current_temp)) {
            return TaskResult::Error;
        }
        
        int current_position = focuser_->getPosition();
        addTemperatureReading(current_temp, current_position);
        
        double compensation_steps;
        if (shouldTriggerCompensation(current_temp, compensation_steps)) {
            return applyCompensation(static_cast<int>(std::round(compensation_steps)),
                                   "Automatic temperature compensation");
        }
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Temperature check failed: ") + e.what());
        return TaskResult::Error;
    }
}

Task::TaskResult TemperatureCompensationTask::calculateRequiredCompensation(
    double temperature_change, int& required_steps) {
    
    double compensation = temperature_change * config_.temperature_coefficient;
    required_steps = static_cast<int>(std::round(compensation));
    
    // Apply limits
    if (std::abs(required_steps) > config_.max_compensation_per_cycle) {
        required_steps = static_cast<int>(
            std::copysign(config_.max_compensation_per_cycle, required_steps));
    }
    
    return TaskResult::Success;
}

Task::TaskResult TemperatureCompensationTask::applyCompensation(
    int steps, const std::string& reason) {
    
    if (!isCompensationReasonable(steps)) {
        setLastError(Task::ErrorType::InvalidParameter, 
                    "Compensation steps are unreasonably large");
        return TaskResult::Error;
    }
    
    try {
        int old_position = focuser_->getPosition();
        double current_temp = getCurrentTemperature();
        
        auto result = moveToPositionRelative(steps);
        if (result != TaskResult::Success) {
            return result;
        }
        
        int new_position = focuser_->getPosition();
        
        // Record compensation event
        CompensationEvent event;
        event.timestamp = std::chrono::steady_clock::now();
        event.old_temperature = last_compensation_temperature_;
        event.new_temperature = current_temp;
        event.old_position = old_position;
        event.new_position = new_position;
        event.compensation_steps = new_position - old_position;
        event.reason = reason;
        
        saveCompensationEvent(event);
        last_compensation_temperature_ = current_temp;
        
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Failed to apply compensation: ") + e.what());
        return TaskResult::Error;
    }
}

void TemperatureCompensationTask::addTemperatureReading(double temperature, int position) {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    TemperatureReading reading;
    reading.timestamp = std::chrono::steady_clock::now();
    reading.temperature = temperature;
    reading.focus_position = position;
    
    temperature_history_.push_back(reading);
    
    // Maintain maximum history size
    if (temperature_history_.size() > MAX_HISTORY_SIZE) {
        temperature_history_.pop_front();
    }
    
    // Invalidate statistics cache
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

double TemperatureCompensationTask::calculateAverageTemperature() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    if (temperature_history_.empty()) {
        return 0.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - config_.averaging_period;
    
    double sum = 0.0;
    size_t count = 0;
    
    for (const auto& reading : temperature_history_) {
        if (reading.timestamp >= cutoff_time) {
            sum += reading.temperature;
            ++count;
        }
    }
    
    return count > 0 ? sum / count : 0.0;
}

double TemperatureCompensationTask::calculateTemperatureTrend() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    if (temperature_history_.size() < 2) {
        return 0.0;
    }
    
    // Use linear regression over the last hour of data
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - std::chrono::hours(1);
    
    std::vector<std::pair<double, double>> data; // time_minutes, temperature
    
    for (const auto& reading : temperature_history_) {
        if (reading.timestamp >= cutoff_time) {
            auto minutes_since = std::chrono::duration_cast<std::chrono::minutes>(
                reading.timestamp - cutoff_time).count();
            data.emplace_back(static_cast<double>(minutes_since), reading.temperature);
        }
    }
    
    if (data.size() < 2) {
        return 0.0;
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
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Convert to degrees per hour
    return slope * 60.0;
}

bool TemperatureCompensationTask::shouldTriggerCompensation(
    double current_temp, double& compensation_steps) {
    
    if (last_compensation_temperature_ == 0.0) {
        last_compensation_temperature_ = current_temp;
        return false;
    }
    
    double temperature_change = current_temp - last_compensation_temperature_;
    
    if (std::abs(temperature_change) < config_.min_temperature_change) {
        return false;
    }
    
    compensation_steps = temperature_change * config_.temperature_coefficient;
    
    // Add predictive component if enabled
    if (config_.enable_predictive) {
        double predictive_compensation = calculatePredictiveCompensation();
        compensation_steps += predictive_compensation;
    }
    
    return std::abs(compensation_steps) >= 1.0;
}

double TemperatureCompensationTask::calculatePredictiveCompensation() const {
    auto trend = getTemperatureTrend();
    double prediction_hours = config_.prediction_window_minutes / 60.0;
    double predicted_change = trend * prediction_hours;
    
    return predicted_change * config_.temperature_coefficient * 0.5; // 50% weight for prediction
}

bool TemperatureCompensationTask::isTemperatureReadingValid(double temperature) const {
    return temperature >= MIN_TEMPERATURE && temperature <= MAX_TEMPERATURE &&
           !std::isnan(temperature) && !std::isinf(temperature);
}

bool TemperatureCompensationTask::isCompensationReasonable(int steps) const {
    return std::abs(steps) <= config_.max_compensation_per_cycle * 2; // Allow some margin
}

void TemperatureCompensationTask::saveCompensationEvent(const CompensationEvent& event) {
    std::lock_guard<std::mutex> lock(compensation_mutex_);
    
    compensation_history_.push_back(event);
    
    // Maintain maximum history size
    if (compensation_history_.size() > MAX_EVENTS_SIZE) {
        compensation_history_.pop_front();
    }
    
    // Invalidate statistics cache
    statistics_cache_time_ = std::chrono::steady_clock::time_point{};
}

double TemperatureCompensationTask::getCurrentTemperature() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    
    if (temperature_history_.empty()) {
        return 0.0;
    }
    
    return temperature_history_.back().temperature;
}

double TemperatureCompensationTask::getAverageTemperature() const {
    return calculateAverageTemperature();
}

double TemperatureCompensationTask::getTemperatureTrend() const {
    return calculateTemperatureTrend();
}

std::vector<TemperatureCompensationTask::TemperatureReading> 
TemperatureCompensationTask::getTemperatureHistory() const {
    std::lock_guard<std::mutex> lock(temperature_mutex_);
    return std::vector<TemperatureReading>(temperature_history_.begin(), temperature_history_.end());
}

std::vector<TemperatureCompensationTask::CompensationEvent> 
TemperatureCompensationTask::getCompensationHistory() const {
    std::lock_guard<std::mutex> lock(compensation_mutex_);
    return std::vector<CompensationEvent>(compensation_history_.begin(), compensation_history_.end());
}

TemperatureCompensationTask::Statistics TemperatureCompensationTask::getStatistics() const {
    auto now = std::chrono::steady_clock::now();
    
    // Use cached statistics if recent
    if (now - statistics_cache_time_ < std::chrono::seconds(5)) {
        return cached_statistics_;
    }
    
    std::lock_guard<std::mutex> comp_lock(compensation_mutex_);
    std::lock_guard<std::mutex> temp_lock(temperature_mutex_);
    
    Statistics stats;
    
    if (!compensation_history_.empty()) {
        stats.total_compensations = compensation_history_.size();
        
        double total_steps = 0.0;
        double max_comp = 0.0;
        
        for (const auto& event : compensation_history_) {
            total_steps += std::abs(event.compensation_steps);
            max_comp = std::max(max_comp, std::abs(event.compensation_steps));
        }
        
        stats.total_compensation_steps = total_steps;
        stats.average_compensation = total_steps / stats.total_compensations;
        stats.max_compensation = max_comp;
    }
    
    if (!temperature_history_.empty()) {
        auto minmax = std::minmax_element(temperature_history_.begin(), 
                                         temperature_history_.end(),
                                         [](const auto& a, const auto& b) {
                                             return a.temperature < b.temperature;
                                         });
        stats.temperature_range_min = minmax.first->temperature;
        stats.temperature_range_max = minmax.second->temperature;
        
        if (monitoring_active_) {
            stats.monitoring_time = std::chrono::duration_cast<std::chrono::seconds>(
                now - monitoring_start_time_);
        }
    }
    
    // Cache the results
    cached_statistics_ = stats;
    statistics_cache_time_ = now;
    
    return stats;
}

// TemperatureMonitorTask implementation

TemperatureMonitorTask::TemperatureMonitorTask(
    std::shared_ptr<device::TemperatureSensor> sensor,
    const Config& config)
    : BaseFocuserTask(nullptr) // No focuser needed for monitoring
    , temperature_sensor_(std::move(sensor))
    , config_(config) {
    
    setTaskName("TemperatureMonitor");
    setTaskDescription("Monitors and logs temperature readings");
}

bool TemperatureMonitorTask::validateParameters() const {
    if (!temperature_sensor_) {
        setLastError(Task::ErrorType::InvalidParameter, "Temperature sensor not provided");
        return false;
    }
    
    if (config_.interval.count() <= 0) {
        setLastError(Task::ErrorType::InvalidParameter, "Invalid monitoring interval");
        return false;
    }
    
    return true;
}

void TemperatureMonitorTask::resetTask() {
    BaseFocuserTask::resetTask();
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    temperature_log_.clear();
}

Task::TaskResult TemperatureMonitorTask::executeImpl() {
    try {
        updateProgress(0.0, "Starting temperature monitoring");
        
        auto start_time = std::chrono::steady_clock::now();
        size_t reading_count = 0;
        
        while (!shouldStop()) {
            double temperature = temperature_sensor_->getTemperature();
            auto timestamp = std::chrono::steady_clock::now();
            
            {
                std::lock_guard<std::mutex> lock(log_mutex_);
                temperature_log_.emplace_back(timestamp, temperature);
                
                // Check for rapid temperature change
                if (config_.alert_on_rapid_change && temperature_log_.size() >= 2) {
                    const auto& prev = temperature_log_[temperature_log_.size() - 2];
                    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                        timestamp - prev.first).count();
                    
                    if (time_diff > 0) {
                        double rate = std::abs(temperature - prev.second) / (time_diff / 60.0);
                        if (rate > config_.rapid_change_threshold) {
                            // Could emit warning/alert here
                        }
                    }
                }
            }
            
            ++reading_count;
            double progress = std::min(99.0, static_cast<double>(reading_count) / 100.0 * 100.0);
            updateProgress(progress, "Monitoring temperature: " + 
                          std::to_string(temperature) + "°C");
            
            // Wait for next reading
            std::this_thread::sleep_for(config_.interval);
        }
        
        updateProgress(100.0, "Temperature monitoring completed");
        return TaskResult::Success;
        
    } catch (const std::exception& e) {
        setLastError(Task::ErrorType::DeviceError, 
                    std::string("Temperature monitoring failed: ") + e.what());
        return TaskResult::Error;
    }
}

void TemperatureMonitorTask::updateProgress() {
    // Progress is updated in executeImpl
}

std::string TemperatureMonitorTask::getTaskInfo() const {
    std::ostringstream info;
    info << "TemperatureMonitor - Interval: " << config_.interval.count() << "s";
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (!temperature_log_.empty()) {
        info << ", Current: " << std::fixed << std::setprecision(1) 
             << temperature_log_.back().second << "°C"
             << ", Readings: " << temperature_log_.size();
    }
    
    return info.str();
}

double TemperatureMonitorTask::getCurrentTemperature() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return temperature_log_.empty() ? 0.0 : temperature_log_.back().second;
}

std::vector<std::pair<std::chrono::steady_clock::time_point, double>> 
TemperatureMonitorTask::getTemperatureLog() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return temperature_log_;
}

} // namespace lithium::task::custom::focuser
