/*
 * temperature_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Temperature Controller Component Implementation

This component manages camera cooling system including temperature
monitoring, cooler control, and thermal management.

*************************************************/

#include "temperature_controller.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "atom/utils/time.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace lithium::device::ascom::camera::components {

TemperatureController::TemperatureController(std::shared_ptr<HardwareInterface> hardware)
    : m_hardware(hardware)
    , m_currentState(CoolerState::OFF)
    , m_targetTemperature(0.0)
    , m_currentTemperature(0.0)
    , m_coolerPower(0.0)
    , m_isMonitoring(false)
    , m_maxTemperatureHistory(100)
    , m_temperatureTolerance(0.5)
    , m_stabilizationTime(30.0)
    , m_maxCoolerPower(100.0)
    , m_monitoringInterval(1.0)
    , m_thermalProtectionEnabled(true)
    , m_maxTemperature(50.0)
    , m_minTemperature(-50.0) {
    LOG_F(INFO, "ASCOM Camera TemperatureController initialized");
}

TemperatureController::~TemperatureController() {
    stopCooling();
    stopMonitoring();
    LOG_F(INFO, "ASCOM Camera TemperatureController destroyed");
}

// =========================================================================
// Cooler Control
// =========================================================================

bool TemperatureController::startCooling(double targetTemp) {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    
    if (!m_hardware || !m_hardware->isConnected()) {
        LOG_F(ERROR, "Cannot start cooling: hardware not connected");
        return false;
    }
    
    if (!validateTemperature(targetTemp)) {
        LOG_F(ERROR, "Invalid target temperature: {:.2f}°C", targetTemp);
        return false;
    }
    
    if (m_currentState != CoolerState::OFF) {
        LOG_F(WARNING, "Cooler already running, stopping current operation");
        stopCooling();
    }
    
    LOG_F(INFO, "Starting cooling to target temperature: {:.2f}°C", targetTemp);
    
    m_targetTemperature = targetTemp;
    setState(CoolerState::STARTING);
    
    // Enable cooler on hardware
    if (!m_hardware->setCoolerEnabled(true)) {
        LOG_F(ERROR, "Failed to enable cooler on hardware");
        setState(CoolerState::ERROR);
        return false;
    }
    
    // Set target temperature on hardware
    if (!m_hardware->setTargetTemperature(targetTemp)) {
        LOG_F(ERROR, "Failed to set target temperature on hardware");
        setState(CoolerState::ERROR);
        return false;
    }
    
    setState(CoolerState::COOLING);
    
    // Start temperature monitoring
    startMonitoring();
    
    return true;
}

bool TemperatureController::stopCooling() {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    
    if (m_currentState == CoolerState::OFF) {
        return true; // Already off
    }
    
    LOG_F(INFO, "Stopping cooling system");
    setState(CoolerState::STOPPING);
    
    // Stop monitoring first
    stopMonitoring();
    
    // Disable cooler on hardware
    if (m_hardware && m_hardware->isConnected()) {
        m_hardware->setCoolerEnabled(false);
    }
    
    setState(CoolerState::OFF);
    
    return true;
}

bool TemperatureController::isCoolingEnabled() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_currentState != CoolerState::OFF && m_currentState != CoolerState::ERROR;
}

bool TemperatureController::setTargetTemperature(double temperature) {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    
    if (!validateTemperature(temperature)) {
        LOG_F(ERROR, "Invalid target temperature: {:.2f}°C", temperature);
        return false;
    }
    
    if (!m_hardware || !m_hardware->isConnected()) {
        LOG_F(ERROR, "Cannot set target temperature: hardware not connected");
        return false;
    }
    
    LOG_F(INFO, "Setting target temperature to {:.2f}°C", temperature);
    
    m_targetTemperature = temperature;
    
    // Update hardware if cooling is active
    if (isCoolingEnabled()) {
        if (!m_hardware->setTargetTemperature(temperature)) {
            LOG_F(ERROR, "Failed to set target temperature on hardware");
            return false;
        }
        
        // Reset stabilization timer
        m_stabilizationStartTime = std::chrono::steady_clock::now();
        if (m_currentState == CoolerState::STABLE) {
            setState(CoolerState::COOLING);
        }
    }
    
    return true;
}

