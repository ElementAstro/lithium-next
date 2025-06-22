/*
 * calibration_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Calibration System Component
Handles calibration procedures and self-testing

*************************************************/

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::device::asi::focuser::components {

// Forward declarations
class HardwareInterface;
class PositionManager;
class MonitoringSystem;

/**
 * @brief Calibration and self-test system for ASI Focuser
 *
 * This component handles various calibration procedures,
 * self-testing, and diagnostic operations.
 */
class CalibrationSystem {
public:
    CalibrationSystem(HardwareInterface* hardware,
                      PositionManager* positionManager,
                      MonitoringSystem* monitoringSystem);
    ~CalibrationSystem();

    // Non-copyable and non-movable
    CalibrationSystem(const CalibrationSystem&) = delete;
    CalibrationSystem& operator=(const CalibrationSystem&) = delete;
    CalibrationSystem(CalibrationSystem&&) = delete;
    CalibrationSystem& operator=(CalibrationSystem&&) = delete;

    // Calibration procedures
    bool performFullCalibration();
    bool calibrateFocuser();
    bool calibrateResolution();
    bool calibrateBacklash();
    bool calibrateTemperatureCoefficient();

    // Homing operations
    bool homeToZero();
    bool findHomePosition();
    bool setCurrentAsHome();

    // Self-test procedures
    bool performSelfTest();
    bool testBasicMovement();
    bool testPositionAccuracy();
    bool testTemperatureSensor();
    bool testBacklashCompensation();

    // Diagnostic operations
    bool runDiagnostics();
    std::vector<std::string> getDiagnosticResults() const;
    bool validateHardware();

    // Calibration results
    struct CalibrationResults {
        bool success = false;
        double stepResolution = 0.0;  // microns per step
        int backlashSteps = 0;
        double temperatureCoefficient = 0.0;
        int positionAccuracy = 0;  // steps
        std::string notes;
    };

    CalibrationResults getLastCalibrationResults() const {
        return lastResults_;
    }

    // Progress callbacks
    void setProgressCallback(
        std::function<void(int, const std::string&)> callback) {
        progressCallback_ = callback;
    }
    void setCompletionCallback(
        std::function<void(bool, const std::string&)> callback) {
        completionCallback_ = callback;
    }

    // Status
    bool isCalibrating() const { return calibrating_; }
    std::string getLastError() const { return lastError_; }

private:
    // Dependencies
    HardwareInterface* hardware_;
    PositionManager* positionManager_;
    MonitoringSystem* monitoringSystem_;

    // Calibration state
    bool calibrating_ = false;
    CalibrationResults lastResults_;
    std::vector<std::string> diagnosticResults_;
    std::string lastError_;

    // Callbacks
    std::function<void(int, const std::string&)>
        progressCallback_;  // progress %, message
    std::function<void(bool, const std::string&)>
        completionCallback_;  // success, message

    // Thread safety
    mutable std::mutex calibrationMutex_;

    // Helper methods
    void reportProgress(int percentage, const std::string& message);
    void reportCompletion(bool success, const std::string& message);
    bool moveAndVerify(int targetPosition, int tolerance = 5);
    bool waitForStable(int timeoutMs = 5000);
    void addDiagnosticResult(const std::string& result);
    void clearDiagnosticResults();

    // Calibration procedures
    bool performMovementTest(int steps, int iterations = 3);
    bool measureBacklash();
    bool measureResolution();
    bool testTemperatureResponse();

    // Validation methods
    bool validateMovementRange();
    bool validatePositionConsistency();
    bool validateTemperatureReading();
};

}  // namespace lithium::device::asi::focuser::components
