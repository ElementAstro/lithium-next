/*
 * azimuth_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Azimuth Management Component Implementation

*************************************************/

#include "azimuth_manager.hpp"
#include "hardware_interface.hpp"

#include <cmath>
#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

AzimuthManager::AzimuthManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    spdlog::info("Initializing Azimuth Manager");
}

AzimuthManager::~AzimuthManager() {
    spdlog::info("Destroying Azimuth Manager");
    stopMovement();
}

auto AzimuthManager::getCurrentAzimuth() -> std::optional<double> {
    if (!hardware_ || !hardware_->isConnected()) {
        return std::nullopt;
    }

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("GET", "azimuth");
        if (response) {
            double azimuth = std::stod(*response);
            current_azimuth_.store(azimuth);
            return azimuth;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->getCOMProperty("Azimuth");
        if (result) {
            double azimuth = result->dblVal;
            current_azimuth_.store(azimuth);
            return azimuth;
        }
    }
#endif

    return std::nullopt;
}

auto AzimuthManager::setTargetAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto AzimuthManager::moveToAzimuth(double azimuth) -> bool {
    if (!hardware_ || !hardware_->isConnected() || is_moving_.load()) {
        return false;
    }

    // Normalize azimuth to 0-360 range
    while (azimuth < 0.0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;

    spdlog::info("Moving dome to azimuth: {:.2f}°", azimuth);

    // Apply backlash compensation if enabled
    double target_azimuth = azimuth;
    if (settings_.backlash_enabled && settings_.backlash_compensation != 0.0) {
        target_azimuth = applyBacklashCompensation(azimuth);
    }

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        std::string params = "Azimuth=" + std::to_string(target_azimuth);
        auto response = hardware_->sendAlpacaRequest("PUT", "slewtoazimuth", params);
        if (response) {
            is_moving_.store(true);
            target_azimuth_.store(azimuth);
            startMovementMonitoring();
            return true;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_R8;
        param.dblVal = target_azimuth;

        auto result = hardware_->invokeCOMMethod("SlewToAzimuth", &param, 1);
        if (result) {
            is_moving_.store(true);
            target_azimuth_.store(azimuth);
            startMovementMonitoring();
            return true;
        }
    }
#endif

    return false;
}

auto AzimuthManager::rotateClockwise(double degrees) -> bool {
    auto current = getCurrentAzimuth();
    if (!current) {
        return false;
    }

    double target = *current + degrees;
    return moveToAzimuth(target);
}

auto AzimuthManager::rotateCounterClockwise(double degrees) -> bool {
    auto current = getCurrentAzimuth();
    if (!current) {
        return false;
    }

    double target = *current - degrees;
    return moveToAzimuth(target);
}

auto AzimuthManager::stopMovement() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    spdlog::info("Stopping dome movement");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "abortslew");
        if (response) {
            is_moving_.store(false);
            stopMovementMonitoring();
            return true;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("AbortSlew");
        if (result) {
            is_moving_.store(false);
            stopMovementMonitoring();
            return true;
        }
    }
#endif

    return false;
}

auto AzimuthManager::syncToAzimuth(double azimuth) -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    spdlog::info("Syncing dome azimuth to: {:.2f}°", azimuth);

    // Most ASCOM domes don't support sync, just update our internal state
    current_azimuth_.store(azimuth);
    return true;
}

auto AzimuthManager::isMoving() const -> bool {
    return is_moving_.load();
}

auto AzimuthManager::getTargetAzimuth() const -> std::optional<double> {
    if (is_moving_.load()) {
        return target_azimuth_.load();
    }
    return std::nullopt;
}

auto AzimuthManager::getMovementProgress() const -> double {
    if (!is_moving_.load()) {
        return 1.0;
    }

    auto current = getCurrentAzimuth();
    if (!current) {
        return 0.0;
    }

    double start = start_azimuth_.load();
    double target = target_azimuth_.load();
    double progress = std::abs(*current - start) / std::abs(target - start);
    return std::clamp(progress, 0.0, 1.0);
}

auto AzimuthManager::setRotationSpeed(double speed) -> bool {
    if (speed < settings_.min_speed || speed > settings_.max_speed) {
        spdlog::error("Rotation speed {} out of range [{}, {}]", speed, settings_.min_speed, settings_.max_speed);
        return false;
    }

    settings_.default_speed = speed;
    spdlog::info("Set rotation speed to: {:.2f}", speed);
    return true;
}