double TemperatureController::getTargetTemperature() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_targetTemperature;
}

// =========================================================================
// Temperature Monitoring
// =========================================================================

double TemperatureController::getCurrentTemperature() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_currentTemperature;
}

double TemperatureController::getCoolerPower() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_coolerPower;
}

TemperatureController::CoolerState TemperatureController::getCoolerState() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_currentState;
}

std::string TemperatureController::getStateString() const {
    switch (getCoolerState()) {
        case CoolerState::OFF: return "Off";
        case CoolerState::STARTING: return "Starting";
        case CoolerState::COOLING: return "Cooling";
        case CoolerState::STABILIZING: return "Stabilizing";
        case CoolerState::STABLE: return "Stable";
        case CoolerState::STOPPING: return "Stopping";
        case CoolerState::ERROR: return "Error";
        default: return "Unknown";
    }
}

bool TemperatureController::isTemperatureStable() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_currentState == CoolerState::STABLE;
}

double TemperatureController::getTemperatureDelta() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_currentTemperature - m_targetTemperature;
}

// =========================================================================
// Temperature History
// =========================================================================

std::vector<TemperatureController::TemperatureReading> 
TemperatureController::getTemperatureHistory() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return std::vector<TemperatureReading>(m_temperatureHistory.begin(), 
                                         m_temperatureHistory.end());
}

TemperatureController::TemperatureStatistics 
TemperatureController::getTemperatureStatistics() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    
    if (m_temperatureHistory.empty()) {
        return TemperatureStatistics{};
    }
    
    TemperatureStatistics stats;
    stats.sampleCount = m_temperatureHistory.size();
    
    double sum = 0.0;
    double powerSum = 0.0;
    stats.minTemperature = m_temperatureHistory[0].temperature;
    stats.maxTemperature = m_temperatureHistory[0].temperature;
    stats.minCoolerPower = m_temperatureHistory[0].coolerPower;
    stats.maxCoolerPower = m_temperatureHistory[0].coolerPower;
    
    for (const auto& reading : m_temperatureHistory) {
        sum += reading.temperature;
        powerSum += reading.coolerPower;
        
        stats.minTemperature = std::min(stats.minTemperature, reading.temperature);
        stats.maxTemperature = std::max(stats.maxTemperature, reading.temperature);
        stats.minCoolerPower = std::min(stats.minCoolerPower, reading.coolerPower);
        stats.maxCoolerPower = std::max(stats.maxCoolerPower, reading.coolerPower);
    }
    
    stats.averageTemperature = sum / stats.sampleCount;
    stats.averageCoolerPower = powerSum / stats.sampleCount;
    
    // Calculate standard deviation
    double varianceSum = 0.0;
    for (const auto& reading : m_temperatureHistory) {
        double diff = reading.temperature - stats.averageTemperature;
        varianceSum += diff * diff;
    }
    stats.temperatureStdDev = std::sqrt(varianceSum / stats.sampleCount);
    
    // Calculate stability (percentage of readings within tolerance)
    size_t stableReadings = 0;
    for (const auto& reading : m_temperatureHistory) {
        if (std::abs(reading.temperature - m_targetTemperature) <= m_temperatureTolerance) {
            stableReadings++;
        }
    }
    stats.stabilityPercentage = (static_cast<double>(stableReadings) / stats.sampleCount) * 100.0;
    
    return stats;
}

void TemperatureController::clearTemperatureHistory() {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_temperatureHistory.clear();
    LOG_F(INFO, "Temperature history cleared");
}

// =========================================================================
// Callbacks
// =========================================================================

void TemperatureController::setTemperatureCallback(TemperatureCallback callback) {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_temperatureCallback = callback;
}

