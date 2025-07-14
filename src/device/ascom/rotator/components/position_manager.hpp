/*
 * position_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Position Manager Component

This component manages rotator position control, movement operations,
and position tracking with enhanced safety and precision features.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "device/template/rotator.hpp"

namespace lithium::device::ascom::rotator::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Movement state enumeration
 */
enum class MovementState {
    IDLE,
    MOVING,
    ABORTING,
    ERROR
};

/**
 * @brief Position tracking information
 */
struct PositionInfo {
    double current_position{0.0};
    double target_position{0.0};
    double mechanical_position{0.0};
    std::chrono::steady_clock::time_point last_update;
    bool is_moving{false};
    MovementState state{MovementState::IDLE};
};

/**
 * @brief Movement parameters
 */
struct MovementParams {
    double target_angle{0.0};
    double speed{10.0};              // degrees per second
    double acceleration{5.0};        // degrees per second squared
    double tolerance{0.1};           // degrees
    int timeout_ms{30000};           // 30 seconds default
    bool use_shortest_path{true};
    bool enable_backlash_compensation{false};
    double backlash_amount{0.0};
};

/**
 * @brief Position statistics
 */
struct PositionStats {
    double total_rotation{0.0};
    double last_move_angle{0.0};
    std::chrono::milliseconds last_move_duration{0};
    int move_count{0};
    double average_move_time{0.0};
    double max_move_time{0.0};
    double min_move_time{std::numeric_limits<double>::max()};
};

/**
 * @brief Position Manager for ASCOM Rotator
 *
 * This component handles all rotator position-related operations including
 * movement control, position tracking, backlash compensation, and statistics.
 */
class PositionManager {
public:
    explicit PositionManager(std::shared_ptr<HardwareInterface> hardware);
    ~PositionManager();

    // Lifecycle management
    auto initialize() -> bool;
    auto destroy() -> bool;

    // Position control
    auto getCurrentPosition() -> std::optional<double>;
    auto getMechanicalPosition() -> std::optional<double>;
    auto getTargetPosition() -> double;

    // Movement operations
    auto moveToAngle(double angle, const MovementParams& params = {}) -> bool;
    auto moveToAngleAsync(double angle, const MovementParams& params = {})
        -> std::shared_ptr<std::future<bool>>;
    auto rotateByAngle(double angle, const MovementParams& params = {}) -> bool;
    auto syncPosition(double angle) -> bool;
    auto abortMove() -> bool;

    // Movement state
    auto isMoving() const -> bool;
    auto getMovementState() const -> MovementState;
    auto getPositionInfo() const -> PositionInfo;

    // Direction and path optimization
    auto getOptimalPath(double from_angle, double to_angle) -> std::pair<double, bool>; // angle, clockwise
    auto normalizeAngle(double angle) -> double;
    auto calculateShortestPath(double from_angle, double to_angle) -> double;

    // Limits and constraints
    auto setPositionLimits(double min_pos, double max_pos) -> bool;
    auto getPositionLimits() -> std::pair<double, double>;
    auto isPositionWithinLimits(double position) -> bool;
    auto enforcePositionLimits(double& position) -> bool;

    // Speed and acceleration
    auto setSpeed(double speed) -> bool;
    auto getSpeed() -> std::optional<double>;
    auto setAcceleration(double acceleration) -> bool;
    auto getAcceleration() -> std::optional<double>;
    auto getMaxSpeed() -> double;
    auto getMinSpeed() -> double;

    // Backlash compensation
    auto enableBacklashCompensation(bool enable) -> bool;
    auto isBacklashCompensationEnabled() -> bool;
    auto setBacklashAmount(double backlash) -> bool;
    auto getBacklashAmount() -> double;
    auto applyBacklashCompensation(double target_angle) -> double;

    // Direction control
    auto getDirection() -> std::optional<RotatorDirection>;
    auto setDirection(RotatorDirection direction) -> bool;
    auto isReversed() -> bool;
    auto setReversed(bool reversed) -> bool;

    // Position monitoring and callbacks
    auto startPositionMonitoring(int interval_ms = 500) -> bool;
    auto stopPositionMonitoring() -> bool;
    auto setPositionCallback(std::function<void(double, double)> callback) -> void; // current, target
    auto setMovementCallback(std::function<void(MovementState)> callback) -> void;

    // Statistics and tracking
    auto getPositionStats() -> PositionStats;
    auto resetPositionStats() -> bool;
    auto getTotalRotation() -> double;
    auto resetTotalRotation() -> bool;
    auto getLastMoveInfo() -> std::pair<double, std::chrono::milliseconds>; // angle, duration

    // Calibration and homing
    auto performHoming() -> bool;
    auto calibratePosition(double known_angle) -> bool;
    auto findHomePosition() -> std::optional<double>;

    // Safety and error handling
    auto setEmergencyStop(bool enabled) -> void;
    auto isEmergencyStopActive() -> bool;
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // Position state
    std::atomic<double> current_position_{0.0};
    std::atomic<double> target_position_{0.0};
    std::atomic<double> mechanical_position_{0.0};
    std::atomic<MovementState> movement_state_{MovementState::IDLE};
    std::atomic<bool> is_moving_{false};

    // Movement control
    MovementParams current_params_;
    std::atomic<bool> abort_requested_{false};
    std::atomic<bool> emergency_stop_{false};
    mutable std::mutex movement_mutex_;

    // Position limits
    double min_position_{0.0};
    double max_position_{360.0};
    bool limits_enabled_{false};

    // Speed and direction
    double current_speed_{10.0};
    double current_acceleration_{5.0};
    double max_speed_{50.0};
    double min_speed_{0.1};
    RotatorDirection current_direction_{RotatorDirection::CLOCKWISE};
    bool is_reversed_{false};

    // Backlash compensation
    bool backlash_enabled_{false};
    double backlash_amount_{0.0};
    double last_direction_angle_{0.0};
    bool last_move_clockwise_{true};

    // Monitoring and callbacks
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitor_running_{false};
    int monitor_interval_ms_{500};
    std::function<void(double, double)> position_callback_;
    std::function<void(MovementState)> movement_callback_;
    mutable std::mutex callback_mutex_;

    // Statistics
    PositionStats stats_;
    mutable std::mutex stats_mutex_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Helper methods
    auto updatePosition() -> bool;
    auto updateMovementState() -> bool;
    auto executeMovement(double target_angle, const MovementParams& params) -> bool;
    auto waitForMovementComplete(int timeout_ms) -> bool;
    auto calculateMovementTime(double angle_diff, const MovementParams& params) -> std::chrono::milliseconds;
    auto validateMovementParams(const MovementParams& params) -> bool;
    auto setLastError(const std::string& error) -> void;
    auto notifyPositionChange() -> void;
    auto notifyMovementStateChange(MovementState new_state) -> void;
    auto updateStatistics(double angle_moved, std::chrono::milliseconds duration) -> void;
    auto positionMonitoringLoop() -> void;
    auto calculateOptimalDirection(double from_angle, double to_angle) -> bool; // true = clockwise
};

} // namespace lithium::device::ascom::rotator::components
