/*
 * dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: INDI Dome Client Implementation

*************************************************/

#include "dome.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <cmath>

INDIDome::INDIDome(std::string name) : AtomDome(std::move(name)) {
    setDomeCapabilities(DomeCapabilities{
        .canPark = true,
        .canSync = true,
        .canAbort = true,
        .hasShutter = true,
        .hasVariable = false,
        .canSetAzimuth = true,
        .canSetParkPosition = true,
        .hasBacklash = false,
        .minAzimuth = 0.0,
        .maxAzimuth = 360.0
    });

    setDomeParameters(DomeParameters{
        .diameter = 3.0,
        .height = 2.5,
        .slitWidth = 0.5,
        .slitHeight = 0.8,
        .telescopeRadius = 0.5
    });
}

auto INDIDome::initialize() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (is_initialized_.load()) {
        logWarning("Dome already initialized");
        return true;
    }

    try {
        setServer("localhost", 7624);

        // Start monitoring thread
        monitoring_thread_running_ = true;
        monitoring_thread_ = std::thread(&INDIDome::monitoringThreadFunction, this);

        is_initialized_ = true;
        logInfo("Dome initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize dome: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDome::destroy() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!is_initialized_.load()) {
        return true;
    }

    try {
        // Stop monitoring thread
        monitoring_thread_running_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }

        if (is_connected_.load()) {
            disconnect();
        }

        disconnectServer();

        is_initialized_ = false;
        logInfo("Dome destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to destroy dome: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDome::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!is_initialized_.load()) {
        logError("Dome not initialized");
        return false;
    }

    if (is_connected_.load()) {
        logWarning("Dome already connected");
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
    for (int retry = 0; retry < maxRetry; ++retry) {
        base_device_ = getDevice(device_name_.c_str());
        if (base_device_.isValid()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    if (!base_device_.isValid()) {
        logError("Device not found: " + device_name_);
        disconnectServer();
        return false;
    }

    // Connect device
    base_device_.getDriverExec();

    // Wait for connection property and set it to connect
    if (!waitForProperty("CONNECTION", timeout)) {
        logError("Connection property not found");
        disconnectServer();
        return false;
    }

    auto connectionProp = getConnectionProperty();
    if (!connectionProp.isValid()) {
        logError("Invalid connection property");
        disconnectServer();
        return false;
    }

    connectionProp.reset();
    connectionProp.findWidgetByName("CONNECT")->setState(ISS_ON);
    connectionProp.findWidgetByName("DISCONNECT")->setState(ISS_OFF);
    sendNewProperty(connectionProp);

    // Wait for connection
    for (int i = 0; i < timeout * 10; ++i) {
        if (base_device_.isConnected()) {
            is_connected_ = true;
            updateFromDevice();
            logInfo("Dome connected successfully: " + device_name_);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logError("Timeout waiting for device connection");
    disconnectServer();
    return false;
}

auto INDIDome::disconnect() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!is_connected_.load()) {
        return true;
    }

    try {
        if (base_device_.isValid()) {
            auto connectionProp = getConnectionProperty();
            if (connectionProp.isValid()) {
                connectionProp.reset();
                connectionProp.findWidgetByName("CONNECT")->setState(ISS_OFF);
                connectionProp.findWidgetByName("DISCONNECT")->setState(ISS_ON);
                sendNewProperty(connectionProp);
            }
        }

        disconnectServer();
        is_connected_ = false;

        logInfo("Dome disconnected successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to disconnect dome: " + std::string(ex.what()));
        return false;
    }
}

auto INDIDome::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return connect(device_name_, timeout, maxRetry);
}

auto INDIDome::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

    if (!server_connected_.load()) {
        logError("Server not connected for scanning");
        return devices;
    }

    auto deviceList = getDevices();
    for (const auto& device : deviceList) {
        if (device.isValid()) {
            devices.emplace_back(device.getDeviceName());
        }
    }

    return devices;
}

auto INDIDome::isConnected() const -> bool {
    return is_connected_.load() && base_device_.isValid() && base_device_.isConnected();
}

auto INDIDome::watchAdditionalProperty() -> bool {
    // Watch for dome-specific properties
    watchDevice(device_name_.c_str());
    return true;
}

// State queries
auto INDIDome::isMoving() const -> bool {
    return is_moving_.load();
}

auto INDIDome::isParked() const -> bool {
    return is_parked_.load();
}

// Azimuth control
auto INDIDome::getAzimuth() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    return current_azimuth_.load();
}

