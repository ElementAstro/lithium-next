/*
 * dome_telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Telescope - Telescope Coordination Implementation

*************************************************/

#include "dome_telescope.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <string_view>

DomeTelescopeManager::DomeTelescopeManager(INDIDomeClient* client)
    : client_(client) {}

// Telescope coordination
[[nodiscard]] auto DomeTelescopeManager::followTelescope(bool enable) -> bool {
    std::scoped_lock lock(telescope_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeTelescopeManager] Device not connected");
        return false;
    }
    auto followProp = client_->getBaseDevice().getProperty("DOME_AUTOSYNC");
    if (followProp.isValid() && followProp.getType() == INDI_SWITCH) {
        auto followSwitch = followProp.getSwitch();
        if (followSwitch) {
            followSwitch->reset();
            if (enable) {
                if (auto* enableWidget =
                        followSwitch->findWidgetByName("DOME_AUTOSYNC_ENABLE");
                    enableWidget) {
                    enableWidget->setState(ISS_ON);
                }
            } else {
                if (auto* disableWidget =
                        followSwitch->findWidgetByName("DOME_AUTOSYNC_DISABLE");
                    disableWidget) {
                    disableWidget->setState(ISS_ON);
                }
            }
            client_->sendNewProperty(followSwitch);
            following_enabled_ = enable;
            spdlog::info("[DomeTelescopeManager] {} telescope following",
                         enable ? "Enabled" : "Disabled");
            return true;
        }
    }
    following_enabled_ = enable;
    spdlog::info("[DomeTelescopeManager] {} telescope following (local only)",
                 enable ? "Enabled" : "Disabled");
    return true;
}

[[nodiscard]] auto DomeTelescopeManager::isFollowingTelescope() -> bool {
    std::scoped_lock lock(telescope_mutex_);
    return following_enabled_;
}

[[nodiscard]] auto DomeTelescopeManager::setTelescopePosition(double az,
                                                              double alt)
    -> bool {
    std::scoped_lock lock(telescope_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeTelescopeManager] Device not connected");
        return false;
    }
    current_telescope_az_ = normalizeAzimuth(az);
    current_telescope_alt_ = alt;
    spdlog::debug(
        "[DomeTelescopeManager] Telescope position updated: Az={:.2f}°, "
        "Alt={:.2f}°",
        current_telescope_az_, current_telescope_alt_);
    if (following_enabled_) {
        double newDomeAz =
            calculateDomeAzimuth(current_telescope_az_, current_telescope_alt_);
        if (auto motionManager = client_->getMotionManager(); motionManager) {
            double currentDomeAz = motionManager->getCurrentAzimuth();
            if (shouldMoveDome(newDomeAz, currentDomeAz)) {
                spdlog::info(
                    "[DomeTelescopeManager] Moving dome to follow telescope: "
                    "{:.2f}°",
                    newDomeAz);
                [[maybe_unused]] bool _ =
                    motionManager->moveToAzimuth(newDomeAz);
                notifyTelescopeEvent(current_telescope_az_,
                                     current_telescope_alt_, newDomeAz);
            }
        }
    }
    return true;
}

[[nodiscard]] auto DomeTelescopeManager::calculateDomeAzimuth(
    double telescopeAz, double telescopeAlt) -> double {
    std::scoped_lock lock(telescope_mutex_);
    double domeAz = normalizeAzimuth(telescopeAz);
    if (telescope_radius_ > 0 || telescope_north_offset_ != 0 ||
        telescope_east_offset_ != 0) {
        double offsetCorrection =
            calculateOffsetCorrection(telescopeAz, telescopeAlt);
        domeAz = normalizeAzimuth(domeAz + offsetCorrection);
    }
    return domeAz;
}

// Telescope offset configuration
auto DomeTelescopeManager::setTelescopeOffset(double northOffset,
                                              double eastOffset) -> bool {
    std::lock_guard<std::mutex> lock(telescope_mutex_);

    telescope_north_offset_ = northOffset;
    telescope_east_offset_ = eastOffset;

    spdlog::info(
        "[DomeTelescopeManager] Telescope offset set: North={:.3f}m, "
        "East={:.3f}m",
        northOffset, eastOffset);
    return true;
}

auto DomeTelescopeManager::getTelescopeOffset() -> std::pair<double, double> {
    std::lock_guard<std::mutex> lock(telescope_mutex_);
    return {telescope_north_offset_, telescope_east_offset_};
}

auto DomeTelescopeManager::setTelescopeRadius(double radius) -> bool {
    std::lock_guard<std::mutex> lock(telescope_mutex_);

    if (radius < 0) {
        spdlog::error("[DomeTelescopeManager] Invalid telescope radius: {}",
                      radius);
        return false;
    }

    telescope_radius_ = radius;
    spdlog::info("[DomeTelescopeManager] Telescope radius set: {:.3f}m",
                 radius);
    return true;
}

auto DomeTelescopeManager::getTelescopeRadius() -> double {
    std::lock_guard<std::mutex> lock(telescope_mutex_);
    return telescope_radius_;
}

