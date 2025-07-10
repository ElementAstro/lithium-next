/*
 * telescope_coordinator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Telescope Coordination Component Implementation

*************************************************/

#include "telescope_coordinator.hpp"
#include "hardware_interface.hpp"
#include "azimuth_manager.hpp"

#include <cmath>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

TelescopeCoordinator::TelescopeCoordinator(std::shared_ptr<HardwareInterface> hardware,
                                           std::shared_ptr<AzimuthManager> azimuth_manager)
    : hardware_(hardware), azimuth_manager_(azimuth_manager) {
    spdlog::info("Initializing Telescope Coordinator");
}

TelescopeCoordinator::~TelescopeCoordinator() {
    spdlog::info("Destroying Telescope Coordinator");
    stopAutomaticFollowing();
}

auto TelescopeCoordinator::followTelescope(bool enable) -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    is_following_.store(enable);
    spdlog::info("{} telescope following", enable ? "Enabling" : "Disabling");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        std::string params = "Slaved=" + std::string(enable ? "true" : "false");
        auto response = hardware_->sendAlpacaRequest("PUT", "slaved", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = enable ? VARIANT_TRUE : VARIANT_FALSE;
        return hardware_->setCOMProperty("Slaved", value);
    }
#endif

    return false;
}

auto TelescopeCoordinator::isFollowingTelescope() -> bool {
    return is_following_.load();
}

auto TelescopeCoordinator::setTelescopePosition(double az, double alt) -> bool {
    if (!is_following_.load()) {
        return false;
    }

    telescope_azimuth_.store(az);
    telescope_altitude_.store(alt);

    // Calculate required dome azimuth
    double domeAz = calculateDomeAzimuth(az, alt);

    // Move dome if necessary
    if (azimuth_manager_) {
        auto currentAz = azimuth_manager_->getCurrentAzimuth();
        if (currentAz && std::abs(*currentAz - domeAz) > following_tolerance_.load()) {
            return azimuth_manager_->moveToAzimuth(domeAz);
        }
    }

    return true;
}

auto TelescopeCoordinator::getTelescopePosition() -> std::optional<std::pair<double, double>> {
    if (is_following_.load()) {
        return std::make_pair(telescope_azimuth_.load(), telescope_altitude_.load());
    }
    return std::nullopt;
}

auto TelescopeCoordinator::calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double {
    // Apply geometric offset calculation
    double geometricOffset = calculateGeometricOffset(telescopeAz, telescopeAlt);
    
    // Apply configured offsets
    double correctedAz = telescopeAz + telescope_params_.azimuth_offset + geometricOffset;
    
    // Normalize to 0-360 range
    while (correctedAz < 0.0) correctedAz += 360.0;
    while (correctedAz >= 360.0) correctedAz -= 360.0;
    
    return correctedAz;
}

auto TelescopeCoordinator::calculateSlitPosition(double telescopeAz, double telescopeAlt) -> std::pair<double, double> {
    // Calculate the position of the telescope in the dome coordinate system
    double domeAz = calculateDomeAzimuth(telescopeAz, telescopeAlt);
    
    // Calculate altitude correction for dome geometry
    double altitudeCorrection = telescope_params_.altitude_offset;
    if (telescope_params_.radius_from_center > 0) {
        // Apply geometric correction for off-center telescope
        altitudeCorrection += std::atan(telescope_params_.radius_from_center / 
                                       (telescope_params_.height_offset + 
                                        telescope_params_.radius_from_center * std::tan(telescopeAlt * M_PI / 180.0))) * 180.0 / M_PI;
    }
    
    double correctedAlt = telescopeAlt + altitudeCorrection;
    
    return std::make_pair(domeAz, correctedAlt);
}

auto TelescopeCoordinator::isTelescopeInSlit() -> bool {
    if (!azimuth_manager_) {
        return false;
    }

    auto currentAz = azimuth_manager_->getCurrentAzimuth();
    if (!currentAz) {
        return false;
    }

    double telescopeAz = telescope_azimuth_.load();
    double requiredDomeAz = calculateDomeAzimuth(telescopeAz, telescope_altitude_.load());
    
    double offset = std::abs(*currentAz - requiredDomeAz);
    if (offset > 180.0) {
        offset = 360.0 - offset;
    }
    
    return offset <= following_tolerance_.load();
}

