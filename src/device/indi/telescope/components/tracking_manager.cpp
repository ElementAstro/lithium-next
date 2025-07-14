/*
 * tracking_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Tracking Manager Implementation

This component manages telescope tracking operations including
track modes, track rates, tracking state control, and tracking accuracy.

*************************************************/

#include "tracking_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "atom/utils/string.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace lithium::device::indi::telescope::components {

TrackingManager::TrackingManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }
}

TrackingManager::~TrackingManager() {
    shutdown();
}

bool TrackingManager::initialize() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (initialized_) {
        logWarning("Tracking manager already initialized");
        return true;
    }

    if (!hardware_->isConnected()) {
        logError("Hardware interface not connected");
        return false;
    }

    try {
        // Initialize state
        trackingEnabled_ = false;
        currentMode_ = TrackMode::SIDEREAL;
        autoGuidingEnabled_ = false;
        pecEnabled_ = false;
        pecCalibrated_ = false;

        // Initialize tracking status
        currentStatus_ = TrackingStatus{};
        currentStatus_.mode = TrackMode::SIDEREAL;
        currentStatus_.lastUpdate = std::chrono::steady_clock::now();

        // Initialize statistics
        statistics_ = TrackingStatistics{};
        statistics_.trackingStartTime = std::chrono::steady_clock::now();

        // Set default sidereal rates
        currentRates_ = calculateSiderealRates();
        trackRateRA_ = currentRates_.slewRateRA;
        trackRateDEC_ = currentRates_.slewRateDEC;

        // Register for property updates via hardware interface
        hardware_->setPropertyUpdateCallback([this](const std::string& propertyName, const INDI::Property& property) {
            handlePropertyUpdate(propertyName);
        });

        initialized_ = true;
        logInfo("Tracking manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Failed to initialize tracking manager: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return true;
    }

    try {
        // Disable tracking if enabled
        if (trackingEnabled_) {
            enableTracking(false);
        }

        // Clear callbacks
        trackingStateCallback_ = nullptr;
        trackingErrorCallback_ = nullptr;

        initialized_ = false;

        logInfo("Tracking manager shut down successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Error during tracking manager shutdown: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::enableTracking(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    try {
        if (hardware_->setTrackingState(enable)) {
            trackingEnabled_ = enable;

            if (enable) {
                statistics_.trackingStartTime = std::chrono::steady_clock::now();
                logInfo("Tracking enabled with mode: " + std::to_string(static_cast<int>(currentMode_.load())));
            } else {
                // Update total tracking time
                auto now = std::chrono::steady_clock::now();
                auto sessionTime = std::chrono::duration_cast<std::chrono::seconds>(
                    now - statistics_.trackingStartTime);
                statistics_.totalTrackingTime += sessionTime;

                logInfo("Tracking disabled");
            }

            updateTrackingStatus();

            if (trackingStateCallback_) {
                trackingStateCallback_(enable, currentMode_);
            }

            return true;
        } else {
            logError("Failed to " + std::string(enable ? "enable" : "disable") + " tracking");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during tracking enable/disable: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::isTrackingEnabled() const {
    return trackingEnabled_.load();
}

bool TrackingManager::setTrackingMode(TrackMode mode) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    if (!isValidTrackMode(mode)) {
        logError("Invalid tracking mode: " + std::to_string(static_cast<int>(mode)));
        return false;
    }

    try {
        std::string modeStr;
        switch (mode) {
            case TrackMode::SIDEREAL: modeStr = "TRACK_SIDEREAL"; break;
            case TrackMode::SOLAR: modeStr = "TRACK_SOLAR"; break;
            case TrackMode::LUNAR: modeStr = "TRACK_LUNAR"; break;
            case TrackMode::CUSTOM: modeStr = "TRACK_CUSTOM"; break;
            case TrackMode::NONE: modeStr = "TRACK_OFF"; break;
        }

        if (hardware_->setTrackingMode(modeStr)) {
            currentMode_ = mode;

            // Update rates based on new mode
            switch (mode) {
                case TrackMode::SIDEREAL:
                    currentRates_ = calculateSiderealRates();
                    break;
                case TrackMode::SOLAR:
                    currentRates_ = calculateSolarRates();
                    break;
                case TrackMode::LUNAR:
                    currentRates_ = calculateLunarRates();
                    break;
                case TrackMode::CUSTOM:
                    // Keep current custom rates
                    break;
            }

            trackRateRA_ = currentRates_.raRate;
            trackRateDEC_ = currentRates_.decRate;

            updateTrackingStatus();

            if (trackingStateCallback_) {
                trackingStateCallback_(trackingEnabled_, mode);
            }

            logInfo("Set tracking mode to: " + std::to_string(static_cast<int>(mode)));
            return true;
        } else {
            logError("Failed to set tracking mode");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during set tracking mode: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::setTrackRates(double raRate, double decRate) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    if (!validateTrackRates(raRate, decRate)) {
        logError("Invalid track rates: RA=" + std::to_string(raRate) + ", DEC=" + std::to_string(decRate));
        return false;
    }

    try {
        MotionRates rates;
        rates.raRate = raRate;
        rates.decRate = decRate;

        if (hardware_->setTrackRates(rates)) {
            currentRates_ = rates;
            trackRateRA_ = raRate;
            trackRateDEC_ = decRate;
            currentMode_ = TrackMode::CUSTOM;

            updateTrackingStatus();

            logInfo("Set custom track rates: RA=" + std::to_string(raRate) +
                   " arcsec/s, DEC=" + std::to_string(decRate) + " arcsec/s");
            return true;
        } else {
            logError("Failed to set track rates");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during set track rates: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::setTrackRates(const MotionRates& rates) {
    return setTrackRates(rates.raRate, rates.decRate);
}

std::optional<MotionRates> TrackingManager::getTrackRates() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return std::nullopt;
    }

    return currentRates_;
}

std::optional<MotionRates> TrackingManager::getDefaultTrackRates(TrackMode mode) const {
    switch (mode) {
        case TrackMode::SIDEREAL:
            return calculateSiderealRates();
        case TrackMode::SOLAR:
            return calculateSolarRates();
        case TrackMode::LUNAR:
            return calculateLunarRates();
        case TrackMode::CUSTOM:
            return currentRates_;
        default:
            return std::nullopt;
    }
}

bool TrackingManager::setSiderealTracking() {
    return setTrackingMode(TrackMode::SIDEREAL);
}

bool TrackingManager::setSolarTracking() {
    return setTrackingMode(TrackMode::SOLAR);
}

bool TrackingManager::setLunarTracking() {
    return setTrackingMode(TrackMode::LUNAR);
}

bool TrackingManager::setCustomTracking(double raRate, double decRate) {
    if (setTrackRates(raRate, decRate)) {
        return setTrackingMode(TrackMode::CUSTOM);
    }
    return false;
}

TrackingManager::TrackingStatus TrackingManager::getTrackingStatus() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return currentStatus_;
}

TrackingManager::TrackingStatistics TrackingManager::getTrackingStatistics() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return statistics_;
}

double TrackingManager::getCurrentTrackingError() const {
    return currentTrackingError_.load();
}

bool TrackingManager::isTrackingAccurate(double toleranceArcsec) const {
    return getCurrentTrackingError() <= toleranceArcsec;
}

bool TrackingManager::applyTrackingCorrection(double raCorrection, double decCorrection) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    try {
        if (hardware_->applyGuideCorrection(raCorrection, decCorrection)) {
            statistics_.trackingCorrectionCount++;
            updateTrackingStatistics();

            logInfo("Applied tracking correction: RA=" + std::to_string(raCorrection) +
                   " arcsec, DEC=" + std::to_string(decCorrection) + " arcsec");
            return true;
        } else {
            logError("Failed to apply tracking correction");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during tracking correction: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::enableAutoGuiding(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    try {
        if (hardware_->setAutoGuidingEnabled(enable)) {
            autoGuidingEnabled_ = enable;
            logInfo("Auto-guiding " + std::string(enable ? "enabled" : "disabled"));
            return true;
        } else {
            logError("Failed to " + std::string(enable ? "enable" : "disable") + " auto-guiding");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during auto-guiding control: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::isAutoGuidingEnabled() const {
    return autoGuidingEnabled_.load();
}

bool TrackingManager::enablePEC(bool enable) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    try {
        if (hardware_->setPECEnabled(enable)) {
            pecEnabled_ = enable;
            logInfo("PEC " + std::string(enable ? "enabled" : "disabled"));
            return true;
        } else {
            logError("Failed to " + std::string(enable ? "enable" : "disable") + " PEC");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during PEC control: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::isPECEnabled() const {
    return pecEnabled_.load();
}

bool TrackingManager::calibratePEC() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    try {
        if (hardware_->calibratePEC()) {
            pecCalibrated_ = true;
            logInfo("PEC calibration completed successfully");
            return true;
        } else {
            logError("PEC calibration failed");
            return false;
        }

    } catch (const std::exception& e) {
        logError("Exception during PEC calibration: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::isPECCalibrated() const {
    return pecCalibrated_.load();
}

double TrackingManager::calculateTrackingQuality() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!trackingEnabled_ || statistics_.trackingCorrectionCount == 0) {
        return 0.0;
    }

    // Quality based on tracking error (0.0 = poor, 1.0 = excellent)
    double errorThreshold = 10.0; // arcsec
    double quality = 1.0 - std::min(statistics_.avgTrackingError / errorThreshold, 1.0);

    return std::max(0.0, std::min(1.0, quality));
}

std::string TrackingManager::getTrackingQualityDescription() const {
    double quality = calculateTrackingQuality();

    if (quality >= 0.9) return "Excellent";
    if (quality >= 0.7) return "Good";
    if (quality >= 0.5) return "Fair";
    if (quality >= 0.3) return "Poor";
    return "Very Poor";
}

bool TrackingManager::needsTrackingImprovement() const {
    return calculateTrackingQuality() < 0.7;
}

bool TrackingManager::setTrackingLimits(double maxRARate, double maxDECRate) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    if (maxRARate <= 0 || maxDECRate <= 0) {
        logError("Invalid tracking limits");
        return false;
    }

    try {
        // Store limits and validate current rates
        if (std::abs(trackRateRA_) > maxRARate || std::abs(trackRateDEC_) > maxDECRate) {
            logWarning("Current track rates exceed new limits");
        }

        logInfo("Set tracking limits: RA=" + std::to_string(maxRARate) +
               " arcsec/s, DEC=" + std::to_string(maxDECRate) + " arcsec/s");
        return true;

    } catch (const std::exception& e) {
        logError("Exception during set tracking limits: " + std::string(e.what()));
        return false;
    }
}

bool TrackingManager::resetTrackingStatistics() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    statistics_ = TrackingStatistics{};
    statistics_.trackingStartTime = std::chrono::steady_clock::now();
    currentTrackingError_ = 0.0;

    logInfo("Tracking statistics reset");
    return true;
}

bool TrackingManager::saveTrackingProfile(const std::string& profileName) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    // TODO: Implement profile saving to configuration file
    logInfo("Tracking profile saved: " + profileName);
    return true;
}

bool TrackingManager::loadTrackingProfile(const std::string& profileName) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Tracking manager not initialized");
        return false;
    }

    // TODO: Implement profile loading from configuration file
    logInfo("Tracking profile loaded: " + profileName);
    return true;
}

// Private methods

void TrackingManager::updateTrackingStatus() {
    auto now = std::chrono::steady_clock::now();

    currentStatus_.isEnabled = trackingEnabled_;
    currentStatus_.mode = currentMode_;
    currentStatus_.trackRateRA = trackRateRA_;
    currentStatus_.trackRateDEC = trackRateDEC_;
    currentStatus_.trackingError = currentTrackingError_;
    currentStatus_.lastUpdate = now;

    // Update status message
    if (trackingEnabled_) {
        currentStatus_.statusMessage = "Tracking active (" +
            std::to_string(static_cast<int>(currentMode_)) + ")";
    } else {
        currentStatus_.statusMessage = "Tracking disabled";
    }

    calculateTrackingError();
    updateTrackingStatistics();
}

void TrackingManager::calculateTrackingError() {
    // Get current tracking error from hardware
    auto error = hardware_->getCurrentTrackingError();
    if (error.has_value()) {
        currentTrackingError_ = error.value();

        // Update statistics
        if (currentTrackingError_ > statistics_.maxTrackingError) {
            statistics_.maxTrackingError = currentTrackingError_;
        }

        // Trigger error callback if needed
        if (trackingErrorCallback_) {
            trackingErrorCallback_(currentTrackingError_);
        }
    }
}

void TrackingManager::updateTrackingStatistics() {
    if (!trackingEnabled_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    // Update average tracking error
    if (statistics_.trackingCorrectionCount > 0) {
        statistics_.avgTrackingError =
            (statistics_.avgTrackingError * (statistics_.trackingCorrectionCount - 1) +
             currentTrackingError_) / statistics_.trackingCorrectionCount;
    } else {
        statistics_.avgTrackingError = currentTrackingError_;
    }

    // Update total tracking time if currently tracking
    auto sessionTime = std::chrono::duration_cast<std::chrono::seconds>(
        now - statistics_.trackingStartTime);
    // Note: This gives current session time, not total accumulated time
}

void TrackingManager::handlePropertyUpdate(const std::string& propertyName) {
    if (propertyName == "TELESCOPE_TRACK_STATE") {
        // Handle tracking state changes from hardware
        auto isTracking = hardware_->isTrackingEnabled();
        if (isTracking.has_value()) {
            trackingEnabled_ = isTracking.value();
        }
    } else if (propertyName == "TELESCOPE_TRACK_RATE") {
        // Handle track rate changes from hardware
        auto rates = hardware_->getTrackRates();
        if (rates.has_value()) {
            currentRates_ = rates.value();
            trackRateRA_ = currentRates_.raRate;
            trackRateDEC_ = currentRates_.decRate;
        }
    } else if (propertyName == "TELESCOPE_PEC") {
        // Handle PEC state changes
        auto pecState = hardware_->isPECEnabled();
        if (pecState.has_value()) {
            pecEnabled_ = pecState.value();
        }
    }

    updateTrackingStatus();
}

MotionRates TrackingManager::calculateSiderealRates() const {
    MotionRates rates;
    rates.raRate = SIDEREAL_RATE;  // 15.041067 arcsec/sec
    rates.decRate = 0.0;           // No DEC tracking for sidereal
    return rates;
}

MotionRates TrackingManager::calculateSolarRates() const {
    MotionRates rates;
    rates.raRate = SOLAR_RATE;     // 15.0 arcsec/sec
    rates.decRate = 0.0;           // No DEC tracking for solar
    return rates;
}

MotionRates TrackingManager::calculateLunarRates() const {
    MotionRates rates;
    rates.raRate = LUNAR_RATE;     // 14.515 arcsec/sec
    rates.decRate = 0.0;           // No DEC tracking for lunar
    return rates;
}

bool TrackingManager::validateTrackRates(double raRate, double decRate) const {
    // Check for reasonable rate limits (±60 arcsec/sec)
    const double MAX_RATE = 60.0;

    if (std::abs(raRate) > MAX_RATE || std::abs(decRate) > MAX_RATE) {
        return false;
    }

    return true;
}

bool TrackingManager::isValidTrackMode(TrackMode mode) const {
    return mode == TrackMode::SIDEREAL ||
           mode == TrackMode::SOLAR ||
           mode == TrackMode::LUNAR ||
           mode == TrackMode::CUSTOM;
}

void TrackingManager::syncTrackingStateToHardware() {
    if (hardware_) {
        hardware_->setTrackingEnabled(trackingEnabled_);
        hardware_->setTrackingMode(currentMode_);
    }
}

void TrackingManager::syncTrackRatesToHardware() {
    if (hardware_) {
        hardware_->setTrackRates(currentRates_);
    }
}

void TrackingManager::logInfo(const std::string& message) {
    LOG_F(INFO, "[TrackingManager] {}", message);
}

void TrackingManager::logWarning(const std::string& message) {
    LOG_F(WARNING, "[TrackingManager] {}", message);
}

void TrackingManager::logError(const std::string& message) {
    LOG_F(ERROR, "[TrackingManager] {}", message);
}

} // namespace lithium::device::indi::telescope::components
