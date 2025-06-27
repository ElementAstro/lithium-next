/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "hardware_interface.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

namespace lithium::device::indi::telescope::components {

HardwareInterface::HardwareInterface() {
    setServer("localhost", 7624);
}

HardwareInterface::~HardwareInterface() {
    shutdown();
}

bool HardwareInterface::initialize() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    if (initialized_.load()) {
        logWarning("Hardware interface already initialized");
        return true;
    }
    
    try {
        // Connect to INDI server
        if (!connectServer()) {
            logError("Failed to connect to INDI server");
            return false;
        }
        
        // Wait for server connection
        if (!waitForConnection(10000)) {
            logError("Failed to establish server connection");
            return false;
        }
        
        initialized_.store(true);
        logInfo("Hardware interface initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    if (!initialized_.load()) {
        return true;
    }
    
    try {
        if (connected_.load()) {
            disconnectFromDevice();
        }
        
        if (serverConnected_.load()) {
            disconnectServer();
        }
        
        initialized_.store(false);
        logInfo("Hardware interface shutdown successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception during shutdown: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::connectToDevice(const std::string& deviceName, int timeout) {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    if (!initialized_.load()) {
        logError("Hardware interface not initialized");
        return false;
    }
    
    if (connected_.load()) {
        if (deviceName_ == deviceName) {
            logInfo("Already connected to device: " + deviceName);
            return true;
        } else {
            // Disconnect from current device first
            disconnectFromDevice();
        }
    }
    
    deviceName_ = deviceName;
    
    try {
        // Watch for the device
        watchDevice(deviceName.c_str(), [this](INDI::BaseDevice device) {
            std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
            device_ = device;
            updateDeviceInfo();
        });
        
        // Wait for device connection
        auto startTime = std::chrono::steady_clock::now();
        while (!device_.isValid() && 
               std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - startTime).count() < timeout) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!device_.isValid()) {
            logError("Device not found or timeout: " + deviceName);
            return false;
        }
        
        // Connect to device
        connectDevice(deviceName.c_str());
        
        // Wait for connection property
        if (!waitForProperty("CONNECTION", 5000)) {
            logError("CONNECTION property not available");
            return false;
        }
        
        // Check connection status
        auto connectionProp = getSwitchPropertyHandle("CONNECTION");
        if (connectionProp.isValid()) {
            auto connectSwitch = connectionProp.findWidgetByName("CONNECT");
            if (connectSwitch && connectSwitch->getState() == ISS_ON) {
                connected_.store(true);
                logInfo("Successfully connected to device: " + deviceName);
                return true;
            }
        }
        
        logError("Failed to connect to device: " + deviceName);
        return false;
        
    } catch (const std::exception& e) {
        logError("Exception connecting to device: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::disconnectFromDevice() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    if (!connected_.load()) {
        return true;
    }
    
    try {
        if (device_.isValid()) {
            disconnectDevice(deviceName_.c_str());
            device_ = INDI::BaseDevice();
        }
        
        connected_.store(false);
        deviceName_.clear();
        
        logInfo("Disconnected from device");
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception disconnecting from device: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> HardwareInterface::scanDevices() {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    std::vector<std::string> devices;
    
    if (!initialized_.load()) {
        logWarning("Hardware interface not initialized");
        return devices;
    }
    
    try {
        auto deviceList = getDevices();
        for (const auto& device : deviceList) {
            if (device.isValid()) {
                devices.push_back(device.getDeviceName());
            }
        }
        
        logInfo("Found " + std::to_string(devices.size()) + " devices");
        return devices;
        
    } catch (const std::exception& e) {
        logError("Exception scanning devices: " + std::string(e.what()));
        return devices;
    }
}

std::optional<HardwareInterface::TelescopeInfo> HardwareInterface::getTelescopeInfo() const {
    std::lock_guard<std::recursive_mutex> lock(deviceMutex_);
    
    if (!connected_.load() || !device_.isValid()) {
        return std::nullopt;
    }
    
    TelescopeInfo info;
    info.deviceName = deviceName_;
    info.isConnected = connected_.load();
    
    // Get driver information
    auto driverInfo = device_.getDriverInterface();
    if (driverInfo & INDI::BaseDevice::TELESCOPE_INTERFACE) {
        info.capabilities |= TELESCOPE_CAN_GOTO | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_ABORT;
    }
    
    return info;
}

bool HardwareInterface::setNumberProperty(const std::string& propertyName, 
                                         const std::string& elementName, 
                                         double value) {
    std::lock_guard<std::recursive_mutex> lock(propertyMutex_);
    
    try {
        auto property = getNumberPropertyHandle(propertyName);
        if (!property.isValid()) {
            logError("Property not found: " + propertyName);
            return false;
        }
        
        auto element = property.findWidgetByName(elementName.c_str());
        if (!element) {
            logError("Element not found: " + elementName + " in " + propertyName);
            return false;
        }
        
        element->setValue(value);
        sendNewProperty(property);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception setting number property: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::setSwitchProperty(const std::string& propertyName, 
                                         const std::string& elementName, 
                                         bool value) {
    std::lock_guard<std::recursive_mutex> lock(propertyMutex_);
    
    try {
        auto property = getSwitchPropertyHandle(propertyName);
        if (!property.isValid()) {
            logError("Property not found: " + propertyName);
            return false;
        }
        
        auto element = property.findWidgetByName(elementName.c_str());
        if (!element) {
            logError("Element not found: " + elementName + " in " + propertyName);
            return false;
        }
        
        element->setState(value ? ISS_ON : ISS_OFF);
        sendNewProperty(property);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception setting switch property: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::setTargetCoordinates(double ra, double dec) {
    return setNumberProperty("EQUATORIAL_EOD_COORD", "RA", ra) &&
           setNumberProperty("EQUATORIAL_EOD_COORD", "DEC", dec);
}

bool HardwareInterface::setTelescopeAction(const std::string& action) {
    if (action == "SLEW") {
        return setSwitchProperty("ON_COORD_SET", "SLEW", true);
    } else if (action == "SYNC") {
        return setSwitchProperty("ON_COORD_SET", "SYNC", true);
    } else if (action == "TRACK") {
        return setSwitchProperty("ON_COORD_SET", "TRACK", true);
    } else if (action == "ABORT") {
        return setSwitchProperty("TELESCOPE_ABORT_MOTION", "ABORT", true);
    }
    
    logError("Unknown telescope action: " + action);
    return false;
}

bool HardwareInterface::setTrackingState(bool enabled) {
    return setSwitchProperty("TELESCOPE_TRACK_STATE", enabled ? "TRACK_ON" : "TRACK_OFF", true);
}

std::optional<std::pair<double, double>> HardwareInterface::getCurrentCoordinates() const {
    std::lock_guard<std::recursive_mutex> lock(propertyMutex_);
    
    try {
        auto property = getNumberPropertyHandle("EQUATORIAL_EOD_COORD");
        if (!property.isValid()) {
            return std::nullopt;
        }
        
        auto raElement = property.findWidgetByName("RA");
        auto decElement = property.findWidgetByName("DEC");
        
        if (!raElement || !decElement) {
            return std::nullopt;
        }
        
        return std::make_pair(raElement->getValue(), decElement->getValue());
        
    } catch (const std::exception& e) {
        logError("Exception getting current coordinates: " + std::string(e.what()));
        return std::nullopt;
    }
}

bool HardwareInterface::isTracking() const {
    std::lock_guard<std::recursive_mutex> lock(propertyMutex_);
    
    try {
        auto property = getSwitchPropertyHandle("TELESCOPE_TRACK_STATE");
        if (!property.isValid()) {
            return false;
        }
        
        auto trackOnElement = property.findWidgetByName("TRACK_ON");
        return trackOnElement && trackOnElement->getState() == ISS_ON;
        
    } catch (const std::exception& e) {
        logError("Exception checking tracking state: " + std::string(e.what()));
        return false;
    }
}

bool HardwareInterface::waitForProperty(const std::string& propertyName, int timeout) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count() < timeout) {
        
        if (device_.isValid()) {
            auto property = device_.getProperty(propertyName.c_str());
            if (property.isValid()) {
                return true;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

// INDI BaseClient virtual methods
void HardwareInterface::newDevice(INDI::BaseDevice baseDevice) {
    logInfo("New device: " + std::string(baseDevice.getDeviceName()));
}

void HardwareInterface::removeDevice(INDI::BaseDevice baseDevice) {
    logInfo("Device removed: " + std::string(baseDevice.getDeviceName()));
    
    if (baseDevice.getDeviceName() == deviceName_) {
        connected_.store(false);
        device_ = INDI::BaseDevice();
    }
}

void HardwareInterface::newProperty(INDI::Property property) {
    handlePropertyUpdate(property);
    
    if (propertyUpdateCallback_) {
        propertyUpdateCallback_(property.getName(), property);
    }
}

void HardwareInterface::updateProperty(INDI::Property property) {
    handlePropertyUpdate(property);
    
    if (propertyUpdateCallback_) {
        propertyUpdateCallback_(property.getName(), property);
    }
}

void HardwareInterface::removeProperty(INDI::Property property) {
    logInfo("Property removed: " + std::string(property.getName()));
}

void HardwareInterface::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    std::string message = baseDevice.messageQueue(messageID);
    logInfo("Message from " + std::string(baseDevice.getDeviceName()) + ": " + message);
    
    if (messageCallback_) {
        messageCallback_(message, messageID);
    }
}

void HardwareInterface::serverConnected() {
    serverConnected_.store(true);
    logInfo("Connected to INDI server");
    
    if (connectionCallback_) {
        connectionCallback_(true);
    }
}

void HardwareInterface::serverDisconnected(int exit_code) {
    serverConnected_.store(false);
    connected_.store(false);
    logInfo("Disconnected from INDI server (exit code: " + std::to_string(exit_code) + ")");
    
    if (connectionCallback_) {
        connectionCallback_(false);
    }
}

// Private methods
bool HardwareInterface::waitForConnection(int timeout) {
    auto startTime = std::chrono::steady_clock::now();
    
    while (!serverConnected_.load() && 
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - startTime).count() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return serverConnected_.load();
}

void HardwareInterface::updateDeviceInfo() {
    if (!device_.isValid()) {
        return;
    }
    
    logInfo("Device info updated for: " + std::string(device_.getDeviceName()));
}

void HardwareInterface::handlePropertyUpdate(const INDI::Property& property) {
    std::string propertyName = property.getName();
    
    // Handle connection property specially
    if (propertyName == "CONNECTION") {
        auto switchProp = property.getSwitch();
        if (switchProp && switchProp->isValid()) {
            auto connectElement = switchProp->findWidgetByName("CONNECT");
            if (connectElement) {
                bool wasConnected = connected_.load();
                bool nowConnected = connectElement->getState() == ISS_ON;
                
                if (wasConnected != nowConnected) {
                    connected_.store(nowConnected);
                    logInfo("Device connection state changed: " + 
                           std::string(nowConnected ? "Connected" : "Disconnected"));
                    
                    if (connectionCallback_) {
                        connectionCallback_(nowConnected);
                    }
                }
            }
        }
    }
}

INDI::PropertyNumber HardwareInterface::getNumberPropertyHandle(const std::string& propertyName) const {
    if (!device_.isValid()) {
        return INDI::PropertyNumber();
    }
    
    auto property = device_.getProperty(propertyName.c_str());
    if (property.isValid()) {
        return property.getNumber();
    }
    
    return INDI::PropertyNumber();
}

INDI::PropertySwitch HardwareInterface::getSwitchPropertyHandle(const std::string& propertyName) const {
    if (!device_.isValid()) {
        return INDI::PropertySwitch();
    }
    
    auto property = device_.getProperty(propertyName.c_str());
    if (property.isValid()) {
        return property.getSwitch();
    }
    
    return INDI::PropertySwitch();
}

INDI::PropertyText HardwareInterface::getTextPropertyHandle(const std::string& propertyName) const {
    if (!device_.isValid()) {
        return INDI::PropertyText();
    }
    
    auto property = device_.getProperty(propertyName.c_str());
    if (property.isValid()) {
        return property.getText();
    }
    
    return INDI::PropertyText();
}

void HardwareInterface::logInfo(const std::string& message) {
    spdlog::info("[HardwareInterface] {}", message);
}

void HardwareInterface::logWarning(const std::string& message) {
    spdlog::warn("[HardwareInterface] {}", message);
}

void HardwareInterface::logError(const std::string& message) {
    spdlog::error("[HardwareInterface] {}", message);
}

} // namespace lithium::device::indi::telescope::components
