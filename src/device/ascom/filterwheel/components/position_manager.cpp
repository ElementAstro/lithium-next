/*
 * position_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Position Manager Implementation

*************************************************/

#include "position_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::ascom::filterwheel::components {

PositionManager::PositionManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    spdlog::debug("PositionManager constructor");
}

PositionManager::~PositionManager() {
    spdlog::debug("PositionManager destructor");
    shutdown();
}

auto PositionManager::initialize() -> bool {
    spdlog::info("Initializing Position Manager");

    if (!hardware_) {
        setError("Hardware interface not available");
        return false;
    }

    // Get filter count from hardware
    auto count = hardware_->getFilterCount();
    if (count) {
        filter_count_ = *count;
        spdlog::info("Filter count: {}", filter_count_);
    } else {
        spdlog::warn("Could not determine filter count, using default of 8");
        filter_count_ = 8;
    }

    // Start monitoring thread
    stop_monitoring_.store(false);
    monitoring_thread_ = std::make_unique<std::thread>(&PositionManager::monitorMovement, this);

    spdlog::info("Position Manager initialized successfully");
    return true;
}

auto PositionManager::shutdown() -> void {
    spdlog::info("Shutting down Position Manager");

    // Stop monitoring thread
    stop_monitoring_.store(true);
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    monitoring_thread_.reset();

    spdlog::info("Position Manager shutdown completed");
}

auto PositionManager::moveToPosition(int position) -> bool {
    spdlog::info("Moving to position: {}", position);

    // Validate position
    auto validation = validatePosition(position);
    if (!validation.is_valid) {
        setError(validation.error_message);
        return false;
    }

    // Check if already moving
    if (is_moving_.load()) {
        setError("Filter wheel is already moving");
        return false;
    }

    // Check if already at target position
    auto current = getCurrentPosition();
    if (current && *current == position) {
        spdlog::info("Already at target position {}", position);
        return true;
    }

    return startMovement(position);
}

auto PositionManager::getCurrentPosition() -> std::optional<int> {
    if (!hardware_) {
        return std::nullopt;
    }

    return hardware_->getCurrentPosition();
}

auto PositionManager::getTargetPosition() -> std::optional<int> {
    if (movement_status_.load() == MovementStatus::MOVING) {
        return target_position_.load();
    }
    return std::nullopt;
}

auto PositionManager::isMoving() -> bool {
    return is_moving_.load();
}

auto PositionManager::abortMovement() -> bool {
    spdlog::info("Aborting movement");

    if (!is_moving_.load()) {
        spdlog::info("No movement in progress");
        return true;
    }

    // ASCOM filterwheels typically don't support abort
    // Movement is usually fast and atomic
    movement_status_.store(MovementStatus::ABORTED);
    is_moving_.store(false);

    finishMovement(false, "Movement aborted");
    return true;
}

auto PositionManager::waitForMovement(int timeout_ms) -> bool {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);

    while (is_moving_.load()) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout_duration) {
            setError("Movement timeout");
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return movement_status_.load() != MovementStatus::ERROR;
}

auto PositionManager::validatePosition(int position) -> PositionValidation {
    PositionValidation result;

    if (position < 0) {
        result.is_valid = false;
        result.error_message = "Position cannot be negative";
        return result;
    }

    if (position >= filter_count_) {
        result.is_valid = false;
        result.error_message = "Position " + std::to_string(position) + 
                              " exceeds maximum position " + std::to_string(filter_count_ - 1);
        return result;
    }

    result.is_valid = true;
    return result;
}

auto PositionManager::isValidPosition(int position) -> bool {
    return validatePosition(position).is_valid;
}

auto PositionManager::getFilterCount() -> int {
    return filter_count_;
}

auto PositionManager::getMaxPosition() -> int {
    return filter_count_ - 1;
}

auto PositionManager::getMovementStatus() -> MovementStatus {
    return movement_status_.load();
}

auto PositionManager::getMovementProgress() -> double {
    if (!is_moving_.load()) {
        return 1.0; // Complete
    }

    // For simple filterwheels, progress is binary
    return 0.5; // In progress
}

auto PositionManager::getEstimatedTimeToCompletion() -> std::chrono::milliseconds {
    if (!is_moving_.load()) {
        return std::chrono::milliseconds(0);
    }

    // Estimate based on average move time
    auto avg_time = getAverageMoveTime();
    if (avg_time.count() > 0) {
        return avg_time;
    }

    // Default estimate
    return std::chrono::milliseconds(2000);
}

auto PositionManager::homeFilterWheel() -> bool {
    spdlog::info("Homing filter wheel");
    return moveToPosition(0);
}

auto PositionManager::findHome() -> bool {
    spdlog::info("Finding home position");
    
    // For most ASCOM filterwheels, position 0 is considered home
    return moveToPosition(0);
}

auto PositionManager::calibratePositions() -> bool {
    spdlog::info("Calibrating positions");

    // Basic calibration - test movement to each position
    for (int i = 0; i < filter_count_; ++i) {
        if (!moveToPosition(i)) {
            setError("Failed to move to position " + std::to_string(i) + " during calibration");
            return false;
        }

        if (!waitForMovement(movement_timeout_ms_)) {
            setError("Timeout during calibration at position " + std::to_string(i));
            return false;
        }
    }

    spdlog::info("Position calibration completed successfully");
    return true;
}

