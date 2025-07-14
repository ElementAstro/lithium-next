/*
 * dome_motion.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Motion - Dome Movement Control Implementation

*************************************************/

#include "dome_motion.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>
#include <cmath>

DomeMotionManager::DomeMotionManager(INDIDomeClient* client)
    : client_(client) {}

// Motion control
[[nodiscard]] auto DomeMotionManager::moveToAzimuth(double azimuth) -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeMotion] Not connected to device");
        return false;
    }
    if (!isValidAzimuth(azimuth)) {
        spdlog::error("[DomeMotion] Invalid azimuth: {}", azimuth);
        return false;
    }
    double normalizedAzimuth = normalizeAzimuth(azimuth);
    target_azimuth_ = normalizedAzimuth;
    if (auto* azProperty = getDomeAzimuthProperty(); azProperty) {
        if (auto* azWidget = azProperty->findWidgetByName("AZ"); azWidget) {
            azWidget->setValue(normalizedAzimuth);
            client_->sendNewProperty(azProperty);
            is_moving_ = true;
            spdlog::info("[DomeMotion] Moving to azimuth: {:.2f}°",
                         normalizedAzimuth);
            notifyMotionEvent(current_azimuth_, normalizedAzimuth, true);
            return true;
        }
    }
    spdlog::error("[DomeMotion] Failed to send azimuth command");
    return false;
}

[[nodiscard]] auto DomeMotionManager::rotateRelative(double degrees) -> bool {
    double currentAz = getCurrentAzimuth();
    double targetAz = currentAz + degrees;
    return moveToAzimuth(targetAz);
}

[[nodiscard]] auto DomeMotionManager::startRotation(DomeMotion direction)
    -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeMotion] Not connected to device");
        return false;
    }
    if (auto* motionProperty = getDomeMotionProperty(); motionProperty) {
        const char* directionWidget = nullptr;
        switch (direction) {
            case DomeMotion::CLOCKWISE:
                directionWidget = "DOME_CW";
                break;
            case DomeMotion::COUNTER_CLOCKWISE:
                directionWidget = "DOME_CCW";
                break;
            default:
                spdlog::error("[DomeMotion] Invalid rotation direction");
                return false;
        }
        if (auto* widget = motionProperty->findWidgetByName(directionWidget);
            widget) {
            widget->setState(ISS_ON);
            client_->sendNewProperty(motionProperty);
            is_moving_ = true;
            spdlog::info(
                "[DomeMotion] Started {} rotation",
                (direction == DomeMotion::CLOCKWISE ? "clockwise"
                                                    : "counter-clockwise"));
            notifyMotionEvent(current_azimuth_, target_azimuth_, true);
            return true;
        }
    }
    spdlog::error("[DomeMotion] Failed to send rotation command");
    return false;
}

[[nodiscard]] auto DomeMotionManager::stopRotation() -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeMotion] Not connected to device");
        return false;
    }
    if (auto* motionProperty = getDomeMotionProperty(); motionProperty) {
        if (auto* stopWidget = motionProperty->findWidgetByName("DOME_ABORT");
            stopWidget) {
            stopWidget->setState(ISS_ON);
            client_->sendNewProperty(motionProperty);
            is_moving_ = false;
            spdlog::info("[DomeMotion] Rotation stopped");
            notifyMotionEvent(current_azimuth_, target_azimuth_, false);
            return true;
        }
    }
    return abortMotion();
}

[[nodiscard]] auto DomeMotionManager::abortMotion() -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeMotion] Not connected to device");
        return false;
    }
    if (auto* abortProperty = getDomeAbortProperty(); abortProperty) {
        if (auto* abortWidget = abortProperty->findWidgetByName("ABORT");
            abortWidget) {
            abortWidget->setState(ISS_ON);
            client_->sendNewProperty(abortProperty);
            is_moving_ = false;
            spdlog::info("[DomeMotion] Motion aborted");
            notifyMotionEvent(current_azimuth_, target_azimuth_, false);
            return true;
        }
    }
    spdlog::error("[DomeMotion] Failed to send abort command");
    return false;
}

// Position queries
auto DomeMotionManager::getCurrentAzimuth() -> double {
    return current_azimuth_;
}

