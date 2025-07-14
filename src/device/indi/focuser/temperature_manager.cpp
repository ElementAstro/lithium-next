#include "temperature_manager.hpp"
#include <cmath>

namespace lithium::device::indi::focuser {

TemperatureManager::TemperatureManager(std::shared_ptr<INDIFocuserCore> core)
    : FocuserComponentBase(std::move(core)) {
    lastCompensationTemperature_ = 20.0; // Default starting temperature
}

bool TemperatureManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    lastCompensationTemperature_ = core->getTemperature();
    core->getLogger()->info("{}: Initializing temperature manager", getComponentName());
    return true;
}

void TemperatureManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("{}: Shutting down temperature manager", getComponentName());
    }
}

std::optional<double> TemperatureManager::getExternalTemperature() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }
    return property[0].getValue();
}

std::optional<double> TemperatureManager::getChipTemperature() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("CHIP_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }
    return property[0].getValue();
}

bool TemperatureManager::hasTemperatureSensor() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return false;
    }

    const auto tempProperty = core->getDevice().getProperty("FOCUS_TEMPERATURE");
    return tempProperty.isValid();
}

TemperatureCompensation TemperatureManager::getTemperatureCompensation() const {
    auto core = getCore();
    TemperatureCompensation comp;

    if (!core || !core->getDevice().isValid()) {
        return comp; // Return default compensation settings
    }

    // Try to read temperature compensation settings from device properties
    INDI::PropertySwitch enabledProp = core->getDevice().getProperty("TEMP_COMPENSATION_ENABLED");
    if (enabledProp.isValid()) {
        comp.enabled = enabledProp[0].getState() == ISS_ON;
    }

    INDI::PropertyNumber coeffProp = core->getDevice().getProperty("TEMP_COMPENSATION_COEFF");
    if (coeffProp.isValid()) {
        comp.coefficient = coeffProp[0].getValue();
    }

    return comp;
}

bool TemperatureManager::setTemperatureCompensation(const TemperatureCompensation& comp) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid() || !core->getClient()) {
        return false;
    }

    bool success = true;

    // Set compensation coefficient
    INDI::PropertyNumber coeffProp = core->getDevice().getProperty("TEMP_COMPENSATION_COEFF");
    if (coeffProp.isValid()) {
        coeffProp[0].value = comp.coefficient;
        core->getClient()->sendNewProperty(coeffProp);
        core->getLogger()->info("Set temperature compensation coefficient to {:.4f}", comp.coefficient);
    } else {
        success = false;
    }

    // Set enabled/disabled state
    INDI::PropertySwitch enabledProp = core->getDevice().getProperty("TEMP_COMPENSATION_ENABLED");
    if (enabledProp.isValid()) {
        enabledProp[0].setState(comp.enabled ? ISS_ON : ISS_OFF);
        enabledProp[1].setState(comp.enabled ? ISS_OFF : ISS_ON);
        core->getClient()->sendNewProperty(enabledProp);
        core->getLogger()->info("Temperature compensation {}", comp.enabled ? "enabled" : "disabled");
    } else {
        success = false;
    }

    return success;
}

bool TemperatureManager::enableTemperatureCompensation(bool enable) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid() || !core->getClient()) {
        return false;
    }

    INDI::PropertySwitch enabledProp = core->getDevice().getProperty("TEMP_COMPENSATION_ENABLED");
    if (!enabledProp.isValid()) {
        core->getLogger()->warn("Temperature compensation property not available");
        return false;
    }

    enabledProp[0].setState(enable ? ISS_ON : ISS_OFF);
    enabledProp[1].setState(enable ? ISS_OFF : ISS_ON);
    core->getClient()->sendNewProperty(enabledProp);

    core->getLogger()->info("Temperature compensation {}", enable ? "enabled" : "disabled");
    return true;
}

bool TemperatureManager::isTemperatureCompensationEnabled() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return false;
    }

    INDI::PropertySwitch enabledProp = core->getDevice().getProperty("TEMP_COMPENSATION_ENABLED");
    if (!enabledProp.isValid()) {
        return false;
    }

    return enabledProp[0].getState() == ISS_ON;
}

void TemperatureManager::checkTemperatureCompensation() {
    auto core = getCore();
    if (!core) {
        return;
    }

    if (!isTemperatureCompensationEnabled()) {
        return; // Compensation is disabled
    }

    double currentTemp = core->getTemperature();
    double temperatureDelta = currentTemp - lastCompensationTemperature_;

    // Only compensate if temperature change is significant (> 0.1°C)
    if (std::abs(temperatureDelta) > 0.1) {
        applyTemperatureCompensation(temperatureDelta);
        lastCompensationTemperature_ = currentTemp;
    }
}

double TemperatureManager::calculateCompensationSteps(double temperatureDelta) const {
    auto comp = getTemperatureCompensation();
    if (!comp.enabled) {
        return 0.0;
    }

    // Steps = coefficient * temperature_change
    // Positive coefficient means focus moves out when temperature increases
    return comp.coefficient * temperatureDelta;
}

void TemperatureManager::applyTemperatureCompensation(double temperatureDelta) {
    auto core = getCore();
    if (!core) {
        return;
    }

    double compensationSteps = calculateCompensationSteps(temperatureDelta);
    if (std::abs(compensationSteps) < 1.0) {
        return; // Too small to matter
    }

    int steps = static_cast<int>(std::round(compensationSteps));

    core->getLogger()->info("Applying temperature compensation: {:.2f}°C change requires {} steps",
                           temperatureDelta, steps);

    // Apply compensation through INDI
    if (core->getDevice().isValid() && core->getClient()) {
        INDI::PropertyNumber relPosProp = core->getDevice().getProperty("REL_FOCUS_POSITION");
        if (relPosProp.isValid()) {
            relPosProp[0].value = steps;
            core->getClient()->sendNewProperty(relPosProp);
        }
    }
}

}  // namespace lithium::device::indi::focuser