auto PositionManager::getTotalMoves() -> uint64_t {
    return total_moves_.load();
}

auto PositionManager::resetMoveCounter() -> void {
    total_moves_.store(0);
    move_times_.clear();
    spdlog::info("Move counter reset");
}

auto PositionManager::getLastMoveTime() -> std::chrono::milliseconds {
    return last_move_duration_;
}

auto PositionManager::getAverageMoveTime() -> std::chrono::milliseconds {
    return calculateAverageTime();
}

auto PositionManager::setMovementCallback(MovementCallback callback) -> void {
    movement_callback_ = std::move(callback);
}

auto PositionManager::setPositionChangeCallback(PositionChangeCallback callback) -> void {
    position_change_callback_ = std::move(callback);
}

auto PositionManager::setMovementTimeout(int timeout_ms) -> void {
    movement_timeout_ms_ = timeout_ms;
}

auto PositionManager::getMovementTimeout() -> int {
    return movement_timeout_ms_;
}

auto PositionManager::setRetryCount(int retries) -> void {
    retry_count_ = retries;
}

auto PositionManager::getRetryCount() -> int {
    return retry_count_;
}

auto PositionManager::getLastError() -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto PositionManager::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private implementation methods

auto PositionManager::startMovement(int position) -> bool {
    if (!validateHardware()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(position_mutex_);

    target_position_.store(position);
    movement_status_.store(MovementStatus::MOVING);
    is_moving_.store(true);
    last_move_start_ = std::chrono::steady_clock::now();

    return performMove(position);
}

auto PositionManager::finishMovement(bool success, const std::string& message) -> void {
    auto end_time = std::chrono::steady_clock::now();
    last_move_duration_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - last_move_start_);

    if (success) {
        movement_status_.store(MovementStatus::IDLE);
        total_moves_.fetch_add(1);
        updateMoveStatistics(last_move_duration_);

        auto new_position = getCurrentPosition();
        if (new_position) {
            int old_position = current_position_.load();
            current_position_.store(*new_position);
            notifyPositionChange(old_position, *new_position);
        }
    } else {
        movement_status_.store(MovementStatus::ERROR);
    }

    is_moving_.store(false);
    notifyMovementComplete(target_position_.load(), success, message);
}

auto PositionManager::updatePosition() -> void {
    if (!hardware_) {
        return;
    }

    auto position = hardware_->getCurrentPosition();
    if (position) {
        int old_position = current_position_.load();
        if (old_position != *position) {
            current_position_.store(*position);
            notifyPositionChange(old_position, *position);
        }
    }
}

auto PositionManager::monitorMovement() -> void {
    while (!stop_monitoring_.load()) {
        if (is_moving_.load()) {
            updatePosition();

            // Check for movement completion
            auto current = getCurrentPosition();
            auto target = target_position_.load();

            if (current && *current == target) {
                finishMovement(true, "Movement completed successfully");
            }

            // Check for timeout
            auto elapsed = std::chrono::steady_clock::now() - last_move_start_;
            if (elapsed >= std::chrono::milliseconds(movement_timeout_ms_)) {
                finishMovement(false, "Movement timeout");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

auto PositionManager::validateHardware() -> bool {
    if (!hardware_) {
        setError("Hardware interface not available");
        return false;
    }

    if (!hardware_->isConnected()) {
        setError("Hardware not connected");
        return false;
    }

    return true;
}

auto PositionManager::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("PositionManager error: {}", error);
}

auto PositionManager::notifyMovementComplete(int position, bool success, const std::string& message) -> void {
    if (movement_callback_) {
        movement_callback_(position, success, message);
    }
}

auto PositionManager::notifyPositionChange(int old_position, int new_position) -> void {
    if (position_change_callback_) {
        position_change_callback_(old_position, new_position);
    }
}

auto PositionManager::performMove(int position, int attempt) -> bool {
    if (!hardware_) {
        setError("Hardware interface not available");
        return false;
    }

    bool success = hardware_->setPosition(position);
    if (!success && attempt < retry_count_) {
        spdlog::warn("Move attempt {} failed, retrying...", attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return performMove(position, attempt + 1);
    }

    return success;
}

auto PositionManager::verifyPosition(int expected_position) -> bool {
    auto actual = getCurrentPosition();
    return actual && *actual == expected_position;
}

auto PositionManager::estimateMovementTime(int from_position, int to_position) -> std::chrono::milliseconds {
    // Simple estimation based on position difference
    int distance = std::abs(to_position - from_position);
    
    if (distance == 0) {
        return std::chrono::milliseconds(0);
    }

    // Base time plus time per position
    auto base_time = std::chrono::milliseconds(500);
    auto per_position_time = std::chrono::milliseconds(200);

    return base_time + (per_position_time * distance);
}

auto PositionManager::updateMoveStatistics(std::chrono::milliseconds duration) -> void {
    move_times_.push_back(duration);

    // Keep only recent move times (last 100 moves)
    if (move_times_.size() > 100) {
        move_times_.erase(move_times_.begin());
    }
}

auto PositionManager::calculateAverageTime() -> std::chrono::milliseconds {
    if (move_times_.empty()) {
        return std::chrono::milliseconds(0);
    }

    auto total = std::chrono::milliseconds(0);
    for (const auto& time : move_times_) {
        total += time;
    }

    return total / move_times_.size();
}

} // namespace lithium::device::ascom::filterwheel::components
