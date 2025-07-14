/*
 * motion_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "motion_controller.hpp"
#include "hardware_interface.hpp"
#include <cmath>
#include <sstream>

namespace lithium::device::indi::telescope::components {

MotionController::MotionController(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware))
    , initialized_(false)
    , currentState_(MotionState::IDLE)
    , currentSlewRate_(SlewRate::CENTERING)
    , customSlewSpeed_(1.0)
{
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }
}

MotionController::~MotionController() {
    shutdown();
}

bool MotionController::initialize() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (initialized_) {
        return true;
    }

    if (!hardware_->isInitialized()) {
        logError("Hardware interface not initialized");
        return false;
    }

    // Initialize available slew rates
    availableSlewRates_ = {0.1, 0.5, 1.0, 2.0, 5.0}; // degrees per second

    // Set up property update callback
    hardware_->setPropertyUpdateCallback([this](const std::string& propertyName, const INDI::Property& property) {
        handlePropertyUpdate(propertyName);
    });

    // Initialize motion status
    updateMotionStatus();

    initialized_ = true;
    logInfo("Motion controller initialized successfully");
    return true;
}

bool MotionController::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return true;
    }

    // Stop any ongoing motion
    abortSlew();
    stopAllMotion();

    initialized_ = false;
    currentState_ = MotionState::IDLE;

    logInfo("Motion controller shutdown successfully");
    return true;
}

bool MotionController::slewToCoordinates(double ra, double dec, bool enableTracking) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_ || !hardware_->isConnected()) {
        logError("Motion controller not ready for slewing");
        return false;
    }

    if (!validateCoordinates(ra, dec)) {
        logError("Invalid coordinates for slewing");
        return false;
    }

    // Set target coordinates
    if (!hardware_->setTargetCoordinates(ra, dec)) {
        logError("Failed to set target coordinates");
        return false;
    }

    // Start slewing
    if (!hardware_->setTelescopeAction("SLEW")) {
        logError("Failed to start slewing");
        return false;
    }

    // Update internal state
    currentSlewCommand_.targetRA = ra;
    currentSlewCommand_.targetDEC = dec;
    currentSlewCommand_.enableTracking = enableTracking;
    currentSlewCommand_.isSync = false;
    currentSlewCommand_.timestamp = std::chrono::steady_clock::now();
    slewStartTime_ = currentSlewCommand_.timestamp;

    currentState_ = MotionState::SLEWING;

    logInfo("Started slewing to RA: " + std::to_string(ra) + ", DEC: " + std::to_string(dec));
    return true;
}

bool MotionController::abortSlew() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_ || !hardware_->isConnected()) {
        return false;
    }

    if (!hardware_->setTelescopeAction("ABORT")) {
        logError("Failed to abort slew");
        return false;
    }

    currentState_ = MotionState::ABORTING;
    logInfo("Slew aborted");

    if (motionCompleteCallback_) {
        motionCompleteCallback_(false, "Slew aborted by user");
    }

    return true;
}

bool MotionController::isSlewing() const {
    return currentState_ == MotionState::SLEWING;
}

// Implementation of other methods...
// [Simplified for brevity - would include all methods from header]

void MotionController::logInfo(const std::string& message) {
    // Implementation depends on logging system
}

void MotionController::logWarning(const std::string& message) {
    // Implementation depends on logging system
}

void MotionController::logError(const std::string& message) {
    // Implementation depends on logging system
}

} // namespace lithium::device::indi::telescope::components
