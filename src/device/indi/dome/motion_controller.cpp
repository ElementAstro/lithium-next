/*
 * motion_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "motion_controller.hpp"
#include "core/indi_dome_core.hpp"
#include "property_manager.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace lithium::device::indi {

MotionController::MotionController(std::shared_ptr<INDIDomeCore> core)
    : DomeComponentBase(std::move(core), "MotionController") {
}

auto MotionController::initialize() -> bool {
    if (isInitialized()) {
        logWarning("Already initialized");
        return true;
    }

    auto core = getCore();
    if (!core) {
        logError("Core is null, cannot initialize");
        return false;
    }

    try {
        // Initialize motion state
        current_azimuth_.store(0.0);
        target_azimuth_.store(0.0);
        is_moving_.store(false);
        motion_direction_.store(static_cast<int>(DomeMotion::STOP));
        
        // Reset statistics
        total_rotation_.store(0.0);
        motion_count_.store(0);
        average_speed_.store(0.0);
        
        // Clear emergency stop
        emergency_stop_active_.store(false);
        
        logInfo("Motion controller initialized");
        setInitialized(true);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize: " + std::string(ex.what()));
        return false;
    }
}

auto MotionController::cleanup() -> bool {
    if (!isInitialized()) {
        return true;
    }

    try {
        // Stop any ongoing motion
        if (is_moving_.load()) {
            stopRotation();
        }
        
        setInitialized(false);
        logInfo("Motion controller cleaned up");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to cleanup: " + std::string(ex.what()));
        return false;
    }
}

void MotionController::handlePropertyUpdate(const INDI::Property& property) {
    if (!isOurProperty(property)) {
        return;
    }

    const std::string prop_name = property.getName();
    
    if (prop_name == "ABS_DOME_POSITION") {
        handleAzimuthUpdate(property);
    } else if (prop_name == "DOME_MOTION") {
        handleMotionUpdate(property);
    } else if (prop_name == "DOME_SPEED") {
        handleSpeedUpdate(property);
    }
}

// Core motion commands
auto MotionController::moveToAzimuth(double azimuth) -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    if (!validateAzimuth(azimuth) || !canStartMotion()) {
        return false;
    }
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    // Normalize target azimuth
    double normalized_azimuth = normalizeAzimuth(azimuth);
    
    // Apply backlash compensation if enabled
    if (backlash_enabled_.load()) {
        normalized_azimuth = calculateBacklashCompensation(normalized_azimuth);
    }
    
    // Update target
    updateTargetAzimuth(normalized_azimuth);
    
    // Start motion
    last_motion_start_ = std::chrono::steady_clock::now();
    notifyMotionStart(normalized_azimuth);
    
    bool success = prop_mgr->moveToAzimuth(normalized_azimuth);
    if (success) {
        updateMotionState(true);
        incrementMotionCount();
        logInfo("Moving to azimuth: " + std::to_string(normalized_azimuth) + "°");
    } else {
        logError("Failed to start motion to azimuth: " + std::to_string(azimuth));
    }
    
    return success;
}

auto MotionController::rotateClockwise() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    if (!canStartMotion()) {
        return false;
    }
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    last_motion_start_ = std::chrono::steady_clock::now();
    updateMotionDirection(DomeMotion::CLOCKWISE);
    updateMotionState(true);
    
    bool success = prop_mgr->startRotation(true);
    if (success) {
        logInfo("Starting clockwise rotation");
    } else {
        logError("Failed to start clockwise rotation");
        updateMotionState(false);
    }
    
    return success;
}

auto MotionController::rotateCounterClockwise() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    if (!canStartMotion()) {
        return false;
    }
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    last_motion_start_ = std::chrono::steady_clock::now();
    updateMotionDirection(DomeMotion::COUNTER_CLOCKWISE);
    updateMotionState(true);
    
    bool success = prop_mgr->startRotation(false);
    if (success) {
        logInfo("Starting counter-clockwise rotation");
    } else {
        logError("Failed to start counter-clockwise rotation");
        updateMotionState(false);
    }
    
    return success;
}

auto MotionController::stopRotation() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    bool success = prop_mgr->stopRotation();
    if (success) {
        updateMotionState(false);
        updateMotionDirection(DomeMotion::STOP);
        
        // Calculate motion duration
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_motion_start_);
        last_motion_duration_ms_.store(duration.count());
        
        notifyMotionComplete(true, "Motion stopped");
        logInfo("Rotation stopped");
    } else {
        logError("Failed to stop rotation");
    }
    
    return success;
}

auto MotionController::abortMotion() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    bool success = prop_mgr->abortMotion();
    if (success) {
        updateMotionState(false);
        updateMotionDirection(DomeMotion::STOP);
        
        // Calculate motion duration
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_motion_start_);
        last_motion_duration_ms_.store(duration.count());
        
        notifyMotionComplete(false, "Motion aborted");
        logInfo("Motion aborted");
    } else {
        logError("Failed to abort motion");
    }
    
    return success;
}

auto MotionController::syncAzimuth(double azimuth) -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    if (!validateAzimuth(azimuth)) {
        return false;
    }
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    double normalized_azimuth = normalizeAzimuth(azimuth);
    bool success = prop_mgr->syncAzimuth(normalized_azimuth);
    
    if (success) {
        updateCurrentAzimuth(normalized_azimuth);
        updateTargetAzimuth(normalized_azimuth);
        logInfo("Synced azimuth to: " + std::to_string(normalized_azimuth) + "°");
    } else {
        logError("Failed to sync azimuth");
    }
    
    return success;
}

// Speed control
auto MotionController::getRotationSpeed() -> std::optional<double> {
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        return std::nullopt;
    }
    
    return prop_mgr->getCurrentSpeed();
}

auto MotionController::setRotationSpeed(double speed) -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    if (!validateSpeed(speed)) {
        logError("Invalid speed: " + std::to_string(speed));
        return false;
    }
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    bool success = prop_mgr->setSpeed(speed);
    if (success) {
        updateSpeed(speed);
        logInfo("Set rotation speed to: " + std::to_string(speed));
    } else {
        logError("Failed to set rotation speed");
    }
    
    return success;
}

auto MotionController::getMotionDirection() const -> DomeMotion {
    return static_cast<DomeMotion>(motion_direction_.load());
}

auto MotionController::getRemainingDistance() const -> double {
    double current = current_azimuth_.load();
    double target = target_azimuth_.load();
    return getAzimuthalDistance(current, target);
}

auto MotionController::getEstimatedTimeToTarget() const -> std::chrono::seconds {
    double remaining = getRemainingDistance();
    double speed = current_speed_.load();
    
    if (speed <= 0.0) {
        return std::chrono::seconds(0);
    }
    
    double time_seconds = remaining / speed;
    return std::chrono::seconds(static_cast<int64_t>(time_seconds));
}

// Backlash compensation
auto MotionController::getBacklash() -> double {
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        return backlash_value_.load();
    }
    
    auto backlash = prop_mgr->getBacklash();
    if (backlash) {
        backlash_value_.store(*backlash);
        return *backlash;
    }
    
    return backlash_value_.load();
}

auto MotionController::setBacklash(double backlash) -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    auto prop_mgr = property_manager_.lock();
    if (!prop_mgr) {
        logError("Property manager not available");
        return false;
    }
    
    // Try to set via INDI property first
    if (prop_mgr->hasBacklash()) {
        bool success = prop_mgr->setNumberValue("DOME_BACKLASH", "DOME_BACKLASH_VALUE", backlash);
        if (success) {
            backlash_value_.store(backlash);
            logInfo("Set backlash compensation to: " + std::to_string(backlash) + "°");
            return true;
        }
    }
    
    // Fall back to local storage
    backlash_value_.store(backlash);
    logInfo("Set local backlash compensation to: " + std::to_string(backlash) + "°");
    return true;
}

auto MotionController::enableBacklashCompensation(bool enable) -> bool {
    backlash_enabled_.store(enable);
    logInfo("Backlash compensation " + std::string(enable ? "enabled" : "disabled"));
    return true;
}

// Motion planning
auto MotionController::calculateOptimalPath(double from, double to) -> std::pair<double, DomeMotion> {
    return getShortestPath(from, to);
}

auto MotionController::normalizeAzimuth(double azimuth) const -> double {
    while (azimuth < 0.0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}

auto MotionController::getAzimuthalDistance(double from, double to) const -> double {
    double diff = normalizeAzimuth(to - from);
    return std::min(diff, 360.0 - diff);
}

auto MotionController::getShortestPath(double from, double to) const -> std::pair<double, DomeMotion> {
    double normalized_from = normalizeAzimuth(from);
    double normalized_to = normalizeAzimuth(to);
    
    double clockwise = normalizeAzimuth(normalized_to - normalized_from);
    double counter_clockwise = 360.0 - clockwise;
    
    if (clockwise <= counter_clockwise) {
        return {clockwise, DomeMotion::CLOCKWISE};
    } else {
        return {counter_clockwise, DomeMotion::COUNTER_CLOCKWISE};
    }
}

// Motion limits and safety
auto MotionController::setSpeedLimits(double minSpeed, double maxSpeed) -> bool {
    if (minSpeed < 0.0 || maxSpeed <= minSpeed) {
        logError("Invalid speed limits");
        return false;
    }
    
    min_speed_ = minSpeed;
    max_speed_ = maxSpeed;
    logInfo("Set speed limits: [" + std::to_string(minSpeed) + ", " + std::to_string(maxSpeed) + "]");
    return true;
}

auto MotionController::setAzimuthLimits(double minAz, double maxAz) -> bool {
    if (minAz < 0.0 || maxAz > 360.0 || minAz >= maxAz) {
        logError("Invalid azimuth limits");
        return false;
    }
    
    min_azimuth_ = minAz;
    max_azimuth_ = maxAz;
    logInfo("Set azimuth limits: [" + std::to_string(minAz) + "°, " + std::to_string(maxAz) + "°]");
    return true;
}

auto MotionController::setSafetyLimits(double maxAcceleration, double maxJerk) -> bool {
    if (maxAcceleration <= 0.0 || maxJerk <= 0.0) {
        logError("Invalid safety limits");
        return false;
    }
    
    max_acceleration_ = maxAcceleration;
    max_jerk_ = maxJerk;
    logInfo("Set safety limits - Accel: " + std::to_string(maxAcceleration) + 
            ", Jerk: " + std::to_string(maxJerk));
    return true;
}

auto MotionController::isPositionSafe(double azimuth) const -> bool {
    if (!safety_limits_enabled_.load()) {
        return true;
    }
    
    double normalized = normalizeAzimuth(azimuth);
    return normalized >= min_azimuth_ && normalized <= max_azimuth_;
}

auto MotionController::isSpeedSafe(double speed) const -> bool {
    if (!safety_limits_enabled_.load()) {
        return true;
    }
    
    return speed >= min_speed_ && speed <= max_speed_;
}

// Motion profiling
auto MotionController::enableMotionProfiling(bool enable) -> bool {
    motion_profiling_enabled_.store(enable);
    logInfo("Motion profiling " + std::string(enable ? "enabled" : "disabled"));
    return true;
}

auto MotionController::setAccelerationProfile(double acceleration, double deceleration) -> bool {
    if (acceleration <= 0.0 || deceleration <= 0.0) {
        logError("Invalid acceleration profile");
        return false;
    }
    
    acceleration_rate_ = acceleration;
    deceleration_rate_ = deceleration;
    logInfo("Set acceleration profile - Accel: " + std::to_string(acceleration) + 
            ", Decel: " + std::to_string(deceleration));
    return true;
}

auto MotionController::getMotionProfile() const -> std::string {
    return "Acceleration: " + std::to_string(acceleration_rate_) + 
           "°/s², Deceleration: " + std::to_string(deceleration_rate_) + "°/s²";
}

// Statistics
auto MotionController::resetTotalRotation() -> bool {
    total_rotation_.store(0.0);
    logInfo("Total rotation counter reset");
    return true;
}

auto MotionController::getLastMotionDuration() const -> std::chrono::milliseconds {
    return std::chrono::milliseconds(last_motion_duration_ms_.load());
}

// Emergency functions
auto MotionController::emergencyStop() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    emergency_stop_active_.store(true);
    bool success = abortMotion();
    
    if (success) {
        logWarning("Emergency stop activated");
    } else {
        logError("Failed to activate emergency stop");
    }
    
    return success;
}

auto MotionController::clearEmergencyStop() -> bool {
    std::lock_guard<std::recursive_mutex> lock(motion_mutex_);
    
    emergency_stop_active_.store(false);
    logInfo("Emergency stop cleared");
    return true;
}

// Private methods
void MotionController::updateCurrentAzimuth(double azimuth) {
    double old_azimuth = current_azimuth_.exchange(azimuth);
    
    // Update statistics
    double distance = getAzimuthalDistance(old_azimuth, azimuth);
    total_rotation_.fetch_add(distance);
    
    notifyPositionUpdate();
}

void MotionController::updateTargetAzimuth(double azimuth) {
    target_azimuth_.store(azimuth);
}

void MotionController::updateMotionState(bool moving) {
    is_moving_.store(moving);
    
    if (!moving) {
        updateMotionDirection(DomeMotion::STOP);
    }
}

void MotionController::updateMotionDirection(DomeMotion direction) {
    motion_direction_.store(static_cast<int>(direction));
}

void MotionController::updateSpeed(double speed) {
    current_speed_.store(speed);
    
    // Update average speed
    uint64_t count = motion_count_.load();
    if (count > 0) {
        double current_avg = average_speed_.load();
        double new_avg = (current_avg * count + speed) / (count + 1);
        average_speed_.store(new_avg);
    } else {
        average_speed_.store(speed);
    }
}

auto MotionController::calculateBacklashCompensation(double targetAz) -> double {
    if (!backlash_enabled_.load()) {
        return targetAz;
    }
    
    double backlash = backlash_value_.load();
    if (backlash == 0.0) {
        return targetAz;
    }
    
    // Apply backlash based on direction
    double current = current_azimuth_.load();
    auto [distance, direction] = getShortestPath(current, targetAz);
    
    if (direction == DomeMotion::CLOCKWISE) {
        return normalizeAzimuth(targetAz + backlash);
    } else {
        return normalizeAzimuth(targetAz - backlash);
    }
}

auto MotionController::applyMotionProfile(double distance, double speed) -> std::pair<double, double> {
    if (!motion_profiling_enabled_.load()) {
        return {distance, speed};
    }
    
    // Simple trapezoidal motion profile
    double accel_time = speed / acceleration_rate_;
    double accel_distance = 0.5 * acceleration_rate_ * accel_time * accel_time;
    
    if (distance <= 2 * accel_distance) {
        // Triangle profile (not enough distance for full acceleration)
        double max_speed = std::sqrt(distance * acceleration_rate_);
        return {distance, std::min(max_speed, speed)};
    }
    
    // Trapezoid profile
    return {distance, speed};
}

void MotionController::notifyMotionStart(double targetAzimuth) {
    if (motion_start_callback_) {
        motion_start_callback_(targetAzimuth);
    }
    
    auto core = getCore();
    if (core) {
        // Notify core about motion start
    }
}

void MotionController::notifyMotionComplete(bool success, const std::string& message) {
    if (motion_complete_callback_) {
        motion_complete_callback_(success, message);
    }
    
    auto core = getCore();
    if (core) {
        core->notifyMoveComplete(success, message);
    }
}

void MotionController::notifyPositionUpdate() {
    if (position_update_callback_) {
        position_update_callback_(current_azimuth_.load(), target_azimuth_.load());
    }
    
    auto core = getCore();
    if (core) {
        core->notifyAzimuthChange(current_azimuth_.load());
    }
}

auto MotionController::validateAzimuth(double azimuth) const -> bool {
    if (std::isnan(azimuth) || std::isinf(azimuth)) {
        return false;
    }
    
    if (safety_limits_enabled_.load()) {
        return isPositionSafe(azimuth);
    }
    
    return true;
}

auto MotionController::validateSpeed(double speed) const -> bool {
    if (std::isnan(speed) || std::isinf(speed) || speed < 0.0) {
        return false;
    }
    
    if (safety_limits_enabled_.load()) {
        return isSpeedSafe(speed);
    }
    
    return true;
}

auto MotionController::canStartMotion() const -> bool {
    if (emergency_stop_active_.load()) {
        logWarning("Cannot start motion: emergency stop active");
        return false;
    }
    
    auto core = getCore();
    if (!core || !core->isConnected()) {
        logWarning("Cannot start motion: not connected");
        return false;
    }
    
    return true;
}

void MotionController::updateMotionStatistics(double distance, std::chrono::milliseconds duration) {
    if (duration.count() > 0) {
        double speed = distance / (duration.count() / 1000.0); // degrees per second
        updateSpeed(speed);
    }
}

void MotionController::incrementMotionCount() {
    motion_count_.fetch_add(1);
}

// Property update handlers
void MotionController::handleAzimuthUpdate(const INDI::Property& property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    auto number_prop = property.getNumber();
    if (!number_prop) {
        return;
    }
    
    auto azimuth_widget = number_prop->findWidgetByName("DOME_ABSOLUTE_POSITION");
    if (azimuth_widget) {
        double azimuth = azimuth_widget->getValue();
        updateCurrentAzimuth(azimuth);
    }
}

void MotionController::handleMotionUpdate(const INDI::Property& property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    auto switch_prop = property.getSwitch();
    if (!switch_prop) {
        return;
    }
    
    bool moving = false;
    DomeMotion direction = DomeMotion::STOP;
    
    auto cw_widget = switch_prop->findWidgetByName("DOME_CW");
    auto ccw_widget = switch_prop->findWidgetByName("DOME_CCW");
    
    if (cw_widget && cw_widget->getState() == ISS_ON) {
        moving = true;
        direction = DomeMotion::CLOCKWISE;
    } else if (ccw_widget && ccw_widget->getState() == ISS_ON) {
        moving = true;
        direction = DomeMotion::COUNTER_CLOCKWISE;
    }
    
    updateMotionState(moving);
    updateMotionDirection(direction);
    
    if (!moving && is_moving_.load()) {
        // Motion just completed
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_motion_start_);
        last_motion_duration_ms_.store(duration.count());
        notifyMotionComplete(true, "Motion completed");
    }
}

void MotionController::handleSpeedUpdate(const INDI::Property& property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    auto number_prop = property.getNumber();
    if (!number_prop) {
        return;
    }
    
    auto speed_widget = number_prop->findWidgetByName("DOME_SPEED_VALUE");
    if (speed_widget) {
        double speed = speed_widget->getValue();
        updateSpeed(speed);
    }
}

} // namespace lithium::device::indi
