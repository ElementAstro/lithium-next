/*
 * temperature_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera temperature controller implementation

*************************************************/

#include "temperature_controller.hpp"
#include "../core/asi_camera_core.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <cmath>

namespace lithium::device::asi::camera {

TemperatureController::TemperatureController(ASICameraCore* core)
    : ComponentBase(core) {
    LOG_F(INFO, "Created ASI temperature controller");
}

TemperatureController::~TemperatureController() {
    if (temperatureMonitoringEnabled_) {
        temperatureMonitoringEnabled_ = false;
        if (temperatureThread_.joinable()) {
            temperatureThread_.join();
        }
    }
    LOG_F(INFO, "Destroyed ASI temperature controller");
}

auto TemperatureController::initialize() -> bool {
    LOG_F(INFO, "Initializing ASI temperature controller");
    
    // Detect hardware capabilities
    detectHardwareCapabilities();
    
    // Initialize monitoring thread
    if (hasCooler_) {
        temperatureMonitoringEnabled_ = true;
        temperatureThread_ = std::thread(&TemperatureController::temperatureThreadFunction, this);
    }
    
    // Reset statistics
    resetTemperatureStatistics();
    
    return true;
}

auto TemperatureController::destroy() -> bool {
    LOG_F(INFO, "Destroying ASI temperature controller");
    
    // Stop cooling
    if (coolerEnabled_) {
        stopCooling();
    }
    
    // Stop monitoring thread
    temperatureMonitoringEnabled_ = false;
    if (temperatureThread_.joinable()) {
        temperatureThread_.join();
    }
    
    return true;
}

auto TemperatureController::getComponentName() const -> std::string {
    return "ASI Temperature Controller";
}

auto TemperatureController::onCameraStateChanged(CameraState state) -> void {
    LOG_F(INFO, "ASI temperature controller: Camera state changed to {}", static_cast<int>(state));
    
    // Adjust cooling behavior based on camera state
    if (state == CameraState::EXPOSING && coolerEnabled_) {
        // Ensure stable cooling during exposure
        updateCoolingControl();
    }
}

auto TemperatureController::startCooling(double targetTemp) -> bool {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    if (!hasCooler_) {
        LOG_F(ERROR, "Camera does not have cooling capability");
        return false;
    }
    
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
    if (!isValidTemperature(targetTemp)) {
        LOG_F(ERROR, "Invalid target temperature: {}", targetTemp);
        return false;
    }
    
    targetTemperature_ = targetTemp;
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // Enable cooler
    if (!getCore()->setControlValue(ASI_COOLER_ON, 1, ASI_FALSE)) {
        LOG_F(ERROR, "Failed to enable ASI cooler");
        return false;
    }
    
    // Set target temperature (converted to ASI units if needed)
    if (!getCore()->setControlValue(ASI_TARGET_TEMP, static_cast<long>(targetTemp), ASI_FALSE)) {
        LOG_F(ERROR, "Failed to set ASI target temperature");
        return false;
    }
#endif
    
    coolerEnabled_ = true;
    LOG_F(INFO, "Started ASI cooling to {}°C", targetTemp);
    return true;
}

auto TemperatureController::stopCooling() -> bool {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    if (!coolerEnabled_) {
        return true;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    // Disable cooler
    if (!getCore()->setControlValue(ASI_COOLER_ON, 0, ASI_FALSE)) {
        LOG_F(ERROR, "Failed to disable ASI cooler");
        return false;
    }
    
    // Disable fan if enabled
    if (fanEnabled_) {
        getCore()->setControlValue(ASI_FAN_ON, 0, ASI_FALSE);
        fanEnabled_ = false;
    }
#endif
    
    coolerEnabled_ = false;
    LOG_F(INFO, "Stopped ASI cooling");
    return true;
}

auto TemperatureController::isCoolerOn() const -> bool {
    return coolerEnabled_;
}

auto TemperatureController::getTemperature() const -> std::optional<double> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    long temperature = 0;
    if (getCore()->getControlValue(ASI_TEMPERATURE, &temperature)) {
        // ASI temperature is in 0.1°C units
        return static_cast<double>(temperature) / 10.0;
    }
    return std::nullopt;
#else
    // Stub implementation - simulate temperature drift
    double baseTemp = coolerEnabled_ ? targetTemperature_ + 2.0 : 25.0;
    static double drift = 0.0;
    drift += (std::rand() % 21 - 10) * 0.01;  // ±0.1°C drift
    return baseTemp + drift;
#endif
}

auto TemperatureController::getTargetTemperature() const -> double {
    return targetTemperature_;
}

auto TemperatureController::getCoolingPower() const -> double {
    if (!coolerEnabled_ || !getCore()->isConnected()) {
        return 0.0;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    long power = 0;
    if (getCore()->getControlValue(ASI_COOLER_POWER_PERC, &power)) {
        return static_cast<double>(power);
    }
    return 0.0;
#else
    // Stub implementation - simulate cooling power
    auto temp = getTemperature();
    if (temp) {
        double tempDiff = temp.value() - targetTemperature_;
        return std::clamp(tempDiff * 10.0, 0.0, 100.0);
    }
    return 0.0;
#endif
}

auto TemperatureController::enableTemperatureMonitoring(bool enable) -> bool {
    if (enable == temperatureMonitoringEnabled_) {
        return true;
    }
    
    temperatureMonitoringEnabled_ = enable;
    
    if (enable && hasCooler_) {
        if (temperatureThread_.joinable()) {
            temperatureThread_.join();
        }
        temperatureThread_ = std::thread(&TemperatureController::temperatureThreadFunction, this);
        LOG_F(INFO, "Enabled ASI temperature monitoring");
    } else {
        if (temperatureThread_.joinable()) {
            temperatureThread_.join();
        }
        LOG_F(INFO, "Disabled ASI temperature monitoring");
    }
    
    return true;
}

auto TemperatureController::isTemperatureMonitoringEnabled() const -> bool {
    return temperatureMonitoringEnabled_;
}

auto TemperatureController::getTemperatureHistory() const -> std::vector<std::pair<std::chrono::system_clock::time_point, double>> {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return temperatureHistory_;
}

auto TemperatureController::clearTemperatureHistory() -> void {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    temperatureHistory_.clear();
    LOG_F(INFO, "Cleared ASI temperature history");
}

auto TemperatureController::hasFan() const -> bool {
    return hasFan_;
}

auto TemperatureController::enableFan(bool enable) -> bool {
    if (!hasFan_) {
        LOG_F(ERROR, "Camera does not have fan capability");
        return false;
    }
    
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (!getCore()->setControlValue(ASI_FAN_ON, enable ? 1 : 0, ASI_FALSE)) {
        LOG_F(ERROR, "Failed to {} ASI fan", enable ? "enable" : "disable");
        return false;
    }
#endif
    
    fanEnabled_ = enable;
    LOG_F(INFO, "{} ASI fan", enable ? "Enabled" : "Disabled");
    return true;
}

auto TemperatureController::isFanEnabled() const -> bool {
    return fanEnabled_;
}

auto TemperatureController::setFanSpeed(int speed) -> bool {
    if (!hasFan_) {
        LOG_F(ERROR, "Camera does not have fan capability");
        return false;
    }
    
    if (speed < 0 || speed > 100) {
        LOG_F(ERROR, "Invalid fan speed: {}", speed);
        return false;
    }
    
    // ASI fans are typically on/off, not variable speed
    // But we can simulate variable speed control
    fanSpeed_ = speed;
    
    if (speed > 0 && !fanEnabled_) {
        enableFan(true);
    } else if (speed == 0 && fanEnabled_) {
        enableFan(false);
    }
    
    LOG_F(INFO, "Set ASI fan speed to {}%", speed);
    return true;
}

auto TemperatureController::getFanSpeed() const -> int {
    return fanSpeed_;
}

auto TemperatureController::hasAntiDewHeater() const -> bool {
    return hasAntiDewHeater_;
}

auto TemperatureController::enableAntiDewHeater(bool enable) -> bool {
    if (!hasAntiDewHeater_) {
        LOG_F(ERROR, "Camera does not have anti-dew heater capability");
        return false;
    }
    
    if (!getCore()->isConnected()) {
        LOG_F(ERROR, "Camera not connected");
        return false;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    if (!getCore()->setControlValue(ASI_ANTI_DEW_HEATER, enable ? 1 : 0, ASI_FALSE)) {
        LOG_F(ERROR, "Failed to {} ASI anti-dew heater", enable ? "enable" : "disable");
        return false;
    }
#endif
    
    antiDewHeaterEnabled_ = enable;
    LOG_F(INFO, "{} ASI anti-dew heater", enable ? "Enabled" : "Disabled");
    return true;
}

auto TemperatureController::isAntiDewHeaterEnabled() const -> bool {
    return antiDewHeaterEnabled_;
}

auto TemperatureController::setAntiDewHeaterPower(int power) -> bool {
    if (!hasAntiDewHeater_) {
        LOG_F(ERROR, "Camera does not have anti-dew heater capability");
        return false;
    }
    
    if (power < 0 || power > 100) {
        LOG_F(ERROR, "Invalid heater power: {}", power);
        return false;
    }
    
    antiDewHeaterPower_ = power;
    
    // Enable/disable heater based on power level
    if (power > 0 && !antiDewHeaterEnabled_) {
        enableAntiDewHeater(true);
    } else if (power == 0 && antiDewHeaterEnabled_) {
        enableAntiDewHeater(false);
    }
    
    LOG_F(INFO, "Set ASI anti-dew heater power to {}%", power);
    return true;
}

auto TemperatureController::getAntiDewHeaterPower() const -> int {
    return antiDewHeaterPower_;
}

auto TemperatureController::getMinTemperature() const -> double {
    return minTemperature_;
}

auto TemperatureController::getMaxTemperature() const -> double {
    return maxTemperature_;
}

auto TemperatureController::getAverageTemperature() const -> double {
    if (temperatureCount_ == 0) {
        return 0.0;
    }
    return temperatureSum_ / temperatureCount_;
}

auto TemperatureController::getTemperatureStability() const -> double {
    return calculateTemperatureStability();
}

auto TemperatureController::resetTemperatureStatistics() -> void {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    minTemperature_ = 100.0;
    maxTemperature_ = -100.0;
    temperatureSum_ = 0.0;
    temperatureCount_ = 0;
    LOG_F(INFO, "Reset ASI temperature statistics");
}

// Private helper methods
auto TemperatureController::temperatureThreadFunction() -> void {
    LOG_F(INFO, "Started ASI temperature monitoring thread");
    
    while (temperatureMonitoringEnabled_) {
        try {
            updateTemperatureReading();
            updateCoolingControl();
            updateFanControl();
            updateAntiDewHeater();
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception in temperature monitoring thread: {}", e.what());
        }
    }
    
    LOG_F(INFO, "Stopped ASI temperature monitoring thread");
}

auto TemperatureController::updateTemperatureReading() -> bool {
    auto temp = getTemperature();
    if (!temp) {
        return false;
    }
    
    double temperature = temp.value();
    currentTemperature_ = temperature;
    
    addTemperatureToHistory(temperature);
    updateTemperatureStatistics(temperature);
    
    return true;
}

auto TemperatureController::updateCoolingControl() -> bool {
    if (!coolerEnabled_ || !getCore()->isConnected()) {
        return true;
    }
    
    // Get current cooling power
    coolingPower_ = getCoolingPower();
    
    // Log cooling status periodically
    static auto lastLog = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::minutes>(now - lastLog).count() >= 1) {
        LOG_F(INFO, "ASI cooling: {:.1f}°C (target: {:.1f}°C, power: {:.1f}%)",
              currentTemperature_, targetTemperature_, coolingPower_);
        lastLog = now;
    }
    
    return true;
}

auto TemperatureController::updateFanControl() -> bool {
    if (!hasFan_ || !getCore()->isConnected()) {
        return true;
    }
    
    // Auto fan control based on temperature
    if (coolerEnabled_ && coolingPower_ > 50.0 && !fanEnabled_) {
        enableFan(true);
        LOG_F(INFO, "Auto-enabled ASI fan due to high cooling power");
    }
    
    return true;
}

auto TemperatureController::updateAntiDewHeater() -> bool {
    if (!hasAntiDewHeater_ || !getCore()->isConnected()) {
        return true;
    }
    
    // No automatic anti-dew heater control - manual only
    return true;
}

auto TemperatureController::addTemperatureToHistory(double temperature) -> void {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    temperatureHistory_.emplace_back(std::chrono::system_clock::now(), temperature);
    
    // Limit history size
    if (temperatureHistory_.size() > MAX_HISTORY_SIZE) {
        temperatureHistory_.erase(temperatureHistory_.begin());
    }
}

auto TemperatureController::updateTemperatureStatistics(double temperature) -> void {
    minTemperature_ = std::min(minTemperature_, temperature);
    maxTemperature_ = std::max(maxTemperature_, temperature);
    temperatureSum_ += temperature;
    temperatureCount_++;
}

auto TemperatureController::detectHardwareCapabilities() -> void {
    if (!getCore()->isConnected()) {
        // Assume basic capabilities for unconnected camera
        hasCooler_ = true;
        hasFan_ = false;
        hasAntiDewHeater_ = false;
        return;
    }
    
#ifdef LITHIUM_ASI_CAMERA_ENABLED
    ASI_CONTROL_CAPS caps;
    
    // Check for cooler
    hasCooler_ = (getCore()->getControlCaps(ASI_COOLER_ON, &caps) && caps.IsWritable);
    
    // Check for fan
    hasFan_ = (getCore()->getControlCaps(ASI_FAN_ON, &caps) && caps.IsWritable);
    
    // Check for anti-dew heater
    hasAntiDewHeater_ = (getCore()->getControlCaps(ASI_ANTI_DEW_HEATER, &caps) && caps.IsWritable);
#else
    // Stub implementation
    const ASI_CAMERA_INFO* info = getCore()->getCameraInfo();
    if (info) {
        hasCooler_ = info->IsCoolerCam == 1;
        hasFan_ = hasCooler_;  // Assume cooled cameras have fans
        hasAntiDewHeater_ = false;  // Uncommon feature
    }
#endif
    
    LOG_F(INFO, "ASI hardware capabilities: Cooler={}, Fan={}, Anti-dew={}",
          hasCooler_, hasFan_, hasAntiDewHeater_);
}

auto TemperatureController::isValidTemperature(double temperature) const -> bool {
    return temperature >= -60.0 && temperature <= 60.0;
}

auto TemperatureController::calculateTemperatureStability() const -> double {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    if (temperatureHistory_.size() < 10) {
        return 0.0;  // Need sufficient data
    }
    
    // Calculate standard deviation of recent temperatures
    auto recent_start = temperatureHistory_.end() - std::min(static_cast<size_t>(100), temperatureHistory_.size());
    
    double sum = 0.0;
    double sumSquares = 0.0;
    size_t count = 0;
    
    for (auto it = recent_start; it != temperatureHistory_.end(); ++it) {
        double temp = it->second;
        sum += temp;
        sumSquares += temp * temp;
        count++;
    }
    
    if (count < 2) {
        return 0.0;
    }
    
    double mean = sum / count;
    double variance = (sumSquares / count) - (mean * mean);
    return std::sqrt(variance);  // Standard deviation
}

} // namespace lithium::device::asi::camera
