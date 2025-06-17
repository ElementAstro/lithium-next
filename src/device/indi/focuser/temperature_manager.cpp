#include "temperature_manager.hpp"
#include <cmath>

namespace lithium::device::indi::focuser {

bool TemperatureManager::initialize(FocuserState& state) {
    state_ = &state;
    lastCompensationTemperature_ = state_->temperature_.load();
    state_->logger_->info("{}: Initializing temperature manager",
                          getComponentName());
    return true;
}

void TemperatureManager::cleanup() {
    if (state_) {
        state_->logger_->info("{}: Cleaning up temperature manager",
                              getComponentName());
    }
    state_ = nullptr;
}

std::optional<double> TemperatureManager::getExternalTemperature() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }
    INDI::PropertyNumber property =
        state_->device_.getProperty("FOCUS_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }
    return property[0].getValue();
}

std::optional<double> TemperatureManager::getChipTemperature() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }
    INDI::PropertyNumber property =
        state_->device_.getProperty("CHIP_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }
    return property[0].getValue();
}

bool TemperatureManager::hasTemperatureSensor() const {
    if (!state_ || !state_->device_.isValid()) {
        return false;
    }
    const auto tempProperty = state_->device_.getProperty("FOCUS_TEMPERATURE");
    const auto chipProperty = state_->device_.getProperty("CHIP_TEMPERATURE");
    return tempProperty.isValid() || chipProperty.isValid();
}

TemperatureCompensation TemperatureManager::getTemperatureCompensation() const {
    if (!state_) {
        return {};
    }
    return state_->tempCompensation_;
}

bool TemperatureManager::setTemperatureCompensation(
    const TemperatureCompensation& comp) {
    if (!state_) {
        return false;
    }
    state_->tempCompensation_ = comp;
    state_->logger_->info(
        "Temperature compensation set: enabled={}, coefficient={}",
        comp.enabled, comp.coefficient);
    return true;
}

bool TemperatureManager::enableTemperatureCompensation(bool enable) {
    if (!state_) {
        return false;
    }
    state_->tempCompensationEnabled_ = enable;
    state_->tempCompensation_.enabled = enable;
    state_->logger_->info("Temperature compensation {}",
                          enable ? "enabled" : "disabled");
    return true;
}

bool TemperatureManager::isTemperatureCompensationEnabled() const {
    return state_ && state_->tempCompensationEnabled_.load();
}

void TemperatureManager::checkTemperatureCompensation() {
    if (!state_ || !isTemperatureCompensationEnabled()) {
        return;
    }
    auto currentTemp = getExternalTemperature();
    if (!currentTemp) {
        return;
    }
    double temperatureDelta = *currentTemp - lastCompensationTemperature_;
    if (std::abs(temperatureDelta) > 0.1) {
        applyTemperatureCompensation(temperatureDelta);
        lastCompensationTemperature_ = *currentTemp;
    }
}

double TemperatureManager::calculateCompensationSteps(
    double temperatureDelta) const {
    if (!state_) {
        return 0.0;
    }
    return temperatureDelta * state_->tempCompensation_.coefficient;
}

void TemperatureManager::applyTemperatureCompensation(double temperatureDelta) {
    if (!state_) {
        return;
    }
    double compensationSteps = calculateCompensationSteps(temperatureDelta);
    if (std::abs(compensationSteps) >= 1.0) {
        int steps = static_cast<int>(std::round(compensationSteps));
        state_->tempCompensation_.compensationOffset += compensationSteps;
        state_->logger_->info(
            "Applying temperature compensation: {} steps for {}°C change",
            steps, temperatureDelta);
        // Note: Actual movement would be handled by MovementController
        // This component just calculates and tracks the compensation
    }
}

}  // namespace lithium::device::indi::focuser
