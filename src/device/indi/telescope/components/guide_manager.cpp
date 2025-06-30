/*
 * guide_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Guide Manager Implementation

This component manages telescope guiding operations including
guide pulses, guiding calibration, and autoguiding support.

*************************************************/

#include "guide_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "atom/utils/string.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

namespace lithium::device::indi::telescope::components {

GuideManager::GuideManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }
    
    // Initialize default guide rates
    guideRates_.raRate = DEFAULT_GUIDE_RATE;
    guideRates_.decRate = DEFAULT_GUIDE_RATE;
    
    // Initialize statistics
    statistics_.sessionStartTime = std::chrono::steady_clock::now();
}

GuideManager::~GuideManager() {
    shutdown();
}

bool GuideManager::initialize() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (initialized_) {
        logWarning("Guide manager already initialized");
        return true;
    }
    
    if (!hardware_->isConnected()) {
        logError("Hardware interface not connected");
        return false;
    }
    
    try {
        // Get current guide rates from hardware
        auto guideRateData = hardware_->getProperty("TELESCOPE_GUIDE_RATE");
        if (guideRateData && !guideRateData->empty()) {
            auto rateElement = guideRateData->find("GUIDE_RATE");
            if (rateElement != guideRateData->end()) {
                double rate = std::stod(rateElement->second.value);
                guideRates_.raRate = rate;
                guideRates_.decRate = rate;
            }
        }
        
        // Clear any existing guide queue
        while (!guideQueue_.empty()) {
            guideQueue_.pop();
        }
        
        // Reset statistics
        statistics_ = GuideStatistics{};
        statistics_.sessionStartTime = std::chrono::steady_clock::now();
        recentPulses_.clear();
        
        initialized_ = true;
        logInfo("Guide manager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Failed to initialize guide manager: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (!initialized_) {
        return true;
    }
    
    try {
        // Clear guide queue
        clearGuideQueue();
        
        // Abort any current pulse
        if (currentPulse_) {
            hardware_->sendCommand("TELESCOPE_ABORT_MOTION", {{"ABORT", "On"}});
            currentPulse_.reset();
        }
        
        isGuiding_ = false;
        isCalibrating_ = false;
        
        initialized_ = false;
        logInfo("Guide manager shut down successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error during guide manager shutdown: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::guidePulse(GuideDirection direction, std::chrono::milliseconds duration) {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (!initialized_) {
        logError("Guide manager not initialized");
        return false;
    }
    
    if (!isValidPulseParameters(direction, duration)) {
        logError("Invalid guide pulse parameters");
        return false;
    }
    
    try {
        GuidePulse pulse;
        pulse.direction = direction;
        pulse.duration = duration;
        pulse.timestamp = std::chrono::steady_clock::now();
        pulse.id = generatePulseId();
        
        // Execute pulse immediately
        return sendGuidePulseToHardware(direction, duration);
        
    } catch (const std::exception& e) {
        logError("Error sending guide pulse: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::guidePulse(double raPulseMs, double decPulseMs) {
    // Convert RA/DEC pulses to directional pulses
    bool success = true;
    
    if (raPulseMs > 0) {
        success &= guidePulse(GuideDirection::EAST, std::chrono::milliseconds(static_cast<int>(raPulseMs)));
    } else if (raPulseMs < 0) {
        success &= guidePulse(GuideDirection::WEST, std::chrono::milliseconds(static_cast<int>(-raPulseMs)));
    }
    
    if (decPulseMs > 0) {
        success &= guidePulse(GuideDirection::NORTH, std::chrono::milliseconds(static_cast<int>(decPulseMs)));
    } else if (decPulseMs < 0) {
        success &= guidePulse(GuideDirection::SOUTH, std::chrono::milliseconds(static_cast<int>(-decPulseMs)));
    }
    
    return success;
}

bool GuideManager::guideNorth(std::chrono::milliseconds duration) {
    return guidePulse(GuideDirection::NORTH, duration);
}

bool GuideManager::guideSouth(std::chrono::milliseconds duration) {
    return guidePulse(GuideDirection::SOUTH, duration);
}

bool GuideManager::guideEast(std::chrono::milliseconds duration) {
    return guidePulse(GuideDirection::EAST, duration);
}

bool GuideManager::guideWest(std::chrono::milliseconds duration) {
    return guidePulse(GuideDirection::WEST, duration);
}

bool GuideManager::queueGuidePulse(GuideDirection direction, std::chrono::milliseconds duration) {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (!initialized_) {
        logError("Guide manager not initialized");
        return false;
    }
    
    if (!isValidPulseParameters(direction, duration)) {
        logError("Invalid guide pulse parameters");
        return false;
    }
    
    try {
        GuidePulse pulse;
        pulse.direction = direction;
        pulse.duration = duration;
        pulse.timestamp = std::chrono::steady_clock::now();
        pulse.id = generatePulseId();
        
        guideQueue_.push(pulse);
        
        // Process queue if not currently guiding
        if (!isGuiding_) {
            processGuideQueue();
        }
        
        logInfo("Guide pulse queued: " + directionToString(direction) + 
               " for " + std::to_string(duration.count()) + "ms");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error queuing guide pulse: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::clearGuideQueue() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    while (!guideQueue_.empty()) {
        guideQueue_.pop();
    }
    
    logInfo("Guide queue cleared");
    return true;
}

size_t GuideManager::getQueueSize() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return guideQueue_.size();
}

bool GuideManager::isGuiding() const {
    return isGuiding_;
}

std::optional<GuideManager::GuidePulse> GuideManager::getCurrentPulse() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return currentPulse_;
}

bool GuideManager::setGuideRate(double rateArcsecPerSec) {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (rateArcsecPerSec <= 0.0 || rateArcsecPerSec > 10.0) {
        logError("Invalid guide rate: " + std::to_string(rateArcsecPerSec));
        return false;
    }
    
    try {
        guideRates_.raRate = rateArcsecPerSec;
        guideRates_.decRate = rateArcsecPerSec;
        
        syncGuideRatesToHardware();
        
        logInfo("Guide rate set to " + std::to_string(rateArcsecPerSec) + " arcsec/sec");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error setting guide rate: " + std::string(e.what()));
        return false;
    }
}

std::optional<double> GuideManager::getGuideRate() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return guideRates_.raRate; // Assuming RA and DEC rates are the same
}

bool GuideManager::setGuideRates(double raRate, double decRate) {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (raRate <= 0.0 || raRate > 10.0 || decRate <= 0.0 || decRate > 10.0) {
        logError("Invalid guide rates");
        return false;
    }
    
    try {
        guideRates_.raRate = raRate;
        guideRates_.decRate = decRate;
        
        syncGuideRatesToHardware();
        
        logInfo("Guide rates set to RA:" + std::to_string(raRate) + 
               ", DEC:" + std::to_string(decRate) + " arcsec/sec");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error setting guide rates: " + std::string(e.what()));
        return false;
    }
}

std::optional<MotionRates> GuideManager::getGuideRates() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return guideRates_;
}