// Following parameters
auto DomeTelescopeManager::setFollowingThreshold(double threshold) -> bool {
    std::lock_guard<std::mutex> lock(telescope_mutex_);

    if (threshold < 0 || threshold > 180) {
        spdlog::error("[DomeTelescopeManager] Invalid following threshold: {}",
                      threshold);
        return false;
    }

    following_threshold_ = threshold;
    spdlog::info("[DomeTelescopeManager] Following threshold set: {:.2f}°",
                 threshold);
    return true;
}

auto DomeTelescopeManager::getFollowingThreshold() -> double {
    std::lock_guard<std::mutex> lock(telescope_mutex_);
    return following_threshold_;
}

auto DomeTelescopeManager::setFollowingDelay(uint32_t delayMs) -> bool {
    std::lock_guard<std::mutex> lock(telescope_mutex_);

    following_delay_ = delayMs;
    spdlog::info("[DomeTelescopeManager] Following delay set: {}ms", delayMs);
    return true;
}

auto DomeTelescopeManager::getFollowingDelay() -> uint32_t {
    std::lock_guard<std::mutex> lock(telescope_mutex_);
    return following_delay_;
}

// INDI property handling
void DomeTelescopeManager::handleTelescopeProperty(
    const INDI::Property& property) {
    if (!property.isValid()) {
        return;
    }
    std::string_view propertyName = property.getName();
    if (propertyName == "EQUATORIAL_COORD" ||
        propertyName == "HORIZONTAL_COORD") {
        if (property.getType() == INDI_NUMBER) {
            auto numberProp = property.getNumber();
            if (numberProp) {
                double az = 0.0, alt = 0.0;
                for (int i = 0; i < numberProp->count(); ++i) {
                    auto widget = numberProp->at(i);
                    std::string_view widgetName = widget->getName();
                    if (widgetName == "AZ" || widgetName == "AZIMUTH") {
                        az = widget->getValue();
                    } else if (widgetName == "ALT" ||
                               widgetName == "ALTITUDE") {
                        alt = widget->getValue();
                    }
                }
                [[maybe_unused]] bool _ = setTelescopePosition(az, alt);
            }
        }
    } else if (propertyName == "DOME_AUTOSYNC") {
        if (property.getType() == INDI_SWITCH) {
            auto switchProp = property.getSwitch();
            if (switchProp) {
                if (auto* enableWidget =
                        switchProp->findWidgetByName("DOME_AUTOSYNC_ENABLE");
                    enableWidget) {
                    bool enabled = (enableWidget->getState() == ISS_ON);
                    std::scoped_lock lock(telescope_mutex_);
                    following_enabled_ = enabled;
                    spdlog::info(
                        "[DomeTelescopeManager] Following state updated: {}",
                        enabled ? "enabled" : "disabled");
                }
            }
        }
    }
}

void DomeTelescopeManager::synchronizeWithDevice() {
    if (!client_->isConnected()) {
        return;
    }

    // Check current autosync state
    auto followProp = client_->getBaseDevice().getProperty("DOME_AUTOSYNC");
    if (followProp.isValid()) {
        handleTelescopeProperty(followProp);
    }

    spdlog::debug("[DomeTelescopeManager] Synchronized with device");
}

// Telescope callback registration
void DomeTelescopeManager::setTelescopeCallback(TelescopeCallback callback) {
    std::lock_guard<std::mutex> lock(telescope_mutex_);
    telescope_callback_ = std::move(callback);
}

// Internal methods
void DomeTelescopeManager::notifyTelescopeEvent(double telescopeAz,
                                                double telescopeAlt,
                                                double domeAz) {
    if (telescope_callback_) {
        try {
            telescope_callback_(telescopeAz, telescopeAlt, domeAz);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeTelescopeManager] Telescope callback error: {}",
                          ex.what());
        }
    }
}

auto DomeTelescopeManager::shouldMoveDome(double newDomeAz,
                                          double currentDomeAz) -> bool {
    // Calculate the angular difference, taking into account the circular nature
    // of azimuth
    double diff = std::abs(newDomeAz - currentDomeAz);
    if (diff > 180.0) {
        diff = 360.0 - diff;
    }

    return diff > following_threshold_;
}

// Calculation helpers
auto DomeTelescopeManager::normalizeAzimuth(double azimuth) -> double {
    while (azimuth < 0.0)
        azimuth += 360.0;
    while (azimuth >= 360.0)
        azimuth -= 360.0;
    return azimuth;
}

auto DomeTelescopeManager::calculateOffsetCorrection(double az, double alt)
    -> double {
    // Convert to radians for calculation
    double azRad = az * M_PI / 180.0;
    double altRad = alt * M_PI / 180.0;

    // Calculate offset correction based on telescope position
    // This is a simplified calculation - real implementations would be more
    // complex
    double northComponent = telescope_north_offset_ * std::cos(azRad);
    double eastComponent = telescope_east_offset_ * std::sin(azRad);
    double heightComponent = 0.0;

    if (telescope_radius_ > 0) {
        // Account for telescope height offset
        heightComponent = telescope_radius_ * std::sin(altRad);
    }

    // Calculate total offset in degrees
    double totalOffset =
        (northComponent + eastComponent + heightComponent) * 180.0 / M_PI;

    return totalOffset;
}
