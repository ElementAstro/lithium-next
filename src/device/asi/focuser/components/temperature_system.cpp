/*
 * temperature_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Temperature System Implementation

*************************************************/

#include "temperature_system.hpp"

#include "hardware_interface.hpp"
#include "position_manager.hpp"

#include <spdlog/spdlog.h>

#include <cmath>

namespace lithium::device::asi::focuser::components {

TemperatureSystem::TemperatureSystem(HardwareInterface* hardware,
                                     PositionManager* positionManager)
    : hardware_(hardware), positionManager_(positionManager) {
    spdlog::info("Created ASI Focuser Temperature System");
}

TemperatureSystem::~TemperatureSystem() {
    spdlog::info("Destroyed ASI Focuser Temperature System");
}

std::optional<double> TemperatureSystem::getCurrentTemperature() const {
    if (!hardware_ || !hardware_->isConnected() ||
        !hardware_->hasTemperatureSensor()) {
        return std::nullopt;
    }

    float temp = 0.0f;
    if (hardware_->getTemperature(temp)) {
        return static_cast<double>(temp);
    }

    return std::nullopt;
}

bool TemperatureSystem::hasTemperatureSensor() const {
    return hardware_ && hardware_->hasTemperatureSensor();
}

bool TemperatureSystem::setTemperatureCoefficient(double coefficient) {
    std::lock_guard<std::mutex> lock(temperatureMutex_);

    temperatureCoefficient_ = coefficient;
    spdlog::info("Set temperature coefficient to: {:.2f} steps/°C",
                 coefficient);
    return true;
}

bool TemperatureSystem::enableTemperatureCompensation(bool enable) {
    std::lock_guard<std::mutex> lock(temperatureMutex_);

    compensationEnabled_ = enable;

    if (enable) {
        // Set current temperature as reference
        auto temp = getCurrentTemperature();
        if (temp.has_value()) {
            referenceTemperature_ = temp.value();
            currentTemperature_ = temp.value();
            lastTemperature_ = temp.value();
        }
    }

    spdlog::info("Temperature compensation {}",
                 enable ? "enabled" : "disabled");
    return true;
}

bool TemperatureSystem::setCompensationThreshold(double threshold) {
    if (threshold < 0.1 || threshold > 10.0) {
        return false;
    }

    compensationThreshold_ = threshold;
    spdlog::info("Set compensation threshold to: {:.1f}°C", threshold);
    return true;
}

bool TemperatureSystem::applyTemperatureCompensation() {
    std::lock_guard<std::mutex> lock(temperatureMutex_);

    if (!compensationEnabled_ || temperatureCoefficient_ == 0.0) {
        return false;
    }

    if (!updateTemperature()) {
        return false;
    }

    double tempDelta = currentTemperature_ - lastTemperature_;

    if (std::abs(tempDelta) < compensationThreshold_) {
        return true;  // No compensation needed
    }

    int compensationSteps = calculateCompensationSteps(tempDelta);

    if (compensationSteps == 0) {
        return true;  // No compensation needed
    }

    spdlog::info(
        "Applying temperature compensation: {} steps for {:.1f}°C change",
        compensationSteps, tempDelta);

    if (!positionManager_) {
        spdlog::error(
            "Position manager not available for temperature compensation");
        return false;
    }

    int currentPosition = positionManager_->getCurrentPosition();
    int newPosition = currentPosition + compensationSteps;

    // Validate new position
    if (!positionManager_->validatePosition(newPosition)) {
        spdlog::warn(
            "Temperature compensation would move to invalid position: {}",
            newPosition);
        return false;
    }

    compensationActive_ = true;

    bool success = positionManager_->moveToPosition(newPosition);

    if (success) {
        lastTemperature_ = currentTemperature_;
        lastCompensationSteps_ = compensationSteps;
        lastTemperatureDelta_ = tempDelta;

        notifyCompensationApplied(compensationSteps, tempDelta);
        spdlog::info("Temperature compensation applied successfully");
    } else {
        spdlog::error("Failed to apply temperature compensation");
    }

    compensationActive_ = false;
    return success;
}

int TemperatureSystem::calculateCompensationSteps(
    double temperatureDelta) const {
    if (temperatureCoefficient_ == 0.0) {
        return 0;
    }

    return static_cast<int>(temperatureDelta * temperatureCoefficient_);
}

bool TemperatureSystem::updateTemperature() {
    auto temp = getCurrentTemperature();
    if (temp.has_value()) {
        double newTemp = temp.value();
        if (std::abs(newTemp - currentTemperature_) > 0.1) {
            currentTemperature_ = newTemp;
            notifyTemperatureChange(newTemp);
        }
        return true;
    }
    return false;
}

void TemperatureSystem::notifyTemperatureChange(double temperature) {
    if (temperatureCallback_) {
        temperatureCallback_(temperature);
    }
}

void TemperatureSystem::notifyCompensationApplied(int steps, double delta) {
    if (compensationCallback_) {
        compensationCallback_(steps, delta);
    }
}

}  // namespace lithium::device::asi::focuser::components