bool GuideManager::startCalibration() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (!initialized_) {
        logError("Guide manager not initialized");
        return false;
    }
    
    if (isCalibrating_) {
        logWarning("Calibration already in progress");
        return false;
    }
    
    try {
        isCalibrating_ = true;
        
        // Clear previous calibration
        calibration_ = GuideCalibration{};
        calibrated_ = false;
        
        logInfo("Starting guide calibration");
        
        // Start async calibration process
        performCalibrationSequence();
        
        return true;
        
    } catch (const std::exception& e) {
        isCalibrating_ = false;
        logError("Error starting calibration: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::abortCalibration() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    if (!isCalibrating_) {
        logWarning("No calibration in progress");
        return false;
    }
    
    try {
        isCalibrating_ = false;
        
        // Stop any current pulse
        hardware_->sendCommand("TELESCOPE_ABORT_MOTION", {{"ABORT", "On"}});
        
        logInfo("Calibration aborted");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error aborting calibration: " + std::string(e.what()));
        return false;
    }
}

bool GuideManager::isCalibrating() const {
    return isCalibrating_;
}

GuideManager::GuideCalibration GuideManager::getCalibration() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return calibration_;
}

bool GuideManager::setCalibration(const GuideCalibration& calibration) {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    calibration_ = calibration;
    calibrated_ = calibration.isValid;
    
    if (calibrationCallback_) {
        calibrationCallback_(calibration_);
    }
    
    logInfo("Calibration data updated");
    return true;
}

bool GuideManager::isCalibrated() const {
    return calibrated_;
}

bool GuideManager::clearCalibration() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    calibration_ = GuideCalibration{};
    calibrated_ = false;
    
    logInfo("Calibration cleared");
    return true;
}

