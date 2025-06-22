#include "temperature_controller.hpp"
#include "hardware_interface.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace lithium::device::asi::camera::components {

TemperatureController::TemperatureController(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    currentInfo_.timestamp = std::chrono::steady_clock::now();
}

TemperatureController::~TemperatureController() {
    if (coolerEnabled_) {
        stopCooling();
    }
    cleanupResources();
}

bool TemperatureController::startCooling(double targetTemperature) {
    CoolingSettings settings;
    settings.targetTemperature = targetTemperature;
    return startCooling(settings);
}

bool TemperatureController::startCooling(const CoolingSettings& settings) {
    if (state_ != CoolerState::OFF) {
        return false;
    }

    if (!validateCoolingSettings(settings)) {
        return false;
    }

    if (!hardware_->isConnected()) {
        return false;
    }

    updateState(CoolerState::STARTING);
    currentSettings_ = settings;
    coolerEnabled_ = true;
    
    // Reset PID controller
    resetPIDController();
    
    // Start worker threads
    stopRequested_ = false;
    monitoringThread_ = std::thread(&TemperatureController::monitoringWorker, this);
    controlThread_ = std::thread(&TemperatureController::controlWorker, this);
    
    coolingStartTime_ = std::chrono::steady_clock::now();
    updateState(CoolerState::COOLING);
    
    return true;
}

bool TemperatureController::stopCooling() {
    if (state_ == CoolerState::OFF) {
        return false;
    }

    updateState(CoolerState::STOPPING);
    
    // Signal threads to stop
    stopRequested_ = true;
    stateCondition_.notify_all();
    
    // Wait for threads to finish
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    if (controlThread_.joinable()) {
        controlThread_.join();
    }
    
    // Turn off cooler
    applyCoolerPower(0.0);
    coolerEnabled_ = false;
    
    updateState(CoolerState::OFF);
    return true;
}

std::string TemperatureController::getStateString() const {
    switch (state_) {
        case CoolerState::OFF: return "OFF";
        case CoolerState::STARTING: return "STARTING";
        case CoolerState::COOLING: return "COOLING";
        case CoolerState::STABILIZING: return "STABILIZING";
        case CoolerState::STABLE: return "STABLE";
        case CoolerState::STOPPING: return "STOPPING";
        case CoolerState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

TemperatureController::TemperatureInfo TemperatureController::getCurrentTemperatureInfo() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return currentInfo_;
}

bool TemperatureController::hasCooler() const {
    return hardware_->isConnected(); // Stub implementation
}

double TemperatureController::getCurrentTemperature() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return currentInfo_.currentTemperature;
}

double TemperatureController::getCoolerPower() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return currentInfo_.coolerPower;
}

bool TemperatureController::hasReachedTarget() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return currentInfo_.hasReachedTarget;
}

double TemperatureController::getTemperatureStability() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    if (temperatureHistory_.size() < 2) {
        return 0.0;
    }
    
    // Calculate standard deviation of recent temperatures
    auto now = std::chrono::steady_clock::now();
    std::vector<double> recentTemps;
    
    for (const auto& info : temperatureHistory_) {
        auto age = std::chrono::duration_cast<std::chrono::minutes>(now - info.timestamp);
        if (age < std::chrono::minutes(5)) { // Last 5 minutes
            recentTemps.push_back(info.currentTemperature);
        }
    }
    
    if (recentTemps.size() < 2) {
        return 0.0;
    }
    
    double mean = std::accumulate(recentTemps.begin(), recentTemps.end(), 0.0) / recentTemps.size();
    double sq_sum = std::inner_product(recentTemps.begin(), recentTemps.end(), recentTemps.begin(), 0.0);
    return std::sqrt(sq_sum / recentTemps.size() - mean * mean);
}

bool TemperatureController::updateSettings(const CoolingSettings& settings) {
    if (state_ == CoolerState::COOLING) {
        return false; // Cannot update while actively cooling
    }
    
    if (!validateCoolingSettings(settings)) {
        return false;
    }
    
    currentSettings_ = settings;
    return true;
}