auto INDIDome::setAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto INDIDome::moveToAzimuth(double azimuth) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto azimuthProp = getDomeAzimuthProperty();
    if (!azimuthProp.isValid()) {
        logError("Dome azimuth property not found");
        return false;
    }

    // Normalize azimuth
    double normalizedAz = normalizeAzimuth(azimuth);

    azimuthProp.at(0)->setValue(normalizedAz);
    sendNewProperty(azimuthProp);

    target_azimuth_ = normalizedAz;
    is_moving_ = true;
    updateDomeState(DomeState::MOVING);

    logInfo("Moving dome to azimuth: " + std::to_string(normalizedAz) + "°");
    return true;
}

auto INDIDome::rotateClockwise() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto motionProp = getDomeMotionProperty();
    if (!motionProp.isValid()) {
        logError("Dome motion property not found");
        return false;
    }

    motionProp.reset();
    auto clockwiseWidget = motionProp.findWidgetByName("DOME_CW");
    if (clockwiseWidget) {
        clockwiseWidget->setState(ISS_ON);
        sendNewProperty(motionProp);

        is_moving_ = true;
        updateDomeState(DomeState::MOVING);

        logInfo("Starting clockwise rotation");
        return true;
    }

    logError("Clockwise motion widget not found");
    return false;
}

auto INDIDome::rotateCounterClockwise() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto motionProp = getDomeMotionProperty();
    if (!motionProp.isValid()) {
        logError("Dome motion property not found");
        return false;
    }

    motionProp.reset();
    auto ccwWidget = motionProp.findWidgetByName("DOME_CCW");
    if (ccwWidget) {
        ccwWidget->setState(ISS_ON);
        sendNewProperty(motionProp);

        is_moving_ = true;
        updateDomeState(DomeState::MOVING);

        logInfo("Starting counter-clockwise rotation");
        return true;
    }

    logError("Counter-clockwise motion widget not found");
    return false;
}

auto INDIDome::stopRotation() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto motionProp = getDomeMotionProperty();
    if (!motionProp.isValid()) {
        logError("Dome motion property not found");
        return false;
    }

    motionProp.reset();
    auto stopWidget = motionProp.findWidgetByName("DOME_STOP");
    if (stopWidget) {
        stopWidget->setState(ISS_ON);
        sendNewProperty(motionProp);

        is_moving_ = false;
        updateDomeState(DomeState::IDLE);

        logInfo("Stopping dome rotation");
        return true;
    }

    logError("Stop motion widget not found");
    return false;
}

auto INDIDome::abortMotion() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto abortProp = getDomeAbortProperty();
    if (!abortProp.isValid()) {
        logError("Dome abort property not found");
        return false;
    }

    abortProp.reset();
    auto abortWidget = abortProp.findWidgetByName("ABORT");
    if (abortWidget) {
        abortWidget->setState(ISS_ON);
        sendNewProperty(abortProp);

        is_moving_ = false;
        updateDomeState(DomeState::IDLE);

        logInfo("Aborting dome motion");
        return true;
    }

    return stopRotation(); // Fallback to stop
}

auto INDIDome::syncAzimuth(double azimuth) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    // Try to find sync property
    auto syncProp = base_device_.getProperty("DOME_SYNC");
    if (syncProp.isValid() && syncProp.getType() == INDI_NUMBER) {
        auto syncNumber = syncProp.getNumber();
        syncNumber.at(0)->setValue(normalizeAzimuth(azimuth));
        sendNewProperty(syncNumber);

        current_azimuth_ = normalizeAzimuth(azimuth);
        logInfo("Synced dome azimuth to: " + std::to_string(azimuth) + "°");
        return true;
    }

    logError("Dome sync property not available");
    return false;
}

// Parking
auto INDIDome::park() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto parkProp = getDomeParkProperty();
    if (!parkProp.isValid()) {
        logError("Dome park property not found");
        return false;
    }

    parkProp.reset();
    auto parkWidget = parkProp.findWidgetByName("PARK");
    if (parkWidget) {
        parkWidget->setState(ISS_ON);
        sendNewProperty(parkProp);

        updateDomeState(DomeState::PARKING);
        logInfo("Parking dome");
        return true;
    }

    logError("Park widget not found");
    return false;
}

auto INDIDome::unpark() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto parkProp = getDomeParkProperty();
    if (!parkProp.isValid()) {
        logError("Dome park property not found");
        return false;
    }

    parkProp.reset();
    auto unparkWidget = parkProp.findWidgetByName("UNPARK");
    if (unparkWidget) {
        unparkWidget->setState(ISS_ON);
        sendNewProperty(parkProp);

        is_parked_ = false;
        updateDomeState(DomeState::IDLE);
        logInfo("Unparking dome");
        return true;
    }

    logError("Unpark widget not found");
    return false;
}

auto INDIDome::getParkPosition() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    return park_position_;
}