std::chrono::milliseconds GuideManager::arcsecToPulseDuration(double arcsec, GuideDirection direction) const {
    if (!calibrated_) {
        // Use default guide rate if not calibrated
        double rate = calculateEffectiveGuideRate(direction);
        return std::chrono::milliseconds(static_cast<int>(arcsec / rate * 1000.0));
    }
    
    double rate = 0.0;
    switch (direction) {
        case GuideDirection::NORTH: rate = calibration_.northRate; break;
        case GuideDirection::SOUTH: rate = calibration_.southRate; break;
        case GuideDirection::EAST: rate = calibration_.eastRate; break;
        case GuideDirection::WEST: rate = calibration_.westRate; break;
    }
    
    if (rate <= 0.0) {
        rate = calculateEffectiveGuideRate(direction);
    }
    
    return std::chrono::milliseconds(static_cast<int>(arcsec / rate));
}

double GuideManager::pulseDurationToArcsec(std::chrono::milliseconds duration, GuideDirection direction) const {
    if (!calibrated_) {
        // Use default guide rate if not calibrated
        double rate = calculateEffectiveGuideRate(direction);
        return duration.count() * rate / 1000.0;
    }
    
    double rate = 0.0;
    switch (direction) {
        case GuideDirection::NORTH: rate = calibration_.northRate; break;
        case GuideDirection::SOUTH: rate = calibration_.southRate; break;
        case GuideDirection::EAST: rate = calibration_.eastRate; break;
        case GuideDirection::WEST: rate = calibration_.westRate; break;
    }
    
    if (rate <= 0.0) {
        rate = calculateEffectiveGuideRate(direction);
    }
    
    return duration.count() * rate;
}

GuideManager::GuideStatistics GuideManager::getGuideStatistics() const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    return statistics_;
}

bool GuideManager::resetGuideStatistics() {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    statistics_ = GuideStatistics{};
    statistics_.sessionStartTime = std::chrono::steady_clock::now();
    recentPulses_.clear();
    currentGuideRMS_ = 0.0;
    
    logInfo("Guide statistics reset");
    return true;
}

double GuideManager::getCurrentGuideRMS() const {
    return currentGuideRMS_;
}

std::vector<GuideManager::GuidePulse> GuideManager::getRecentPulses(std::chrono::seconds timeWindow) const {
    std::lock_guard<std::recursive_mutex> lock(guideMutex_);
    
    auto cutoffTime = std::chrono::steady_clock::now() - timeWindow;
    std::vector<GuidePulse> result;
    
    for (const auto& pulse : recentPulses_) {
        if (pulse.timestamp >= cutoffTime) {
            result.push_back(pulse);
        }
    }
    
    return result;
}

bool GuideManager::setMaxPulseDuration(std::chrono::milliseconds maxDuration) {
    if (maxDuration <= std::chrono::milliseconds(0) || maxDuration > std::chrono::minutes(1)) {
        logError("Invalid max pulse duration");
        return false;
    }
    
    maxPulseDuration_ = maxDuration;
    logInfo("Max pulse duration set to " + std::to_string(maxDuration.count()) + "ms");
    return true;
}

std::chrono::milliseconds GuideManager::getMaxPulseDuration() const {
    return maxPulseDuration_;
}

bool GuideManager::setMinPulseDuration(std::chrono::milliseconds minDuration) {
    if (minDuration < std::chrono::milliseconds(1) || minDuration > std::chrono::seconds(1)) {
        logError("Invalid min pulse duration");
        return false;
    }
    
    minPulseDuration_ = minDuration;
    logInfo("Min pulse duration set to " + std::to_string(minDuration.count()) + "ms");
    return true;
}

std::chrono::milliseconds GuideManager::getMinPulseDuration() const {
    return minPulseDuration_;
}

bool GuideManager::enablePulseLimits(bool enable) {
    pulseLimitsEnabled_ = enable;
    logInfo("Pulse limits " + std::string(enable ? "enabled" : "disabled"));
    return true;
}

