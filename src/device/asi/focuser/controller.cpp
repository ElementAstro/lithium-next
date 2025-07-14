/*
 * asi_focuser_controller_v2.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASI Focuser Controller Implementation

*************************************************/

#include "controller.hpp"

// Component includes
#include "./components/calibration_system.hpp"
#include "./components/configuration_manager.hpp"
#include "./components/hardware_interface.hpp"
#include "./components/monitoring_system.hpp"
#include "./components/position_manager.hpp"
#include "./components/temperature_system.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::asi::focuser::controller {

ASIFocuserControllerV2::ASIFocuserControllerV2(ASIFocuser* parent)
    : parent_(parent) {
    spdlog::info("Created Modular ASI Focuser Controller");
}

ASIFocuserControllerV2::~ASIFocuserControllerV2() {
    destroy();
    spdlog::info("Destroyed Modular ASI Focuser Controller");
}

bool ASIFocuserControllerV2::initialize() {
    spdlog::info("Initializing Modular ASI Focuser Controller");

    if (initialized_) {
        return true;
    }

    try {
        // Create hardware interface first
        hardware_ = std::make_unique<components::HardwareInterface>();
        if (!hardware_->initialize()) {
            lastError_ = "Failed to initialize hardware interface";
            return false;
        }

        // Create position manager
        positionManager_ =
            std::make_unique<components::PositionManager>(hardware_.get());

        // Create temperature system
        temperatureSystem_ = std::make_unique<components::TemperatureSystem>(
            hardware_.get(), positionManager_.get());

        // Create configuration manager
        configManager_ = std::make_unique<components::ConfigurationManager>(
            hardware_.get(), positionManager_.get(), temperatureSystem_.get());

        // Create monitoring system
        monitoringSystem_ = std::make_unique<components::MonitoringSystem>(
            hardware_.get(), positionManager_.get(), temperatureSystem_.get());

        // Create calibration system
        calibrationSystem_ = std::make_unique<components::CalibrationSystem>(
            hardware_.get(), positionManager_.get(), monitoringSystem_.get());

        // Setup callbacks between components
        setupComponentCallbacks();

        initialized_ = true;
        spdlog::info("Modular ASI Focuser Controller initialized successfully");
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Initialization failed: " + std::string(e.what());
        spdlog::error("Controller initialization failed: {}", e.what());
        return false;
    }
}

bool ASIFocuserControllerV2::destroy() {
    spdlog::info("Destroying Modular ASI Focuser Controller");

    if (isConnected()) {
        disconnect();
    }

    // Destroy components in reverse order
    calibrationSystem_.reset();
    monitoringSystem_.reset();
    configManager_.reset();
    temperatureSystem_.reset();
    positionManager_.reset();

    if (hardware_) {
        hardware_->destroy();
        hardware_.reset();
    }

    initialized_ = false;
    return true;
}

bool ASIFocuserControllerV2::connect(const std::string& deviceName, int timeout,
                                     int maxRetry) {
    if (!initialized_ || !hardware_) {
        lastError_ = "Controller not initialized";
        return false;
    }

    spdlog::info("Connecting to ASI Focuser: {}", deviceName);

    if (!hardware_->connect(deviceName, timeout, maxRetry)) {
        lastError_ = hardware_->getLastError();
        return false;
    }

    // Start monitoring if hardware is connected
    if (monitoringSystem_) {
        monitoringSystem_->startMonitoring();
    }

    spdlog::info("Successfully connected to ASI Focuser");
    return true;
}

bool ASIFocuserControllerV2::disconnect() {
    if (!hardware_) {
        return true;
    }

    spdlog::info("Disconnecting ASI Focuser");

    // Stop monitoring first
    if (monitoringSystem_) {
        monitoringSystem_->stopMonitoring();
    }

    bool result = hardware_->disconnect();
    if (!result) {
        lastError_ = hardware_->getLastError();
    }

    return result;
}

bool ASIFocuserControllerV2::scan(std::vector<std::string>& devices) {
    if (!hardware_) {
        lastError_ = "Hardware interface not available";
        return false;
    }

    return hardware_->scan(devices);
}

