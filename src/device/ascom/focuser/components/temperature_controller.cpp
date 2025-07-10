#include "temperature_controller.hpp"
#include "hardware_interface.hpp"
#include "movement_controller.hpp"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cstdlib>

namespace lithium::device::ascom::focuser::components {

TemperatureController::TemperatureController(std::shared_ptr<HardwareInterface> hardware,
                                           std::shared_ptr<MovementController> movement)
    : hardware_(hardware)
    , movement_(movement)
    , config_{}
    , compensation_{}
    , monitoring_active_(false)
    , current_temperature_(0.0)
    , last_compensation_temperature_(0.0)
    , stats_{}
{
}

TemperatureController::~TemperatureController() {
    stopMonitoring();
}

auto TemperatureController::initialize() -> bool {
    try {
        // Initialize temperature sensor if available
        if (!hardware_->hasTemperatureSensor()) {
            return true; // Not an error if no sensor
        }
        
        // Reset statistics
        resetTemperatureStats();
        
        // Initialize compensation settings
        compensation_.enabled = config_.enabled;
        compensation_.coefficient = config_.coefficient;
        
        return true;
    } catch (const std::exception& e) {
        // Log error
        return false;
    }
}

auto TemperatureController::destroy() -> bool {
    try {
        stopMonitoring();
        clearTemperatureHistory();
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto TemperatureController::setCompensationConfig(const CompensationConfig& config) -> void {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
    
    // Update compensation settings
    compensation_.coefficient = config.coefficient;
    compensation_.enabled = config.enabled;
}

auto TemperatureController::getCompensationConfig() const -> CompensationConfig {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

auto TemperatureController::hasTemperatureSensor() -> bool {
    return hardware_->hasTemperatureSensor();
}

auto TemperatureController::getExternalTemperature() -> std::optional<double> {
    return hardware_->getExternalTemperature();
}

auto TemperatureController::getChipTemperature() -> std::optional<double> {
    return hardware_->getChipTemperature();
}

auto TemperatureController::getTemperatureStats() -> TemperatureStats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

auto TemperatureController::resetTemperatureStats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = TemperatureStats{};
    stats_.lastUpdateTime = std::chrono::steady_clock::now();
}

auto TemperatureController::startMonitoring() -> bool {
    if (monitoring_active_.load()) {
        return true; // Already monitoring
    }
    
    if (!hardware_->hasTemperatureSensor()) {
        return false; // No sensor available
    }
    
    monitoring_active_.store(true);
    monitoring_thread_ = std::thread(&TemperatureController::monitorTemperature, this);
    
    return true;
}

auto TemperatureController::stopMonitoring() -> bool {
    if (!monitoring_active_.load()) {
        return true; // Already stopped
    }
    
    monitoring_active_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    return true;
}

auto TemperatureController::isMonitoring() -> bool {
    return monitoring_active_.load();
}

auto TemperatureController::getTemperatureCompensation() -> TemperatureCompensation {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return compensation_;
}

auto TemperatureController::setTemperatureCompensation(const TemperatureCompensation& compensation) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    compensation_ = compensation;
    
    // Update config to match
    config_.enabled = compensation.enabled;
    config_.coefficient = compensation.coefficient;
    
    return true;
}

auto TemperatureController::enableTemperatureCompensation(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    compensation_.enabled = enable;
    config_.enabled = enable;
    
    return true;
}

auto TemperatureController::isTemperatureCompensationEnabled() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return compensation_.enabled;
}

auto TemperatureController::calibrateCompensation(double temperatureChange, int focusChange) -> bool {
    if (std::abs(temperatureChange) < 0.1) {
        return false; // Temperature change too small
    }
    
    double coefficient = static_cast<double>(focusChange) / temperatureChange;
    
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.coefficient = coefficient;
    compensation_.coefficient = coefficient;
    
    return true;
}

auto TemperatureController::applyCompensation(double temperatureChange) -> bool {
    if (!isTemperatureCompensationEnabled()) {
        return false;
    }
    
    int steps = calculateCompensationSteps(temperatureChange);
    if (steps == 0) {
        return true; // No compensation needed
    }
    
    // Apply compensation through movement controller
    bool success = movement_->moveRelative(steps);
    
    // Notify callback if set
    if (compensation_callback_) {
        compensation_callback_(temperatureChange, steps, success);
    }
    
    return success;
}

auto TemperatureController::calculateCompensationSteps(double temperatureChange) -> int {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!compensation_.enabled || std::abs(temperatureChange) < config_.deadband) {
        return 0;
    }
    
    int steps = 0;
    