auto AzimuthManager::getRotationSpeed() const -> double {
    return settings_.default_speed;
}

auto AzimuthManager::getSpeedRange() const -> std::pair<double, double> {
    return {settings_.min_speed, settings_.max_speed};
}

auto AzimuthManager::setBacklashCompensation(double backlash) -> bool {
    settings_.backlash_compensation = backlash;
    spdlog::info("Set backlash compensation to: {:.2f}°", backlash);
    return true;
}

auto AzimuthManager::getBacklashCompensation() const -> double {
    return settings_.backlash_compensation;
}

auto AzimuthManager::enableBacklashCompensation(bool enable) -> bool {
    settings_.backlash_enabled = enable;
    spdlog::info("{} backlash compensation", enable ? "Enabled" : "Disabled");
    return true;
}

auto AzimuthManager::isBacklashCompensationEnabled() const -> bool {
    return settings_.backlash_enabled;
}

auto AzimuthManager::setPositionTolerance(double tolerance) -> bool {
    settings_.position_tolerance = tolerance;
    spdlog::info("Set position tolerance to: {:.2f}°", tolerance);
    return true;
}

auto AzimuthManager::getPositionTolerance() const -> double {
    return settings_.position_tolerance;
}

auto AzimuthManager::setMovementTimeout(int timeout) -> bool {
    settings_.movement_timeout = timeout;
    spdlog::info("Set movement timeout to: {} seconds", timeout);
    return true;
}

auto AzimuthManager::getMovementTimeout() const -> int {
    return settings_.movement_timeout;
}

auto AzimuthManager::getAzimuthSettings() const -> AzimuthSettings {
    return settings_;
}

auto AzimuthManager::setAzimuthSettings(const AzimuthSettings& settings) -> bool {
    settings_ = settings;
    spdlog::info("Updated azimuth settings");
    return true;
}

auto AzimuthManager::setMovementCallback(std::function<void(bool, const std::string&)> callback) -> void {
    movement_callback_ = callback;
}

auto AzimuthManager::applyBacklashCompensation(double target_azimuth) -> double {
    auto current = getCurrentAzimuth();
    if (!current) {
        return target_azimuth;
    }

    double diff = target_azimuth - *current;

    // Normalize difference to [-180, 180]
    while (diff > 180.0) diff -= 360.0;
    while (diff < -180.0) diff += 360.0;

    // Apply backlash compensation based on direction
    if (diff > 0 && settings_.backlash_compensation > 0) {
        // Moving clockwise, apply positive compensation
        return target_azimuth + settings_.backlash_compensation;
    } else if (diff < 0 && settings_.backlash_compensation > 0) {
        // Moving counter-clockwise, apply negative compensation
        return target_azimuth - settings_.backlash_compensation;
    }

    return target_azimuth;
}

auto AzimuthManager::startMovementMonitoring() -> void {
    auto current = getCurrentAzimuth();
    if (current) {
        start_azimuth_.store(*current);
    }

    if (!monitoring_thread_) {
        stop_monitoring_.store(false);
        monitoring_thread_ = std::make_unique<std::thread>(&AzimuthManager::monitoringLoop, this);
    }
}

auto AzimuthManager::stopMovementMonitoring() -> void {
    if (monitoring_thread_) {
        stop_monitoring_.store(true);
        if (monitoring_thread_->joinable()) {
            monitoring_thread_->join();
        }
        monitoring_thread_.reset();
    }
}

auto AzimuthManager::monitoringLoop() -> void {
    auto start_time = std::chrono::steady_clock::now();

    while (!stop_monitoring_.load() && is_moving_.load()) {
        auto current = getCurrentAzimuth();
        if (!current) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        double target = target_azimuth_.load();
        double diff = std::abs(*current - target);

        // Normalize difference
        if (diff > 180.0) {
            diff = 360.0 - diff;
        }

        // Check if we've reached the target
        if (diff <= settings_.position_tolerance) {
            is_moving_.store(false);
            if (movement_callback_) {
                movement_callback_(true, "Movement completed successfully");
            }
            spdlog::info("Dome movement completed. Position: {:.2f}°", *current);
            break;
        }

        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > std::chrono::seconds(settings_.movement_timeout)) {
            is_moving_.store(false);
            if (movement_callback_) {
                movement_callback_(false, "Movement timeout");
            }
            spdlog::error("Dome movement timeout");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace lithium::ascom::dome::components