// Position control methods
bool ASIFocuserControllerV2::moveToPosition(int position) {
    if (!positionManager_) {
        lastError_ = "Position manager not available";
        return false;
    }

    bool result = positionManager_->moveToPosition(position);
    if (!result) {
        lastError_ = positionManager_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::moveSteps(int steps) {
    if (!positionManager_) {
        lastError_ = "Position manager not available";
        return false;
    }

    bool result = positionManager_->moveSteps(steps);
    if (!result) {
        lastError_ = positionManager_->getLastError();
    }
    return result;
}

int ASIFocuserControllerV2::getPosition() {
    if (!positionManager_) {
        return -1;
    }

    return positionManager_->getCurrentPosition();
}

bool ASIFocuserControllerV2::syncPosition(int position) {
    if (!positionManager_) {
        lastError_ = "Position manager not available";
        return false;
    }

    bool result = positionManager_->syncPosition(position);
    if (!result) {
        lastError_ = positionManager_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::isMoving() const {
    if (!positionManager_) {
        return false;
    }

    return positionManager_->isMoving();
}

bool ASIFocuserControllerV2::abortMove() {
    if (!positionManager_) {
        lastError_ = "Position manager not available";
        return false;
    }

    bool result = positionManager_->abortMove();
    if (!result) {
        lastError_ = positionManager_->getLastError();
    }
    return result;
}

// Position limits
int ASIFocuserControllerV2::getMaxPosition() const {
    return positionManager_ ? positionManager_->getMaxLimit() : 30000;
}

int ASIFocuserControllerV2::getMinPosition() const {
    return positionManager_ ? positionManager_->getMinLimit() : 0;
}

bool ASIFocuserControllerV2::setMaxLimit(int limit) {
    return positionManager_ ? positionManager_->setMaxLimit(limit) : false;
}

bool ASIFocuserControllerV2::setMinLimit(int limit) {
    return positionManager_ ? positionManager_->setMinLimit(limit) : false;
}

// Speed control
bool ASIFocuserControllerV2::setSpeed(double speed) {
    return positionManager_ ? positionManager_->setSpeed(speed) : false;
}

double ASIFocuserControllerV2::getSpeed() const {
    return positionManager_ ? positionManager_->getSpeed() : 0.0;
}

int ASIFocuserControllerV2::getMaxSpeed() const {
    return positionManager_ ? positionManager_->getMaxSpeed() : 500;
}

std::pair<int, int> ASIFocuserControllerV2::getSpeedRange() const {
    return positionManager_ ? positionManager_->getSpeedRange()
                            : std::make_pair(1, 500);
}

// Direction control
bool ASIFocuserControllerV2::setDirection(bool inward) {
    return positionManager_ ? positionManager_->setDirection(inward) : false;
}

bool ASIFocuserControllerV2::isDirectionReversed() const {
    return positionManager_ ? positionManager_->isDirectionReversed() : false;
}

// Home operations
bool ASIFocuserControllerV2::homeToZero() {
    return calibrationSystem_ ? calibrationSystem_->homeToZero() : false;
}

bool ASIFocuserControllerV2::setHomePosition() {
    return positionManager_ ? positionManager_->setHomePosition() : false;
}

bool ASIFocuserControllerV2::goToHome() {
    return positionManager_ ? positionManager_->goToHome() : false;
}

// Temperature operations
std::optional<double> ASIFocuserControllerV2::getTemperature() const {
    return temperatureSystem_ ? temperatureSystem_->getCurrentTemperature()
                              : std::nullopt;
}

bool ASIFocuserControllerV2::hasTemperatureSensor() const {
    return temperatureSystem_ ? temperatureSystem_->hasTemperatureSensor()
                              : false;
}

bool ASIFocuserControllerV2::setTemperatureCoefficient(double coefficient) {
    return temperatureSystem_
               ? temperatureSystem_->setTemperatureCoefficient(coefficient)
               : false;
}

double ASIFocuserControllerV2::getTemperatureCoefficient() const {
    return temperatureSystem_ ? temperatureSystem_->getTemperatureCoefficient()
                              : 0.0;
}

bool ASIFocuserControllerV2::enableTemperatureCompensation(bool enable) {
    return temperatureSystem_
               ? temperatureSystem_->enableTemperatureCompensation(enable)
               : false;
}

bool ASIFocuserControllerV2::isTemperatureCompensationEnabled() const {
    return temperatureSystem_
               ? temperatureSystem_->isTemperatureCompensationEnabled()
               : false;
}

// Configuration operations
bool ASIFocuserControllerV2::saveConfiguration(const std::string& filename) {
    if (!configManager_) {
        lastError_ = "Configuration manager not available";
        return false;
    }

    bool result = configManager_->saveConfiguration(filename);
    if (!result) {
        lastError_ = configManager_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::loadConfiguration(const std::string& filename) {
    if (!configManager_) {
        lastError_ = "Configuration manager not available";
        return false;
    }

    bool result = configManager_->loadConfiguration(filename);
    if (!result) {
        lastError_ = configManager_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::enableBeep(bool enable) {
    // Use hardware interface directly for immediate effect
    if (hardware_) {
        bool result = hardware_->setBeep(enable);
        if (!result) {
            lastError_ = hardware_->getLastError();
            return false;
        }
    }

    // Also update configuration manager if available
    if (configManager_) {
        configManager_->enableBeep(enable);
    }

    return true;
}

bool ASIFocuserControllerV2::isBeepEnabled() const {
    // Get current state from hardware interface
    if (hardware_) {
        bool enabled = false;
        if (hardware_->getBeep(enabled)) {
            return enabled;
        }
    }

    // Fallback to configuration manager
    return configManager_ ? configManager_->isBeepEnabled() : false;
}

bool ASIFocuserControllerV2::enableHighResolutionMode(bool enable) {
    return configManager_ ? configManager_->enableHighResolutionMode(enable)
                          : false;
}

bool ASIFocuserControllerV2::isHighResolutionMode() const {
    return configManager_ ? configManager_->isHighResolutionMode() : false;
}

double ASIFocuserControllerV2::getResolution() const {
    return configManager_ ? configManager_->getResolution() : 0.5;
}

bool ASIFocuserControllerV2::setBacklash(int backlash) {
    return configManager_ ? configManager_->setBacklashSteps(backlash) : false;
}

int ASIFocuserControllerV2::getBacklash() const {
    return configManager_ ? configManager_->getBacklashSteps() : 0;
}

bool ASIFocuserControllerV2::enableBacklashCompensation(bool enable) {
    return configManager_ ? configManager_->enableBacklashCompensation(enable)
                          : false;
}

bool ASIFocuserControllerV2::isBacklashCompensationEnabled() const {
    return configManager_ ? configManager_->isBacklashCompensationEnabled()
                          : false;
}

// Monitoring operations
bool ASIFocuserControllerV2::startMonitoring() {
    return monitoringSystem_ ? monitoringSystem_->startMonitoring() : false;
}

bool ASIFocuserControllerV2::stopMonitoring() {
    return monitoringSystem_ ? monitoringSystem_->stopMonitoring() : false;
}

bool ASIFocuserControllerV2::isMonitoring() const {
    return monitoringSystem_ ? monitoringSystem_->isMonitoring() : false;
}

std::vector<std::string> ASIFocuserControllerV2::getOperationHistory() const {
    return monitoringSystem_ ? monitoringSystem_->getOperationHistory()
                             : std::vector<std::string>{};
}

bool ASIFocuserControllerV2::waitForMovement(int timeoutMs) {
    return monitoringSystem_ ? monitoringSystem_->waitForMovement(timeoutMs)
                             : false;
}

// Calibration operations
bool ASIFocuserControllerV2::performSelfTest() {
    return calibrationSystem_ ? calibrationSystem_->performSelfTest() : false;
}

bool ASIFocuserControllerV2::calibrateFocuser() {
    return calibrationSystem_ ? calibrationSystem_->calibrateFocuser() : false;
}

bool ASIFocuserControllerV2::performFullCalibration() {
    return calibrationSystem_ ? calibrationSystem_->performFullCalibration()
                              : false;
}

std::vector<std::string> ASIFocuserControllerV2::getDiagnosticResults() const {
    return calibrationSystem_ ? calibrationSystem_->getDiagnosticResults()
                              : std::vector<std::string>{};
}

// Hardware information
std::string ASIFocuserControllerV2::getFirmwareVersion() const {
    return hardware_ ? hardware_->getFirmwareVersion() : "Unknown";
}

std::string ASIFocuserControllerV2::getModelName() const {
    return hardware_ ? hardware_->getModelName() : "Unknown";
}

// Enhanced hardware control methods
std::string ASIFocuserControllerV2::getSerialNumber() const {
    if (!hardware_) {
        return "Unknown";
    }

    std::string serialNumber;
    if (hardware_->getSerialNumber(serialNumber)) {
        return serialNumber;
    }
    return "Unknown";
}

bool ASIFocuserControllerV2::setDeviceAlias(const std::string& alias) {
    if (!hardware_) {
        lastError_ = "Hardware interface not available";
        return false;
    }

    bool result = hardware_->setDeviceAlias(alias);
    if (!result) {
        lastError_ = hardware_->getLastError();
    }
    return result;
}

std::string ASIFocuserControllerV2::getSDKVersion() {
    return components::HardwareInterface::getSDKVersion();
}

bool ASIFocuserControllerV2::resetPosition(int position) {
    if (!hardware_) {
        lastError_ = "Hardware interface not available";
        return false;
    }

    bool result;
    if (position == 0) {
        result = hardware_->resetToZero();
    } else {
        result = hardware_->resetPosition(position);
    }

    if (!result) {
        lastError_ = hardware_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::setBeep(bool enable) {
    if (!hardware_) {
        lastError_ = "Hardware interface not available";
        return false;
    }

    bool result = hardware_->setBeep(enable);
    if (!result) {
        lastError_ = hardware_->getLastError();
    }
    return result;
}

bool ASIFocuserControllerV2::getBeep() const {
    if (!hardware_) {
        return false;
    }

    bool enabled = false;
    hardware_->getBeep(enabled);
    return enabled;
}

bool ASIFocuserControllerV2::setMaxStep(int maxStep) {
    if (!hardware_) {
        lastError_ = "Hardware interface not available";
        return false;
    }

    bool result = hardware_->setMaxStep(maxStep);
    if (!result) {
        lastError_ = hardware_->getLastError();
    }
    return result;
}

int ASIFocuserControllerV2::getMaxStep() const {
    if (!hardware_) {
        return 0;
    }

    int maxStep = 0;
    return hardware_->getMaxStep(maxStep) ? maxStep : 0;
}

int ASIFocuserControllerV2::getStepRange() const {
    if (!hardware_) {
        return 0;
    }

    int range = 0;
    return hardware_->getStepRange(range) ? range : 0;
}

// Statistics
uint32_t ASIFocuserControllerV2::getMovementCount() const {
    return positionManager_ ? positionManager_->getMovementCount() : 0;
}

uint64_t ASIFocuserControllerV2::getTotalSteps() const {
    return positionManager_ ? positionManager_->getTotalSteps() : 0;
}

int ASIFocuserControllerV2::getLastMoveSteps() const {
    return positionManager_ ? positionManager_->getLastMoveSteps() : 0;
}

int ASIFocuserControllerV2::getLastMoveDuration() const {
    // This would typically be tracked by position manager or monitoring system
    if (positionManager_) {
        // Return duration from position manager if available
        return 0; // Placeholder - would need to be implemented in position manager
    }
    return 0;
}

// Connection state
bool ASIFocuserControllerV2::isInitialized() const { return initialized_; }

bool ASIFocuserControllerV2::isConnected() const {
    return hardware_ ? hardware_->isConnected() : false;
}

std::string ASIFocuserControllerV2::getLastError() const {
    // Aggregate errors from all components
    if (!lastError_.empty()) {
        return lastError_;
    }

    if (hardware_ && !hardware_->getLastError().empty()) {
        return hardware_->getLastError();
    }

    if (positionManager_ && !positionManager_->getLastError().empty()) {
        return positionManager_->getLastError();
    }

    if (configManager_ && !configManager_->getLastError().empty()) {
        return configManager_->getLastError();
    }

    if (calibrationSystem_ && !calibrationSystem_->getLastError().empty()) {
        return calibrationSystem_->getLastError();
    }

    return "";
}

// Callbacks
void ASIFocuserControllerV2::setPositionCallback(
    std::function<void(int)> callback) {
    if (positionManager_) {
        positionManager_->setPositionCallback(callback);
    }
}

void ASIFocuserControllerV2::setTemperatureCallback(
    std::function<void(double)> callback) {
    if (temperatureSystem_) {
        temperatureSystem_->setTemperatureCallback(callback);
    }
}

void ASIFocuserControllerV2::setMoveCompleteCallback(
    std::function<void(bool)> callback) {
    if (positionManager_) {
        positionManager_->setMoveCompleteCallback(callback);
    }
}

void ASIFocuserControllerV2::setupComponentCallbacks() {
    // This method can be used to setup inter-component communication
    // For example, temperature system can notify position manager of
    // compensation events

    if (temperatureSystem_ && monitoringSystem_) {
        temperatureSystem_->setCompensationCallback(
            [this](int steps, double delta) {
                if (monitoringSystem_) {
                    monitoringSystem_->addOperationHistory(
                        "Temperature compensation: " + std::to_string(steps) +
                        " steps for " + std::to_string(delta) + "°C change");
                }
            });
    }
}

void ASIFocuserControllerV2::updateLastError() {
    // This method is called to aggregate errors from components
    // Implementation can be expanded as needed
}

}  // namespace lithium::device::asi::focuser::controller