bool TemperatureController::updateTargetTemperature(double temperature) {
    if (!validateTargetTemperature(temperature)) {
        return false;
    }
    
    currentSettings_.targetTemperature = temperature;
    
    if (coolerEnabled_) {
        resetPIDController(); // Reset PID when target changes
    }
    
    return true;
}

bool TemperatureController::updateMaxCoolerPower(double power) {
    power = std::clamp(power, 0.0, 100.0);
    currentSettings_.maxCoolerPower = power;
    pidParams_.maxOutput = power;
    return true;
}

void TemperatureController::setPIDParams(const PIDParams& params) {
    std::lock_guard<std::mutex> lock(pidMutex_);
    pidParams_ = params;
}

void TemperatureController::resetPIDController() {
    std::lock_guard<std::mutex> lock(pidMutex_);
    previousError_ = 0.0;
    integralSum_ = 0.0;
    lastControlUpdate_ = std::chrono::steady_clock::time_point{};
}

std::vector<TemperatureController::TemperatureInfo> 
TemperatureController::getTemperatureHistory(std::chrono::seconds duration) const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    std::vector<TemperatureInfo> result;
    auto cutoff = std::chrono::steady_clock::now() - duration;
    
    for (const auto& info : temperatureHistory_) {
        if (info.timestamp >= cutoff) {
            result.push_back(info);
        }
    }
    
    return result;
}

void TemperatureController::clearTemperatureHistory() {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    temperatureHistory_.clear();
}

size_t TemperatureController::getHistorySize() const {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return temperatureHistory_.size();
}

void TemperatureController::setTemperatureCallback(TemperatureCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    temperatureCallback_ = std::move(callback);
}

void TemperatureController::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    stateCallback_ = std::move(callback);
}

// Private methods
void TemperatureController::monitoringWorker() {
    while (!stopRequested_) {
        try {
            if (readCurrentTemperature()) {
                updateTemperatureHistory(currentInfo_);
                checkTemperatureStability();
                checkCoolingTimeout();
                notifyTemperatureChange(currentInfo_);
            }
        } catch (const std::exception& e) {
            notifyStateChange(CoolerState::ERROR, 
                            formatTemperatureError("Monitoring", e.what()));
        }
        
        std::this_thread::sleep_for(monitoringInterval_);
    }
}

