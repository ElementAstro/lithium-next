/*
 * movement_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Movement Controller Component

This component handles all aspects of focuser movement including
absolute and relative positioning, speed control, direction management,
and movement validation.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "device/template/focuser.hpp"

namespace lithium::device::ascom::focuser::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Movement Controller for ASCOM Focuser
 *
 * This component manages all aspects of focuser movement, including:
 * - Absolute and relative positioning
 * - Speed control and validation
 * - Direction management
 * - Movement limits enforcement
 * - Movement monitoring and callbacks
 */
class MovementController {
public:
    // Movement statistics
    struct MovementStats {
        uint64_t totalSteps = 0;
        int lastMoveSteps = 0;
        std::chrono::milliseconds lastMoveDuration{0};
        int moveCount = 0;
        std::chrono::steady_clock::time_point lastMoveTime;
    };

    // Movement configuration
    struct MovementConfig {
        int maxPosition = 65535;
        int minPosition = 0;
        int maxSpeed = 100;
        int minSpeed = 1;
        int defaultSpeed = 50;
        bool enableSoftLimits = true;
        int moveTimeoutMs = 30000;  // 30 seconds
        int positionToleranceSteps = 1;
    };

    // Constructor and destructor
    explicit MovementController(std::shared_ptr<HardwareInterface> hardware);
    ~MovementController();

    // Non-copyable and non-movable
    MovementController(const MovementController&) = delete;
    MovementController& operator=(const MovementController&) = delete;
    MovementController(MovementController&&) = delete;
    MovementController& operator=(MovementController&&) = delete;

    // =========================================================================
    // Initialization and Configuration
    // =========================================================================

    /**
     * @brief Initialize the movement controller
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the movement controller
     */
    auto destroy() -> bool;

    /**
     * @brief Set movement configuration
     */
    auto setMovementConfig(const MovementConfig& config) -> void;

    /**
     * @brief Get movement configuration
     */
    auto getMovementConfig() const -> MovementConfig;

    // =========================================================================
    // Position Control
    // =========================================================================

    /**
     * @brief Get current focuser position
     */
    auto getCurrentPosition() -> std::optional<int>;

    /**
     * @brief Move to absolute position
     */
    auto moveToPosition(int position) -> bool;

    /**
     * @brief Move by relative steps
     */
    auto moveSteps(int steps) -> bool;

    /**
     * @brief Move inward by steps
     */
    auto moveInward(int steps) -> bool;

    /**
     * @brief Move outward by steps
     */
    auto moveOutward(int steps) -> bool;

    /**
     * @brief Move for specified duration
     */
    auto moveForDuration(int durationMs) -> bool;

    /**
     * @brief Sync position (set current position without moving)
     */
    auto syncPosition(int position) -> bool;

    // =========================================================================
    // Movement State
    // =========================================================================

    /**
     * @brief Check if focuser is currently moving
     */
    auto isMoving() -> bool;

    /**
     * @brief Abort current movement
     */
    auto abortMove() -> bool;

    /**
     * @brief Get target position
     */
    auto getTargetPosition() -> int;

    /**
     * @brief Get movement progress (0.0 to 1.0)
     */
    auto getMovementProgress() -> double;

    /**
     * @brief Get estimated time remaining for current move
     */
    auto getEstimatedTimeRemaining() -> std::chrono::milliseconds;

    // =========================================================================
    // Speed Control
    // =========================================================================

    /**
     * @brief Get current speed
     */
    auto getSpeed() -> double;

    /**
     * @brief Set movement speed
     */
    auto setSpeed(double speed) -> bool;

    /**
     * @brief Get maximum speed
     */
    auto getMaxSpeed() -> int;

    /**
     * @brief Get speed range
     */
    auto getSpeedRange() -> std::pair<int, int>;

    // =========================================================================
    // Direction Control
    // =========================================================================

    /**
     * @brief Get focus direction
     */
    auto getDirection() -> std::optional<FocusDirection>;

    /**
     * @brief Set focus direction
     */
    auto setDirection(FocusDirection direction) -> bool;

    /**
     * @brief Check if focuser is reversed
     */
    auto isReversed() -> bool;

    /**
     * @brief Set focuser reversed state
     */
    auto setReversed(bool reversed) -> bool;

    // =========================================================================
    // Limits Control
    // =========================================================================

