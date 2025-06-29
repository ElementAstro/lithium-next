/*
 * motion_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Motion Controller Implementation

This component manages all telescope motion operations including
slewing, directional movement, speed control, and motion state tracking.

*************************************************/

#include "motion_controller.hpp"
#include "hardware_interface.hpp"

#include "atom/log/loguru.hpp"
#include "atom/utils/string.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace lithium::device::indi::telescope::components {

MotionController::MotionController(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }

    // Initialize available slew rates (degrees per second)
    availableSlewRates_ = {0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
}

MotionController::~MotionController() {
    shutdown();
}

bool MotionController::initialize() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (initialized_) {
        logWarning("Motion controller already initialized");
        return true;
    }

    if (!hardware_->isConnected()) {
        logError("Hardware interface not connected");
        return false;
    }

    try {
        // Reset state
        currentState_ = MotionState::IDLE;
        currentSlewRate_ = SlewRate::CENTERING;
        customSlewSpeed_ = 1.0;

        // Initialize motion status
        currentStatus_ = MotionStatus{};
        currentStatus_.state = MotionState::IDLE;
        currentStatus_.lastUpdate = std::chrono::steady_clock::now();

        // Register for property updates
        hardware_->registerPropertyCallback("EQUATORIAL_EOD_COORD",
            [this](const std::string& name) { onCoordinateUpdate(); });
        hardware_->registerPropertyCallback("TELESCOPE_SLEW_RATE",
            [this](const std::string& name) { handlePropertyUpdate(name); });
        hardware_->registerPropertyCallback("TELESCOPE_MOTION_NS",
            [this](const std::string& name) { onMotionStateUpdate(); });
        hardware_->registerPropertyCallback("TELESCOPE_MOTION_WE",
            [this](const std::string& name) { onMotionStateUpdate(); });

        initialized_ = true;
        logInfo("Motion controller initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Failed to initialize motion controller: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return true;
    }

    try {
        // Stop any ongoing motion
        stopAllMotion();

        // Clear callbacks
        motionCompleteCallback_ = nullptr;
        motionProgressCallback_ = nullptr;

        initialized_ = false;
        currentState_ = MotionState::IDLE;

        logInfo("Motion controller shut down successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Error during motion controller shutdown: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::slewToCoordinates(double ra, double dec, bool enableTracking) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    if (!validateCoordinates(ra, dec)) {
        logError("Invalid coordinates: RA=" + std::to_string(ra) + ", DEC=" + std::to_string(dec));
        return false;
    }

    if (currentState_ == MotionState::SLEWING) {
        logWarning("Already slewing, aborting current slew");
        abortSlew();
    }

    try {
        // Prepare slew command
        currentSlewCommand_.targetRA = ra;
        currentSlewCommand_.targetDEC = dec;
        currentSlewCommand_.enableTracking = enableTracking;
        currentSlewCommand_.isSync = false;
        currentSlewCommand_.timestamp = std::chrono::steady_clock::now();

        // Execute slew via hardware interface
        if (hardware_->slewToCoordinates(ra, dec)) {
            currentState_ = MotionState::SLEWING;
            slewStartTime_ = std::chrono::steady_clock::now();

            // Update status
            updateMotionStatus();

            logInfo("Started slew to RA: " + std::to_string(ra) + "h, DEC: " + std::to_string(dec) + "°");
            return true;
        } else {
            logError("Hardware failed to start slew");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during slew: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::slewToAltAz(double azimuth, double altitude) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    if (!validateAltAz(azimuth, altitude)) {
        logError("Invalid Alt/Az coordinates: Az=" + std::to_string(azimuth) + "°, Alt=" + std::to_string(altitude) + "°");
        return false;
    }

    try {
        // Execute Alt/Az slew via hardware interface
        if (hardware_->slewToAltAz(azimuth, altitude)) {
            currentState_ = MotionState::SLEWING;
            slewStartTime_ = std::chrono::steady_clock::now();

            updateMotionStatus();

            logInfo("Started slew to Az: " + std::to_string(azimuth) + "°, Alt: " + std::to_string(altitude) + "°");
            return true;
        } else {
            logError("Hardware failed to start Alt/Az slew");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during Alt/Az slew: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::syncToCoordinates(double ra, double dec) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    if (!validateCoordinates(ra, dec)) {
        logError("Invalid sync coordinates: RA=" + std::to_string(ra) + ", DEC=" + std::to_string(dec));
        return false;
    }

    try {
        // Prepare sync command
        currentSlewCommand_.targetRA = ra;
        currentSlewCommand_.targetDEC = dec;
        currentSlewCommand_.isSync = true;
        currentSlewCommand_.timestamp = std::chrono::steady_clock::now();

        // Execute sync via hardware interface
        if (hardware_->syncToCoordinates(ra, dec)) {
            logInfo("Synced to RA: " + std::to_string(ra) + "h, DEC: " + std::to_string(dec) + "°");
            return true;
        } else {
            logError("Hardware failed to sync coordinates");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during sync: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::abortSlew() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    try {
        if (hardware_->abortSlew()) {
            currentState_ = MotionState::ABORTING;

            // Wait briefly for abort to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            currentState_ = MotionState::IDLE;

            updateMotionStatus();

            if (motionCompleteCallback_) {
                motionCompleteCallback_(false, "Slew aborted by user");
            }

            logInfo("Slew aborted successfully");
            return true;
        } else {
            logError("Hardware failed to abort slew");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during abort: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::isSlewing() const {
    return currentState_ == MotionState::SLEWING;
}

bool MotionController::startDirectionalMove(MotionNS nsDirection, MotionEW ewDirection) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    try {
        bool success = true;

        // Start NS motion if specified
        if (nsDirection != MotionNS::MOTION_STOP) {
            success &= hardware_->startDirectionalMove(nsDirection, MotionEW::MOTION_STOP);
            if (success) {
                currentState_ = (nsDirection == MotionNS::MOTION_NORTH) ?
                    MotionState::MOVING_NORTH : MotionState::MOVING_SOUTH;
            }
        }

        // Start EW motion if specified
        if (ewDirection != MotionEW::MOTION_STOP) {
            success &= hardware_->startDirectionalMove(MotionNS::MOTION_STOP, ewDirection);
            if (success) {
                currentState_ = (ewDirection == MotionEW::MOTION_EAST) ?
                    MotionState::MOVING_EAST : MotionState::MOVING_WEST;
            }
        }

        if (success) {
            updateMotionStatus();
            logInfo("Started directional movement");
        } else {
            logError("Failed to start directional movement");
        }

        return success;

    } catch (const std::exception& e) {
        logError("Exception during directional move: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::stopDirectionalMove(MotionNS nsDirection, MotionEW ewDirection) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    try {
        bool success = hardware_->stopDirectionalMove(nsDirection, ewDirection);

        if (success) {
            // Check if all motion has stopped
            if (nsDirection != MotionNS::MOTION_STOP && ewDirection != MotionEW::MOTION_STOP) {
                currentState_ = MotionState::IDLE;
            }

            updateMotionStatus();
            logInfo("Stopped directional movement");
        } else {
            logError("Failed to stop directional movement");
        }

        return success;

    } catch (const std::exception& e) {
        logError("Exception during stop directional move: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::stopAllMotion() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    try {
        bool success = hardware_->stopAllMotion();

        if (success) {
            currentState_ = MotionState::IDLE;
            updateMotionStatus();
            logInfo("All motion stopped");
        } else {
            logError("Failed to stop all motion");
        }

        return success;

    } catch (const std::exception& e) {
        logError("Exception during stop all motion: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::setSlewRate(SlewRate rate) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    try {
        if (hardware_->setSlewRate(rate)) {
            currentSlewRate_ = rate;
            logInfo("Set slew rate to: " + std::to_string(static_cast<int>(rate)));
            return true;
        } else {
            logError("Failed to set slew rate");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during set slew rate: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::setSlewRate(double degreesPerSecond) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Motion controller not initialized");
        return false;
    }

    if (degreesPerSecond <= 0.0 || degreesPerSecond > 10.0) {
        logError("Invalid slew rate: " + std::to_string(degreesPerSecond) + " deg/s");
        return false;
    }

    try {
        if (hardware_->setSlewRate(degreesPerSecond)) {
            customSlewSpeed_ = degreesPerSecond;
            logInfo("Set custom slew rate to: " + std::to_string(degreesPerSecond) + " deg/s");
            return true;
        } else {
            logError("Failed to set custom slew rate");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during set custom slew rate: " + std::string(e.what()));
        return false;
    }
}

std::optional<SlewRate> MotionController::getCurrentSlewRate() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    return currentSlewRate_;
}

std::optional<double> MotionController::getCurrentSlewSpeed() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    return customSlewSpeed_;
}

std::vector<double> MotionController::getAvailableSlewRates() const {
    return availableSlewRates_;
}

std::string MotionController::getMotionStateString() const {
    return stateToString(currentState_);
}

MotionController::MotionStatus MotionController::getMotionStatus() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return currentStatus_;
}

bool MotionController::isMoving() const {
    MotionState state = currentState_;
    return state != MotionState::IDLE && state != MotionState::ERROR;
}

bool MotionController::canMove() const {
    return initialized_ && currentState_ != MotionState::ERROR;
}

double MotionController::getSlewProgress() const {
    return calculateSlewProgress();
}

std::chrono::seconds MotionController::getEstimatedSlewTime() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (currentState_ != MotionState::SLEWING) {
        return std::chrono::seconds(0);
    }

    // Calculate based on angular distance and slew rate
    double distance = calculateAngularDistance(
        currentStatus_.currentRA, currentStatus_.currentDEC,
        currentSlewCommand_.targetRA, currentSlewCommand_.targetDEC);

    double slewSpeed = customSlewSpeed_; // degrees per second
    return std::chrono::seconds(static_cast<long>(distance / slewSpeed));
}

std::chrono::seconds MotionController::getElapsedSlewTime() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (currentState_ != MotionState::SLEWING) {
        return std::chrono::seconds(0);
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - slewStartTime_);
    return elapsed;
}

bool MotionController::setTargetCoordinates(double ra, double dec) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!validateCoordinates(ra, dec)) {
        return false;
    }

    currentSlewCommand_.targetRA = ra;
    currentSlewCommand_.targetDEC = dec;
    currentStatus_.targetRA = ra;
    currentStatus_.targetDEC = dec;

    return true;
}

std::optional<std::pair<double, double>> MotionController::getTargetCoordinates() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    return std::make_pair(currentStatus_.targetRA, currentStatus_.targetDEC);
}

std::optional<std::pair<double, double>> MotionController::getCurrentCoordinates() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    return std::make_pair(currentStatus_.currentRA, currentStatus_.currentDEC);
}

bool MotionController::emergencyStop() {
    try {
        if (hardware_->emergencyStop()) {
            currentState_ = MotionState::IDLE;
            updateMotionStatus();

            if (motionCompleteCallback_) {
                motionCompleteCallback_(false, "Emergency stop activated");
            }

            logWarning("Emergency stop activated");
            return true;
        }
        return false;

    } catch (const std::exception& e) {
        logError("Exception during emergency stop: " + std::string(e.what()));
        return false;
    }
}

bool MotionController::recoverFromError() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (currentState_ != MotionState::ERROR) {
        return true;
    }

    try {
        // Attempt to reset hardware state
        if (hardware_->resetConnection()) {
            currentState_ = MotionState::IDLE;
            currentStatus_.errorMessage.clear();
            updateMotionStatus();

            logInfo("Recovered from error state");
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        logError("Exception during error recovery: " + std::string(e.what()));
        return false;
    }
}

// Private methods

void MotionController::updateMotionStatus() {
    auto now = std::chrono::steady_clock::now();

    currentStatus_.state = currentState_;
    currentStatus_.lastUpdate = now;
    currentStatus_.slewProgress = calculateSlewProgress();

    // Get current coordinates from hardware
    auto coords = hardware_->getCurrentCoordinates();
    if (coords.has_value()) {
        currentStatus_.currentRA = coords->first;
        currentStatus_.currentDEC = coords->second;
    }

    // Update target coordinates
    currentStatus_.targetRA = currentSlewCommand_.targetRA;
    currentStatus_.targetDEC = currentSlewCommand_.targetDEC;

    // Trigger progress callback
    if (motionProgressCallback_) {
        motionProgressCallback_(currentStatus_);
    }
}

void MotionController::handlePropertyUpdate(const std::string& propertyName) {
    if (propertyName == "TELESCOPE_SLEW_RATE") {
        // Handle slew rate property update
        auto rate = hardware_->getCurrentSlewRate();
        if (rate.has_value()) {
            currentSlewRate_ = rate.value();
        }
    }

    updateMotionStatus();
}

double MotionController::calculateSlewProgress() const {
    if (currentState_ != MotionState::SLEWING) {
        return 0.0;
    }

    // Calculate progress based on angular distance
    double totalDistance = calculateAngularDistance(
        currentStatus_.currentRA, currentStatus_.currentDEC,
        currentSlewCommand_.targetRA, currentSlewCommand_.targetDEC);

    if (totalDistance < 0.01) { // Very close, consider complete
        return 1.0;
    }

    double remainingDistance = calculateAngularDistance(
        currentStatus_.currentRA, currentStatus_.currentDEC,
        currentSlewCommand_.targetRA, currentSlewCommand_.targetDEC);

    double progress = 1.0 - (remainingDistance / totalDistance);
    return std::max(0.0, std::min(1.0, progress));
}

double MotionController::calculateAngularDistance(double ra1, double dec1, double ra2, double dec2) const {
    // Convert to radians
    double ra1_rad = ra1 * M_PI / 12.0;  // hours to radians
    double dec1_rad = dec1 * M_PI / 180.0;
    double ra2_rad = ra2 * M_PI / 12.0;
    double dec2_rad = dec2 * M_PI / 180.0;

    // Calculate angular separation using spherical law of cosines
    double cos_sep = std::sin(dec1_rad) * std::sin(dec2_rad) +
                     std::cos(dec1_rad) * std::cos(dec2_rad) * std::cos(ra1_rad - ra2_rad);

    cos_sep = std::max(-1.0, std::min(1.0, cos_sep)); // Clamp to valid range
    double separation = std::acos(cos_sep);

    // Convert back to degrees
    return separation * 180.0 / M_PI;
}

std::string MotionController::stateToString(MotionState state) const {
    switch (state) {
        case MotionState::IDLE:         return "IDLE";
        case MotionState::SLEWING:      return "SLEWING";
        case MotionState::TRACKING:     return "TRACKING";
        case MotionState::MOVING_NORTH: return "MOVING_NORTH";
        case MotionState::MOVING_SOUTH: return "MOVING_SOUTH";
        case MotionState::MOVING_EAST:  return "MOVING_EAST";
        case MotionState::MOVING_WEST:  return "MOVING_WEST";
        case MotionState::ABORTING:     return "ABORTING";
        case MotionState::ERROR:        return "ERROR";
        default:                        return "UNKNOWN";
    }
}

void MotionController::onCoordinateUpdate() {
    updateMotionStatus();

    // Check if slew is complete
    if (currentState_ == MotionState::SLEWING) {
        double progress = calculateSlewProgress();
        if (progress >= 0.95) { // Consider slew complete at 95%
            currentState_ = MotionState::IDLE;

            if (motionCompleteCallback_) {
                motionCompleteCallback_(true, "Slew completed successfully");
            }

            logInfo("Slew completed");
        }
    }
}

void MotionController::onSlewStateUpdate() {
    // Handle slew state changes from hardware
    updateMotionStatus();
}

void MotionController::onMotionStateUpdate() {
    // Handle motion state changes from hardware
    updateMotionStatus();
}

bool MotionController::validateCoordinates(double ra, double dec) const {
    // RA should be 0-24 hours
    if (ra < 0.0 || ra >= 24.0) {
        return false;
    }

    // DEC should be -90 to +90 degrees
    if (dec < -90.0 || dec > 90.0) {
        return false;
    }

    return true;
}

bool MotionController::validateAltAz(double azimuth, double altitude) const {
    // Azimuth should be 0-360 degrees
    if (azimuth < 0.0 || azimuth >= 360.0) {
        return false;
    }

    // Altitude should be -90 to +90 degrees (though typically 0-90)
    if (altitude < -90.0 || altitude > 90.0) {
        return false;
    }

    return true;
}

void MotionController::logInfo(const std::string& message) {
    LOG_F(INFO, "[MotionController] {}", message);
}

void MotionController::logWarning(const std::string& message) {
    LOG_F(WARNING, "[MotionController] {}", message);
}

void MotionController::logError(const std::string& message) {
    LOG_F(ERROR, "[MotionController] {}", message);
}

} // namespace lithium::device::indi::telescope::components
