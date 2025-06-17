/*
 * rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Rotator Implementation

*************************************************/

#include "rotator.hpp"

#ifdef _WIN32
#include "ascom_com_helper.hpp"
#else
#include "ascom_alpaca_client.hpp"
#endif

#include "atom/log/loguru.hpp"

ASCOMRotator::ASCOMRotator(std::string name) 
    : AtomRotator(std::move(name)) {
    LOG_F(INFO, "ASCOMRotator constructor called with name: {}", getName());
}

ASCOMRotator::~ASCOMRotator() {
    LOG_F(INFO, "ASCOMRotator destructor called");
    disconnect();
    
#ifdef _WIN32
    if (com_rotator_) {
        com_rotator_->Release();
        com_rotator_ = nullptr;
    }
#endif
}

auto ASCOMRotator::initialize() -> bool {
    LOG_F(INFO, "Initializing ASCOM Rotator");
    
    // Initialize COM on Windows
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_F(ERROR, "Failed to initialize COM");
        return false;
    }
#endif
    
    return true;
}

auto ASCOMRotator::destroy() -> bool {
    LOG_F(INFO, "Destroying ASCOM Rotator");
    
    stopMonitoring();
    disconnect();
    
#ifdef _WIN32
    CoUninitialize();
#endif
    
    return true;
}

auto ASCOMRotator::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    LOG_F(INFO, "Connecting to ASCOM rotator device: {}", deviceName);
    
    device_name_ = deviceName;
    
    // Determine connection type
    if (deviceName.find("://") != std::string::npos) {
        // Alpaca REST API - parse URL
        connection_type_ = ConnectionType::ALPACA_REST;
        // Parse host, port, device number from URL
        return connectToAlpacaDevice("localhost", 11111, 0);
    }
    
#ifdef _WIN32
    // Try as COM ProgID
    connection_type_ = ConnectionType::COM_DRIVER;
    return connectToCOMDriver(deviceName);
#else
    LOG_F(ERROR, "COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto ASCOMRotator::disconnect() -> bool {
    LOG_F(INFO, "Disconnecting ASCOM Rotator");
    
    stopMonitoring();
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        disconnectFromAlpacaDevice();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        disconnectFromCOMDriver();
    }
#endif
    
    is_connected_.store(false);
    return true;
}

auto ASCOMRotator::scan() -> std::vector<std::string> {
    LOG_F(INFO, "Scanning for ASCOM rotator devices");
    
    std::vector<std::string> devices;
    
#ifdef _WIN32
    // Scan Windows registry for ASCOM Rotator drivers
    // TODO: Implement registry scanning
#endif

    // Scan for Alpaca devices
    auto alpacaDevices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpacaDevices.begin(), alpacaDevices.end());
    
    return devices;
}

auto ASCOMRotator::isConnected() const -> bool {
    return is_connected_.load();
}

auto ASCOMRotator::isMoving() const -> bool {
    return is_moving_.load();
}

// Position control
auto ASCOMRotator::getPosition() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }
    
    // Get position from ASCOM device
    auto response = sendAlpacaRequest("GET", "position");
    if (response) {
        // Parse response and update current position
        double position = 0.0; // Would parse from response
        current_position_.store(position);
        return position;
    }
    
    return current_position_.load();
}

auto ASCOMRotator::setPosition(double angle) -> bool {
    return moveToAngle(angle);
}

auto ASCOMRotator::moveToAngle(double angle) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Moving rotator to angle: {:.2f}°", angle);
    
    // Normalize angle to 0-360 range
    while (angle < 0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    
    target_position_.store(angle);
    is_moving_.store(true);
    
    // Send command to ASCOM device
    std::string params = "Position=" + std::to_string(angle);
    auto response = sendAlpacaRequest("PUT", "move", params);
    
    return response.has_value();
}

auto ASCOMRotator::rotateByAngle(double angle) -> bool {
    auto currentPos = getPosition();
    if (currentPos) {
        double newPosition = *currentPos + angle;
        return moveToAngle(newPosition);
    }
    return false;
}

auto ASCOMRotator::abortMove() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Aborting rotator movement");
    
    auto response = sendAlpacaRequest("PUT", "halt");
    if (response) {
        is_moving_.store(false);
        return true;
    }
    
    return false;
}

auto ASCOMRotator::syncPosition(double angle) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Syncing rotator position to: {:.2f}°", angle);
    
    // Send sync command to ASCOM device
    std::string params = "Position=" + std::to_string(angle);
    auto response = sendAlpacaRequest("PUT", "sync", params);
    
    if (response) {
        current_position_.store(angle);
        return true;
    }
    
    return false;
}

// Direction control
auto ASCOMRotator::getDirection() -> std::optional<RotatorDirection> {
    // ASCOM rotators typically don't have direction concept
    return RotatorDirection::CLOCKWISE;
}

auto ASCOMRotator::setDirection(RotatorDirection direction) -> bool {
    // ASCOM rotators typically don't support direction setting
    LOG_F(WARNING, "Direction setting not supported for ASCOM rotators");
    return false;
}

auto ASCOMRotator::isReversed() -> bool {
    return ascom_rotator_info_.is_reversed;
}

auto ASCOMRotator::setReversed(bool reversed) -> bool {
    ascom_rotator_info_.is_reversed = reversed;
    
    // Send command to ASCOM device if supported
    std::string params = "Reverse=" + std::string(reversed ? "true" : "false");
    auto response = sendAlpacaRequest("PUT", "reverse", params);
    
    return response.has_value();
}

// Speed control
auto ASCOMRotator::getSpeed() -> std::optional<double> {
    // Most ASCOM rotators don't expose speed control
    return std::nullopt;
}

