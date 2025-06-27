#include "temperature_manager.hpp"

namespace lithium::device::indi::filterwheel {

TemperatureManager::TemperatureManager(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {}

bool TemperatureManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing TemperatureManager");
    
    checkTemperatureCapability();
    
    if (hasSensor_) {
        setupTemperatureWatchers();
        core->getLogger()->info("Temperature sensor detected and monitoring enabled");
    } else {
        core->getLogger()->debug("No temperature sensor detected for this filter wheel");
    }
    
    initialized_ = true;
    return true;
}

void TemperatureManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down TemperatureManager");
    }
    
    currentTemperature_.reset();
    hasSensor_ = false;
    initialized_ = false;
}

bool TemperatureManager::hasTemperatureSensor() const {
    return hasSensor_;
}

std::optional<double> TemperatureManager::getTemperature() const {
    return currentTemperature_;
}

void TemperatureManager::setupTemperatureWatchers() {
    auto core = getCore();
    if (!core || !core->isConnected()) {
        return;
    }

    auto& device = core->getDevice();
    
    // Watch FILTER_TEMPERATURE property if available
    device.watchProperty("FILTER_TEMPERATURE",
        [this](const INDI::PropertyNumber& property) {
            handleTemperatureProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Some filter wheels might use TEMPERATURE property instead
    device.watchProperty("TEMPERATURE",
        [this](const INDI::PropertyNumber& property) {
            handleTemperatureProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    core->getLogger()->debug("Temperature property watchers set up");
}

void TemperatureManager::handleTemperatureProperty(const INDI::PropertyNumber& property) {
    auto core = getCore();
    if (!core) return;

    if (property.count() > 0) {
        double temperature = property[0].getValue();
        currentTemperature_ = temperature;
        
        core->getLogger()->debug("Temperature updated: {:.2f}°C", temperature);
        
        // Notify about temperature change if callback is set
        // This would require extending the core to support callbacks
    }
}

void TemperatureManager::checkTemperatureCapability() {
    auto core = getCore();
    if (!core || !core->isConnected()) {
        hasSensor_ = false;
        return;
    }

    auto& device = core->getDevice();
    
    // Check for common temperature property names
    INDI::PropertyNumber tempProp1 = device.getProperty("FILTER_TEMPERATURE");
    INDI::PropertyNumber tempProp2 = device.getProperty("TEMPERATURE");
    
    hasSensor_ = tempProp1.isValid() || tempProp2.isValid();
    
    if (hasSensor_) {
        // Try to get initial temperature reading
        if (tempProp1.isValid() && tempProp1.count() > 0) {
            currentTemperature_ = tempProp1[0].getValue();
        } else if (tempProp2.isValid() && tempProp2.count() > 0) {
            currentTemperature_ = tempProp2[0].getValue();
        }
    }
}

}  // namespace lithium::device::indi::filterwheel
