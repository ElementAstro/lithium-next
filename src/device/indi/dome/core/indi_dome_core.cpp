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

lithium::device::indi::INDIDomeCore::INDIDomeCore(std::string name) 
    : is_initialized_(false), is_connected_(false) {
    // Note: We don't store the name here as it's typically set during connection
}

lithium::device::indi::INDIDomeCore::~INDIDomeCore() {
    if (is_connected_.load()) {
        disconnect();
    }
    destroy();
}

auto lithium::device::indi::INDIDomeCore::initialize() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (is_initialized_.load()) {
        logWarning("Already initialized");
        return true;
    }
    
    try {
        setServer("localhost", 7624);
        
        // Note: Components are registered by ModularINDIDome, not created here
        // This initialization just sets up the INDI client
        
        is_initialized_ = true;
        logInfo("Core initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize core: " + std::string(ex.what()));
        return false;
    }
}

auto lithium::device::indi::INDIDomeCore::destroy() -> bool {
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

auto lithium::device::indi::INDIDomeCore::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
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
        auto switch_prop_ptr = connection_prop.getSwitch();
        if (switch_prop_ptr) {
            switch_prop_ptr->reset();
            switch_prop_ptr->findWidgetByName("CONNECT")->setState(ISS_ON);
            switch_prop_ptr->findWidgetByName("DISCONNECT")->setState(ISS_OFF);
            sendNewProperty(connection_prop);
        }
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

auto lithium::device::indi::INDIDomeCore::disconnect() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_connected_.load()) {
        return true;
    }
    
    try {
        if (base_device_.isValid()) {
            auto connection_prop = base_device_.getProperty("CONNECTION");
            if (connection_prop.isValid() && connection_prop.getType() == INDI_SWITCH) {
                auto switch_prop = connection_prop.getSwitch();
                if (switch_prop) {
                    switch_prop->reset();
                    switch_prop->findWidgetByName("CONNECT")->setState(ISS_OFF);
                    switch_prop->findWidgetByName("DISCONNECT")->setState(ISS_ON);
                    sendNewProperty(connection_prop);
                }
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

auto lithium::device::indi::INDIDomeCore::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return connect(device_name_, timeout, maxRetry);
}

auto lithium::device::indi::INDIDomeCore::getDeviceName() const -> std::string {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return device_name_;
}

auto lithium::device::indi::INDIDomeCore::getDevice() -> INDI::BaseDevice {
    std::lock_guard<std::recursive_mutex> lock(device_mutex_);
    return base_device_;
}

// Component registration methods
void lithium::device::indi::INDIDomeCore::registerPropertyManager(std::shared_ptr<PropertyManager> manager) {
    property_manager_ = manager;
    logInfo("Property manager registered");
}

void lithium::device::indi::INDIDomeCore::registerMotionController(std::shared_ptr<MotionController> controller) {
    motion_controller_ = controller;
    logInfo("Motion controller registered");
}

void lithium::device::indi::INDIDomeCore::registerShutterController(std::shared_ptr<ShutterController> controller) {
    shutter_controller_ = controller;
    logInfo("Shutter controller registered");
}

void lithium::device::indi::INDIDomeCore::registerParkingController(std::shared_ptr<ParkingController> controller) {
    parking_controller_ = controller;
    logInfo("Parking controller registered");
}

void lithium::device::indi::INDIDomeCore::registerTelescopeController(std::shared_ptr<TelescopeController> controller) {
    telescope_controller_ = controller;
    logInfo("Telescope controller registered");
}

void lithium::device::indi::INDIDomeCore::registerWeatherManager(std::shared_ptr<WeatherManager> manager) {
    weather_manager_ = manager;
    logInfo("Weather manager registered");
}

void lithium::device::indi::INDIDomeCore::registerStatisticsManager(std::shared_ptr<StatisticsManager> manager) {
    statistics_manager_ = manager;
    logInfo("Statistics manager registered");
}

void lithium::device::indi::INDIDomeCore::registerConfigurationManager(std::shared_ptr<ConfigurationManager> manager) {
    configuration_manager_ = manager;
    logInfo("Configuration manager registered");
}

void lithium::device::indi::INDIDomeCore::registerProfiler(std::shared_ptr<DomeProfiler> profiler) {
    profiler_ = profiler;
    logInfo("Profiler registered");
}

// Internal monitoring and property handling methods
void lithium::device::indi::INDIDomeCore::monitoringThreadFunction() {
    logInfo("Monitoring thread started");
    
    while (monitoring_running_.load()) {
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Monitor device state, check for timeouts, etc.
            if (is_connected_.load()) {
                // Update component states from cached properties
                // This is where we would normally poll for updates
            }
        } catch (const std::exception& ex) {
            logError("Monitoring thread error: " + std::string(ex.what()));
        }
    }
    
    logInfo("Monitoring thread stopped");
}

auto lithium::device::indi::INDIDomeCore::waitForConnection(int timeout) -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout);
    
    while (!server_connected_.load()) {
        if (std::chrono::steady_clock::now() - start > timeout_duration) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return true;
}