auto DomeMotionManager::getTargetAzimuth() -> double { return target_azimuth_; }

auto DomeMotionManager::isMoving() -> bool { return is_moving_; }

// Speed control
auto DomeMotionManager::setRotationSpeed(double degreesPerSecond) -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (degreesPerSecond < min_speed_ || degreesPerSecond > max_speed_) {
        spdlog::error(
            "[DomeMotion] Invalid speed: {:.2f} (range: {:.2f} - {:.2f})",
            degreesPerSecond, min_speed_, max_speed_);
        return false;
    }
    rotation_speed_ = degreesPerSecond;
    if (auto* speedProperty = getDomeSpeedProperty(); speedProperty) {
        if (auto* speedWidget = speedProperty->findWidgetByName("DOME_SPEED");
            speedWidget) {
            speedWidget->setValue(degreesPerSecond);
            client_->sendNewProperty(speedProperty);
            spdlog::info("[DomeMotion] Set rotation speed to: {:.2f}°/s",
                         degreesPerSecond);
            return true;
        }
    }
    spdlog::warn("[DomeMotion] Speed property not available, storing locally");
    return true;
}

auto DomeMotionManager::getRotationSpeed() -> double { return rotation_speed_; }

auto DomeMotionManager::getMaxSpeed() -> double { return max_speed_; }

auto DomeMotionManager::getMinSpeed() -> double { return min_speed_; }

// Motion limits
auto DomeMotionManager::setAzimuthLimits(double minAz, double maxAz) -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (minAz >= maxAz) {
        spdlog::error(
            "[DomeMotion] Invalid azimuth limits: min={:.2f}, max={:.2f}",
            minAz, maxAz);
        return false;
    }
    min_azimuth_ = normalizeAzimuth(minAz);
    max_azimuth_ = normalizeAzimuth(maxAz);
    has_azimuth_limits_ = true;
    spdlog::info("[DomeMotion] Set azimuth limits: {:.2f}° - {:.2f}°",
                 min_azimuth_, max_azimuth_);
    return true;
}

auto DomeMotionManager::getAzimuthLimits() -> std::pair<double, double> {
    std::scoped_lock lock(motion_mutex_);
    return {min_azimuth_, max_azimuth_};
}

auto DomeMotionManager::hasAzimuthLimits() -> bool {
    return has_azimuth_limits_;
}

// Backlash compensation
auto DomeMotionManager::getBacklash() -> double {
    return backlash_compensation_;
}

auto DomeMotionManager::setBacklash(double backlash) -> bool {
    std::scoped_lock lock(motion_mutex_);
    if (backlash < 0.0 || backlash > 10.0) {
        spdlog::error("[DomeMotion] Invalid backlash value: {:.2f}", backlash);
        return false;
    }
    backlash_compensation_ = backlash;
    spdlog::info("[DomeMotion] Set backlash compensation to: {:.2f}°",
                 backlash);
    return true;
}

auto DomeMotionManager::enableBacklashCompensation(bool enable) -> bool {
    std::scoped_lock lock(motion_mutex_);
    backlash_enabled_ = enable;
    spdlog::info("[DomeMotion] Backlash compensation {}",
                 enable ? "enabled" : "disabled");
    return true;
}

auto DomeMotionManager::isBacklashCompensationEnabled() -> bool {
    return backlash_enabled_;
}

// INDI property handling
void DomeMotionManager::handleMotionProperty(const INDI::Property& property) {
    if (property.getType() == INDI_NUMBER) {
        auto numberProperty = property.getNumber();
        if (property.getName() == std::string("ABS_DOME_POSITION") ||
            property.getName() == std::string("DOME_ABSOLUTE_POSITION")) {
            updateAzimuthFromProperty(numberProperty);
        } else if (property.getName() == std::string("DOME_SPEED")) {
            updateSpeedFromProperty(numberProperty);
        }
    }
}

