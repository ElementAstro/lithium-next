#include "temperature_controller.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi::camera {

TemperatureController::TemperatureController(std::shared_ptr<INDICameraCore> core) 
    : ComponentBase(core) {
    spdlog::debug("Creating temperature controller");
}

auto TemperatureController::initialize() -> bool {
    spdlog::debug("Initializing temperature controller");
    
    // Reset temperature state
    isCooling_.store(false);
    currentTemperature_.store(0.0);
    targetTemperature_.store(0.0);
    coolingPower_.store(0.0);
    
    // Initialize temperature info
    temperatureInfo_.current = 0.0;
    temperatureInfo_.target = 0.0;
    temperatureInfo_.coolingPower = 0.0;
    temperatureInfo_.coolerOn = false;
    temperatureInfo_.canSetTemperature = false;
    
    return true;
}

auto TemperatureController::destroy() -> bool {
    spdlog::debug("Destroying temperature controller");
    
    // Stop cooling if active
    if (isCoolerOn()) {
        stopCooling();
    }
    
    return true;
}

auto TemperatureController::getComponentName() const -> std::string {
    return "TemperatureController";
}

auto TemperatureController::handleProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        return false;
    }
    
    std::string propertyName = property.getName();
    
    if (propertyName == "CCD_TEMPERATURE") {
        handleTemperatureProperty(property);
        return true;
    } else if (propertyName == "CCD_COOLER") {
        handleCoolerProperty(property);
        return true;
    } else if (propertyName == "CCD_COOLER_POWER") {
        handleCoolerPowerProperty(property);
        return true;
    }
    
    return false;
}

auto TemperatureController::startCooling(double targetTemp) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    // Set target temperature first
    if (!setTemperature(targetTemp)) {
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdCooler = device.getProperty("CCD_COOLER");
        if (!ccdCooler.isValid()) {
            spdlog::error("CCD_COOLER property not found - camera may not support cooling");
            return false;
        }

        spdlog::info("Starting cooler with target temperature: {} C", targetTemp);
        ccdCooler[0].setState(ISS_ON);
        getCore()->sendNewProperty(ccdCooler);
        
        targetTemperature_.store(targetTemp);
        temperatureInfo_.target = targetTemp;
        isCooling_.store(true);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start cooling: {}", e.what());
        return false;
    }
}

auto TemperatureController::stopCooling() -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdCooler = device.getProperty("CCD_COOLER");
        if (!ccdCooler.isValid()) {
            spdlog::error("CCD_COOLER property not found");
            return false;
        }

        spdlog::info("Stopping cooler...");
        ccdCooler[0].setState(ISS_OFF);
        getCore()->sendNewProperty(ccdCooler);
        isCooling_.store(false);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to stop cooling: {}", e.what());
        return false;
    }
}

auto TemperatureController::isCoolerOn() const -> bool {
    return isCooling_.load();
}

auto TemperatureController::setTemperature(double temperature) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber ccdTemperature = device.getProperty("CCD_TEMPERATURE");
        if (!ccdTemperature.isValid()) {
            spdlog::error("CCD_TEMPERATURE property not found");
            return false;
        }

        spdlog::info("Setting temperature to {} C...", temperature);
        ccdTemperature[0].setValue(temperature);
        getCore()->sendNewProperty(ccdTemperature);
        
        targetTemperature_.store(temperature);
        temperatureInfo_.target = temperature;
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set temperature: {}", e.what());
        return false;
    }
}

auto TemperatureController::getTemperature() const -> std::optional<double> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }
    return currentTemperature_.load();
}

auto TemperatureController::getTemperatureInfo() const -> TemperatureInfo {
    return temperatureInfo_;
}

auto TemperatureController::getCoolingPower() const -> std::optional<double> {
    if (!getCore()->isConnected()) {
        return std::nullopt;
    }
    return coolingPower_.load();
}

auto TemperatureController::hasCooler() const -> bool {
    if (!getCore()->isConnected()) {
        return false;
    }
    
    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdCooler = device.getProperty("CCD_COOLER");
        return ccdCooler.isValid();
    } catch (const std::exception& e) {
        return false;
    }
}

// Private methods
void TemperatureController::handleTemperatureProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber tempProperty = property;
    if (!tempProperty.isValid()) {
        return;
    }
    
    double temp = tempProperty[0].getValue();
    currentTemperature_.store(temp);
    temperatureInfo_.current = temp;
    
    spdlog::debug("Temperature updated: {} C", temp);
    updateTemperatureInfo();
}

void TemperatureController::handleCoolerProperty(INDI::Property property) {
    if (property.getType() != INDI_SWITCH) {
        return;
    }
    
    INDI::PropertySwitch coolerProperty = property;
    if (!coolerProperty.isValid()) {
        return;
    }
    
    bool coolerOn = (coolerProperty[0].getState() == ISS_ON);
    isCooling_.store(coolerOn);
    temperatureInfo_.canSetTemperature = true;
    
    spdlog::debug("Cooler state: {}", coolerOn ? "ON" : "OFF");
}

void TemperatureController::handleCoolerPowerProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }
    
    INDI::PropertyNumber powerProperty = property;
    if (!powerProperty.isValid()) {
        return;
    }
    
    double power = powerProperty[0].getValue();
    coolingPower_.store(power);
    temperatureInfo_.coolingPower = power;
    
    spdlog::debug("Cooling power: {}%", power);
}

void TemperatureController::updateTemperatureInfo() {
    temperatureInfo_.current = currentTemperature_.load();
    temperatureInfo_.target = targetTemperature_.load();
    temperatureInfo_.coolingPower = coolingPower_.load();
    temperatureInfo_.canSetTemperature = hasCooler();
}

} // namespace lithium::device::indi::camera