auto INDIDome::setParkPosition(double azimuth) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto parkPosProp = base_device_.getProperty("DOME_PARK_POSITION");
    if (parkPosProp.isValid() && parkPosProp.getType() == INDI_NUMBER) {
        auto parkPosNumber = parkPosProp.getNumber();
        parkPosNumber.at(0)->setValue(normalizeAzimuth(azimuth));
        sendNewProperty(parkPosNumber);

        park_position_ = normalizeAzimuth(azimuth);
        logInfo("Set dome park position to: " + std::to_string(azimuth) + "°");
        return true;
    }

    logError("Dome park position property not available");
    return false;
}

auto INDIDome::canPark() -> bool {
    return dome_capabilities_.canPark;
}

// Shutter control
auto INDIDome::openShutter() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    if (!hasShutter()) {
        logError("Dome has no shutter");
        return false;
    }

    if (!canOpenShutter()) {
        logError("Not safe to open shutter");
        return false;
    }

    auto shutterProp = getDomeShutterProperty();
    if (!shutterProp.isValid()) {
        logError("Dome shutter property not found");
        return false;
    }

    shutterProp.reset();
    auto openWidget = shutterProp.findWidgetByName("SHUTTER_OPEN");
    if (openWidget) {
        openWidget->setState(ISS_ON);
        sendNewProperty(shutterProp);

        updateShutterState(ShutterState::OPENING);
        shutter_operations_++;
        logInfo("Opening dome shutter");
        return true;
    }

    logError("Shutter open widget not found");
    return false;
}

auto INDIDome::closeShutter() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    if (!hasShutter()) {
        logError("Dome has no shutter");
        return false;
    }

    auto shutterProp = getDomeShutterProperty();
    if (!shutterProp.isValid()) {
        logError("Dome shutter property not found");
        return false;
    }

    shutterProp.reset();
    auto closeWidget = shutterProp.findWidgetByName("SHUTTER_CLOSE");
    if (closeWidget) {
        closeWidget->setState(ISS_ON);
        sendNewProperty(shutterProp);

        updateShutterState(ShutterState::CLOSING);
        shutter_operations_++;
        logInfo("Closing dome shutter");
        return true;
    }

    logError("Shutter close widget not found");
    return false;
}

auto INDIDome::abortShutter() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    if (!hasShutter()) {
        logError("Dome has no shutter");
        return false;
    }

    auto shutterProp = getDomeShutterProperty();
    if (!shutterProp.isValid()) {
        logError("Dome shutter property not found");
        return false;
    }

    shutterProp.reset();
    auto abortWidget = shutterProp.findWidgetByName("SHUTTER_ABORT");
    if (abortWidget) {
        abortWidget->setState(ISS_ON);
        sendNewProperty(shutterProp);

        logInfo("Aborting shutter operation");
        return true;
    }

    logError("Shutter abort widget not found");
    return false;
}

auto INDIDome::getShutterState() -> ShutterState {
    return static_cast<ShutterState>(shutter_state_.load());
}

auto INDIDome::hasShutter() -> bool {
    return dome_capabilities_.hasShutter;
}

// Speed control
auto INDIDome::getRotationSpeed() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    return rotation_speed_.load();
}

auto INDIDome::setRotationSpeed(double speed) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto speedProp = getDomeSpeedProperty();
    if (!speedProp.isValid()) {
        logError("Dome speed property not found");
        return false;
    }

    speedProp.at(0)->setValue(speed);
    sendNewProperty(speedProp);

    rotation_speed_ = speed;
    logInfo("Set dome rotation speed to: " + std::to_string(speed));
    return true;
}

auto INDIDome::getMaxSpeed() -> double {
    return 10.0; // Default maximum speed
}

auto INDIDome::getMinSpeed() -> double {
    return 0.1; // Default minimum speed
}

// INDI BaseClient virtual method implementations
void INDIDome::newDevice(INDI::BaseDevice baseDevice) {
    logInfo("New device: " + std::string(baseDevice.getDeviceName()));
}

void INDIDome::removeDevice(INDI::BaseDevice baseDevice) {
    logInfo("Device removed: " + std::string(baseDevice.getDeviceName()));
}

void INDIDome::newProperty(INDI::Property property) {
    handleDomeProperty(property);
}

void INDIDome::updateProperty(INDI::Property property) {
    handleDomeProperty(property);
}

void INDIDome::removeProperty(INDI::Property property) {
    logInfo("Property removed: " + std::string(property.getName()));
}

void INDIDome::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    // Handle device messages
}

void INDIDome::serverConnected() {
    server_connected_ = true;
    logInfo("Server connected");
}

