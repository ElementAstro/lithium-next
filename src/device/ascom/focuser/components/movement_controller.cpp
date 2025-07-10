/*
 * movement_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Movement Controller Implementation

*************************************************/

#include "movement_controller.hpp"
#include "hardware_interface.hpp"

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace lithium::device::ascom::focuser::components {

MovementController::MovementController(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    spdlog::info("MovementController constructor called");
}

MovementController::~MovementController() {
    spdlog::info("MovementController destructor called");
    destroy();
}

auto MovementController::initialize() -> bool {
    spdlog::info("Initializing Movement Controller");
    
    if (!hardware_) {
        spdlog::error("Hardware interface is null");
        return false;
    }
    
    // Update current position from hardware
    auto position = hardware_->getPosition();
    if (position) {
        current_position_.store(*position);
        target_position_.store(*position);
    }
    
    // Reset statistics
    resetMovementStats();
    
    return true;
}

auto MovementController::destroy() -> bool {
    spdlog::info("Destroying Movement Controller");
    
    stopMovementMonitoring();
    
    // Abort any ongoing movement
    if (is_moving_.load()) {
        abortMove();
    }
    
    return true;
}

auto MovementController::setMovementConfig(const MovementConfig& config) -> void {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    config_ = config;
    spdlog::info("Movement configuration updated");
}

auto MovementController::getMovementConfig() const -> MovementConfig {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    return config_;
}

// Position control methods
auto MovementController::getCurrentPosition() -> std::optional<int> {
    if (!hardware_) {
        return std::nullopt;
    }
    
    auto position = hardware_->getPosition();
    if (position) {
        current_position_.store(*position);
        return *position;
    }
    
    return std::nullopt;
}

auto MovementController::moveToPosition(int position) -> bool {
    if (!hardware_) {
        return false;
    }
    
    if (is_moving_.load()) {
        spdlog::warn("Cannot move to position: focuser is already moving");
        return false;
    }
    
    if (!validateMovement(position)) {
        spdlog::error("Invalid movement to position: {}", position);
        return false;
    }
    
    int startPosition = current_position_.load();
    target_position_.store(position);
    
    spdlog::info("Moving to position: {} (from {})", position, startPosition);
    
    // Record movement start time
    move_start_time_ = std::chrono::steady_clock::now();
    
    // Start hardware movement
    if (hardware_->moveToPosition(position)) {
        is_moving_.store(true);
        startMovementMonitoring();
        
        // Notify movement start
        notifyMovementStart(startPosition, position);
        
        // Update statistics
        int steps = std::abs(position - startPosition);
        updateMovementStats(steps, std::chrono::milliseconds(0));
        
        return true;
    }
    
    return false;
}

auto MovementController::moveSteps(int steps) -> bool {
    if (!hardware_) {
        return false;
    }
    
    int currentPos = current_position_.load();
    int targetPos = currentPos + (is_reversed_.load() ? -steps : steps);
    
    return moveToPosition(targetPos);
}

auto MovementController::moveInward(int steps) -> bool {
    return moveSteps(-steps);
}

auto MovementController::moveOutward(int steps) -> bool {
    return moveSteps(steps);
}

auto MovementController::moveForDuration(int durationMs) -> bool {
    if (!hardware_ || durationMs <= 0) {
        return false;
    }
    
    if (is_moving_.load()) {
        spdlog::warn("Cannot move for duration: focuser is already moving");
        return false;
    }
    
    spdlog::info("Moving for duration: {} ms", durationMs);
    
    // Calculate approximate steps based on speed and duration
    double speed = current_speed_.load();
    int approximateSteps = static_cast<int>(speed * durationMs / 1000.0);
    
    // Use current direction
    FocusDirection dir = direction_.load();
    if (dir == FocusDirection::IN) {
        approximateSteps = -approximateSteps;
    }
    
    // Start movement
    int currentPos = current_position_.load();
    int targetPos = currentPos + approximateSteps;
    
    if (moveToPosition(targetPos)) {
        // Stop movement after specified duration
        std::thread([this, durationMs]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
            abortMove();
        }).detach();
        
        return true;
    }
    
    return false;
}

auto MovementController::syncPosition(int position) -> bool {
    if (!validatePosition(position)) {
        return false;
    }
    
    spdlog::info("Syncing position to: {}", position);
    
    current_position_.store(position);
    target_position_.store(position);
    
    notifyPositionChange(position);
    
    return true;
}