void TemperatureController::controlWorker() {
    while (!stopRequested_) {
        try {
            if (coolerEnabled_ && state_ != CoolerState::ERROR) {
                double output = calculatePIDOutput(
                    currentInfo_.currentTemperature, 
                    currentSettings_.targetTemperature);
                
                output = clampCoolerPower(output);
                applyCoolerPower(output);
                
                currentInfo_.coolerPower = output;
                currentInfo_.hasReachedTarget = 
                    std::abs(currentInfo_.currentTemperature - currentSettings_.targetTemperature) 
                    <= currentSettings_.temperatureTolerance;
            }
        } catch (const std::exception& e) {
            notifyStateChange(CoolerState::ERROR, 
                            formatTemperatureError("Control", e.what()));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool TemperatureController::readCurrentTemperature() {
    // Stub implementation - would read from hardware
    currentInfo_.currentTemperature = 25.0; // Placeholder
    currentInfo_.timestamp = std::chrono::steady_clock::now();
    return true;
}

bool TemperatureController::applyCoolerPower(double power) {
    // Stub implementation - would apply to hardware
    return true;
}

double TemperatureController::calculatePIDOutput(double currentTemp, double targetTemp) {
    std::lock_guard<std::mutex> lock(pidMutex_);
    
    double error = targetTemp - currentTemp;
    auto now = std::chrono::steady_clock::now();
    
    if (lastControlUpdate_ == std::chrono::steady_clock::time_point{}) {
        lastControlUpdate_ = now;
        previousError_ = error;
        return 0.0;
    }
    
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastControlUpdate_).count() / 1000.0;
    
    if (dt <= 0) {
        return 0.0;
    }
    
    // Proportional term
    double proportional = pidParams_.kp * error;
    
    // Integral term
    integralSum_ += error * dt;
    integralSum_ = std::clamp(integralSum_, -pidParams_.integralWindup, pidParams_.integralWindup);
    double integral = pidParams_.ki * integralSum_;
    
    // Derivative term
    double derivative = pidParams_.kd * (error - previousError_) / dt;
    
    // Calculate output
    double output = proportional + integral + derivative;
    output = std::clamp(output, pidParams_.minOutput, pidParams_.maxOutput);
    
    previousError_ = error;
    lastControlUpdate_ = now;
    
    return output;
}

void TemperatureController::updateTemperatureHistory(const TemperatureInfo& info) {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    
    temperatureHistory_.push_back(info);
    
    // Clean old history
    auto cutoff = std::chrono::steady_clock::now() - historyDuration_;
    while (!temperatureHistory_.empty() && 
           temperatureHistory_.front().timestamp < cutoff) {
        temperatureHistory_.pop_front();
    }
}

void TemperatureController::checkTemperatureStability() {
    if (state_ != CoolerState::COOLING && state_ != CoolerState::STABILIZING) {
        return;
    }
    
    bool atTarget = std::abs(currentInfo_.currentTemperature - currentSettings_.targetTemperature) 
                   <= currentSettings_.temperatureTolerance;
    
    if (atTarget) {
        if (state_ == CoolerState::COOLING) {
            updateState(CoolerState::STABILIZING);
            lastStableTime_ = std::chrono::steady_clock::now();
        } else if (state_ == CoolerState::STABILIZING) {
            auto stableTime = std::chrono::steady_clock::now() - lastStableTime_;
            if (stableTime >= currentSettings_.stabilizationTime) {
                updateState(CoolerState::STABLE);
                hasBeenStable_ = true;
            }
        }
    } else {
        if (state_ == CoolerState::STABILIZING || state_ == CoolerState::STABLE) {
            updateState(CoolerState::COOLING);
        }
    }
}

void TemperatureController::checkCoolingTimeout() {
    if (state_ == CoolerState::COOLING || state_ == CoolerState::STABILIZING) {
        auto elapsed = std::chrono::steady_clock::now() - coolingStartTime_;
        if (elapsed >= currentSettings_.timeout) {
            notifyStateChange(CoolerState::ERROR, "Cooling timeout exceeded");
        }
    }
}

void TemperatureController::notifyTemperatureChange(const TemperatureInfo& info) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (temperatureCallback_) {
        temperatureCallback_(info);
    }
}

void TemperatureController::notifyStateChange(CoolerState newState, const std::string& message) {
    updateState(newState);
    
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stateCallback_) {
        stateCallback_(newState, message);
    }
}

void TemperatureController::updateState(CoolerState newState) {
    state_ = newState;
}

bool TemperatureController::validateCoolingSettings(const CoolingSettings& settings) {
    return validateTargetTemperature(settings.targetTemperature) &&
           settings.maxCoolerPower >= 0.0 && settings.maxCoolerPower <= 100.0 &&
           settings.temperatureTolerance > 0.0;
}

bool TemperatureController::validateTargetTemperature(double temperature) {
    return temperature >= -50.0 && temperature <= 50.0; // Reasonable range
}

double TemperatureController::clampCoolerPower(double power) {
    return std::clamp(power, 0.0, currentSettings_.maxCoolerPower);
}

std::string TemperatureController::formatTemperatureError(const std::string& operation, 
                                                        const std::string& error) {
    return operation + " error: " + error;
}

void TemperatureController::cleanupResources() {
    stopRequested_ = true;
    stateCondition_.notify_all();
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    if (controlThread_.joinable()) {
        controlThread_.join();
    }
} // namespace lithium::device::asi::camera::components

} // namespace lithium::device::asi::camera::components