void DomeMotionManager::updateAzimuthFromProperty(
    INDI::PropertyViewNumber* property) {
    if (!property) {
        return;
    }
    std::scoped_lock lock(motion_mutex_);
    for (int i = 0; i < property->count(); ++i) {
        auto widget = property->at(i);
        std::string widgetName = widget->getName();
        if (widgetName == "AZ" || widgetName == "DOME_ABSOLUTE_POSITION") {
            double newAzimuth = widget->getValue();
            current_azimuth_ = normalizeAzimuth(newAzimuth);
            double diff = std::abs(current_azimuth_ - target_azimuth_);
            if (diff < 1.0 && is_moving_) {  // Within 1 degree
                is_moving_ = false;
                notifyMotionEvent(current_azimuth_, target_azimuth_, false);
            }
            break;
        }
    }
}

void DomeMotionManager::updateSpeedFromProperty(
    INDI::PropertyViewNumber* property) {
    if (!property) {
        return;
    }
    std::scoped_lock lock(motion_mutex_);
    for (int i = 0; i < property->count(); ++i) {
        auto widget = property->at(i);
        std::string widgetName = widget->getName();
        if (widgetName == "DOME_SPEED") {
            rotation_speed_ = widget->getValue();
            break;
        }
    }
}

void DomeMotionManager::synchronizeWithDevice() {
    if (!client_->isConnected()) {
        return;
    }
    auto azProperty = getDomeAzimuthProperty();
    if (azProperty) {
        updateAzimuthFromProperty(azProperty);
    }
    auto speedProperty = getDomeSpeedProperty();
    if (speedProperty) {
        updateSpeedFromProperty(speedProperty);
    }
}

// Utility methods
double DomeMotionManager::normalizeAzimuth(double azimuth) {
    azimuth = std::fmod(azimuth, 360.0);
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    return azimuth;
}

void DomeMotionManager::setMotionCallback(MotionCallback callback) {
    std::scoped_lock lock(motion_mutex_);
    motion_callback_ = std::move(callback);
}

// Internal methods
void DomeMotionManager::notifyMotionEvent(double currentAz, double targetAz,
                                          bool moving) {
    if (motion_callback_) {
        try {
            motion_callback_(currentAz, targetAz, moving);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeMotion] Motion callback error: {}", ex.what());
        }
    }
}

auto DomeMotionManager::isValidAzimuth(double azimuth) -> bool {
    if (has_azimuth_limits_) {
        double normalized = normalizeAzimuth(azimuth);
        return normalized >= min_azimuth_ && normalized <= max_azimuth_;
    }
    return true;
}

auto DomeMotionManager::calculateShortestPath(double from, double to)
    -> double {
    double diff = to - from;
    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff < -180.0) {
        diff += 360.0;
    }
    return diff;
}

// INDI property helpers
auto DomeMotionManager::getDomeAzimuthProperty() -> INDI::PropertyViewNumber* {
    if (!client_->isConnected()) {
        return nullptr;
    }
    auto& device = client_->getBaseDevice();
    std::vector<std::string> propertyNames = {
        "ABS_DOME_POSITION", "DOME_ABSOLUTE_POSITION", "DOME_POSITION"};
    for (const auto& propName : propertyNames) {
        auto property = device.getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_NUMBER) {
            return property.getNumber();
        }
    }
    return nullptr;
}

auto DomeMotionManager::getDomeSpeedProperty() -> INDI::PropertyViewNumber* {
    if (!client_->isConnected()) {
        return nullptr;
    }
    auto& device = client_->getBaseDevice();
    auto property = device.getProperty("DOME_SPEED");
    if (property.isValid() && property.getType() == INDI_NUMBER) {
        return property.getNumber();
    }
    return nullptr;
}

auto DomeMotionManager::getDomeMotionProperty() -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) {
        return nullptr;
    }
    auto& device = client_->getBaseDevice();
    std::vector<std::string> propertyNames = {"DOME_MOTION", "DOME_DIRECTION"};
    for (const auto& propName : propertyNames) {
        auto property = device.getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}

auto DomeMotionManager::getDomeAbortProperty() -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) {
        return nullptr;
    }
    auto& device = client_->getBaseDevice();
    std::vector<std::string> propertyNames = {"DOME_ABORT_MOTION", "DOME_ABORT",
                                              "ABORT_MOTION"};
    for (const auto& propName : propertyNames) {
        auto property = device.getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}
