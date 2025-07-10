/*
 * position_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Position Manager Component Implementation

*************************************************/

#include "position_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace lithium::device::ascom::rotator::components {

PositionManager::PositionManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    spdlog::debug("PositionManager constructor called");
}

PositionManager::~PositionManager() {
    spdlog::debug("PositionManager destructor called");
    destroy();
}

auto PositionManager::initialize() -> bool {
    spdlog::info("Initializing Position Manager");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        return false;
    }
    
    clearLastError();
    
    // Initialize position from hardware
    updatePosition();
    
    // Reset statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = PositionStats{};
    }
    
    spdlog::info("Position Manager initialized successfully");
    return true;
}

auto PositionManager::destroy() -> bool {
    spdlog::info("Destroying Position Manager");
    
    stopPositionMonitoring();
    abortMove();
    
    return true;
}

auto PositionManager::getCurrentPosition() -> std::optional<double> {
    if (!updatePosition()) {
        return std::nullopt;
    }
    
    return current_position_.load();
}

auto PositionManager::getMechanicalPosition() -> std::optional<double> {
    if (!hardware_ || !hardware_->isConnected()) {
        return std::nullopt;
    }
    
    auto mechanical = hardware_->getProperty("mechanicalposition");
    if (mechanical) {
        try {
            double pos = std::stod(*mechanical);
            mechanical_position_.store(pos);
            return pos;
        } catch (const std::exception& e) {
            setLastError("Failed to parse mechanical position: " + std::string(e.what()));
        }
    }
    
    return mechanical_position_.load();
}

auto PositionManager::getTargetPosition() -> double {
    return target_position_.load();
}

auto PositionManager::moveToAngle(double angle, const MovementParams& params) -> bool {
    spdlog::info("Moving rotator to angle: {:.2f}°", angle);
    
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }
    
    if (emergency_stop_.load()) {
        setLastError("Emergency stop is active");
        return false;
    }
    
    if (!validateMovementParams(params)) {
        return false;
    }
    
    // Normalize target angle
    double normalized_angle = normalizeAngle(angle);
    
    // Check position limits
    if (limits_enabled_ && !isPositionWithinLimits(normalized_angle)) {
        setLastError("Target position outside limits");
        return false;
    }
    
    // Apply backlash compensation if enabled
    if (backlash_enabled_) {
        normalized_angle = applyBacklashCompensation(normalized_angle);
    }
    
    std::lock_guard<std::mutex> lock(movement_mutex_);
    
    target_position_.store(normalized_angle);
    current_params_ = params;
    abort_requested_.store(false);
    
    return executeMovement(normalized_angle, params);
}

auto PositionManager::moveToAngleAsync(double angle, const MovementParams& params) 
    -> std::shared_ptr<std::future<bool>> {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = std::make_shared<std::future<bool>>(promise->get_future());
    
    // Execute movement in hardware interface's async context
    hardware_->executeAsync([this, angle, params, promise]() {
        try {
            bool result = moveToAngle(angle, params);
            promise->set_value(result);
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });
    
    return future;
}

auto PositionManager::rotateByAngle(double angle, const MovementParams& params) -> bool {
    auto current = getCurrentPosition();
    if (!current) {
        setLastError("Cannot get current position");
        return false;
    }
    
    double target = *current + angle;
    return moveToAngle(target, params);
}

auto PositionManager::syncPosition(double angle) -> bool {
    spdlog::info("Syncing rotator position to: {:.2f}°", angle);
    
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }
    
    // Normalize angle
    double normalized_angle = normalizeAngle(angle);
    
    // Send sync command to hardware
    if (!hardware_->setProperty("position", std::to_string(normalized_angle))) {
        setLastError("Failed to sync position on hardware");
        return false;
    }
    
    // Update local position
    current_position_.store(normalized_angle);
    target_position_.store(normalized_angle);
    
    spdlog::info("Position synced successfully to {:.2f}°", normalized_angle);
    return true;
}

auto PositionManager::abortMove() -> bool {
    spdlog::info("Aborting rotator movement");
    
    abort_requested_.store(true);
    
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }
    
    auto result = hardware_->invokeMethod("halt");
    if (result) {
        is_moving_.store(false);
        movement_state_.store(MovementState::IDLE);
        notifyMovementStateChange(MovementState::IDLE);
        return true;
    }
    
    return false;
}

auto PositionManager::isMoving() const -> bool {
    return is_moving_.load();
}

auto PositionManager::getMovementState() const -> MovementState {
    return movement_state_.load();
}