void INDIDome::serverDisconnected(int exit_code) {
    server_connected_ = false;
    is_connected_ = false;
    logInfo("Server disconnected with code: " + std::to_string(exit_code));
}

// Private helper method implementations
void INDIDome::monitoringThreadFunction() {
    while (monitoring_thread_running_.load()) {
        if (isConnected()) {
            updateFromDevice();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

auto INDIDome::waitForConnection(int timeout) -> bool {
    for (int i = 0; i < timeout * 10; ++i) {
        if (server_connected_.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

auto INDIDome::waitForProperty(const std::string& propertyName, int timeout) -> bool {
    for (int i = 0; i < timeout * 10; ++i) {
        if (base_device_.isValid()) {
            auto property = base_device_.getProperty(propertyName.c_str());
            if (property.isValid()) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void INDIDome::updateFromDevice() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!base_device_.isValid()) {
        return;
    }

    // Update azimuth
    auto azimuthProp = getDomeAzimuthProperty();
    if (azimuthProp.isValid()) {
        updateAzimuthFromProperty(azimuthProp);
    }

    // Update speed
    auto speedProp = getDomeSpeedProperty();
    if (speedProp.isValid()) {
        updateSpeedFromProperty(speedProp);
    }

    // Update shutter
    auto shutterProp = getDomeShutterProperty();
    if (shutterProp.isValid()) {
        updateShutterFromProperty(shutterProp);
    }

    // Update parking
    auto parkProp = getDomeParkProperty();
    if (parkProp.isValid()) {
        updateParkingFromProperty(parkProp);
    }
}

void INDIDome::handleDomeProperty(const INDI::Property& property) {
    std::string propName = property.getName();

    if (propName.find("DOME_AZIMUTH") != std::string::npos && property.getType() == INDI_NUMBER) {
        updateAzimuthFromProperty(property.getNumber());
    } else if (propName.find("DOME_SPEED") != std::string::npos && property.getType() == INDI_NUMBER) {
        updateSpeedFromProperty(property.getNumber());
    } else if (propName.find("DOME_SHUTTER") != std::string::npos && property.getType() == INDI_SWITCH) {
        updateShutterFromProperty(property.getSwitch());
    } else if (propName.find("DOME_PARK") != std::string::npos && property.getType() == INDI_SWITCH) {
        updateParkingFromProperty(property.getSwitch());
    }
}

void INDIDome::updateAzimuthFromProperty(const INDI::PropertyNumber& property) {
    if (property.count() > 0) {
        double azimuth = property.at(0)->getValue();
        current_azimuth_ = azimuth;
        current_azimuth = azimuth;

        // Check if movement is complete
        double targetAz = target_azimuth_.load();
        if (std::abs(azimuth - targetAz) < 1.0) { // Within 1 degree tolerance
            is_moving_ = false;
            updateDomeState(DomeState::IDLE);
            notifyMoveComplete(true, "Azimuth reached");
        }

        notifyAzimuthChange(azimuth);
    }
}

void INDIDome::updateShutterFromProperty(const INDI::PropertySwitch& property) {
    for (int i = 0; i < property.count(); ++i) {
        auto widget = property.at(i);
        std::string widgetName = widget->getName();

        if (widgetName == "SHUTTER_OPEN" && widget->getState() == ISS_ON) {
            if (property.getState() == IPS_OK) {
                shutter_state_ = static_cast<int>(ShutterState::OPEN);
                updateShutterState(ShutterState::OPEN);
            } else if (property.getState() == IPS_BUSY) {
                shutter_state_ = static_cast<int>(ShutterState::OPENING);
                updateShutterState(ShutterState::OPENING);
            }
        } else if (widgetName == "SHUTTER_CLOSE" && widget->getState() == ISS_ON) {
            if (property.getState() == IPS_OK) {
                shutter_state_ = static_cast<int>(ShutterState::CLOSED);
                updateShutterState(ShutterState::CLOSED);
            } else if (property.getState() == IPS_BUSY) {
                shutter_state_ = static_cast<int>(ShutterState::CLOSING);
                updateShutterState(ShutterState::CLOSING);
            }
        }
    }
}

void INDIDome::updateParkingFromProperty(const INDI::PropertySwitch& property) {
    for (int i = 0; i < property.count(); ++i) {
        auto widget = property.at(i);
        std::string widgetName = widget->getName();

        if (widgetName == "PARK" && widget->getState() == ISS_ON) {
            if (property.getState() == IPS_OK) {
                is_parked_ = true;
                updateDomeState(DomeState::PARKED);
                notifyParkChange(true);
            } else if (property.getState() == IPS_BUSY) {
                updateDomeState(DomeState::PARKING);
            }
        } else if (widgetName == "UNPARK" && widget->getState() == ISS_ON) {
            if (property.getState() == IPS_OK) {
                is_parked_ = false;
                updateDomeState(DomeState::IDLE);
                notifyParkChange(false);
            }
        }
    }
}

void INDIDome::updateSpeedFromProperty(const INDI::PropertyNumber& property) {
    if (property.count() > 0) {
        double speed = property.at(0)->getValue();
        rotation_speed_ = speed;
    }
}

// Property helper implementations
auto INDIDome::getDomeAzimuthProperty() -> INDI::PropertyNumber {
    if (!base_device_.isValid()) {
        return INDI::PropertyNumber();
    }

    auto property = base_device_.getProperty("DOME_AZIMUTH");
    if (property.isValid() && property.getType() == INDI_NUMBER) {
        return property.getNumber();
    }

    return INDI::PropertyNumber();
}

auto INDIDome::getDomeSpeedProperty() -> INDI::PropertyNumber {
    if (!base_device_.isValid()) {
        return INDI::PropertyNumber();
    }

    auto property = base_device_.getProperty("DOME_SPEED");
    if (property.isValid() && property.getType() == INDI_NUMBER) {
        return property.getNumber();
    }

    return INDI::PropertyNumber();
}

auto INDIDome::getDomeMotionProperty() -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }

    auto property = base_device_.getProperty("DOME_MOTION");
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }

    return INDI::PropertySwitch();
}

auto INDIDome::getDomeParkProperty() -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }

    auto property = base_device_.getProperty("DOME_PARK");
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }

    return INDI::PropertySwitch();
}

auto INDIDome::getDomeShutterProperty() -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }

    auto property = base_device_.getProperty("DOME_SHUTTER");
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }

    return INDI::PropertySwitch();
}