// Movement state methods
auto MovementController::isMoving() -> bool {
    if (!hardware_) {
        return false;
    }
    
    bool moving = hardware_->isMoving();
    
    // Update our state based on hardware state
    if (!moving && is_moving_.load()) {
        // Movement completed
        is_moving_.store(false);
        stopMovementMonitoring();
        
        // Update final position
        auto finalPos = getCurrentPosition();
        if (finalPos) {
            current_position_.store(*finalPos);
            
            // Calculate actual move duration
            auto moveDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - move_start_time_);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.lastMoveDuration = moveDuration;
                stats_.lastMoveTime = std::chrono::steady_clock::now();
            }
            
            // Notify completion
            bool success = (std::abs(*finalPos - target_position_.load()) <= config_.positionToleranceSteps);
            notifyMovementComplete(success, *finalPos, 
                                 success ? "Movement completed successfully" : "Movement completed with position error");
        }
    }
    
    return moving;
}

auto MovementController::abortMove() -> bool {
    if (!hardware_) {
        return false;
    }
    
    if (!is_moving_.load()) {
        return true;
    }
    
    spdlog::info("Aborting focuser movement");
    
    bool result = hardware_->halt();
    if (result) {
        is_moving_.store(false);
        stopMovementMonitoring();
        
        // Update position after abort
        auto currentPos = getCurrentPosition();
        if (currentPos) {
            notifyMovementComplete(false, *currentPos, "Movement aborted");
        }
    }
    
    return result;
}

auto MovementController::getTargetPosition() -> int {
    return target_position_.load();
}

auto MovementController::getMovementProgress() -> double {
    if (!is_moving_.load()) {
        return 1.0;
    }
    
    int currentPos = current_position_.load();
    int startPos = currentPos;  // We don't store start position, use current as approximation
    int targetPos = target_position_.load();
    
    return calculateProgress(currentPos, startPos, targetPos);
}

auto MovementController::getEstimatedTimeRemaining() -> std::chrono::milliseconds {
    if (!is_moving_.load()) {
        return std::chrono::milliseconds(0);
    }
    
    int currentPos = current_position_.load();
    int targetPos = target_position_.load();
    int remainingSteps = std::abs(targetPos - currentPos);
    
    return estimateMoveTime(remainingSteps);
}

// Speed control methods
auto MovementController::getSpeed() -> double {
    return current_speed_.load();
}

auto MovementController::setSpeed(double speed) -> bool {
    if (!validateSpeed(speed)) {
        return false;
    }
    
    double clampedSpeed = clampSpeed(speed);
    current_speed_.store(clampedSpeed);
    
    spdlog::info("Speed set to: {}", clampedSpeed);
    return true;
}

auto MovementController::getMaxSpeed() -> int {
    return config_.maxSpeed;
}

auto MovementController::getSpeedRange() -> std::pair<int, int> {
    return {config_.minSpeed, config_.maxSpeed};
}

// Direction control methods
auto MovementController::getDirection() -> std::optional<FocusDirection> {
    FocusDirection dir = direction_.load();
    return (dir != FocusDirection::NONE) ? std::optional<FocusDirection>(dir) : std::nullopt;
}

auto MovementController::setDirection(FocusDirection direction) -> bool {
    direction_.store(direction);
    spdlog::info("Direction set to: {}", static_cast<int>(direction));
    return true;
}

auto MovementController::isReversed() -> bool {
    return is_reversed_.load();
}

auto MovementController::setReversed(bool reversed) -> bool {
    is_reversed_.store(reversed);
    spdlog::info("Reversed set to: {}", reversed);
    return true;
}

// Limits control methods
auto MovementController::getMaxLimit() -> int {
    return config_.maxPosition;
}