auto PositionManager::getPositionInfo() const -> PositionInfo {
    PositionInfo info;
    info.current_position = current_position_.load();
    info.target_position = target_position_.load();
    info.mechanical_position = mechanical_position_.load();
    info.is_moving = is_moving_.load();
    info.state = movement_state_.load();
    info.last_update = std::chrono::steady_clock::now();
    return info;
}

auto PositionManager::getOptimalPath(double from_angle, double to_angle) -> std::pair<double, bool> {
    double normalized_from = normalizeAngle(from_angle);
    double normalized_to = normalizeAngle(to_angle);
    
    double clockwise_diff = normalized_to - normalized_from;
    if (clockwise_diff < 0) clockwise_diff += 360.0;
    
    double counter_clockwise_diff = 360.0 - clockwise_diff;
    
    if (clockwise_diff <= counter_clockwise_diff) {
        return {clockwise_diff, true}; // clockwise
    } else {
        return {counter_clockwise_diff, false}; // counter-clockwise
    }
}

auto PositionManager::normalizeAngle(double angle) -> double {
    angle = std::fmod(angle, 360.0);
    if (angle < 0) {
        angle += 360.0;
    }
    return angle;
}

auto PositionManager::calculateShortestPath(double from_angle, double to_angle) -> double {
    auto [diff, clockwise] = getOptimalPath(from_angle, to_angle);
    return clockwise ? diff : -diff;
}

auto PositionManager::setPositionLimits(double min_pos, double max_pos) -> bool {
    if (min_pos >= max_pos) {
        setLastError("Invalid position limits: min >= max");
        return false;
    }
    
    min_position_ = normalizeAngle(min_pos);
    max_position_ = normalizeAngle(max_pos);
    limits_enabled_ = true;
    
    spdlog::info("Position limits set: {:.2f}° to {:.2f}°", min_position_, max_position_);
    return true;
}

auto PositionManager::getPositionLimits() -> std::pair<double, double> {
    return {min_position_, max_position_};
}

auto PositionManager::isPositionWithinLimits(double position) -> bool {
    if (!limits_enabled_) {
        return true;
    }
    
    double norm_pos = normalizeAngle(position);
    
    if (min_position_ <= max_position_) {
        return norm_pos >= min_position_ && norm_pos <= max_position_;
    } else {
        // Wraps around 0°
        return norm_pos >= min_position_ || norm_pos <= max_position_;
    }
}

auto PositionManager::enforcePositionLimits(double& position) -> bool {
    if (!limits_enabled_) {
        return true;
    }
    
    if (!isPositionWithinLimits(position)) {
        // Clamp to nearest limit
        double norm_pos = normalizeAngle(position);
        double dist_to_min = std::abs(norm_pos - min_position_);
        double dist_to_max = std::abs(norm_pos - max_position_);
        
        position = (dist_to_min < dist_to_max) ? min_position_ : max_position_;
        return false;
    }
    
    return true;
}

auto PositionManager::setSpeed(double speed) -> bool {
    if (speed < min_speed_ || speed > max_speed_) {
        setLastError("Speed out of range");
        return false;
    }
    
    current_speed_ = speed;
    
    // Send to hardware if supported
    if (hardware_ && hardware_->isConnected()) {
        hardware_->setProperty("speed", std::to_string(speed));
    }
    
    return true;
}

auto PositionManager::getSpeed() -> std::optional<double> {
    if (hardware_ && hardware_->isConnected()) {
        auto speed = hardware_->getProperty("speed");
        if (speed) {
            try {
                return std::stod(*speed);
            } catch (const std::exception&) {
                // Fall through to return current speed
            }
        }
    }
    
    return current_speed_;
}

auto PositionManager::setAcceleration(double acceleration) -> bool {
    if (acceleration <= 0) {
        setLastError("Acceleration must be positive");
        return false;
    }
    
    current_acceleration_ = acceleration;
    return true;
}

auto PositionManager::getAcceleration() -> std::optional<double> {
    return current_acceleration_;
}

auto PositionManager::getMaxSpeed() -> double {
    return max_speed_;
}

auto PositionManager::getMinSpeed() -> double {
    return min_speed_;
}

auto PositionManager::enableBacklashCompensation(bool enable) -> bool {
    backlash_enabled_ = enable;
    spdlog::info("Backlash compensation {}", enable ? "enabled" : "disabled");
    return true;
}

auto PositionManager::isBacklashCompensationEnabled() -> bool {
    return backlash_enabled_;
}

auto PositionManager::setBacklashAmount(double backlash) -> bool {
    backlash_amount_ = std::abs(backlash);
    spdlog::info("Backlash amount set to {:.2f}°", backlash_amount_);
    return true;
}

auto PositionManager::getBacklashAmount() -> double {
    return backlash_amount_;
}