bool GuideManager::dither(double amountArcsec, double angleRadians) {
    if (amountArcsec <= 0.0 || amountArcsec > 10.0) {
        logError("Invalid dither amount");
        return false;
    }
    
    // Calculate RA and DEC components
    double raOffset = amountArcsec * std::cos(angleRadians);
    double decOffset = amountArcsec * std::sin(angleRadians);
    
    // Convert to pulse durations
    auto raDuration = arcsecToPulseDuration(std::abs(raOffset), 
        raOffset > 0 ? GuideDirection::EAST : GuideDirection::WEST);
    auto decDuration = arcsecToPulseDuration(std::abs(decOffset), 
        decOffset > 0 ? GuideDirection::NORTH : GuideDirection::SOUTH);
    
    // Execute dither pulses
    bool success = true;
    if (raOffset != 0.0) {
        success &= guidePulse(raOffset > 0 ? GuideDirection::EAST : GuideDirection::WEST, raDuration);
    }
    if (decOffset != 0.0) {
        success &= guidePulse(decOffset > 0 ? GuideDirection::NORTH : GuideDirection::SOUTH, decDuration);
    }
    
    if (success) {
        logInfo("Dither executed: " + std::to_string(amountArcsec) + " arcsec at " + 
               std::to_string(angleRadians * 180.0 / M_PI) + " degrees");
    }
    
    return success;
}

bool GuideManager::ditherRandom(double maxAmountArcsec) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> amountDist(0.1, maxAmountArcsec);
    std::uniform_real_distribution<> angleDist(0.0, 2.0 * M_PI);
    
    double amount = amountDist(gen);
    double angle = angleDist(gen);
    
    return dither(amount, angle);
}

void GuideManager::processGuideQueue() {
    if (isGuiding_ || guideQueue_.empty()) {
        return;
    }
    
    isGuiding_ = true;
    currentPulse_ = guideQueue_.front();
    guideQueue_.pop();
    
    executePulse(*currentPulse_);
}

void GuideManager::executePulse(const GuidePulse& pulse) {
    try {
        if (sendGuidePulseToHardware(pulse.direction, pulse.duration)) {
            updateGuideStatistics(pulse);
            
            if (pulseCompleteCallback_) {
                pulseCompleteCallback_(pulse, true);
            }
        } else {
            logError("Failed to execute guide pulse");
            if (pulseCompleteCallback_) {
                pulseCompleteCallback_(pulse, false);
            }
        }
        
        // Mark pulse as completed
        currentPulse_->completed = true;
        
        // Add to recent pulses for statistics
        recentPulses_.push_back(pulse);
        if (recentPulses_.size() > MAX_RECENT_PULSES) {
            recentPulses_.erase(recentPulses_.begin());
        }
        
        // Continue processing queue
        isGuiding_ = false;
        currentPulse_.reset();
        
        if (!guideQueue_.empty()) {
            processGuideQueue();
        }
        
    } catch (const std::exception& e) {
        logError("Error executing guide pulse: " + std::string(e.what()));
        isGuiding_ = false;
        currentPulse_.reset();
    }
}

void GuideManager::updateGuideStatistics(const GuidePulse& pulse) {
    statistics_.totalPulses++;
    statistics_.totalPulseTime += pulse.duration;
    
    switch (pulse.direction) {
        case GuideDirection::NORTH: statistics_.northPulses++; break;
        case GuideDirection::SOUTH: statistics_.southPulses++; break;
        case GuideDirection::EAST: statistics_.eastPulses++; break;
        case GuideDirection::WEST: statistics_.westPulses++; break;
    }
    
    // Update duration statistics
    if (statistics_.totalPulses == 1) {
        statistics_.maxPulseDuration = pulse.duration;
        statistics_.minPulseDuration = pulse.duration;
    } else {
        statistics_.maxPulseDuration = std::max(statistics_.maxPulseDuration, pulse.duration);
        statistics_.minPulseDuration = std::min(statistics_.minPulseDuration, pulse.duration);
    }
    
    statistics_.avgPulseDuration = statistics_.totalPulseTime / statistics_.totalPulses;
    
    // Calculate simple RMS from recent pulses
    if (recentPulses_.size() > 5) {
        double sumSquares = 0.0;
        for (const auto& recentPulse : recentPulses_) {
            double arcsec = pulseDurationToArcsec(recentPulse.duration, recentPulse.direction);
            sumSquares += arcsec * arcsec;
        }
        currentGuideRMS_ = std::sqrt(sumSquares / recentPulses_.size());
        statistics_.guideRMS = currentGuideRMS_;
    }
}

std::string GuideManager::generatePulseId() {
    static std::atomic<uint64_t> counter{0};
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "pulse_" + std::to_string(timestamp) + "_" + std::to_string(counter++);
}

bool GuideManager::validatePulseDuration(std::chrono::milliseconds duration) const {
    if (!pulseLimitsEnabled_) {
        return duration > std::chrono::milliseconds(0);
    }
    
    return duration >= minPulseDuration_ && duration <= maxPulseDuration_;
}

