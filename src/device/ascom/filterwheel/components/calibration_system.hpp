/*
 * calibration_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Calibration System Component

This component handles calibration, precision testing, and accuracy
optimization for the ASCOM filterwheel.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lithium::device::ascom::filterwheel::components {

// Forward declarations
class HardwareInterface;
class PositionManager;
class MonitoringSystem;

// Calibration status
enum class CalibrationStatus {
    NOT_CALIBRATED,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    EXPIRED
};

// Calibration test result
struct CalibrationTest {
    int position;
    bool success;
    std::chrono::milliseconds move_time;
    double accuracy;
    std::string error_message;
};

// Calibration result
struct CalibrationResult {
    CalibrationStatus status;
    std::chrono::system_clock::time_point timestamp;
    std::vector<CalibrationTest> tests;
    double overall_accuracy;
    std::chrono::milliseconds average_move_time;
    std::vector<std::string> issues;
    std::vector<std::string> recommendations;
    std::map<std::string, double> parameters;
};

// Position accuracy data
struct PositionAccuracy {
    int target_position;
    int actual_position;
    double error_magnitude;
    std::chrono::milliseconds settle_time;
    bool within_tolerance;
};

/**
 * @brief Calibration System for ASCOM Filter Wheels
 *
 * This component handles calibration procedures, accuracy testing,
 * and precision optimization for filterwheel operations.
 */
class CalibrationSystem {
public:
    using CalibrationCallback = std::function<void(CalibrationStatus status, double progress, const std::string& message)>;
    using TestResultCallback = std::function<void(const CalibrationTest& test)>;

    CalibrationSystem(std::shared_ptr<HardwareInterface> hardware,
                     std::shared_ptr<PositionManager> position_manager,
                     std::shared_ptr<MonitoringSystem> monitoring_system);
    ~CalibrationSystem();

    // Initialization
    auto initialize() -> bool;
    auto shutdown() -> void;

    // Calibration operations
    auto performFullCalibration() -> CalibrationResult;
    auto performQuickCalibration() -> CalibrationResult;
    auto performCustomCalibration(const std::vector<int>& positions) -> CalibrationResult;
    auto isCalibrationValid() -> bool;
    auto getCalibrationStatus() -> CalibrationStatus;
    auto getLastCalibrationResult() -> std::optional<CalibrationResult>;

    // Position testing
    auto testPosition(int position, int iterations = 3) -> std::vector<PositionAccuracy>;
    auto testAllPositions() -> std::map<int, std::vector<PositionAccuracy>>;
    auto measurePositionAccuracy(int position) -> PositionAccuracy;
    auto verifyPositionRepeatable(int position, int iterations = 5) -> bool;

    // Precision testing
    auto measureMovementPrecision() -> std::map<std::string, double>;
    auto testMovementConsistency() -> std::map<int, std::chrono::milliseconds>;
    auto analyzeBacklash() -> std::map<int, double>;
    auto measureSettlingTime() -> std::map<int, std::chrono::milliseconds>;

    // Optimization
    auto optimizeMovementParameters() -> bool;
    auto calibrateMovementTiming() -> bool;
    auto optimizePositionAccuracy() -> bool;
    auto generateOptimizationReport() -> std::string;

    // Home position calibration
    auto calibrateHomePosition() -> bool;
    auto findOptimalHomePosition() -> std::optional<int>;
    auto verifyHomePosition() -> bool;
    auto setHomePosition(int position) -> bool;

    // Advanced calibration
    auto performTemperatureCalibration() -> bool;
    auto calibrateForEnvironment() -> bool;
    auto compensateForWear() -> bool;
    auto adaptiveCalibration() -> bool;

    // Configuration
    auto setCalibrationTolerance(double tolerance) -> void;
    auto getCalibrationTolerance() -> double;
    auto setCalibrationTimeout(std::chrono::milliseconds timeout) -> void;
    auto getCalibrationTimeout() -> std::chrono::milliseconds;
    auto setMaxRetries(int retries) -> void;
    auto getMaxRetries() -> int;