auto PositionManager::applyBacklashCompensation(double target_angle) -> double {
    if (!backlash_enabled_ || backlash_amount_ == 0.0) {
        return target_angle;
    }
    
    double current = current_position_.load();
    bool target_clockwise = calculateOptimalDirection(current, target_angle);
    
    // If direction changed, apply backlash compensation
    if (target_clockwise != last_move_clockwise_) {
        double compensation = target_clockwise ? backlash_amount_ : -backlash_amount_;
        target_angle += compensation;
        spdlog::debug("Applied backlash compensation: {:.2f}°", compensation);
    }
    
    last_move_clockwise_ = target_clockwise;
    last_direction_angle_ = target_angle;
    
    return normalizeAngle(target_angle);
}

auto PositionManager::getDirection() -> std::optional<RotatorDirection> {
    return current_direction_;
}

auto PositionManager::setDirection(RotatorDirection direction) -> bool {
    current_direction_ = direction;
    return true;
}

auto PositionManager::isReversed() -> bool {
    return is_reversed_;
}

auto PositionManager::setReversed(bool reversed) -> bool {
    is_reversed_ = reversed;
    
    // Send to hardware if supported
    if (hardware_ && hardware_->isConnected()) {
        hardware_->setProperty("reverse", reversed ? "true" : "false");
    }
    
    return true;
}

auto PositionManager::startPositionMonitoring(int interval_ms) -> bool {
    if (monitor_running_.load()) {
        return true; // Already running
    }
    
    monitor_interval_ms_ = interval_ms;
    monitor_running_.store(true);
    
    monitor_thread_ = std::make_unique<std::thread>(&PositionManager::positionMonitoringLoop, this);
    
    spdlog::info("Position monitoring started with {}ms interval", interval_ms);
    return true;
}

auto PositionManager::stopPositionMonitoring() -> bool {
    if (!monitor_running_.load()) {
        return true; // Already stopped
    }
    
    monitor_running_.store(false);
    
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    
    monitor_thread_.reset();
    
    spdlog::info("Position monitoring stopped");
    return true;
}

auto PositionManager::setPositionCallback(std::function<void(double, double)> callback) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    position_callback_ = callback;
}

auto PositionManager::setMovementCallback(std::function<void(MovementState)> callback) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    movement_callback_ = callback;
}

auto PositionManager::getPositionStats() -> PositionStats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

auto PositionManager::resetPositionStats() -> bool {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = PositionStats{};
    spdlog::info("Position statistics reset");
    return true;
}

auto PositionManager::getTotalRotation() -> double {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_.total_rotation;
}

auto PositionManager::resetTotalRotation() -> bool {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_rotation = 0.0;
    return true;
}

auto PositionManager::getLastMoveInfo() -> std::pair<double, std::chrono::milliseconds> {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {stats_.last_move_angle, stats_.last_move_duration};
}

auto PositionManager::performHoming() -> bool {
    spdlog::info("Performing rotator homing operation");
    
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }
    
    // Try to invoke home method on hardware
    auto result = hardware_->invokeMethod("findhome");
    if (result) {
        // Wait for homing to complete
        auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        
        while (std::chrono::steady_clock::now() < timeout) {
            if (!isMoving()) {
                updatePosition();
                spdlog::info("Homing completed successfully");
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        setLastError("Homing operation timed out");
        return false;
    }
    
    setLastError("Hardware does not support homing");
    return false;
}

auto PositionManager::calibratePosition(double known_angle) -> bool {
    return syncPosition(known_angle);
}

auto PositionManager::findHomePosition() -> std::optional<double> {
    if (performHoming()) {
        return getCurrentPosition();
    }
    return std::nullopt;
}

auto PositionManager::setEmergencyStop(bool enabled) -> void {
    emergency_stop_.store(enabled);
    if (enabled && isMoving()) {
        abortMove();
    }
    spdlog::info("Emergency stop {}", enabled ? "activated" : "deactivated");
}

auto PositionManager::isEmergencyStopActive() -> bool {
    return emergency_stop_.load();
}

auto PositionManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto PositionManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private helper methods

auto PositionManager::updatePosition() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }
    
    auto position = hardware_->getProperty("position");
    if (position) {
        try {
            double pos = std::stod(*position);
            current_position_.store(normalizeAngle(pos));
            return true;
        } catch (const std::exception& e) {
            setLastError("Failed to parse position: " + std::string(e.what()));
        }
    }
    
    return false;
}