auto INDIDome::getDomeAbortProperty() -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }

    auto property = base_device_.getProperty("DOME_ABORT");
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }

    return INDI::PropertySwitch();
}

auto INDIDome::getConnectionProperty() -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }

    auto property = base_device_.getProperty("CONNECTION");
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }

    return INDI::PropertySwitch();
}

void INDIDome::logInfo(const std::string& message) {
    spdlog::info("[INDIDome::{}] {}", getName(), message);
}

void INDIDome::logWarning(const std::string& message) {
    spdlog::warn("[INDIDome::{}] {}", getName(), message);
}

void INDIDome::logError(const std::string& message) {
    spdlog::error("[INDIDome::{}] {}", getName(), message);
}

auto INDIDome::convertShutterState(ISState state) -> ShutterState {
    return (state == ISS_ON) ? ShutterState::OPEN : ShutterState::CLOSED;
}

auto INDIDome::convertToISState(bool value) -> ISState {
    return value ? ISS_ON : ISS_OFF;
}

// Telescope coordination implementations
auto INDIDome::followTelescope(bool enable) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto followProp = base_device_.getProperty("DOME_AUTOSYNC");
    if (followProp.isValid() && followProp.getType() == INDI_SWITCH) {
        auto followSwitch = followProp.getSwitch();
        followSwitch.reset();

        if (enable) {
            auto enableWidget = followSwitch.findWidgetByName("DOME_AUTOSYNC_ENABLE");
            if (enableWidget) {
                enableWidget->setState(ISS_ON);
            }
        } else {
            auto disableWidget = followSwitch.findWidgetByName("DOME_AUTOSYNC_DISABLE");
            if (disableWidget) {
                disableWidget->setState(ISS_ON);
            }
        }

        sendNewProperty(followSwitch);

        logInfo(enable ? "Enabled telescope following" : "Disabled telescope following");
        return true;
    }

    logError("Dome autosync property not available");
    return false;
}

auto INDIDome::isFollowingTelescope() -> bool {
    if (!isConnected()) {
        return false;
    }

    auto followProp = base_device_.getProperty("DOME_AUTOSYNC");
    if (followProp.isValid() && followProp.getType() == INDI_SWITCH) {
        auto followSwitch = followProp.getSwitch();
        auto enableWidget = followSwitch.findWidgetByName("DOME_AUTOSYNC_ENABLE");
        return enableWidget && enableWidget->getState() == ISS_ON;
    }

    return false;
}

auto INDIDome::calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double {
    // Basic dome azimuth calculation
    // For most domes, the dome azimuth matches telescope azimuth
    // More sophisticated implementations would account for:
    // - Dome geometry parameters
    // - Telescope offset from dome center
    // - Slit dimensions

    const auto& params = getDomeParameters();

    // Simple calculation with telescope radius offset
    double domeAz = telescopeAz;

    // Apply offset correction based on telescope position relative to dome center
    if (params.telescopeRadius > 0) {
        // Calculate offset based on altitude (height compensation)
        double heightCorrection = std::atan2(params.telescopeRadius * std::sin(telescopeAlt * M_PI / 180.0),
                                           params.diameter / 2.0) * 180.0 / M_PI;

        domeAz += heightCorrection;
    }

    // Normalize to 0-360 range
    return normalizeAzimuth(domeAz);
}

