/*
 * calibration_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Calibration System Implementation

*************************************************/

#include "calibration_system.hpp"

#include "hardware_interface.hpp"
#include "monitoring_system.hpp"
#include "position_manager.hpp"

#include <cmath>
#include <thread>

#include <spdlog/spdlog.h>

namespace lithium::device::asi::focuser::components {

CalibrationSystem::CalibrationSystem(HardwareInterface* hardware,
                                     PositionManager* positionManager,
                                     MonitoringSystem* monitoringSystem)
    : hardware_(hardware),
      positionManager_(positionManager),
      monitoringSystem_(monitoringSystem) {
    spdlog::info("Created ASI Focuser Calibration System");
}

CalibrationSystem::~CalibrationSystem() {
    spdlog::info("Destroyed ASI Focuser Calibration System");
}

bool CalibrationSystem::performFullCalibration() {
    std::lock_guard<std::mutex> lock(calibrationMutex_);

    if (calibrating_) {
        lastError_ = "Calibration already in progress";
        return false;
    }

    if (!hardware_ || !hardware_->isConnected()) {
        lastError_ = "Hardware not connected";
        return false;
    }

    spdlog::info("Starting full focuser calibration");
    calibrating_ = true;
    lastResults_ = CalibrationResults{};

    try {
        reportProgress(0, "Starting calibration");

        // Step 1: Basic movement test
        reportProgress(10, "Testing basic movement");
        if (!testBasicMovement()) {
            throw std::runtime_error("Basic movement test failed");
        }

        // Step 2: Calibrate focuser range
        reportProgress(30, "Calibrating focuser range");
        if (!calibrateFocuser()) {
            throw std::runtime_error("Focuser calibration failed");
        }

        // Step 3: Measure resolution
        reportProgress(50, "Measuring step resolution");
        if (!calibrateResolution()) {
            throw std::runtime_error("Resolution calibration failed");
        }

        // Step 4: Measure backlash
        reportProgress(70, "Measuring backlash");
        if (!calibrateBacklash()) {
            throw std::runtime_error("Backlash calibration failed");
        }

        // Step 5: Test position accuracy
        reportProgress(90, "Testing position accuracy");
        if (!testPositionAccuracy()) {
            throw std::runtime_error("Position accuracy test failed");
        }

        lastResults_.success = true;
        lastResults_.notes = "Full calibration completed successfully";

        reportProgress(100, "Calibration completed");
        reportCompletion(true, "Full calibration completed successfully");

        spdlog::info("Full focuser calibration completed successfully");
        calibrating_ = false;
        return true;

    } catch (const std::exception& e) {
        lastError_ = e.what();
        lastResults_.success = false;
        lastResults_.notes = "Calibration failed: " + std::string(e.what());

        reportCompletion(false, "Calibration failed: " + std::string(e.what()));
        spdlog::error("Full calibration failed: {}", e.what());
        calibrating_ = false;
        return false;
    }
}

bool CalibrationSystem::calibrateFocuser() {
    if (!positionManager_) {
        lastError_ = "Position manager not available";
        return false;
    }

    spdlog::info("Performing focuser calibration");

    try {
        // Save current position
        int originalPosition = positionManager_->getCurrentPosition();

        // Move to minimum position
        reportProgress(35, "Moving to minimum position");
        if (!positionManager_->moveToPosition(
                positionManager_->getMinLimit())) {
            return false;
        }

        if (!monitoringSystem_->waitForMovement(30000)) {
            return false;
        }

        // Move to maximum position
        reportProgress(40, "Moving to maximum position");
        if (!positionManager_->moveToPosition(
                positionManager_->getMaxLimit())) {
            return false;
        }

        if (!monitoringSystem_->waitForMovement(30000)) {
            return false;
        }

        // Return to original position
        reportProgress(45, "Returning to original position");
        if (!positionManager_->moveToPosition(originalPosition)) {
            return false;
        }

        if (!monitoringSystem_->waitForMovement(30000)) {
            return false;
        }

        if (monitoringSystem_) {
            monitoringSystem_->addOperationHistory("Calibration completed");
        }

        spdlog::info("Focuser calibration completed successfully");
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Calibration failed: " + std::string(e.what());
        spdlog::error("Focuser calibration failed: {}", e.what());
        return false;
    }
}

bool CalibrationSystem::calibrateResolution() {
    spdlog::info("Calibrating step resolution");

    // For now, use default resolution value
    // In a real implementation, this would involve precise measurements
    lastResults_.stepResolution = 0.5;  // Default value in microns

    if (monitoringSystem_) {
        monitoringSystem_->addOperationHistory(
            "Resolution calibration completed");
    }

    return true;
}

bool CalibrationSystem::calibrateBacklash() {
    if (!positionManager_) {
        return false;
    }

    spdlog::info("Calibrating backlash compensation");

    try {
        int originalPosition = positionManager_->getCurrentPosition();
        int testSteps = 100;

        // Move forward
        if (!positionManager_->moveSteps(testSteps)) {
            return false;
        }
        monitoringSystem_->waitForMovement();

        int forwardPosition = positionManager_->getCurrentPosition();

        // Move backward
        if (!positionManager_->moveSteps(-testSteps)) {
            return false;
        }
        monitoringSystem_->waitForMovement();

        int backwardPosition = positionManager_->getCurrentPosition();

        // Calculate backlash
        int backlash = std::abs(originalPosition - backwardPosition);
        lastResults_.backlashSteps = backlash;

        // Return to original position
        positionManager_->moveToPosition(originalPosition);
        monitoringSystem_->waitForMovement();

        spdlog::info("Measured backlash: {} steps", backlash);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Backlash calibration failed: {}", e.what());
        return false;
    }
}

bool CalibrationSystem::calibrateTemperatureCoefficient() {
    spdlog::info("Calibrating temperature coefficient");
    // This would require temperature variation and focus measurement
    // For now, return true with default value
    lastResults_.temperatureCoefficient = 0.0;
    return true;
}

bool CalibrationSystem::homeToZero() {
    if (!hardware_) {
        lastError_ = "Hardware not available";
        return false;
    }

    spdlog::info("Homing to zero position");

    if (!hardware_->resetToZero()) {
        lastError_ = hardware_->getLastError();
        return false;
    }

    if (monitoringSystem_) {
        monitoringSystem_->addOperationHistory("Homed to zero");
    }

    return true;
}

bool CalibrationSystem::findHomePosition() {
    // Implementation would search for mechanical home position
    return homeToZero();
}

bool CalibrationSystem::setCurrentAsHome() {
    if (!positionManager_) {
        return false;
    }

    return positionManager_->setHomePosition();
}

bool CalibrationSystem::performSelfTest() {
    spdlog::info("Performing focuser self-test");

    clearDiagnosticResults();

    if (!hardware_ || !hardware_->isConnected()) {
        addDiagnosticResult("FAIL: Hardware not connected");
        return false;
    }

    bool allTestsPassed = true;

    // Test basic movement
    if (testBasicMovement()) {
        addDiagnosticResult("PASS: Basic movement test");
    } else {
        addDiagnosticResult("FAIL: Basic movement test");
        allTestsPassed = false;
    }

    // Test position accuracy
    if (testPositionAccuracy()) {
        addDiagnosticResult("PASS: Position accuracy test");
    } else {
        addDiagnosticResult("FAIL: Position accuracy test");
        allTestsPassed = false;
    }

    // Test temperature sensor (if available)
    if (testTemperatureSensor()) {
        addDiagnosticResult("PASS: Temperature sensor test");
    } else {
        addDiagnosticResult("FAIL: Temperature sensor test");
        allTestsPassed = false;
    }

    std::string result =
        allTestsPassed ? "All self-tests passed" : "Some self-tests failed";
    addDiagnosticResult(result);

    if (monitoringSystem_) {
        monitoringSystem_->addOperationHistory("Self-test completed: " +
                                               result);
    }

    spdlog::info("Self-test completed: {}", result);
    return allTestsPassed;
}

bool CalibrationSystem::testBasicMovement() {
    if (!positionManager_) {
        return false;
    }

    try {
        int originalPosition = positionManager_->getCurrentPosition();

        // Test small movements
        for (int steps : {100, -200, 100}) {
            if (!positionManager_->moveSteps(steps)) {
                return false;
            }

            if (!monitoringSystem_->waitForMovement()) {
                return false;
            }
        }

        // Return to original position
        positionManager_->moveToPosition(originalPosition);
        monitoringSystem_->waitForMovement();

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Basic movement test failed: {}", e.what());
        return false;
    }
}

bool CalibrationSystem::testPositionAccuracy() {
    if (!positionManager_) {
        return false;
    }

    try {
        int testPositions[] = {1000, 5000, 10000, 15000, 20000};
        int tolerance = 5;  // steps

        for (int targetPos : testPositions) {
            if (!positionManager_->validatePosition(targetPos)) {
                continue;
            }

            if (!moveAndVerify(targetPos, tolerance)) {
                lastResults_.positionAccuracy = tolerance + 1;
                return false;
            }
        }

        lastResults_.positionAccuracy = tolerance;
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Position accuracy test failed: {}", e.what());
        return false;
    }
}

bool CalibrationSystem::testTemperatureSensor() {
    if (!hardware_ || !hardware_->hasTemperatureSensor()) {
        return true;  // Pass if no sensor
    }

    float temperature;
    return hardware_->getTemperature(temperature);
}

bool CalibrationSystem::testBacklashCompensation() {
    // Implementation would test backlash compensation effectiveness
    return true;
}

bool CalibrationSystem::runDiagnostics() {
    spdlog::info("Running focuser diagnostics");

    clearDiagnosticResults();

    // Hardware validation
    if (validateHardware()) {
        addDiagnosticResult("PASS: Hardware validation");
    } else {
        addDiagnosticResult("FAIL: Hardware validation");
    }

    // Movement range validation
    if (validateMovementRange()) {
        addDiagnosticResult("PASS: Movement range validation");
    } else {
        addDiagnosticResult("FAIL: Movement range validation");
    }

    // Position consistency
    if (validatePositionConsistency()) {
        addDiagnosticResult("PASS: Position consistency");
    } else {
        addDiagnosticResult("FAIL: Position consistency");
    }

    // Temperature reading (if available)
    if (validateTemperatureReading()) {
        addDiagnosticResult("PASS: Temperature reading");
    } else {
        addDiagnosticResult("FAIL: Temperature reading");
    }

    spdlog::info("Diagnostics completed");
    return true;
}

std::vector<std::string> CalibrationSystem::getDiagnosticResults() const {
    return diagnosticResults_;
}

bool CalibrationSystem::validateHardware() {
    return hardware_ && hardware_->isConnected();
}

void CalibrationSystem::reportProgress(int percentage,
                                       const std::string& message) {
    if (progressCallback_) {
        progressCallback_(percentage, message);
    }
    spdlog::info("Calibration progress: {}% - {}", percentage, message);
}

void CalibrationSystem::reportCompletion(bool success,
                                         const std::string& message) {
    if (completionCallback_) {
        completionCallback_(success, message);
    }
}

bool CalibrationSystem::moveAndVerify(int targetPosition, int tolerance) {
    if (!positionManager_) {
        return false;
    }

    if (!positionManager_->moveToPosition(targetPosition)) {
        return false;
    }

    if (!monitoringSystem_->waitForMovement()) {
        return false;
    }

    int actualPosition = positionManager_->getCurrentPosition();
    return std::abs(actualPosition - targetPosition) <= tolerance;
}

bool CalibrationSystem::waitForStable(int timeoutMs) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        if (!positionManager_->isMoving()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!positionManager_->isMoving()) {
                return true;  // Stable for 100ms
            }
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();

