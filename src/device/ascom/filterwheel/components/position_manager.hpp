/*
 * position_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Position Manager Component

This component manages filter wheel positions, movements, and related
validation and safety checks.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lithium::device::ascom::filterwheel::components {

// Forward declaration
class HardwareInterface;

// Movement status
enum class MovementStatus {
    IDLE,
    MOVING,
    ERROR,
    ABORTED
};

// Position validation result
struct PositionValidation {
    bool is_valid;
    std::string error_message;
};

/**
 * @brief Position Manager for ASCOM Filter Wheels
 *
 * This component handles position management, movement control, and
 * safety validation for filterwheel operations.
 */
class PositionManager {
public:
    using MovementCallback = std::function<void(int position, bool success, const std::string& message)>;
    using PositionChangeCallback = std::function<void(int old_position, int new_position)>;

    explicit PositionManager(std::shared_ptr<HardwareInterface> hardware);
    ~PositionManager();

    // Initialization
    auto initialize() -> bool;
    auto shutdown() -> void;

    // Position control
    auto moveToPosition(int position) -> bool;
    auto getCurrentPosition() -> std::optional<int>;
    auto getTargetPosition() -> std::optional<int>;
    auto isMoving() -> bool;
    auto abortMovement() -> bool;
    auto waitForMovement(int timeout_ms = 30000) -> bool;

    // Position validation
    auto validatePosition(int position) -> PositionValidation;
    auto isValidPosition(int position) -> bool;
    auto getFilterCount() -> int;
    auto getMaxPosition() -> int;

    // Movement status
    auto getMovementStatus() -> MovementStatus;
    auto getMovementProgress() -> double; // 0.0 to 1.0
    auto getEstimatedTimeToCompletion() -> std::chrono::milliseconds;

    // Home and calibration
    auto homeFilterWheel() -> bool;
    auto findHome() -> bool;
    auto calibratePositions() -> bool;

    // Statistics
    auto getTotalMoves() -> uint64_t;
    auto resetMoveCounter() -> void;
    auto getLastMoveTime() -> std::chrono::milliseconds;
    auto getAverageMoveTime() -> std::chrono::milliseconds;

    // Callbacks
    auto setMovementCallback(MovementCallback callback) -> void;
    auto setPositionChangeCallback(PositionChangeCallback callback) -> void;

    // Configuration
    auto setMovementTimeout(int timeout_ms) -> void;
    auto getMovementTimeout() -> int;
    auto setRetryCount(int retries) -> void;
    auto getRetryCount() -> int;

    // Error handling
    auto getLastError() -> std::string;
    auto clearError() -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;

    // Position state
    std::atomic<int> current_position_{0};
    std::atomic<int> target_position_{0};
    std::atomic<MovementStatus> movement_status_{MovementStatus::IDLE};
    std::atomic<bool> is_moving_{false};

    // Configuration
    int movement_timeout_ms_{30000};
    int retry_count_{3};
    int filter_count_{0};

    // Statistics
    std::atomic<uint64_t> total_moves_{0};
    std::chrono::steady_clock::time_point last_move_start_;
    std::chrono::milliseconds last_move_duration_{0};
    std::vector<std::chrono::milliseconds> move_times_;

    // Threading
    std::unique_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> stop_monitoring_{false};
    std::mutex position_mutex_;

    // Callbacks
    MovementCallback movement_callback_;
    PositionChangeCallback position_change_callback_;

    // Error handling
    std::string last_error_;
    std::mutex error_mutex_;

    // Internal methods
    auto startMovement(int position) -> bool;
    auto finishMovement(bool success, const std::string& message = "") -> void;
    auto updatePosition() -> void;
    auto monitorMovement() -> void;
    auto validateHardware() -> bool;
    auto setError(const std::string& error) -> void;
    auto notifyMovementComplete(int position, bool success, const std::string& message) -> void;
    auto notifyPositionChange(int old_position, int new_position) -> void;

    // Movement implementation
    auto performMove(int position, int attempt = 1) -> bool;
    auto verifyPosition(int expected_position) -> bool;
    auto estimateMovementTime(int from_position, int to_position) -> std::chrono::milliseconds;

    // Statistics helpers
    auto updateMoveStatistics(std::chrono::milliseconds duration) -> void;
    auto calculateAverageTime() -> std::chrono::milliseconds;
};

} // namespace lithium::device::ascom::filterwheel::components