auto INDIDome::setTelescopePosition(double az, double alt) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    // Update telescope position for dome coordination
    auto telescopeProp = base_device_.getProperty("TELESCOPE_TIMED_GUIDE_NS");
    if (telescopeProp.isValid()) {
        // Store telescope position for dome calculations
        current_telescope_az_ = az;
        current_telescope_alt_ = alt;

        // If following is enabled, calculate and move to new dome position
        if (isFollowingTelescope()) {
            double newDomeAz = calculateDomeAzimuth(az, alt);
            double currentDomeAz = current_azimuth_.load();

            // Only move if difference is significant (> 1 degree)
            if (std::abs(newDomeAz - currentDomeAz) > 1.0) {
                return moveToAzimuth(newDomeAz);
            }
        }

        return true;
    }

    logWarning("Telescope position property not available");
    return false;
}
// Home position implementations
auto INDIDome::findHome() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto homeProp = base_device_.getProperty("DOME_HOME");
    if (!homeProp.isValid()) {
        // Try alternative property names
        homeProp = base_device_.getProperty("HOME_DISCOVER");
        if (!homeProp.isValid()) {
            logError("Dome home discovery property not found");
            return false;
        }
    }

    if (homeProp.getType() == INDI_SWITCH) {
        auto homeSwitch = homeProp.getSwitch();
        homeSwitch.reset();
        auto discoverWidget = homeSwitch.findWidgetByName("HOME_DISCOVER");
        if (!discoverWidget) {
            discoverWidget = homeSwitch.findWidgetByName("DOME_HOME_FIND");
        }

        if (discoverWidget) {
            discoverWidget->setState(ISS_ON);
            sendNewProperty(homeSwitch);

            updateDomeState(DomeState::MOVING);
            logInfo("Finding home position");
            return true;
        }
    }

    logError("Home discovery widget not found");
    return false;
}

auto INDIDome::setHome() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto homeProp = base_device_.getProperty("DOME_HOME");
    if (!homeProp.isValid()) {
        homeProp = base_device_.getProperty("HOME_SET");
    }

    if (homeProp.isValid() && homeProp.getType() == INDI_SWITCH) {
        auto homeSwitch = homeProp.getSwitch();
        homeSwitch.reset();
        auto setWidget = homeSwitch.findWidgetByName("HOME_SET");
        if (!setWidget) {
            setWidget = homeSwitch.findWidgetByName("DOME_HOME_SET");
        }

        if (setWidget) {
            setWidget->setState(ISS_ON);
            sendNewProperty(homeSwitch);

            home_position_ = current_azimuth_.load();
            logInfo("Set home position to current azimuth: " + std::to_string(home_position_));
            return true;
        }
    }

    // Fallback: just store current position as home
    home_position_ = current_azimuth_.load();
    logInfo("Set home position to: " + std::to_string(home_position_) + "°");
    return true;
}

auto INDIDome::gotoHome() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto homeProp = base_device_.getProperty("DOME_HOME");
    if (!homeProp.isValid()) {
        homeProp = base_device_.getProperty("HOME_GOTO");
    }

    if (homeProp.isValid() && homeProp.getType() == INDI_SWITCH) {
        auto homeSwitch = homeProp.getSwitch();
        homeSwitch.reset();
        auto gotoWidget = homeSwitch.findWidgetByName("HOME_GOTO");
        if (!gotoWidget) {
            gotoWidget = homeSwitch.findWidgetByName("DOME_HOME_GOTO");
        }

        if (gotoWidget) {
            gotoWidget->setState(ISS_ON);
            sendNewProperty(homeSwitch);

            updateDomeState(DomeState::MOVING);
            target_azimuth_ = home_position_;
            logInfo("Going to home position: " + std::to_string(home_position_) + "°");
            return true;
        }
    }

    // Fallback: move to stored home position
    if (home_position_ >= 0) {
        return moveToAzimuth(home_position_);
    }

    logError("Home position not set");
    return false;
}

auto INDIDome::getHomePosition() -> std::optional<double> {
    if (home_position_ >= 0) {
        return home_position_;
    }
    return std::nullopt;
}
// Backlash compensation implementations
auto INDIDome::getBacklash() -> double {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return backlash_compensation_;
}