auto PositionManager::updateMovementState() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }
    
    auto isMoving = hardware_->getProperty("ismoving");
    if (isMoving) {
        bool moving = (*isMoving == "true");
        is_moving_.store(moving);
        
        MovementState newState = moving ? MovementState::MOVING : MovementState::IDLE;
        MovementState oldState = movement_state_.exchange(newState);
        
        if (oldState != newState) {
            notifyMovementStateChange(newState);
        }
        
        return true;
    }
    
    return false;
}

auto PositionManager::executeMovement(double target_angle, const MovementParams& params) -> bool {
    auto start_time = std::chrono::steady_clock::now();
    double start_position = current_position_.load();
    
    // Set target position on hardware
    if (!hardware_->setProperty("position", std::to_string(target_angle))) {
        setLastError("Failed to set target position on hardware");
        return false;
    }
    
    // Start movement
    auto moveResult = hardware_->invokeMethod("move", {std::to_string(target_angle)});
    if (!moveResult) {
        setLastError("Failed to start movement");
        return false;
    }
    
    // Update state
    is_moving_.store(true);
    movement_state_.store(MovementState::MOVING);
    notifyMovementStateChange(MovementState::MOVING);
    
    // Wait for movement to complete
    bool success = waitForMovementComplete(params.timeout_ms);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Update statistics
    double angle_moved = std::abs(target_angle - start_position);
    updateStatistics(angle_moved, duration);
    
    return success;
}

auto PositionManager::waitForMovementComplete(int timeout_ms) -> bool {
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    
    while (std::chrono::steady_clock::now() < timeout) {
        if (abort_requested_.load()) {
            abortMove();
            setLastError("Movement aborted by user");
            return false;
        }
        
        if (emergency_stop_.load()) {
            abortMove();
            setLastError("Movement aborted by emergency stop");
            return false;
        }
        
        updateMovementState();
        if (!is_moving_.load()) {
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    setLastError("Movement timed out");
    abortMove();
    return false;
}

auto PositionManager::calculateMovementTime(double angle_diff, const MovementParams& params) 
    -> std::chrono::milliseconds {
    // Simple calculation: time = distance / speed + acceleration time
    double accel_time = params.speed / params.acceleration;
    double accel_distance = 0.5 * params.acceleration * accel_time * accel_time;
    
    double remaining_distance = std::abs(angle_diff) - 2 * accel_distance;
    if (remaining_distance < 0) remaining_distance = 0;
    
    double total_time = 2 * accel_time + remaining_distance / params.speed;
    return std::chrono::milliseconds(static_cast<int>(total_time * 1000));
}

auto PositionManager::validateMovementParams(const MovementParams& params) -> bool {
    if (params.speed <= 0 || params.speed > max_speed_) {
        setLastError("Invalid movement speed");
        return false;
    }
    
    if (params.acceleration <= 0) {
        setLastError("Invalid movement acceleration");
        return false;
    }
    
    if (params.tolerance < 0) {
        setLastError("Invalid movement tolerance");
        return false;
    }
    
    if (params.timeout_ms <= 0) {
        setLastError("Invalid movement timeout");
        return false;
    }
    
    return true;
}

auto PositionManager::setLastError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("PositionManager error: {}", error);
}

auto PositionManager::notifyPositionChange() -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (position_callback_) {
        position_callback_(current_position_.load(), target_position_.load());
    }
}

auto PositionManager::notifyMovementStateChange(MovementState new_state) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (movement_callback_) {
        movement_callback_(new_state);
    }
}

auto PositionManager::updateStatistics(double angle_moved, std::chrono::milliseconds duration) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.total_rotation += angle_moved;
    stats_.last_move_angle = angle_moved;
    stats_.last_move_duration = duration;
    stats_.move_count++;
    
    double duration_seconds = duration.count() / 1000.0;
    stats_.average_move_time = (stats_.average_move_time * (stats_.move_count - 1) + duration_seconds) / stats_.move_count;
    stats_.max_move_time = std::max(stats_.max_move_time, duration_seconds);
    stats_.min_move_time = std::min(stats_.min_move_time, duration_seconds);
}

auto PositionManager::positionMonitoringLoop() -> void {
    spdlog::debug("Position monitoring loop started");
    
    while (monitor_running_.load()) {
        try {
            updatePosition();
            updateMovementState();
            notifyPositionChange();
        } catch (const std::exception& e) {
            spdlog::warn("Error in position monitoring: {}", e.what());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(monitor_interval_ms_));
    }
    
    spdlog::debug("Position monitoring loop ended");
}

auto PositionManager::calculateOptimalDirection(double from_angle, double to_angle) -> bool {
    auto [diff, clockwise] = getOptimalPath(from_angle, to_angle);
    return clockwise;
}

} // namespace lithium::device::ascom::rotator::components
