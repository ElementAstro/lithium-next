/*
 * temperature_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Temperature Controller Component

This component handles temperature monitoring and compensation for
ASCOM focuser devices, providing temperature readings and automatic
focus adjustment based on temperature changes.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "device/template/focuser.hpp"

namespace lithium::device::ascom::focuser::components {

// Forward declarations
class HardwareInterface;
class MovementController;

/**
 * @brief Temperature Controller for ASCOM Focuser
 *
 * This component manages temperature monitoring and compensation:
 * - Temperature sensor reading
 * - Temperature compensation calculation
 * - Automatic focus adjustment based on temperature changes
 * - Temperature history tracking
 * - Compensation algorithm configuration
 */
class TemperatureController {
public:
    // Temperature compensation algorithms
    enum class CompensationAlgorithm {
        LINEAR,           // Simple linear compensation
        POLYNOMIAL,       // Polynomial curve fitting
        LOOKUP_TABLE,     // Predefined lookup table
        ADAPTIVE         // Adaptive learning algorithm
    };

    // Temperature compensation configuration
    struct CompensationConfig {
        bool enabled = false;
        CompensationAlgorithm algorithm = CompensationAlgorithm::LINEAR;
        double coefficient = 0.0;              // Steps per degree C
        double deadband = 0.1;                 // Minimum temperature change to trigger compensation
        int minCompensationSteps = 1;          // Minimum steps to move for compensation
        int maxCompensationSteps = 1000;       // Maximum steps to move for compensation
        std::chrono::seconds updateInterval{30}; // Temperature monitoring interval
        bool requireManualCalibration = false;
    };

    // Temperature history entry
    struct TemperatureReading {
        std::chrono::steady_clock::time_point timestamp;
        double temperature;
        int focuserPosition;
        bool compensationApplied;
        int compensationSteps;
    };

    // Temperature statistics
    struct TemperatureStats {
        double currentTemperature = 0.0;
        double minTemperature = 0.0;
        double maxTemperature = 0.0;
        double averageTemperature = 0.0;
        double temperatureRange = 0.0;
        int totalCompensations = 0;
        int totalCompensationSteps = 0;
        std::chrono::steady_clock::time_point lastUpdateTime;
        std::chrono::steady_clock::time_point lastCompensationTime;
    };

    // Constructor and destructor
    explicit TemperatureController(std::shared_ptr<HardwareInterface> hardware,
                                  std::shared_ptr<MovementController> movement);
    ~TemperatureController();

    // Non-copyable and non-movable
    TemperatureController(const TemperatureController&) = delete;
    TemperatureController& operator=(const TemperatureController&) = delete;
    TemperatureController(TemperatureController&&) = delete;
    TemperatureController& operator=(TemperatureController&&) = delete;

    // =========================================================================
    // Initialization and Configuration
    // =========================================================================

    /**
     * @brief Initialize the temperature controller
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the temperature controller
     */
    auto destroy() -> bool;

    /**
     * @brief Set compensation configuration
     */
    auto setCompensationConfig(const CompensationConfig& config) -> void;

    /**
     * @brief Get compensation configuration
     */
    auto getCompensationConfig() const -> CompensationConfig;

    // =========================================================================
    // Temperature Monitoring
    // =========================================================================

    /**
     * @brief Check if temperature sensor is available
     */
    auto hasTemperatureSensor() -> bool;

    /**
     * @brief Get current external temperature
     */
    auto getExternalTemperature() -> std::optional<double>;

    /**
     * @brief Get current chip temperature
     */
    auto getChipTemperature() -> std::optional<double>;

    /**
     * @brief Get temperature statistics
     */
    auto getTemperatureStats() -> TemperatureStats;

    /**
     * @brief Reset temperature statistics
     */
    auto resetTemperatureStats() -> void;

    /**
     * @brief Start temperature monitoring
     */
    auto startMonitoring() -> bool;

    /**
     * @brief Stop temperature monitoring
     */
    auto stopMonitoring() -> bool;

    /**
     * @brief Check if monitoring is active
     */
    auto isMonitoring() -> bool;

    // =========================================================================
    // Temperature Compensation
    // =========================================================================

    /**
     * @brief Get temperature compensation settings
     */
    auto getTemperatureCompensation() -> TemperatureCompensation;

    /**
     * @brief Set temperature compensation settings
     */
    auto setTemperatureCompensation(const TemperatureCompensation& compensation) -> bool;

    /**
     * @brief Enable/disable temperature compensation
     */
    auto enableTemperatureCompensation(bool enable) -> bool;

    /**
     * @brief Check if temperature compensation is enabled
     */
    auto isTemperatureCompensationEnabled() -> bool;