auto INDIDome::setBacklash(double backlash) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto backlashProp = base_device_.getProperty("DOME_BACKLASH");
    if (backlashProp.isValid() && backlashProp.getType() == INDI_NUMBER) {
        auto backlashNumber = backlashProp.getNumber();
        backlashNumber.at(0)->setValue(backlash);
        sendNewProperty(backlashNumber);

        backlash_compensation_ = backlash;
        logInfo("Set backlash compensation to: " + std::to_string(backlash) + "°");
        return true;
    }

    // Store locally even if device doesn't support it
    backlash_compensation_ = backlash;
    logWarning("Device doesn't support backlash property, storing locally");
    return true;
}

auto INDIDome::enableBacklashCompensation(bool enable) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }

    auto backlashEnableProp = base_device_.getProperty("DOME_BACKLASH_TOGGLE");
    if (backlashEnableProp.isValid() && backlashEnableProp.getType() == INDI_SWITCH) {
        auto backlashSwitch = backlashEnableProp.getSwitch();
        backlashSwitch.reset();

        if (enable) {
            auto enableWidget = backlashSwitch.findWidgetByName("DOME_BACKLASH_ENABLE");
            if (enableWidget) {
                enableWidget->setState(ISS_ON);
            }
        } else {
            auto disableWidget = backlashSwitch.findWidgetByName("DOME_BACKLASH_DISABLE");
            if (disableWidget) {
                disableWidget->setState(ISS_ON);
            }
        }

        sendNewProperty(backlashSwitch);

        backlash_enabled_ = enable;
        logInfo(enable ? "Enabled backlash compensation" : "Disabled backlash compensation");
        return true;
    }

    // Store locally even if device doesn't support it
    backlash_enabled_ = enable;
    logWarning("Device doesn't support backlash enable property, storing locally");
    return true;
}

auto INDIDome::isBacklashCompensationEnabled() -> bool {
    return backlash_enabled_;
}

// Weather monitoring implementations
auto INDIDome::enableWeatherMonitoring(bool enable) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    weather_monitoring_enabled_ = enable;

    if (enable) {
        logInfo("Weather monitoring enabled");
        // Start monitoring weather status
        if (isConnected()) {
            checkWeatherStatus();
        }
    } else {
        logInfo("Weather monitoring disabled");
        weather_safe_ = true; // Assume safe when not monitoring
    }

    return true;
}

auto INDIDome::isWeatherMonitoringEnabled() -> bool {
    return weather_monitoring_enabled_;
}

auto INDIDome::isWeatherSafe() -> bool {
    if (weather_monitoring_enabled_ && isConnected()) {
        checkWeatherStatus();
    }
    return weather_safe_;
}

auto INDIDome::getWeatherCondition() -> std::optional<WeatherCondition> {
    if (!weather_monitoring_enabled_) {
        return std::nullopt;
    }

    // Check various weather-related properties
    WeatherCondition condition;
    condition.safe = weather_safe_;
    condition.temperature = 20.0; // Default values
    condition.humidity = 50.0;
    condition.windSpeed = 0.0;
    condition.rainDetected = false;

    if (isConnected()) {
        // Try to get weather data from device
        auto weatherProp = base_device_.getProperty("WEATHER_PARAMETERS");
        if (weatherProp.isValid() && weatherProp.getType() == INDI_NUMBER) {
            auto weatherNumber = weatherProp.getNumber();

            for (int i = 0; i < weatherNumber.count(); ++i) {
                auto widget = weatherNumber.at(i);
                std::string name = widget->getName();
                double value = widget->getValue();

                if (name.find("TEMP") != std::string::npos) {
                    condition.temperature = value;
                } else if (name.find("HUM") != std::string::npos) {
                    condition.humidity = value;
                } else if (name.find("WIND") != std::string::npos) {
                    condition.windSpeed = value;
                }
            }
        }

        // Check rain sensor
        auto rainProp = base_device_.getProperty("WEATHER_RAIN");
        if (rainProp.isValid() && rainProp.getType() == INDI_SWITCH) {
            auto rainSwitch = rainProp.getSwitch();
            auto rainWidget = rainSwitch.findWidgetByName("RAIN_ALERT");
            if (rainWidget) {
                condition.rainDetected = (rainWidget->getState() == ISS_ON);
            }
        }
    }

    return condition;
}

auto INDIDome::setWeatherLimits(const WeatherLimits& limits) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);

    weather_limits_ = limits;

    logInfo("Updated weather limits:");
    logInfo("  Max wind speed: " + std::to_string(limits.maxWindSpeed) + " m/s");
    logInfo("  Min temperature: " + std::to_string(limits.minTemperature) + "°C");
    logInfo("  Max temperature: " + std::to_string(limits.maxTemperature) + "°C");
    logInfo("  Max humidity: " + std::to_string(limits.maxHumidity) + "%");
    logInfo("  Rain protection: " + std::string(limits.rainProtection ? "enabled" : "disabled"));

    return true;
}