    /**
     * @brief Get maximum position limit
     */
    auto getMaxLimit() -> int;

    /**
     * @brief Set maximum position limit
     */
    auto setMaxLimit(int maxLimit) -> bool;

    /**
     * @brief Get minimum position limit
     */
    auto getMinLimit() -> int;

    /**
     * @brief Set minimum position limit
     */
    auto setMinLimit(int minLimit) -> bool;

    /**
     * @brief Check if position is within limits
     */
    auto isPositionWithinLimits(int position) -> bool;

    // =========================================================================
    // Movement Statistics
    // =========================================================================

    /**
     * @brief Get movement statistics
     */
    auto getMovementStats() const -> MovementStats;

    /**
     * @brief Reset movement statistics
     */
    auto resetMovementStats() -> void;

    /**
     * @brief Get total steps moved
     */
    auto getTotalSteps() -> uint64_t;

    /**
     * @brief Get last move steps
     */
    auto getLastMoveSteps() -> int;

    /**
     * @brief Get last move duration
     */
    auto getLastMoveDuration() -> std::chrono::milliseconds;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using PositionCallback = std::function<void(int position)>;
    using MovementStartCallback = std::function<void(int startPosition, int targetPosition)>;
    using MovementCompleteCallback = std::function<void(bool success, int finalPosition, const std::string& message)>;
    using MovementProgressCallback = std::function<void(double progress, int currentPosition)>;

    /**
     * @brief Set position change callback
     */
    auto setPositionCallback(PositionCallback callback) -> void;

    /**
     * @brief Set movement start callback
     */
    auto setMovementStartCallback(MovementStartCallback callback) -> void;

    /**
     * @brief Set movement complete callback
     */
    auto setMovementCompleteCallback(MovementCompleteCallback callback) -> void;

    /**
     * @brief Set movement progress callback
     */
    auto setMovementProgressCallback(MovementProgressCallback callback) -> void;

    // =========================================================================
    // Validation and Utilities
    // =========================================================================

    /**
     * @brief Validate movement parameters
     */
    auto validateMovement(int targetPosition) -> bool;

    /**
     * @brief Calculate move time estimate
     */
    auto estimateMoveTime(int steps) -> std::chrono::milliseconds;

    /**
     * @brief Monitor movement progress
     */
    auto startMovementMonitoring() -> void;

    /**
     * @brief Stop movement monitoring
     */
    auto stopMovementMonitoring() -> void;

private:
    // Hardware interface reference
    std::shared_ptr<HardwareInterface> hardware_;
    
    // Configuration
    MovementConfig config_;
    
    // Current state
    std::atomic<int> current_position_{0};
    std::atomic<int> target_position_{0};
    std::atomic<double> current_speed_{50.0};
    std::atomic<bool> is_moving_{false};
    std::atomic<bool> is_reversed_{false};
    std::atomic<FocusDirection> direction_{FocusDirection::NONE};
    
    // Movement timing
    std::chrono::steady_clock::time_point move_start_time_;
    std::chrono::steady_clock::time_point last_position_update_;
    
    // Statistics
    MovementStats stats_;
    mutable std::mutex stats_mutex_;
    
    // Callbacks
    PositionCallback position_callback_;
    MovementStartCallback movement_start_callback_;
    MovementCompleteCallback movement_complete_callback_;
    MovementProgressCallback movement_progress_callback_;
    
    // Monitoring
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    mutable std::mutex controller_mutex_;
    
    // Private methods
    auto updateCurrentPosition() -> void;
    auto notifyPositionChange(int position) -> void;
    auto notifyMovementStart(int startPosition, int targetPosition) -> void;
    auto notifyMovementComplete(bool success, int finalPosition, const std::string& message) -> void;
    auto notifyMovementProgress(double progress, int currentPosition) -> void;
    
    auto monitorMovementProgress() -> void;
    auto calculateProgress(int currentPos, int startPos, int targetPos) -> double;
    auto updateMovementStats(int steps, std::chrono::milliseconds duration) -> void;
    
    // Validation helpers
    auto validateSpeed(double speed) -> bool;
    auto validatePosition(int position) -> bool;
    auto clampPosition(int position) -> int;
    auto clampSpeed(double speed) -> double;
};

} // namespace lithium::device::ascom::focuser::components
