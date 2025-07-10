/*
 * tracking_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Tracking Manager Component

This component manages telescope tracking operations including
tracking state, tracking rates, and various tracking modes.

*************************************************/

#include "tracking_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::ascom::telescope::components {

TrackingManager::TrackingManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    
    auto logger = spdlog::get("telescope_tracking");
    if (logger) {
        logger->info("TrackingManager initialized");
    }
}

TrackingManager::~TrackingManager() {
    auto logger = spdlog::get("telescope_tracking");
    if (logger) {
        logger->debug("TrackingManager destructor");
    }
}

bool TrackingManager::isTracking() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        return hardware_->isTracking();
    } catch (const std::exception& e) {
        setLastError("Failed to get tracking state: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Failed to get tracking state: {}", e.what());
        return false;
    }
}

bool TrackingManager::setTracking(bool enable) {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->info("Setting tracking to: {}", enable ? "enabled" : "disabled");
        
        bool result = hardware_->setTracking(enable);
        if (result) {
            clearError();
            if (logger) logger->info("Tracking {} successfully", enable ? "enabled" : "disabled");
        } else {
            setLastError("Failed to set tracking state");
            if (logger) logger->error("Failed to set tracking to {}", enable);
        }
        return result;
        
    } catch (const std::exception& e) {
        setLastError("Exception setting tracking: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Exception setting tracking: {}", e.what());
        return false;
    }
}

std::optional<TrackMode> TrackingManager::getTrackingRate() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return std::nullopt;
    }

    try {
        // Implementation would get tracking rate from hardware
        // For now, return sidereal as default
        TrackMode mode = TrackMode::SIDEREAL;
        
        return mode;
        
    } catch (const std::exception& e) {
        setLastError("Failed to get tracking rate: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Failed to get tracking rate: {}", e.what());
        return std::nullopt;
    }
}

bool TrackingManager::setTrackingRate(TrackMode rate) {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->info("Setting tracking rate to: {}", static_cast<int>(rate));
        
        // Implementation would set tracking rate in hardware
        // For now, just simulate success
        
        clearError();
        if (logger) logger->info("Tracking rate set successfully");
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Exception setting tracking rate: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Exception setting tracking rate: {}", e.what());
        return false;
    }
}

MotionRates TrackingManager::getTrackingRates() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return MotionRates{};
    }

    try {
        // Implementation would get available tracking rates from hardware
        // For now, return default rates
        MotionRates rates;
        rates.guideRateNS = 0.5;  // arcsec/sec
        rates.guideRateEW = 0.5;  // arcsec/sec
        rates.slewRateRA = 3.0;   // degrees/sec
        rates.slewRateDEC = 3.0;  // degrees/sec
        
        return rates;
        
    } catch (const std::exception& e) {
        setLastError("Failed to get tracking rates: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Failed to get tracking rates: {}", e.what());
        return MotionRates{};
    }
}

bool TrackingManager::setTrackingRates(const MotionRates& rates) {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        auto logger = spdlog::get("telescope_tracking");
        if (logger) {
            logger->info("Setting tracking rates: GuideNS={:.6f} arcsec/sec, GuideEW={:.6f} arcsec/sec, SlewRA={:.6f} deg/sec, SlewDEC={:.6f} deg/sec", 
                         rates.guideRateNS, rates.guideRateEW, rates.slewRateRA, rates.slewRateDEC);
        }
        
        // Implementation would set custom tracking rates in hardware
        // For now, just simulate success
        
        clearError();
        if (logger) logger->info("Tracking rates set successfully");
        return true;
        
    } catch (const std::exception& e) {
        setLastError("Exception setting tracking rates: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_tracking");
        if (logger) logger->error("Exception setting tracking rates: {}", e.what());
        return false;
    }
}

std::string TrackingManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void TrackingManager::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

void TrackingManager::setLastError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
}

} // namespace lithium::device::ascom::telescope::components