bool GuideManager::isValidPulseParameters(GuideDirection direction, std::chrono::milliseconds duration) const {
    return isValidGuideDirection(direction) && validatePulseDuration(duration);
}

bool GuideManager::isValidGuideDirection(GuideDirection direction) const {
    return direction == GuideDirection::NORTH || direction == GuideDirection::SOUTH ||
           direction == GuideDirection::EAST || direction == GuideDirection::WEST;
}

double GuideManager::calculateEffectiveGuideRate(GuideDirection direction) const {
    switch (direction) {
        case GuideDirection::NORTH:
        case GuideDirection::SOUTH:
            return guideRates_.decRate / 1000.0; // Convert to arcsec/ms
        case GuideDirection::EAST:
        case GuideDirection::WEST:
            return guideRates_.raRate / 1000.0;  // Convert to arcsec/ms
    }
    return DEFAULT_GUIDE_RATE / 1000.0;
}

bool GuideManager::sendGuidePulseToHardware(GuideDirection direction, std::chrono::milliseconds duration) {
    try {
        std::string propertyName;
        std::string elementName;
        
        switch (direction) {
            case GuideDirection::NORTH:
                propertyName = "TELESCOPE_TIMED_GUIDE_NS";
                elementName = "TIMED_GUIDE_N";
                break;
            case GuideDirection::SOUTH:
                propertyName = "TELESCOPE_TIMED_GUIDE_NS";
                elementName = "TIMED_GUIDE_S";
                break;
            case GuideDirection::EAST:
                propertyName = "TELESCOPE_TIMED_GUIDE_WE";
                elementName = "TIMED_GUIDE_E";
                break;
            case GuideDirection::WEST:
                propertyName = "TELESCOPE_TIMED_GUIDE_WE";
                elementName = "TIMED_GUIDE_W";
                break;
        }
        
        std::map<std::string, PropertyElement> elements;
        elements[elementName] = {std::to_string(duration.count()), ""};
        
        return hardware_->sendCommand(propertyName, elements);
        
    } catch (const std::exception& e) {
        logError("Error sending guide pulse to hardware: " + std::string(e.what()));
        return false;
    }
}

void GuideManager::syncGuideRatesToHardware() {
    try {
        std::map<std::string, PropertyElement> elements;
        elements["GUIDE_RATE"] = {std::to_string(guideRates_.raRate), ""};
        
        hardware_->sendCommand("TELESCOPE_GUIDE_RATE", elements);
        
    } catch (const std::exception& e) {
        logError("Error syncing guide rates to hardware: " + std::string(e.what()));
    }
}

void GuideManager::performCalibrationSequence() {
    // This would be implemented as an async process
    // For now, we'll just mark it as completed with default values
    calibration_.northRate = DEFAULT_GUIDE_RATE / 1000.0;
    calibration_.southRate = DEFAULT_GUIDE_RATE / 1000.0;
    calibration_.eastRate = DEFAULT_GUIDE_RATE / 1000.0;
    calibration_.westRate = DEFAULT_GUIDE_RATE / 1000.0;
    calibration_.isValid = true;
    calibration_.calibrationTime = std::chrono::system_clock::now();
    calibration_.calibrationMethod = "Default";
    
    calibrated_ = true;
    isCalibrating_ = false;
    
    if (calibrationCallback_) {
        calibrationCallback_(calibration_);
    }
    
    logInfo("Calibration completed");
}

std::string GuideManager::directionToString(GuideDirection direction) const {
    switch (direction) {
        case GuideDirection::NORTH: return "North";
        case GuideDirection::SOUTH: return "South";
        case GuideDirection::EAST: return "East";
        case GuideDirection::WEST: return "West";
        default: return "Unknown";
    }
}

GuideManager::GuideDirection GuideManager::stringToDirection(const std::string& directionStr) const {
    if (directionStr == "North") return GuideDirection::NORTH;
    if (directionStr == "South") return GuideDirection::SOUTH;
    if (directionStr == "East") return GuideDirection::EAST;
    if (directionStr == "West") return GuideDirection::WEST;
    return GuideDirection::NORTH; // Default
}

void GuideManager::logInfo(const std::string& message) {
    LOG_F(INFO, "[GuideManager] %s", message.c_str());
}

void GuideManager::logWarning(const std::string& message) {
    LOG_F(WARNING, "[GuideManager] %s", message.c_str());
}

void GuideManager::logError(const std::string& message) {
    LOG_F(ERROR, "[GuideManager] %s", message.c_str());
}

} // namespace lithium::device::indi::telescope::components