void TemperatureController::setStateCallback(StateCallback callback) {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_stateCallback = callback;
}

void TemperatureController::setStabilityCallback(StabilityCallback callback) {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_stabilityCallback = callback;
}

// =========================================================================
// Configuration
// =========================================================================

bool TemperatureController::setTemperatureTolerance(double tolerance) {
    if (tolerance <= 0.0) {
        LOG_F(ERROR, "Invalid temperature tolerance: {:.2f}°C", tolerance);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_temperatureTolerance = tolerance;
    LOG_F(INFO, "Temperature tolerance set to {:.2f}°C", tolerance);
    return true;
}

double TemperatureController::getTemperatureTolerance() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_temperatureTolerance;
}

bool TemperatureController::setStabilizationTime(double seconds) {
    if (seconds <= 0.0) {
        LOG_F(ERROR, "Invalid stabilization time: {:.2f}s", seconds);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_stabilizationTime = seconds;
    LOG_F(INFO, "Stabilization time set to {:.2f}s", seconds);
    return true;
}

double TemperatureController::getStabilizationTime() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_stabilizationTime;
}

bool TemperatureController::setMonitoringInterval(double seconds) {
    if (seconds <= 0.0) {
        LOG_F(ERROR, "Invalid monitoring interval: {:.2f}s", seconds);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_monitoringInterval = seconds;
    LOG_F(INFO, "Temperature monitoring interval set to {:.2f}s", seconds);
    return true;
}

double TemperatureController::getMonitoringInterval() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_monitoringInterval;
}

bool TemperatureController::setMaxHistorySize(size_t maxSize) {
    if (maxSize == 0) {
        LOG_F(ERROR, "Invalid max history size: {}", maxSize);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_maxTemperatureHistory = maxSize;
    
    // Trim history if necessary
    while (m_temperatureHistory.size() > maxSize) {
        m_temperatureHistory.pop_front();
    }
    
    LOG_F(INFO, "Max temperature history size set to {}", maxSize);
    return true;
}

size_t TemperatureController::getMaxHistorySize() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_maxTemperatureHistory;
}

// =========================================================================
// Thermal Protection
// =========================================================================

bool TemperatureController::setThermalProtection(bool enabled, double maxTemp, double minTemp) {
    if (enabled && maxTemp <= minTemp) {
        LOG_F(ERROR, "Invalid thermal protection range: max={:.2f}°C, min={:.2f}°C", 
              maxTemp, minTemp);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    m_thermalProtectionEnabled = enabled;
    m_maxTemperature = maxTemp;
    m_minTemperature = minTemp;
    
    LOG_F(INFO, "Thermal protection {}: range {:.2f}°C to {:.2f}°C", 
          enabled ? "enabled" : "disabled", minTemp, maxTemp);
    return true;
}

bool TemperatureController::isThermalProtectionEnabled() const {
    std::lock_guard<std::mutex> lock(m_temperatureMutex);
    return m_thermalProtectionEnabled;
}

// =========================================================================
// Utility Methods
// =========================================================================

bool TemperatureController::waitForStability(double timeoutSec) {
    auto start = std::chrono::steady_clock::now();
    
    while (!isTemperatureStable()) {
        if (timeoutSec > 0) {
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeoutSec) {
                LOG_F(WARNING, "Temperature stability wait timeout after {:.2f}s", timeoutSec);
                return false;
            }
        }
        
        // Check for error state
        if (getCoolerState() == CoolerState::ERROR) {
            LOG_F(ERROR, "Cooler error during stability wait");
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    return true;
}

// =========================================================================
// Private Methods
// =========================================================================

void TemperatureController::setState(CoolerState newState) {
    CoolerState oldState = m_currentState;
    m_currentState = newState;
    
    LOG_F(INFO, "Cooler state changed: {} -> {}", 
          static_cast<int>(oldState), static_cast<int>(newState));
    
    // Handle state transitions
    if (newState == CoolerState::STABILIZING) {
        m_stabilizationStartTime = std::chrono::steady_clock::now();
    }
    
    // Notify state callback
    if (m_stateCallback) {
        m_stateCallback(oldState, newState);
    }
}

bool TemperatureController::validateTemperature(double temperature) const {
    if (m_thermalProtectionEnabled) {
        return temperature >= m_minTemperature && temperature <= m_maxTemperature;
    }
    return true; // No validation if thermal protection is disabled
}

void TemperatureController::startMonitoring() {
    stopMonitoring(); // Ensure any existing monitor is stopped
    
    m_isMonitoring = true;
    m_monitoringThread = std::thread([this]() {
        while (m_isMonitoring) {
            {
                std::lock_guard<std::mutex> lock(m_temperatureMutex);
                updateTemperatureReading();
                checkTemperatureStability();
                checkThermalProtection();
            }
            
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(m_monitoringInterval * 1000)));
        }
    });
}