    switch (config_.algorithm) {
        case CompensationAlgorithm::LINEAR:
            steps = calculateLinearCompensation(temperatureChange);
            break;
        case CompensationAlgorithm::POLYNOMIAL:
            steps = calculatePolynomialCompensation(temperatureChange);
            break;
        case CompensationAlgorithm::LOOKUP_TABLE:
            steps = calculateLookupTableCompensation(temperatureChange);
            break;
        case CompensationAlgorithm::ADAPTIVE:
            steps = calculateAdaptiveCompensation(temperatureChange);
            break;
    }
    
    return validateCompensationSteps(steps);
}

auto TemperatureController::getTemperatureHistory() -> std::vector<TemperatureReading> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return temperature_history_;
}

auto TemperatureController::getTemperatureHistory(std::chrono::seconds duration) -> std::vector<TemperatureReading> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::vector<TemperatureReading> recent_history;
    
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    
    for (const auto& reading : temperature_history_) {
        if (reading.timestamp >= cutoff_time) {
            recent_history.push_back(reading);
        }
    }
    
    return recent_history;
}

auto TemperatureController::clearTemperatureHistory() -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);
    temperature_history_.clear();
}

auto TemperatureController::getTemperatureTrend() -> double {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (temperature_history_.size() < 2) {
        return 0.0;
    }
    
    // Calculate trend over last 5 minutes
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::minutes(5);
    
    std::vector<TemperatureReading> recent_readings;
    for (const auto& reading : temperature_history_) {
        if (reading.timestamp >= cutoff) {
            recent_readings.push_back(reading);
        }
    }
    
    if (recent_readings.size() < 2) {
        return 0.0;
    }
    
    // Simple linear trend calculation
    double first_temp = recent_readings.front().temperature;
    double last_temp = recent_readings.back().temperature;
    auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(
        recent_readings.back().timestamp - recent_readings.front().timestamp);
    
    if (time_diff.count() == 0) {
        return 0.0;
    }
    
    return (last_temp - first_temp) / time_diff.count(); // degrees per minute
}

auto TemperatureController::setTemperatureCallback(TemperatureCallback callback) -> void {
    temperature_callback_ = std::move(callback);
}

auto TemperatureController::setCompensationCallback(CompensationCallback callback) -> void {
    compensation_callback_ = std::move(callback);
}

auto TemperatureController::setTemperatureAlertCallback(TemperatureAlertCallback callback) -> void {
    temperature_alert_callback_ = std::move(callback);
}

auto TemperatureController::setCompensationCoefficient(double coefficient) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.coefficient = coefficient;
    compensation_.coefficient = coefficient;
    return true;
}

auto TemperatureController::getCompensationCoefficient() -> double {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return compensation_.coefficient;
}

auto TemperatureController::autoCalibrate(std::chrono::seconds duration) -> bool {
    // Implementation for auto-calibration
    // This would involve monitoring temperature and position changes
    // over the specified duration and calculating the best coefficient
    return false; // Placeholder
}

auto TemperatureController::saveCompensationSettings(const std::string& filename) -> bool {
    // Implementation for saving settings to file
    return false; // Placeholder
}

auto TemperatureController::loadCompensationSettings(const std::string& filename) -> bool {
    // Implementation for loading settings from file
    return false; // Placeholder
}

// Private methods

auto TemperatureController::calculateLinearCompensation(double tempChange) -> int {
    return static_cast<int>(std::round(tempChange * compensation_.coefficient));
}

auto TemperatureController::calculatePolynomialCompensation(double tempChange) -> int {
    // Placeholder for polynomial compensation
    return calculateLinearCompensation(tempChange);
}

auto TemperatureController::calculateLookupTableCompensation(double tempChange) -> int {
    // Placeholder for lookup table compensation
    return calculateLinearCompensation(tempChange);
}

auto TemperatureController::calculateAdaptiveCompensation(double tempChange) -> int {
    // Placeholder for adaptive compensation
    return calculateLinearCompensation(tempChange);
}

auto TemperatureController::monitorTemperature() -> void {
    while (monitoring_active_.load()) {
        try {
            auto temperature = getExternalTemperature();
            if (temperature.has_value()) {
                updateTemperatureReading(temperature.value());
                checkTemperatureCompensation();
            }
            
            std::this_thread::sleep_for(config_.updateInterval);
        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }
    }
}

auto TemperatureController::updateTemperatureReading(double temperature) -> void {
    current_temperature_.store(temperature);
    
    // Update statistics
    updateTemperatureStats(temperature);
    
    // Add to history
    int current_position = movement_->getCurrentPosition();
    addTemperatureReading(temperature, current_position, false, 0);
    
    // Notify callback
    notifyTemperatureChange(temperature);
}