auto lithium::device::indi::INDIDomeCore::waitForDevice(int timeout) -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout);
    
    while (!base_device_ || !base_device_.isConnected()) {
        if (std::chrono::steady_clock::now() - start > timeout_duration) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return true;
}

void lithium::device::indi::INDIDomeCore::updateComponentsFromProperty(const INDI::Property& property) {
    // Update internal state based on property changes
    std::string propName = property.getName();
    
    if (propName == "DOME_ABSOLUTE_POSITION" && property.getType() == INDI_NUMBER) {
        auto number_prop = property.getNumber();
        if (number_prop) {
            auto azimuth_widget = number_prop->findWidgetByName("DOME_ABSOLUTE_POSITION");
            if (azimuth_widget) {
                double azimuth = azimuth_widget->getValue();
                setCurrentAzimuth(azimuth);
                notifyAzimuthChange(azimuth);
            }
        }
    } else if (propName == "DOME_SHUTTER" && property.getType() == INDI_SWITCH) {
        auto switch_prop = property.getSwitch();
        if (switch_prop) {
            auto open_widget = switch_prop->findWidgetByName("SHUTTER_OPEN");
            auto close_widget = switch_prop->findWidgetByName("SHUTTER_CLOSE");
            
            if (open_widget && open_widget->getState() == ISS_ON) {
                setShutterState(ShutterState::OPEN);
                notifyShutterChange(ShutterState::OPEN);
            } else if (close_widget && close_widget->getState() == ISS_ON) {
                setShutterState(ShutterState::CLOSED);
                notifyShutterChange(ShutterState::CLOSED);
            }
        }
    } else if (propName == "DOME_PARK" && property.getType() == INDI_SWITCH) {
        auto switch_prop = property.getSwitch();
        if (switch_prop) {
            auto park_widget = switch_prop->findWidgetByName("PARK");
            if (park_widget && park_widget->getState() == ISS_ON) {
                setParked(true);
                notifyParkChange(true);
            } else {
                setParked(false);
                notifyParkChange(false);
            }
        }
    }
}

void lithium::device::indi::INDIDomeCore::distributePropertyToComponents(const INDI::Property& property) {
    // Distribute property updates to registered components
    
    if (auto prop_mgr = property_manager_.lock()) {
        // Property manager handles all properties
        // This would call methods on the property manager
    }
    
    if (auto motion_ctrl = motion_controller_.lock()) {
        // Motion controller handles motion-related properties
        if (property.getName() == std::string("DOME_MOTION") || 
            property.getName() == std::string("DOME_ABSOLUTE_POSITION") ||
            property.getName() == std::string("DOME_RELATIVE_POSITION")) {
            // Forward to motion controller
        }
    }
    
    if (auto shutter_ctrl = shutter_controller_.lock()) {
        // Shutter controller handles shutter-related properties
        if (property.getName() == std::string("DOME_SHUTTER")) {
            // Forward to shutter controller
        }
    }
    
    // Similar forwarding for other components...
}

// INDI BaseClient virtual methods
void lithium::device::indi::INDIDomeCore::newDevice(INDI::BaseDevice device) {
    if (device.getDeviceName() == device_name_) {
        base_device_ = device;
        logInfo("Device found: " + device_name_);
    }
}

void lithium::device::indi::INDIDomeCore::removeDevice(INDI::BaseDevice device) {
    if (device.getDeviceName() == device_name_) {
        logInfo("Device disconnected: " + device_name_);
        is_connected_ = false;
        notifyConnectionChange(false);
    }
}

void lithium::device::indi::INDIDomeCore::newProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    logInfo("New property: " + std::string(property.getName()));
    // Note: notifyPropertyChange doesn't exist, components handle their own property updates
}