auto MovementController::setMaxLimit(int maxLimit) -> bool {
    if (maxLimit < config_.minPosition) {
        spdlog::error("Max limit {} is less than min position {}", maxLimit, config_.minPosition);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(controller_mutex_);
    config_.maxPosition = maxLimit;
    spdlog::info("Max limit set to: {}", maxLimit);
    return true;
}

auto MovementController::getMinLimit() -> int {
    return config_.minPosition;
}

auto MovementController::setMinLimit(int minLimit) -> bool {
    if (minLimit > config_.maxPosition) {
        spdlog::error("Min limit {} is greater than max position {}", minLimit, config_.maxPosition);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(controller_mutex_);
    config_.minPosition = minLimit;
    spdlog::info("Min limit set to: {}", minLimit);
    return true;
}

auto MovementController::isPositionWithinLimits(int position) -> bool {
    return (position >= config_.minPosition && position <= config_.maxPosition);
}

// Statistics methods
auto MovementController::getMovementStats() const -> MovementStats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

auto MovementController::resetMovementStats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = MovementStats{};
    spdlog::info("Movement statistics reset");
}

auto MovementController::getTotalSteps() -> uint64_t {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.totalSteps;
}

auto MovementController::getLastMoveSteps() -> int {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.lastMoveSteps;
}

auto MovementController::getLastMoveDuration() -> std::chrono::milliseconds {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.lastMoveDuration;
}

// Callback methods
auto MovementController::setPositionCallback(PositionCallback callback) -> void {
    position_callback_ = std::move(callback);
}

auto MovementController::setMovementStartCallback(MovementStartCallback callback) -> void {
    movement_start_callback_ = std::move(callback);
}

auto MovementController::setMovementCompleteCallback(MovementCompleteCallback callback) -> void {
    movement_complete_callback_ = std::move(callback);
}

auto MovementController::setMovementProgressCallback(MovementProgressCallback callback) -> void {
    movement_progress_callback_ = std::move(callback);
}

// Validation and utility methods
auto MovementController::validateMovement(int targetPosition) -> bool {
    if (!validatePosition(targetPosition)) {
        return false;
    }
    
    if (is_moving_.load()) {
        spdlog::warn("Cannot start movement: focuser is already moving");
        return false;
    }
    
    return true;
}

auto MovementController::estimateMoveTime(int steps) -> std::chrono::milliseconds {
    if (steps <= 0) {
        return std::chrono::milliseconds(0);
    }
    
    double speed = current_speed_.load();
    if (speed <= 0) {
        speed = config_.defaultSpeed;
    }
    
    // Estimate time based on speed (steps per second)
    double timeSeconds = steps / speed;
    return std::chrono::milliseconds(static_cast<int>(timeSeconds * 1000));
}

auto MovementController::startMovementMonitoring() -> void {
    if (monitoring_active_.load()) {
        return;
    }
    
    monitoring_active_.store(true);
    monitoring_thread_ = std::thread(&MovementController::monitorMovementProgress, this);
}

auto MovementController::stopMovementMonitoring() -> void {
    if (!monitoring_active_.load()) {
        return;
    }
    
    monitoring_active_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

// Private methods
auto MovementController::updateCurrentPosition() -> void {
    if (!hardware_) {
        return;
    }
    
    auto position = hardware_->getPosition();
    if (position) {
        int oldPos = current_position_.exchange(*position);
        if (oldPos != *position) {
            notifyPositionChange(*position);
        }
    }
}

auto MovementController::notifyPositionChange(int position) -> void {
    if (position_callback_) {
        position_callback_(position);
    }
}

auto MovementController::notifyMovementStart(int startPosition, int targetPosition) -> void {
    if (movement_start_callback_) {
        movement_start_callback_(startPosition, targetPosition);
    }
}

auto MovementController::notifyMovementComplete(bool success, int finalPosition, const std::string& message) -> void {
    if (movement_complete_callback_) {
        movement_complete_callback_(success, finalPosition, message);
    }
}

auto MovementController::notifyMovementProgress(double progress, int currentPosition) -> void {
    if (movement_progress_callback_) {
        movement_progress_callback_(progress, currentPosition);
    }
}

auto MovementController::monitorMovementProgress() -> void {
    while (monitoring_active_.load()) {
        updateCurrentPosition();
        
        if (is_moving_.load()) {
            int currentPos = current_position_.load();
            double progress = getMovementProgress();
            notifyMovementProgress(progress, currentPos);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

auto MovementController::calculateProgress(int currentPos, int startPos, int targetPos) -> double {
    if (startPos == targetPos) {
        return 1.0;
    }
    
    int totalDistance = std::abs(targetPos - startPos);
    int remainingDistance = std::abs(targetPos - currentPos);
    
    double progress = 1.0 - (static_cast<double>(remainingDistance) / totalDistance);
    return std::clamp(progress, 0.0, 1.0);
}

auto MovementController::updateMovementStats(int steps, std::chrono::milliseconds duration) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.totalSteps += std::abs(steps);
    stats_.lastMoveSteps = steps;
    stats_.lastMoveDuration = duration;
    stats_.moveCount++;
    stats_.lastMoveTime = std::chrono::steady_clock::now();
}

// Validation helpers
auto MovementController::validateSpeed(double speed) -> bool {
    return (speed >= config_.minSpeed && speed <= config_.maxSpeed);
}

auto MovementController::validatePosition(int position) -> bool {
    if (!config_.enableSoftLimits) {
        return true;
    }
    
    return isPositionWithinLimits(position);
}

auto MovementController::clampPosition(int position) -> int {
    return std::clamp(position, config_.minPosition, config_.maxPosition);
}

auto MovementController::clampSpeed(double speed) -> double {
    return std::clamp(speed, static_cast<double>(config_.minSpeed), static_cast<double>(config_.maxSpeed));
}

} // namespace lithium::device::ascom::focuser::components