void TemperatureController::stopMonitoring() {
    m_isMonitoring = false;
    if (m_monitoringThread.joinable()) {
        m_monitoringThread.join();
    }
}

void TemperatureController::updateTemperatureReading() {
    if (!m_hardware || !m_hardware->isConnected()) {
        return;
    }
    
    // Get current temperature and cooler power from hardware
    double newTemperature = m_hardware->getCurrentTemperature();
    double newCoolerPower = m_hardware->getCoolerPower();
    
    // Update current values
    m_currentTemperature = newTemperature;
    m_coolerPower = newCoolerPower;
    
    // Add to history
    TemperatureReading reading;
    reading.timestamp = std::chrono::steady_clock::now();
    reading.temperature = newTemperature;
    reading.coolerPower = newCoolerPower;
    reading.targetTemperature = m_targetTemperature;
    reading.state = m_currentState;
    
    m_temperatureHistory.push_back(reading);
    
    // Limit history size
    while (m_temperatureHistory.size() > m_maxTemperatureHistory) {
        m_temperatureHistory.pop_front();
    }
    
    // Notify temperature callback
    if (m_temperatureCallback) {
        m_temperatureCallback(newTemperature, newCoolerPower);
    }
}

void TemperatureController::checkTemperatureStability() {
    if (m_currentState != CoolerState::COOLING && m_currentState != CoolerState::STABILIZING) {
        return;
    }
    
    double delta = std::abs(m_currentTemperature - m_targetTemperature);
    
    if (delta <= m_temperatureTolerance) {
        if (m_currentState == CoolerState::COOLING) {
            setState(CoolerState::STABILIZING);
        } else if (m_currentState == CoolerState::STABILIZING) {
            // Check if stabilization time has elapsed
            auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_stabilizationStartTime).count();
            
            if (elapsed >= m_stabilizationTime) {
                setState(CoolerState::STABLE);
                
                // Notify stability callback
                if (m_stabilityCallback) {
                    m_stabilityCallback(true, delta);
                }
            }
        }
    } else {
        // Temperature moved out of tolerance
        if (m_currentState == CoolerState::STABILIZING || m_currentState == CoolerState::STABLE) {
            setState(CoolerState::COOLING);
            
            if (m_stabilityCallback) {
                m_stabilityCallback(false, delta);
            }
        }
    }
}

void TemperatureController::checkThermalProtection() {
    if (!m_thermalProtectionEnabled) {
        return;
    }
    
    if (m_currentTemperature > m_maxTemperature || m_currentTemperature < m_minTemperature) {
        LOG_F(ERROR, "Thermal protection triggered: temperature {:.2f}°C outside safe range [{:.2f}, {:.2f}]°C", 
              m_currentTemperature, m_minTemperature, m_maxTemperature);
        
        // Emergency stop cooling
        setState(CoolerState::ERROR);
        if (m_hardware) {
            m_hardware->setCoolerEnabled(false);
        }
    }
}

} // namespace lithium::device::ascom::camera::components
