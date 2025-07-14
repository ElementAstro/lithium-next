/*
 * backlash_compensator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Backlash Compensator Component

This component handles backlash compensation for ASCOM focuser devices,
providing automatic compensation for mechanical backlash in the focuser
mechanism.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "device/template/focuser.hpp"

namespace lithium::device::ascom::focuser::components {

// Forward declarations
class HardwareInterface;
class MovementController;

/**
 * @brief Backlash Compensator for ASCOM Focuser
 *
 * This component manages backlash compensation:
 * - Backlash detection and measurement
 * - Automatic compensation during direction changes
 * - Compensation algorithm configuration
 * - Backlash calibration procedures
 * - Compensation statistics and monitoring
 */
class BacklashCompensator {
public:
    // Backlash compensation methods
    enum class CompensationMethod {
        NONE,           // No compensation
        FIXED,          // Fixed compensation steps
        ADAPTIVE,       // Adaptive compensation based on history
        MEASURED        // Compensation based on measured backlash
    };

    // Backlash compensation configuration
    struct BacklashConfig {
        bool enabled = false;
        CompensationMethod method = CompensationMethod::FIXED;
        int compensationSteps = 0;
        int maxCompensationSteps = 200;
        int minCompensationSteps = 0;
        double adaptiveFactorIn = 1.0;   // Factor for inward moves
        double adaptiveFactorOut = 1.0;  // Factor for outward moves
        bool compensateOnDirectionChange = true;
        bool compensateOnSmallMoves = false;
        int smallMoveThreshold = 10;
        std::chrono::milliseconds compensationDelay{100};
        double calibrationTolerance = 0.1;
    };

    // Backlash measurement results
    struct BacklashMeasurement {
        int inwardBacklash = 0;
        int outwardBacklash = 0;
        double measurementAccuracy = 0.0;
        std::chrono::steady_clock::time_point measurementTime;
        bool measurementValid = false;
        std::string measurementMethod;
    };

    // Backlash statistics
    struct BacklashStats {
        int totalCompensations = 0;
        int inwardCompensations = 0;
        int outwardCompensations = 0;
        int totalCompensationSteps = 0;
        int averageCompensationSteps = 0;
        int maxCompensationSteps = 0;
        int minCompensationSteps = 0;
        double successRate = 0.0;
        std::chrono::steady_clock::time_point lastCompensationTime;
        std::chrono::milliseconds totalCompensationTime{0};
    };

    // Direction tracking for compensation
    enum class LastDirection {
        NONE,
        INWARD,
        OUTWARD
    };

    // Constructor and destructor
    explicit BacklashCompensator(std::shared_ptr<HardwareInterface> hardware,
                                std::shared_ptr<MovementController> movement);
    ~BacklashCompensator();

    // Non-copyable and non-movable
    BacklashCompensator(const BacklashCompensator&) = delete;
    BacklashCompensator& operator=(const BacklashCompensator&) = delete;
    BacklashCompensator(BacklashCompensator&&) = delete;
    BacklashCompensator& operator=(BacklashCompensator&&) = delete;

    // =========================================================================
    // Initialization and Configuration
    // =========================================================================

    /**
     * @brief Initialize the backlash compensator
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the backlash compensator
     */
    auto destroy() -> bool;

    /**
     * @brief Set backlash configuration
     */
    auto setBacklashConfig(const BacklashConfig& config) -> void;

    /**
     * @brief Get backlash configuration
     */
    auto getBacklashConfig() const -> BacklashConfig;

    // =========================================================================
    // Backlash Compensation Control
    // =========================================================================

    /**
     * @brief Enable/disable backlash compensation
     */
    auto enableBacklashCompensation(bool enable) -> bool;

    /**
     * @brief Check if backlash compensation is enabled
     */
    auto isBacklashCompensationEnabled() -> bool;

    /**
     * @brief Get current backlash compensation steps
     */
    auto getBacklash() -> int;

    /**
     * @brief Set backlash compensation steps
     */
    auto setBacklash(int backlash) -> bool;

    /**
     * @brief Get backlash compensation steps for specific direction
     */
    auto getBacklashForDirection(FocusDirection direction) -> int;

    /**
     * @brief Set backlash compensation steps for specific direction
     */
    auto setBacklashForDirection(FocusDirection direction, int backlash) -> bool;

    // =========================================================================
    // Movement Processing
    // =========================================================================

    /**
     * @brief Process movement for backlash compensation
     */
    auto processMovement(int startPosition, int targetPosition) -> bool;

    /**
     * @brief Check if compensation is needed for a movement
     */
    auto needsCompensation(int startPosition, int targetPosition) -> bool;

    /**
     * @brief Calculate compensation steps for a movement
     */
    auto calculateCompensationSteps(int startPosition, int targetPosition) -> int;

    /**
     * @brief Apply backlash compensation
     */
    auto applyCompensation(FocusDirection direction, int steps) -> bool;

    /**
     * @brief Get last movement direction
     */
    auto getLastDirection() -> LastDirection;

    /**
     * @brief Update last movement direction
     */
    auto updateLastDirection(int startPosition, int targetPosition) -> void;

    // =========================================================================
    // Backlash Measurement and Calibration
    // =========================================================================

    /**
     * @brief Measure backlash automatically
     */
    auto measureBacklash() -> BacklashMeasurement;

    /**
     * @brief Calibrate backlash compensation
     */
    auto calibrateBacklash() -> bool;

