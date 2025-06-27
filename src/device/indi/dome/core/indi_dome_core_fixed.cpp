/*
 * indi_dome_core.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indi_dome_core.hpp"
#include "../property_manager.hpp"
#include "../motion_controller.hpp"
#include "../shutter_controller.hpp"
#include "../parking_controller.hpp"
#include "../telescope_controller.hpp"
#include "../weather_manager.hpp"
#include "../statistics_manager.hpp"
#include "../configuration_manager.hpp"
#include "../profiler.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace lithium::device::indi {

INDIDomeCore::INDIDomeCore(const std::string& name) 
    : name_(name), is_initialized_(false), is_connected_(false) {
}

INDIDomeCore::~INDIDomeCore() {
    if (is_connected_.load()) {
        disconnect();
    }
    destroy();
}

auto INDIDomeCore::initialize() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (is_initialized_.load()) {
        logWarning("Already initialized");
        return true;
    }
    
    try {
        setServer("localhost", 7624);
        
        // Initialize components
        property_manager_ = std::make_unique<PropertyManager>(this);
        motion_controller_ = std::make_unique<MotionController>(this);
        shutter_controller_ = std::make_unique<ShutterController>(this);
        parking_controller_ = std::make_unique<ParkingController>(this);
        telescope_controller_ = std::make_unique<TelescopeController>(this);
        weather_manager_ = std::make_unique<WeatherManager>(this);
        statistics_manager_ = std::make_unique<StatisticsManager>(this);
        configuration_manager_ = std::make_unique<ConfigurationManager>(this);
        profiler_ = std::make_unique<DomeProfiler>(this);
        
        is_initialized_ = true;
        logInfo("Core initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize core: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDomeCore::destroy() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_initialized_.load()) {
        return true;
    }
    
    try {
        // Cleanup components
        profiler_.reset();
        configuration_manager_.reset();
        statistics_manager_.reset();
        weather_manager_.reset();
        telescope_controller_.reset();
        parking_controller_.reset();
        shutter_controller_.reset();
        motion_controller_.reset();
        property_manager_.reset();
        
        is_initialized_ = false;
        logInfo("Core destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to destroy core: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDomeCore::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_initialized_.load()) {
        logError("Core not initialized");
        return false;
    }
    
    if (is_connected_.load()) {
        logWarning("Already connected");
        return true;
    }
    
    device_name_ = deviceName;
    
    // Connect to INDI server
    if (!connectServer()) {
        logError("Failed to connect to INDI server");
        return false;
    }
    
    // Wait for server connection
    if (!waitForConnection(timeout)) {
        logError("Timeout waiting for server connection");
        disconnectServer();
        return false;
    }
    
    // Wait for device
    for (int i = 0; i < maxRetry; ++i) {
        // Note: getDevice() in INDI client takes no parameters and returns the device
        // You need to call watchDevice() first to watch a specific device
        watchDevice(device_name_.c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto devices = getDevices();
        for (auto& device : devices) {
            if (device.getDeviceName() == device_name_) {
                base_device_ = device;
                break;
            }
        }
        
        if (base_device_.isValid()) {
            break;
        }
    }
    
    if (!base_device_.isValid()) {
        logError("Device not found: " + device_name_);
        disconnectServer();
        return false;
    }
    
    // Connect device
    base_device_.getDriverExec();
    
    // Enable BLOBs for this device
    setBLOBMode(B_ALSO, device_name_.c_str());
    
    // Wait for connection property and connect
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto connection_prop = base_device_.getProperty("CONNECTION");
    if (connection_prop.isValid() && connection_prop.getType() == INDI_SWITCH) {
        auto switch_prop = connection_prop.getSwitch();
        switch_prop.reset();
        switch_prop.findWidgetByName("CONNECT")->setState(ISS_ON);
        switch_prop.findWidgetByName("DISCONNECT")->setState(ISS_OFF);
        sendNewProperty(switch_prop);
    }
    
    // Wait for actual connection
    for (int i = 0; i < maxRetry; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (base_device_.isConnected()) {
            is_connected_ = true;
            notifyConnectionChange(true);
            logInfo("Successfully connected to device: " + device_name_);
            return true;
        }
    }
    
    logError("Failed to connect to device after retries");
    disconnectServer();
    return false;
}

auto INDIDomeCore::disconnect() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_connected_.load()) {
        return true;
    }
    
    try {
        if (base_device_.isValid()) {
            auto connection_prop = base_device_.getProperty("CONNECTION");
            if (connection_prop.isValid() && connection_prop.getType() == INDI_SWITCH) {
                auto switch_prop = connection_prop.getSwitch();
                switch_prop.reset();
                switch_prop.findWidgetByName("CONNECT")->setState(ISS_OFF);
                switch_prop.findWidgetByName("DISCONNECT")->setState(ISS_ON);
                sendNewProperty(switch_prop);
            }
        }
        
        disconnectServer();
        is_connected_ = false;
        notifyConnectionChange(false);
        logInfo("Disconnected from device");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to disconnect: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDomeCore::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return connect(device_name_, timeout, maxRetry);
}

auto INDIDomeCore::isConnected() const -> bool {
    return is_connected_.load();
}

auto INDIDomeCore::getDeviceName() const -> std::string {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return device_name_;
}

auto INDIDomeCore::getBaseDevice() -> INDI::BaseDevice& {
    return base_device_;
}

auto INDIDomeCore::getPropertyManager() -> PropertyManager* {
    return property_manager_.get();
}

auto INDIDomeCore::getMotionController() -> MotionController* {
    return motion_controller_.get();
}

auto INDIDomeCore::getShutterController() -> ShutterController* {
    return shutter_controller_.get();
}

auto INDIDomeCore::getParkingController() -> ParkingController* {
    return parking_controller_.get();
}

auto INDIDomeCore::getTelescopeController() -> TelescopeController* {
    return telescope_controller_.get();
}

auto INDIDomeCore::getWeatherManager() -> WeatherManager* {
    return weather_manager_.get();
}

auto INDIDomeCore::getStatisticsManager() -> StatisticsManager* {
    return statistics_manager_.get();
}

auto INDIDomeCore::getConfigurationManager() -> ConfigurationManager* {
    return configuration_manager_.get();
}

auto INDIDomeCore::getProfiler() -> DomeProfiler* {
    return profiler_.get();
}

void INDIDomeCore::registerConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callbacks_.push_back(std::move(callback));
}

void INDIDomeCore::registerPropertyCallback(PropertyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    property_callbacks_.push_back(std::move(callback));
}

void INDIDomeCore::registerMotionCallback(MotionCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    motion_callbacks_.push_back(std::move(callback));
}

void INDIDomeCore::registerShutterCallback(ShutterCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    shutter_callbacks_.push_back(std::move(callback));
}

void INDIDomeCore::clearCallbacks() {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callbacks_.clear();
    property_callbacks_.clear();
    motion_callbacks_.clear();
    shutter_callbacks_.clear();
}

void INDIDomeCore::newDevice(INDI::BaseDevice device) {
    if (device.getDeviceName() == device_name_) {
        base_device_ = device;
        logInfo("Device found: " + device_name_);
    }
}

void INDIDomeCore::deleteDevice(INDI::BaseDevice device) {
    if (device.getDeviceName() == device_name_) {
        logInfo("Device disconnected: " + device_name_);
        is_connected_ = false;
        notifyConnectionChange(false);
    }
}

void INDIDomeCore::newProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    logInfo("New property: " + std::string(property.getName()));
    notifyPropertyChange(property.getName(), "NEW");
}

void INDIDomeCore::updateProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    std::string prop_name = property.getName();
    
    // Handle dome-specific property updates
    if (prop_name == "DOME_ABSOLUTE_POSITION") {
        handleAzimuthUpdate(property);
    } else if (prop_name == "DOME_MOTION") {
        handleMotionUpdate(property);
    } else if (prop_name == "DOME_SHUTTER") {
        handleShutterUpdate(property);
    } else if (prop_name == "DOME_PARK") {
        handleParkUpdate(property);
    }
    
    notifyPropertyChange(prop_name, "UPDATE");
}

void INDIDomeCore::deleteProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    logInfo("Property deleted: " + std::string(property.getName()));
    notifyPropertyChange(property.getName(), "DELETE");
}

void INDIDomeCore::handleAzimuthUpdate(const INDI::Property& property) {
    if (property.getType() == INDI_NUMBER) {
        auto number_prop = property.getNumber();
        auto azimuth_widget = number_prop.findWidgetByName("DOME_ABSOLUTE_POSITION");
        if (azimuth_widget) {
            double azimuth = azimuth_widget->getValue();
            notifyMotionChange("azimuth", azimuth);
        }
    }
}

void INDIDomeCore::handleMotionUpdate(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        auto switch_prop = property.getSwitch();
        
        for (int i = 0; i < switch_prop.count(); ++i) {
            if (switch_prop.at(i)->getState() == ISS_ON) {
                std::string motion_name = switch_prop.at(i)->getName();
                notifyMotionChange("direction", motion_name == "DOME_CW" ? 1.0 : -1.0);
                break;
            }
        }
    }
}

void INDIDomeCore::handleShutterUpdate(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        auto switch_prop = property.getSwitch();
        auto open_widget = switch_prop.findWidgetByName("SHUTTER_OPEN");
        auto close_widget = switch_prop.findWidgetByName("SHUTTER_CLOSE");
        
        bool is_open = open_widget && open_widget->getState() == ISS_ON;
        bool is_closed = close_widget && close_widget->getState() == ISS_ON;
        
        std::string state = is_open ? "OPEN" : (is_closed ? "CLOSED" : "UNKNOWN");
        notifyShutterChange(state);
    }
}

void INDIDomeCore::handleParkUpdate(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        auto switch_prop = property.getSwitch();
        auto park_widget = switch_prop.findWidgetByName("PARK");
        auto unpark_widget = switch_prop.findWidgetByName("UNPARK");
        
        bool is_parked = park_widget && park_widget->getState() == ISS_ON;
        bool is_unparked = unpark_widget && unpark_widget->getState() == ISS_ON;
        
        std::string state = is_parked ? "PARKED" : (is_unparked ? "UNPARKED" : "UNKNOWN");
        notifyMotionChange("park_state", state == "PARKED" ? 1.0 : 0.0);
    }
}

void INDIDomeCore::notifyConnectionChange(bool connected) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (auto& callback : connection_callbacks_) {
        try {
            callback(connected);
        } catch (const std::exception& ex) {
            logError("Connection callback error: " + std::string(ex.what()));
        }
    }
}

void INDIDomeCore::notifyPropertyChange(const std::string& name, const std::string& state) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (auto& callback : property_callbacks_) {
        try {
            callback(name, state);
        } catch (const std::exception& ex) {
            logError("Property callback error: " + std::string(ex.what()));
        }
    }
}

void INDIDomeCore::notifyMotionChange(const std::string& type, double value) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (auto& callback : motion_callbacks_) {
        try {
            callback(type, value);
        } catch (const std::exception& ex) {
            logError("Motion callback error: " + std::string(ex.what()));
        }
    }
}

void INDIDomeCore::notifyShutterChange(const std::string& state) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    for (auto& callback : shutter_callbacks_) {
        try {
            callback(state);
        } catch (const std::exception& ex) {
            logError("Shutter callback error: " + std::string(ex.what()));
        }
    }
}

void INDIDomeCore::logInfo(const std::string& message) {
    spdlog::info("[INDIDomeCore::{}] {}", name_, message);
}

void INDIDomeCore::logWarning(const std::string& message) {
    spdlog::warn("[INDIDomeCore::{}] {}", name_, message);
}

void INDIDomeCore::logError(const std::string& message) {
    spdlog::error("[INDIDomeCore::{}] {}", name_, message);
}

} // namespace lithium::device::indi