    // Validation
    auto validateCalibration() -> std::pair<bool, std::string>;
    auto checkCalibrationExpiry() -> bool;
    auto extendCalibrationValidity() -> bool;
    auto scheduleRecalibration(std::chrono::hours interval) -> void;

    // Data management
    auto saveCalibrationData(const std::string& file_path = "") -> bool;
    auto loadCalibrationData(const std::string& file_path = "") -> bool;
    auto exportCalibrationReport(const std::string& file_path) -> bool;
    auto clearCalibrationData() -> void;

    // Callbacks
    auto setCalibrationCallback(CalibrationCallback callback) -> void;
    auto setTestResultCallback(TestResultCallback callback) -> void;

    // Error handling
    auto getLastError() -> std::string;
    auto clearError() -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<PositionManager> position_manager_;
    std::shared_ptr<MonitoringSystem> monitoring_system_;

    // Calibration state
    std::atomic<CalibrationStatus> calibration_status_{CalibrationStatus::NOT_CALIBRATED};
    std::optional<CalibrationResult> last_calibration_;
    std::chrono::system_clock::time_point calibration_timestamp_;

    // Configuration
    double calibration_tolerance_{0.1};
    std::chrono::milliseconds calibration_timeout_{30000};
    int max_retries_{3};
    std::chrono::hours calibration_validity_{24 * 7}; // 1 week

    // Calibration data
    std::map<int, std::vector<PositionAccuracy>> position_data_;
    std::map<std::string, double> calibration_parameters_;
    std::map<int, double> backlash_compensation_;

    // Threading and synchronization
    std::atomic<bool> calibration_in_progress_{false};
    mutable std::mutex calibration_mutex_;
    mutable std::mutex data_mutex_;

    // Callbacks
    CalibrationCallback calibration_callback_;
    TestResultCallback test_result_callback_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Internal calibration methods
    auto performBasicCalibration() -> CalibrationResult;
    auto performAdvancedCalibration() -> CalibrationResult;
    auto runCalibrationTest(int position) -> CalibrationTest;
    auto analyzeCalibrationResults(const std::vector<CalibrationTest>& tests) -> CalibrationResult;

    // Position testing implementation
    auto performPositionTest(int position, bool measure_settling = true) -> PositionAccuracy;
    auto calculatePositionError(int target, int actual) -> double;
    auto measureActualPosition(int target_position) -> int;
    auto waitForSettling(int position) -> std::chrono::milliseconds;

    // Movement analysis
    auto analyzeMovementPattern(const std::vector<CalibrationTest>& tests) -> std::map<std::string, double>;
    auto detectMovementAnomalies(const std::vector<CalibrationTest>& tests) -> std::vector<std::string>;
    auto calculateBacklash(int position) -> double;
    auto optimizeMovementPath(int from, int to) -> std::vector<int>;

    // Optimization algorithms
    auto optimizeUsingGradientDescent() -> bool;
    auto optimizeUsingGeneticAlgorithm() -> bool;
    auto optimizeUsingBayesian() -> bool;
    auto applyOptimizationResults(const std::map<std::string, double>& parameters) -> bool;

    // Data persistence
    auto calibrationToJson(const CalibrationResult& result) -> std::string;
    auto calibrationFromJson(const std::string& json) -> std::optional<CalibrationResult>;
    auto saveParametersToFile() -> bool;
    auto loadParametersFromFile() -> bool;

    // Utility methods
    auto setError(const std::string& error) -> void;
    auto notifyCalibrationProgress(CalibrationStatus status, double progress, const std::string& message) -> void;
    auto notifyTestResult(const CalibrationTest& test) -> void;
    auto isCalibrationExpired() -> bool;
    auto calculateOverallAccuracy(const std::vector<CalibrationTest>& tests) -> double;
    auto generateCalibrationId() -> std::string;
};

} // namespace lithium::device::ascom::filterwheel::components
