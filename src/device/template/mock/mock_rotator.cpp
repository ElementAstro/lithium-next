/*
 * mock_rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mock_rotator.hpp"

#include <cmath>

MockRotator::MockRotator(const std::string& name) 
    : AtomRotator(name), gen_(rd_()), noise_dist_(-0.1, 0.1) {
    // Set default capabilities
    RotatorCapabilities caps;
    caps.canAbsoluteMove = true;
    caps.canRelativeMove = true;
    caps.canAbort = true;
    caps.canReverse = true;
    caps.canSync = true;
    caps.hasTemperature = true;
    caps.hasBacklash = true;
    caps.minAngle = 0.0;
    caps.maxAngle = 360.0;
    caps.stepSize = 0.1;
    setRotatorCapabilities(caps);
    
    // Initialize current position to 0
    current_position_ = 0.0;
    target_position_ = 0.0;
}

bool MockRotator::initialize() {
    setState(DeviceState::IDLE);
    updateRotatorState(RotatorState::IDLE);
    return true;
}

bool MockRotator::destroy() {
    if (is_moving_) {
        abortMove();
    }
    setState(DeviceState::UNKNOWN);
    return true;
}

bool MockRotator::connect(const std::string& port, int timeout, int maxRetry) {
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (!isSimulated()) {
        // In real mode, we would actually connect to hardware
        return false;
    }
    
    connected_ = true;
    setState(DeviceState::IDLE);
    updateRotatorState(RotatorState::IDLE);
    return true;
}

bool MockRotator::disconnect() {
    if (is_moving_) {
        abortMove();
    }
    connected_ = false;
    setState(DeviceState::UNKNOWN);
    return true;
}

std::vector<std::string> MockRotator::scan() {
    if (isSimulated()) {
        return {"MockRotator_1", "MockRotator_2"};
    }
    return {};
}

bool MockRotator::isMoving() const {
    std::lock_guard<std::mutex> lock(move_mutex_);
    return is_moving_;
}

auto MockRotator::getPosition() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    
    addPositionNoise();
    return current_position_;
}

auto MockRotator::setPosition(double angle) -> bool {
    return moveToAngle(angle);
}

auto MockRotator::moveToAngle(double angle) -> bool {
    if (!isConnected()) return false;
    if (isMoving()) return false;
    
    double normalized_angle = normalizeAngle(angle);
    target_position_ = normalized_angle;
    
    updateRotatorState(RotatorState::MOVING);
    
    // Start move simulation in separate thread
    if (move_thread_.joinable()) {
        move_thread_.join();
    }
    
    move_thread_ = std::thread(&MockRotator::simulateMove, this, normalized_angle);
    return true;
}

auto MockRotator::rotateByAngle(double angle) -> bool {
    if (!isConnected()) return false;
    
    double new_position = normalizeAngle(current_position_ + angle);
    return moveToAngle(new_position);
}

auto MockRotator::abortMove() -> bool {
    if (!isConnected()) return false;
    
    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = false;
    }
    
    if (move_thread_.joinable()) {
        move_thread_.join();
    }
    
    updateRotatorState(RotatorState::IDLE);
    return true;
}

auto MockRotator::syncPosition(double angle) -> bool {
    if (!isConnected()) return false;
    if (isMoving()) return false;
    
    current_position_ = normalizeAngle(angle);
    return true;
}

auto MockRotator::getDirection() -> std::optional<RotatorDirection> {
    if (!isConnected()) return std::nullopt;
    
    if (!isMoving()) return std::nullopt;
    
    auto [distance, direction] = getShortestPath(current_position_, target_position_);
    return direction;
}

auto MockRotator::setDirection(RotatorDirection direction) -> bool {
    // This is mainly for informational purposes in mock implementation
    return true;
}

auto MockRotator::isReversed() -> bool {
    return is_reversed_;
}

auto MockRotator::setReversed(bool reversed) -> bool {
    is_reversed_ = reversed;
    return true;
}

auto MockRotator::getSpeed() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    return current_speed_;
}

auto MockRotator::setSpeed(double speed) -> bool {
    if (!isConnected()) return false;
    if (speed < getMinSpeed() || speed > getMaxSpeed()) return false;
    
    current_speed_ = speed;
    return true;
}

auto MockRotator::getMaxSpeed() -> double {
    return 30.0; // degrees per second
}

auto MockRotator::getMinSpeed() -> double {
    return 1.0; // degrees per second
}

auto MockRotator::getMinPosition() -> double {
    return rotator_capabilities_.minAngle;
}

auto MockRotator::getMaxPosition() -> double {
    return rotator_capabilities_.maxAngle;
}

auto MockRotator::setLimits(double min, double max) -> bool {
    if (min >= max) return false;
    
    rotator_capabilities_.minAngle = min;
    rotator_capabilities_.maxAngle = max;
    return true;
}

auto MockRotator::getBacklash() -> double {
    return backlash_angle_;
}

auto MockRotator::setBacklash(double backlash) -> bool {
    backlash_angle_ = std::abs(backlash);
    return true;
}

auto MockRotator::enableBacklashCompensation(bool enable) -> bool {
    // Mock implementation always returns true
    return true;
}

auto MockRotator::isBacklashCompensationEnabled() -> bool {
    return backlash_angle_ > 0.0;
}

auto MockRotator::getTemperature() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    if (!rotator_capabilities_.hasTemperature) return std::nullopt;
    
    return generateTemperature();
}

auto MockRotator::hasTemperatureSensor() -> bool {
    return rotator_capabilities_.hasTemperature;
}

auto MockRotator::savePreset(int slot, double angle) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;
    
    presets_[slot] = normalizeAngle(angle);
    return true;
}

auto MockRotator::loadPreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;
    if (!presets_[slot].has_value()) return false;
    
    return moveToAngle(*presets_[slot]);
}

auto MockRotator::getPreset(int slot) -> std::optional<double> {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return std::nullopt;
    return presets_[slot];
}

auto MockRotator::deletePreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;
    
    presets_[slot].reset();
    return true;
}

auto MockRotator::getTotalRotation() -> double {
    return total_rotation_;
}

auto MockRotator::resetTotalRotation() -> bool {
    total_rotation_ = 0.0;
    return true;
}

auto MockRotator::getLastMoveAngle() -> double {
    return last_move_angle_;
}

auto MockRotator::getLastMoveDuration() -> int {
    return last_move_duration_;
}

void MockRotator::simulateMove(double target_angle) {
    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = true;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    double start_position = current_position_;
    
    auto [total_distance, direction] = getShortestPath(current_position_, target_angle);
    
    // Apply reversal if enabled
    if (is_reversed_) {
        direction = (direction == RotatorDirection::CLOCKWISE) ? 
                   RotatorDirection::COUNTER_CLOCKWISE : RotatorDirection::CLOCKWISE;
    }
    
    // Calculate move duration based on speed
    double move_duration = total_distance / current_speed_;
    auto move_duration_ms = std::chrono::milliseconds(static_cast<int>(move_duration * 1000));
    
    // Simulate gradual movement
    const int steps = 20;
    auto step_duration = move_duration_ms / steps;
    double step_angle = total_distance / steps;
    
    if (direction == RotatorDirection::COUNTER_CLOCKWISE) {
        step_angle = -step_angle;
    }
    
    for (int i = 0; i < steps; ++i) {
        {
            std::lock_guard<std::mutex> lock(move_mutex_);
            if (!is_moving_) break; // Check for abort
        }
        
        std::this_thread::sleep_for(step_duration);
        
        // Update position
        current_position_ = normalizeAngle(current_position_ + step_angle);
        
        // Notify position change
        if (position_callback_) {
            position_callback_(current_position_);
        }
    }
    
    // Ensure we reach the exact target
    current_position_ = target_angle;
    
    auto end_time = std::chrono::steady_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Update statistics
    last_move_angle_ = getAngularDistance(start_position, target_angle);
    last_move_duration_ = actual_duration.count();
    total_rotation_ += last_move_angle_;
    
    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = false;
    }
    
    updateRotatorState(RotatorState::IDLE);
    
    // Notify move complete
    if (move_complete_callback_) {
        move_complete_callback_(true, "Move completed successfully");
    }
}

void MockRotator::addPositionNoise() {
    // Add small random noise to simulate encoder precision
    current_position_ += noise_dist_(gen_);
    current_position_ = normalizeAngle(current_position_);
}

double MockRotator::generateTemperature() const {
    // Generate realistic temperature around 20°C with some variation
    std::uniform_real_distribution<> temp_dist(15.0, 25.0);
    return temp_dist(gen_);
}