auto TelescopeCoordinator::getSlitOffset() -> double {
    if (!azimuth_manager_) {
        return 0.0;
    }

    auto currentAz = azimuth_manager_->getCurrentAzimuth();
    if (!currentAz) {
        return 0.0;
    }

    double telescopeAz = telescope_azimuth_.load();
    double requiredDomeAz = calculateDomeAzimuth(telescopeAz, telescope_altitude_.load());
    
    double offset = *currentAz - requiredDomeAz;
    
    // Normalize to [-180, 180]
    while (offset > 180.0) offset -= 360.0;
    while (offset < -180.0) offset += 360.0;
    
    return offset;
}

auto TelescopeCoordinator::setTelescopeParameters(const TelescopeParameters& params) -> bool {
    telescope_params_ = params;
    spdlog::info("Updated telescope parameters: radius={:.2f}m, height_offset={:.2f}m, az_offset={:.2f}°, alt_offset={:.2f}°",
                 params.radius_from_center, params.height_offset, params.azimuth_offset, params.altitude_offset);
    return true;
}

auto TelescopeCoordinator::getTelescopeParameters() -> TelescopeParameters {
    return telescope_params_;
}

auto TelescopeCoordinator::setFollowingTolerance(double tolerance) -> bool {
    following_tolerance_.store(tolerance);
    spdlog::info("Set following tolerance to: {:.2f}°", tolerance);
    return true;
}

auto TelescopeCoordinator::getFollowingTolerance() -> double {
    return following_tolerance_.load();
}

auto TelescopeCoordinator::setFollowingDelay(int delay) -> bool {
    following_delay_ = delay;
    spdlog::info("Set following delay to: {}ms", delay);
    return true;
}

auto TelescopeCoordinator::getFollowingDelay() -> int {
    return following_delay_;
}

auto TelescopeCoordinator::startAutomaticFollowing() -> bool {
    if (is_automatic_following_.load()) {
        return true;
    }

    if (!followTelescope(true)) {
        return false;
    }

    is_automatic_following_.store(true);
    stop_following_.store(false);
    
    following_thread_ = std::make_unique<std::thread>(&TelescopeCoordinator::followingLoop, this);
    
    spdlog::info("Started automatic telescope following");
    return true;
}

auto TelescopeCoordinator::stopAutomaticFollowing() -> bool {
    if (!is_automatic_following_.load()) {
        return true;
    }

    stop_following_.store(true);
    is_automatic_following_.store(false);
    
    if (following_thread_ && following_thread_->joinable()) {
        following_thread_->join();
    }
    following_thread_.reset();
    
    followTelescope(false);
    
    spdlog::info("Stopped automatic telescope following");
    return true;
}

auto TelescopeCoordinator::isAutomaticFollowing() -> bool {
    return is_automatic_following_.load();
}

auto TelescopeCoordinator::setFollowingCallback(std::function<void(bool, const std::string&)> callback) -> void {
    following_callback_ = callback;
}

auto TelescopeCoordinator::updateFollowingStatus() -> void {
    if (!hardware_ || !hardware_->isConnected()) {
        return;
    }

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("GET", "slaved");
        if (response) {
            bool slaved = (*response == "true");
            is_following_.store(slaved);
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->getCOMProperty("Slaved");
        if (result) {
            bool slaved = (result->boolVal == VARIANT_TRUE);
            is_following_.store(slaved);
        }
    }
#endif
}

auto TelescopeCoordinator::followingLoop() -> void {
    while (!stop_following_.load()) {
        if (is_following_.load()) {
            updateFollowingStatus();
            
            // Check if dome needs to move to follow telescope
            if (!isTelescopeInSlit()) {
                double telescopeAz = telescope_azimuth_.load();
                double telescopeAlt = telescope_altitude_.load();
                double requiredDomeAz = calculateDomeAzimuth(telescopeAz, telescopeAlt);
                
                if (azimuth_manager_) {
                    azimuth_manager_->moveToAzimuth(requiredDomeAz);
                }
                
                if (following_callback_) {
                    following_callback_(true, "Following telescope movement");
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(following_delay_));
    }
}

auto TelescopeCoordinator::calculateGeometricOffset(double telescopeAz, double telescopeAlt) -> double {
    // If telescope is at dome center, no geometric offset
    if (telescope_params_.radius_from_center == 0.0) {
        return 0.0;
    }
    
    // Calculate the geometric offset due to telescope being off-center
    double altRad = telescopeAlt * M_PI / 180.0;
    double offset = std::atan2(telescope_params_.radius_from_center * std::sin(altRad),
                              telescope_params_.height_offset + telescope_params_.radius_from_center * std::cos(altRad));
    
    return offset * 180.0 / M_PI;
}

} // namespace lithium::ascom::dome::components