auto INDIDome::getWeatherLimits() -> WeatherLimits {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return weather_limits_;
}

// Helper method implementations
void INDIDome::checkWeatherStatus() {
    if (!weather_monitoring_enabled_ || !isConnected()) {
        return;
    }

    auto condition = getWeatherCondition();
    if (!condition) {
        return;
    }

    bool safe = true;
    std::string issues;

    // Check wind speed
    if (condition->windSpeed > weather_limits_.maxWindSpeed) {
        safe = false;
        issues += "Wind speed too high (" + std::to_string(condition->windSpeed) + " > " +
                 std::to_string(weather_limits_.maxWindSpeed) + " m/s); ";
    }

    // Check temperature
    if (condition->temperature < weather_limits_.minTemperature ||
        condition->temperature > weather_limits_.maxTemperature) {
        safe = false;
        issues += "Temperature out of range (" + std::to_string(condition->temperature) + "°C); ";
    }

    // Check humidity
    if (condition->humidity > weather_limits_.maxHumidity) {
        safe = false;
        issues += "Humidity too high (" + std::to_string(condition->humidity) + "%); ";
    }

    // Check rain
    if (weather_limits_.rainProtection && condition->rainDetected) {
        safe = false;
        issues += "Rain detected; ";
    }

    if (weather_safe_ != safe) {
        weather_safe_ = safe;

        if (!safe) {
            logWarning("Weather unsafe: " + issues);
            // Auto-close shutter if enabled and weather becomes unsafe
            if (auto_close_on_unsafe_weather_ && getShutterState() == ShutterState::OPEN) {
                logInfo("Auto-closing shutter due to unsafe weather");
                closeShutter();
            }
        } else {
            logInfo("Weather conditions are safe");
        }

        notifyWeatherEvent(safe, issues);
    }
}

void INDIDome::updateDomeParameters() {
    // Update dome parameters from INDI properties if available
    if (!isConnected()) {
        return;
    }

    auto paramsProp = base_device_.getProperty("DOME_PARAMS");
    if (paramsProp.isValid() && paramsProp.getType() == INDI_NUMBER) {
        auto paramsNumber = paramsProp.getNumber();

        for (int i = 0; i < paramsNumber.count(); ++i) {
            auto widget = paramsNumber.at(i);
            std::string name = widget->getName();
            double value = widget->getValue();

            if (name == "DOME_RADIUS") {
                dome_parameters_.radius = value;
            } else if (name == "DOME_SHUTTER_WIDTH") {
                dome_parameters_.shutterWidth = value;
            } else if (name == "TELESCOPE_OFFSET_NS") {
                dome_parameters_.telescopeOffset.north = value;
            } else if (name == "TELESCOPE_OFFSET_EW") {
                dome_parameters_.telescopeOffset.east = value;
            }
        }
    }
}

double INDIDome::normalizeAzimuth(double azimuth) {
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}
auto INDIDome::canOpenShutter() -> bool {
    return is_safe_to_operate_.load() && weather_safe_;
}

auto INDIDome::isSafeToOperate() -> bool {
    return is_safe_to_operate_.load() && weather_safe_;
}

auto INDIDome::getWeatherStatus() -> std::string {
    return weather_status_;
}

auto INDIDome::getTotalRotation() -> double {
    return total_rotation_;
}

auto INDIDome::resetTotalRotation() -> bool {
    total_rotation_ = 0.0;
    logInfo("Total rotation reset to zero");
    return true;
}

auto INDIDome::getShutterOperations() -> uint64_t {
    return shutter_operations_;
}

auto INDIDome::resetShutterOperations() -> bool {
    shutter_operations_ = 0;
    logInfo("Shutter operations count reset to zero");
    return true;
}

auto INDIDome::savePreset(int slot, double azimuth) -> bool {
    // Implementation would save to config file
    logInfo("Preset " + std::to_string(slot) + " saved at azimuth " + std::to_string(azimuth) + "°");
    return true;
}

auto INDIDome::loadPreset(int slot) -> bool {
    // Implementation would load from config file and move to azimuth
    logInfo("Loading preset " + std::to_string(slot));
    return false;
}

auto INDIDome::getPreset(int slot) -> std::optional<double> {
    // Implementation would get from config file
    return std::nullopt;
}

auto INDIDome::deletePreset(int slot) -> bool {
    // Implementation would remove from config file
    logInfo("Deleted preset " + std::to_string(slot));
    return true;
}