    /**
     * @brief Calibrate temperature compensation
     */
    auto calibrateCompensation(double temperatureChange, int focusChange) -> bool;

    /**
     * @brief Apply temperature compensation manually
     */
    auto applyCompensation(double temperatureChange) -> bool;

    /**
     * @brief Get suggested compensation steps for temperature change
     */
    auto calculateCompensationSteps(double temperatureChange) -> int;

    // =========================================================================
    // Temperature History
    // =========================================================================

    /**
     * @brief Get temperature history
     */
    auto getTemperatureHistory() -> std::vector<TemperatureReading>;

    /**
     * @brief Get temperature history for specified duration
     */
    auto getTemperatureHistory(std::chrono::seconds duration) -> std::vector<TemperatureReading>;

    /**
     * @brief Clear temperature history
     */
    auto clearTemperatureHistory() -> void;

    /**
     * @brief Get temperature trend (degrees per minute)
     */
    auto getTemperatureTrend() -> double;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using TemperatureCallback = std::function<void(double temperature)>;
    using CompensationCallback = std::function<void(double tempChange, int steps, bool success)>;
    using TemperatureAlertCallback = std::function<void(double temperature, const std::string& message)>;

    /**
     * @brief Set temperature change callback
     */
    auto setTemperatureCallback(TemperatureCallback callback) -> void;

    /**
     * @brief Set compensation callback
     */
    auto setCompensationCallback(CompensationCallback callback) -> void;

    /**
     * @brief Set temperature alert callback
     */
    auto setTemperatureAlertCallback(TemperatureAlertCallback callback) -> void;

    // =========================================================================
    // Advanced Features
    // =========================================================================

    /**
     * @brief Set temperature compensation coefficient
     */
    auto setCompensationCoefficient(double coefficient) -> bool;

    /**
     * @brief Get temperature compensation coefficient
     */
    auto getCompensationCoefficient() -> double;

    /**
     * @brief Auto-calibrate temperature compensation
     */
    auto autoCalibrate(std::chrono::seconds duration) -> bool;

    /**
     * @brief Save compensation settings to file
     */
    auto saveCompensationSettings(const std::string& filename) -> bool;

    /**
     * @brief Load compensation settings from file
     */
    auto loadCompensationSettings(const std::string& filename) -> bool;

private:
    // Component references
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<MovementController> movement_;
    
    // Configuration
    CompensationConfig config_;
    TemperatureCompensation compensation_;
    
    // Temperature monitoring
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    std::atomic<double> current_temperature_{0.0};
    std::atomic<double> last_compensation_temperature_{0.0};
    
    // Temperature history
    std::vector<TemperatureReading> temperature_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    // Statistics
    TemperatureStats stats_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex history_mutex_;
    mutable std::mutex config_mutex_;
    
    // Callbacks
    TemperatureCallback temperature_callback_;
    CompensationCallback compensation_callback_;
    TemperatureAlertCallback temperature_alert_callback_;
    
    // Compensation algorithms
    auto calculateLinearCompensation(double tempChange) -> int;
    auto calculatePolynomialCompensation(double tempChange) -> int;
    auto calculateLookupTableCompensation(double tempChange) -> int;
    auto calculateAdaptiveCompensation(double tempChange) -> int;
    
    // Private methods
    auto monitorTemperature() -> void;
    auto updateTemperatureReading(double temperature) -> void;
    auto addTemperatureReading(double temperature, int position, bool compensated, int steps) -> void;
    auto updateTemperatureStats(double temperature) -> void;
    auto checkTemperatureCompensation() -> void;
    auto applyTemperatureCompensation(double tempChange) -> bool;
    auto validateCompensationSteps(int steps) -> int;
    
    // Notification methods
    auto notifyTemperatureChange(double temperature) -> void;
    auto notifyCompensationApplied(double tempChange, int steps, bool success) -> void;
    auto notifyTemperatureAlert(double temperature, const std::string& message) -> void;
    
    // Calibration helpers
    auto recordCalibrationPoint(double temperature, int position) -> void;
    auto calculateBestFitCoefficient() -> double;
    auto validateCalibrationData() -> bool;
    
    // Utility methods
    auto clampTemperature(double temperature) -> double;
    auto isValidTemperature(double temperature) -> bool;
    auto formatTemperature(double temperature) -> std::string;
    
    // Compensation data for adaptive algorithm
    struct CalibrationPoint {
        double temperature;
        int position;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    std::vector<CalibrationPoint> calibration_points_;
    static constexpr size_t MAX_CALIBRATION_POINTS = 50;
};

} // namespace lithium::device::ascom::focuser::components