auto ASCOMRotator::setSpeed(double speed) -> bool {
    LOG_F(WARNING, "Speed control not supported for most ASCOM rotators");
    return false;
}

auto ASCOMRotator::getMaxSpeed() -> double {
    return 10.0; // Default max speed in degrees per second
}

auto ASCOMRotator::getMinSpeed() -> double {
    return 0.1; // Default min speed in degrees per second
}

// Limits
auto ASCOMRotator::getMinPosition() -> double {
    return 0.0;
}

auto ASCOMRotator::getMaxPosition() -> double {
    return 360.0;
}

auto ASCOMRotator::setLimits(double min, double max) -> bool {
    LOG_F(WARNING, "Position limits not configurable for ASCOM rotators");
    return false;
}

// Backlash compensation
auto ASCOMRotator::getBacklash() -> double {
    // TODO: Get from ASCOM device if supported
    return 0.0;
}

auto ASCOMRotator::setBacklash(double backlash) -> bool {
    LOG_F(WARNING, "Backlash compensation typically not supported via ASCOM");
    return false;
}

auto ASCOMRotator::enableBacklashCompensation(bool enable) -> bool {
    LOG_F(WARNING, "Backlash compensation typically not supported via ASCOM");
    return false;
}

auto ASCOMRotator::isBacklashCompensationEnabled() -> bool {
    return false;
}

// Temperature
auto ASCOMRotator::getTemperature() -> std::optional<double> {
    // Most ASCOM rotators don't have temperature sensors
    return std::nullopt;
}

auto ASCOMRotator::hasTemperatureSensor() -> bool {
    return false;
}

// Presets
auto ASCOMRotator::savePreset(int slot, double angle) -> bool {
    LOG_F(WARNING, "Presets not implemented in ASCOM rotator");
    return false;
}

auto ASCOMRotator::loadPreset(int slot) -> bool {
    LOG_F(WARNING, "Presets not implemented in ASCOM rotator");
    return false;
}

auto ASCOMRotator::getPreset(int slot) -> std::optional<double> {
    return std::nullopt;
}

auto ASCOMRotator::deletePreset(int slot) -> bool {
    return false;
}

// Statistics
auto ASCOMRotator::getTotalRotation() -> double {
    return 0.0; // Not tracked by ASCOM
}

auto ASCOMRotator::resetTotalRotation() -> bool {
    return false;
}

auto ASCOMRotator::getLastMoveAngle() -> double {
    return 0.0;
}

auto ASCOMRotator::getLastMoveDuration() -> int {
    return 0;
}

// ASCOM-specific methods
auto ASCOMRotator::getASCOMDriverInfo() -> std::optional<std::string> {
    return driver_info_;
}

auto ASCOMRotator::getASCOMVersion() -> std::optional<std::string> {
    return driver_version_;
}

auto ASCOMRotator::getASCOMInterfaceVersion() -> std::optional<int> {
    return interface_version_;
}

auto ASCOMRotator::setASCOMClientID(const std::string &clientId) -> bool {
    client_id_ = clientId;
    return true;
}

auto ASCOMRotator::getASCOMClientID() -> std::optional<std::string> {
    return client_id_;
}

auto ASCOMRotator::canReverse() -> bool {
    return ascom_rotator_info_.can_reverse;
}

// Alpaca discovery and connection
auto ASCOMRotator::discoverAlpacaDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    // TODO: Implement Alpaca discovery
    return devices;
}

auto ASCOMRotator::connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool {
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;
    
    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateRotatorInfo();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMRotator::disconnectFromAlpacaDevice() -> bool {
    sendAlpacaRequest("PUT", "connected", "Connected=false");
    return true;
}

#ifdef _WIN32
auto ASCOMRotator::connectToCOMDriver(const std::string &progId) -> bool {
    com_prog_id_ = progId;
    
    HRESULT hr = CoCreateInstance(
        CLSID_NULL, // Would need to resolve ProgID to CLSID
        nullptr,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch,
        reinterpret_cast<void**>(&com_rotator_)
    );
    
    if (SUCCEEDED(hr)) {
        is_connected_.store(true);
        updateRotatorInfo();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMRotator::disconnectFromCOMDriver() -> bool {
    if (com_rotator_) {
        com_rotator_->Release();
        com_rotator_ = nullptr;
    }
    return true;
}

auto ASCOMRotator::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    return std::nullopt;
}
#endif

// Helper methods
auto ASCOMRotator::sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                                    const std::string &params) -> std::optional<std::string> {
    // TODO: Implement HTTP request to Alpaca server
    return std::nullopt;
}

auto ASCOMRotator::parseAlpacaResponse(const std::string &response) -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto ASCOMRotator::updateRotatorInfo() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    // Get rotator information from device
    // TODO: Query device properties
    
    return true;
}

auto ASCOMRotator::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ = std::make_unique<std::thread>(&ASCOMRotator::monitoringLoop, this);
    }
}

auto ASCOMRotator::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMRotator::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            // Update position and moving status
            getPosition();
            
            // Check if movement is complete
            auto response = sendAlpacaRequest("GET", "ismoving");
            if (response) {
                // Parse response to update is_moving_
                // For now, assume false
                is_moving_.store(false);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

#ifdef _WIN32
auto ASCOMRotator::invokeCOMMethod(const std::string &method, VARIANT* params, 
                                  int param_count) -> std::optional<VARIANT> {
    // TODO: Implement COM method invocation
    return std::nullopt;
}

auto ASCOMRotator::getCOMProperty(const std::string &property) -> std::optional<VARIANT> {
    // TODO: Implement COM property getter
    return std::nullopt;
}

auto ASCOMRotator::setCOMProperty(const std::string &property, const VARIANT &value) -> bool {
    // TODO: Implement COM property setter
    return false;
}
#endif