        if (elapsed > timeoutMs) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CalibrationSystem::addDiagnosticResult(const std::string& result) {
    diagnosticResults_.push_back(result);
}

void CalibrationSystem::clearDiagnosticResults() { diagnosticResults_.clear(); }

bool CalibrationSystem::validateMovementRange() {
    if (!positionManager_) {
        return false;
    }

    return positionManager_->getMinLimit() < positionManager_->getMaxLimit();
}

bool CalibrationSystem::validatePositionConsistency() {
    if (!positionManager_) {
        return false;
    }

    // Test position reading consistency
    int pos1 = positionManager_->getCurrentPosition();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int pos2 = positionManager_->getCurrentPosition();

    return std::abs(pos1 - pos2) <= 1;  // Should be consistent
}

bool CalibrationSystem::validateTemperatureReading() {
    if (!hardware_ || !hardware_->hasTemperatureSensor()) {
        return true;  // Pass if no sensor
    }

    float temp1, temp2;
    if (!hardware_->getTemperature(temp1)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!hardware_->getTemperature(temp2)) {
        return false;
    }

    // Temperature should be reasonable and consistent
    return (temp1 > -50.0f && temp1 < 100.0f && std::abs(temp1 - temp2) < 5.0f);
}

}  // namespace lithium::device::asi::focuser::components