    /**
     * @brief Get last backlash measurement
     */
    auto getLastBacklashMeasurement() -> BacklashMeasurement;

    /**
     * @brief Validate backlash measurement
     */
    auto validateMeasurement(const BacklashMeasurement& measurement) -> bool;

    /**
     * @brief Auto-calibrate backlash based on usage
     */
    auto autoCalibrate() -> bool;

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get backlash statistics
     */
    auto getBacklashStats() -> BacklashStats;

    /**
     * @brief Reset backlash statistics
     */
    auto resetBacklashStats() -> void;

    /**
     * @brief Get compensation success rate
     */
    auto getCompensationSuccessRate() -> double;

    /**
     * @brief Get average compensation steps
     */
    auto getAverageCompensationSteps() -> int;

    // =========================================================================
    // Advanced Features
    // =========================================================================

    /**
     * @brief Set adaptive compensation factors
     */
    auto setAdaptiveFactors(double inwardFactor, double outwardFactor) -> bool;

    /**
     * @brief Get adaptive compensation factors
     */
    auto getAdaptiveFactors() -> std::pair<double, double>;

    /**
     * @brief Learn from compensation results
     */
    auto learnFromCompensation(FocusDirection direction, int steps, bool success) -> void;

    /**
     * @brief Get compensation recommendation
     */
    auto getCompensationRecommendation(FocusDirection direction) -> int;

    /**
     * @brief Test compensation effectiveness
     */
    auto testCompensationEffectiveness(int testIterations = 10) -> double;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using CompensationCallback = std::function<void(FocusDirection direction, int steps, bool success)>;
    using CalibrationCallback = std::function<void(const BacklashMeasurement& measurement)>;
    using CompensationStatsCallback = std::function<void(const BacklashStats& stats)>;

    /**
     * @brief Set compensation callback
     */
    auto setCompensationCallback(CompensationCallback callback) -> void;

    /**
     * @brief Set calibration callback
     */
    auto setCalibrationCallback(CalibrationCallback callback) -> void;

    /**
     * @brief Set compensation statistics callback
     */
    auto setCompensationStatsCallback(CompensationStatsCallback callback) -> void;

    // =========================================================================
    // Persistence and Configuration
    // =========================================================================

    /**
     * @brief Save backlash settings to file
     */
    auto saveBacklashSettings(const std::string& filename) -> bool;

    /**
     * @brief Load backlash settings from file
     */
    auto loadBacklashSettings(const std::string& filename) -> bool;

    /**
     * @brief Export backlash data to JSON
     */
    auto exportBacklashData() -> std::string;

    /**
     * @brief Import backlash data from JSON
     */
    auto importBacklashData(const std::string& json) -> bool;

private:
    // Component references
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<MovementController> movement_;

    // Configuration
    BacklashConfig config_;

    // Backlash tracking
    std::atomic<LastDirection> last_direction_{LastDirection::NONE};
    std::atomic<int> last_position_{0};

    // Backlash measurements
    BacklashMeasurement last_measurement_;
    std::vector<BacklashMeasurement> measurement_history_;

    // Statistics
    BacklashStats stats_;

    // Adaptive learning data
    struct LearningData {
        FocusDirection direction;
        int steps;
        bool success;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<LearningData> learning_history_;
    static constexpr size_t MAX_LEARNING_HISTORY = 100;

    // Threading and synchronization
    mutable std::mutex config_mutex_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex learning_mutex_;
    mutable std::mutex measurement_mutex_;

    // Callbacks
    CompensationCallback compensation_callback_;
    CalibrationCallback calibration_callback_;
    CompensationStatsCallback compensation_stats_callback_;

    // Private methods
    auto determineDirection(int startPosition, int targetPosition) -> FocusDirection;
    auto hasDirectionChanged(int startPosition, int targetPosition) -> bool;
    auto updateBacklashStats(FocusDirection direction, int steps, bool success) -> void;
    auto addLearningData(FocusDirection direction, int steps, bool success) -> void;
    auto analyzeCompensationSuccess(int targetPosition, int finalPosition) -> bool;

    // Measurement algorithms
    auto measureBacklashBidirectional() -> BacklashMeasurement;
    auto measureBacklashUnidirectional() -> BacklashMeasurement;
    auto measureBacklashRepeated() -> BacklashMeasurement;

    // Compensation algorithms
    auto calculateFixedCompensation(FocusDirection direction) -> int;
    auto calculateAdaptiveCompensation(FocusDirection direction) -> int;
    auto calculateMeasuredCompensation(FocusDirection direction) -> int;

    // Calibration helpers
    auto findOptimalCompensationSteps(FocusDirection direction) -> int;
    auto validateCompensationSteps(int steps) -> bool;
    auto adjustCompensationBasedOnHistory() -> void;

    // Notification methods
    auto notifyCompensationApplied(FocusDirection direction, int steps, bool success) -> void;
    auto notifyCalibrationCompleted(const BacklashMeasurement& measurement) -> void;
    auto notifyStatsUpdated(const BacklashStats& stats) -> void;

    // Utility methods
    auto clampCompensationSteps(int steps) -> int;
    auto isSmallMove(int steps) -> bool;
    auto calculateMovingAverage(const std::vector<int>& values) -> double;
    auto formatDirection(FocusDirection direction) -> std::string;
    auto formatStats(const BacklashStats& stats) -> std::string;
};

} // namespace lithium::device::ascom::focuser::components