void lithium::device::indi::INDIDomeCore::updateProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    std::string prop_name = property.getName();
    
    // Handle dome-specific property updates by notifying registered components
    if (prop_name == "DOME_ABSOLUTE_POSITION") {
        if (property.getType() == INDI_NUMBER) {
            auto number_prop = property.getNumber();
            if (number_prop) {
                auto azimuth_widget = number_prop->findWidgetByName("DOME_ABSOLUTE_POSITION");
                if (azimuth_widget) {
                    double azimuth = azimuth_widget->getValue();
                    notifyAzimuthChange(azimuth);
                }
            }
        }
    } else if (prop_name == "DOME_SHUTTER") {
        if (property.getType() == INDI_SWITCH) {
            auto switch_prop = property.getSwitch();
            if (switch_prop) {
                auto open_widget = switch_prop->findWidgetByName("SHUTTER_OPEN");
                auto close_widget = switch_prop->findWidgetByName("SHUTTER_CLOSE");
                
                ShutterState state = ShutterState::UNKNOWN;
                if (open_widget && open_widget->getState() == ISS_ON) {
                    state = ShutterState::OPEN;
                } else if (close_widget && close_widget->getState() == ISS_ON) {
                    state = ShutterState::CLOSED;
                }
                
                notifyShutterChange(state);
            }
        }
    } else if (prop_name == "DOME_PARK") {
        if (property.getType() == INDI_SWITCH) {
            auto switch_prop = property.getSwitch();
            if (switch_prop) {
                auto park_widget = switch_prop->findWidgetByName("PARK");
                bool is_parked = park_widget && park_widget->getState() == ISS_ON;
                notifyParkChange(is_parked);
            }
        }
    }
}

void lithium::device::indi::INDIDomeCore::removeProperty(INDI::Property property) {
    if (property.getDeviceName() != device_name_) {
        return;
    }
    
    logInfo("Property removed: " + std::string(property.getName()));
}

void lithium::device::indi::INDIDomeCore::notifyAzimuthChange(double azimuth) {
    current_azimuth_.store(azimuth);
    if (azimuth_callback_) {
        try {
            azimuth_callback_(azimuth);
        } catch (const std::exception& ex) {
            logError("Azimuth callback error: " + std::string(ex.what()));
        }
    }
}

void lithium::device::indi::INDIDomeCore::notifyShutterChange(ShutterState state) {
    setShutterState(state);
    if (shutter_callback_) {
        try {
            shutter_callback_(state);
        } catch (const std::exception& ex) {
            logError("Shutter callback error: " + std::string(ex.what()));
        }
    }
}

void lithium::device::indi::INDIDomeCore::notifyParkChange(bool parked) {
    setParked(parked);
    if (park_callback_) {
        try {
            park_callback_(parked);
        } catch (const std::exception& ex) {
            logError("Park callback error: " + std::string(ex.what()));
        }
    }
}

void lithium::device::indi::INDIDomeCore::notifyMoveComplete(bool success, const std::string& message) {
    setMoving(false);
    if (move_complete_callback_) {
        try {
            move_complete_callback_(success, message);
        } catch (const std::exception& ex) {
            logError("Move complete callback error: " + std::string(ex.what()));
        }
    }
}

void lithium::device::indi::INDIDomeCore::notifyWeatherChange(bool safe, const std::string& status) {
    setSafeToOperate(safe);
    if (weather_callback_) {
        try {
            weather_callback_(safe, status);
        } catch (const std::exception& ex) {
            logError("Weather callback error: " + std::string(ex.what()));
        }
    }
}

void lithium::device::indi::INDIDomeCore::notifyConnectionChange(bool connected) {
    is_connected_.store(connected);
    if (connection_callback_) {
        try {
            connection_callback_(connected);
        } catch (const std::exception& ex) {
            logError("Connection callback error: " + std::string(ex.what()));
        }
    }
}

auto lithium::device::indi::INDIDomeCore::getShutterState() const -> ShutterState {
    return static_cast<ShutterState>(shutter_state_.load());
}

void lithium::device::indi::INDIDomeCore::setShutterState(ShutterState state) {
    shutter_state_.store(static_cast<int>(state));
}

auto lithium::device::indi::INDIDomeCore::scanForDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    
    // In a real implementation, this would scan the INDI server for available dome devices
    // For now, return empty vector - components will handle device discovery
    logInfo("Scanning for dome devices...");
    
    return devices;
}

auto lithium::device::indi::INDIDomeCore::getAvailableDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    
    // In a real implementation, this would return currently available dome devices
    // For now, return empty vector - components will handle device management
    logInfo("Getting available dome devices...");
    
    return devices;
}

void lithium::device::indi::INDIDomeCore::logInfo(const std::string& message) const {
    spdlog::info("[INDIDomeCore::{}] {}", device_name_, message);
}

void lithium::device::indi::INDIDomeCore::logWarning(const std::string& message) const {
    spdlog::warn("[INDIDomeCore::{}] {}", device_name_, message);
}

void lithium::device::indi::INDIDomeCore::logError(const std::string& message) const {
    spdlog::error("[INDIDomeCore::{}] {}", device_name_, message);
}

} // namespace lithium::device::indi