auto TemperatureController::addTemperatureReading(double temperature, int position, bool compensated, int steps) -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    TemperatureReading reading{
        .timestamp = std::chrono::steady_clock::now(),
        .temperature = temperature,
        .focuserPosition = position,
        .compensationApplied = compensated,
        .compensationSteps = steps
    };
    
    temperature_history_.push_back(reading);
    
    // Limit history size
    if (temperature_history_.size() > MAX_HISTORY_SIZE) {
        temperature_history_.erase(temperature_history_.begin());
    }
}

auto TemperatureController::updateTemperatureStats(double temperature) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.currentTemperature = temperature;
    stats_.lastUpdateTime = std::chrono::steady_clock::now();
    
    if (stats_.minTemperature == 0.0 || temperature < stats_.minTemperature) {
        stats_.minTemperature = temperature;
    }
    
    if (stats_.maxTemperature == 0.0 || temperature > stats_.maxTemperature) {
        stats_.maxTemperature = temperature;
    }
    
    stats_.temperatureRange = stats_.maxTemperature - stats_.minTemperature;
    
    // Update running average (simple implementation)
    static int reading_count = 0;
    reading_count++;
    stats_.averageTemperature = (stats_.averageTemperature * (reading_count - 1) + temperature) / reading_count;
}

auto TemperatureController::checkTemperatureCompensation() -> void {
    if (!isTemperatureCompensationEnabled()) {
        return;
    }
    
    double current_temp = current_temperature_.load();
    double last_temp = last_compensation_temperature_.load();
    
    double temp_change = current_temp - last_temp;
    
    if (std::abs(temp_change) >= config_.deadband) {
        if (applyTemperatureCompensation(temp_change)) {
            last_compensation_temperature_.store(current_temp);
        }
    }
}

auto TemperatureController::applyTemperatureCompensation(double tempChange) -> bool {
    int steps = calculateCompensationSteps(tempChange);
    if (steps == 0) {
        return true;
    }
    
    bool success = movement_->moveRelative(steps);
    
    if (success) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.totalCompensations++;
        stats_.totalCompensationSteps += std::abs(steps);
        stats_.lastCompensationTime = std::chrono::steady_clock::now();
        
        // Add compensated reading to history
        int current_position = movement_->getCurrentPosition();
        addTemperatureReading(current_temperature_.load(), current_position, true, steps);
    }
    
    notifyCompensationApplied(tempChange, steps, success);
    
    return success;
}

auto TemperatureController::validateCompensationSteps(int steps) -> int {
    if (steps == 0) {
        return 0;
    }
    
    // Clamp to configured limits
    if (std::abs(steps) < config_.minCompensationSteps) {
        return 0;
    }
    
    if (std::abs(steps) > config_.maxCompensationSteps) {
        return (steps > 0) ? config_.maxCompensationSteps : -config_.maxCompensationSteps;
    }
    
    return steps;
}

auto TemperatureController::notifyTemperatureChange(double temperature) -> void {
    if (temperature_callback_) {
        try {
            temperature_callback_(temperature);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto TemperatureController::notifyCompensationApplied(double tempChange, int steps, bool success) -> void {
    if (compensation_callback_) {
        try {
            compensation_callback_(tempChange, steps, success);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto TemperatureController::notifyTemperatureAlert(double temperature, const std::string& message) -> void {
    if (temperature_alert_callback_) {
        try {
            temperature_alert_callback_(temperature, message);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto TemperatureController::recordCalibrationPoint(double temperature, int position) -> void {
    CalibrationPoint point{
        .temperature = temperature,
        .position = position,
        .timestamp = std::chrono::steady_clock::now()
    };
    
    calibration_points_.push_back(point);
    
    // Limit calibration points
    if (calibration_points_.size() > MAX_CALIBRATION_POINTS) {
        calibration_points_.erase(calibration_points_.begin());
    }
}

auto TemperatureController::calculateBestFitCoefficient() -> double {
    if (calibration_points_.size() < 2) {
        return 0.0;
    }
    
    // Simple linear regression
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    int n = calibration_points_.size();
    
    for (const auto& point : calibration_points_) {
        sum_x += point.temperature;
        sum_y += point.position;
        sum_xy += point.temperature * point.position;
        sum_x2 += point.temperature * point.temperature;
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    return slope;
}

auto TemperatureController::validateCalibrationData() -> bool {
    return calibration_points_.size() >= 2;
}

auto TemperatureController::clampTemperature(double temperature) -> double {
    static constexpr double MIN_TEMP = -50.0;
    static constexpr double MAX_TEMP = 100.0;
    
    return std::clamp(temperature, MIN_TEMP, MAX_TEMP);
}

auto TemperatureController::isValidTemperature(double temperature) -> bool {
    return temperature >= -50.0 && temperature <= 100.0 && !std::isnan(temperature) && !std::isinf(temperature);
}

auto TemperatureController::formatTemperature(double temperature) -> std::string {
    return std::to_string(temperature) + "°C";
}

} // namespace lithium::device::ascom::focuser::components
