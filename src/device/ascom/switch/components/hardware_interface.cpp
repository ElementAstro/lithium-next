/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Hardware Interface Component Implementation

This component handles low-level communication with ASCOM switch devices,
supporting both COM drivers and Alpaca REST API.

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "ascom_com_helper.hpp"
#endif

namespace lithium::device::ascom::sw::components {

HardwareInterface::HardwareInterface()
    : connected_(false),
      initialized_(false),
      connection_type_(ConnectionType::ALPACA_REST),
      alpaca_host_("localhost"),
      alpaca_port_(11111),
      alpaca_device_number_(0),
      client_id_("Lithium-Next"),
      interface_version_(2),
      switch_count_(0),
      polling_enabled_(false),
      polling_interval_ms_(1000),
      stop_polling_(false)
#ifdef _WIN32
      , com_switch_(nullptr)
#endif
{
    spdlog::debug("HardwareInterface component created");
}

HardwareInterface::~HardwareInterface() {
    spdlog::debug("HardwareInterface component destroyed");
    disconnect();

#ifdef _WIN32
    if (com_switch_) {
        com_switch_->Release();
        com_switch_ = nullptr;
    }
#endif
}

auto HardwareInterface::initialize() -> bool {
    spdlog::info("Initializing ASCOM Switch Hardware Interface");

    // Initialize COM on Windows
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        setLastError("Failed to initialize COM");
        return false;
    }
#endif

    initialized_.store(true);
    return true;
}

auto HardwareInterface::destroy() -> bool {
    spdlog::info("Destroying ASCOM Switch Hardware Interface");

    stopPolling();
    disconnect();

#ifdef _WIN32
    CoUninitialize();
#endif

    initialized_.store(false);
    return true;
}

auto HardwareInterface::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    spdlog::info("Connecting to ASCOM switch device: {}", deviceName);

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
    setLastError("COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto HardwareInterface::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Switch Hardware Interface");

    stopPolling();

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        disconnectFromAlpacaDevice();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        disconnectFromCOMDriver();
    }
#endif

    connected_.store(false);
    notifyConnectionChange(false);
    return true;
}

auto HardwareInterface::isConnected() const -> bool {
    return connected_.load();
}

auto HardwareInterface::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM switch devices");

    std::vector<std::string> devices;

#ifdef _WIN32
    // Scan Windows registry for ASCOM Switch drivers
    // TODO: Implement registry scanning
#endif

    // Scan for Alpaca devices
    auto alpacaDevices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpacaDevices.begin(), alpacaDevices.end());

    return devices;
}

auto HardwareInterface::getDriverInfo() -> std::optional<std::string> {
    return driver_info_.empty() ? std::nullopt : std::make_optional(driver_info_);
}

auto HardwareInterface::getDriverVersion() -> std::optional<std::string> {
    return driver_version_.empty() ? std::nullopt : std::make_optional(driver_version_);
}

auto HardwareInterface::getInterfaceVersion() -> std::optional<int> {
    return interface_version_;
}

auto HardwareInterface::getDeviceName() const -> std::string {
    return device_name_;
}

auto HardwareInterface::getConnectionType() const -> ConnectionType {
    return connection_type_;
}

auto HardwareInterface::getSwitchCount() -> uint32_t {
    if (!isConnected()) {
        return 0;
    }

    if (switch_count_ > 0) {
        return switch_count_;
    }

    // Get switch count from ASCOM device
    updateSwitchInfo();
    return switch_count_;
}

auto HardwareInterface::getSwitchInfo(uint32_t index) -> std::optional<ASCOMSwitchInfo> {
    std::lock_guard<std::mutex> lock(switches_mutex_);

    if (index >= switches_.size()) {
        return std::nullopt;
    }

    return switches_[index];
}

auto HardwareInterface::setSwitchState(uint32_t index, bool state) -> bool {
    if (!isConnected() || !validateSwitchIndex(index)) {
        return false;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // Send command to ASCOM device via Alpaca
        std::string params = "Id=" + std::to_string(index) +
                             "&State=" + (state ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "setswitch", params);

        if (response) {
            std::lock_guard<std::mutex> lock(switches_mutex_);
            if (index < switches_.size()) {
                switches_[index].state = state;
                notifyStateChange(index, state);
            }
            return true;
        }
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        // Send command via COM interface
        return setCOMProperty("Switch", VARIANT{/* TODO: construct VARIANT */});
    }
#endif

    return false;
}

auto HardwareInterface::getSwitchState(uint32_t index) -> std::optional<bool> {
    if (!isConnected() || !validateSwitchIndex(index)) {
        return std::nullopt;
    }

    // Update from device if needed
    updateSwitchInfo();

    std::lock_guard<std::mutex> lock(switches_mutex_);
    if (index < switches_.size()) {
        return switches_[index].state;
    }
    return std::nullopt;
}

auto HardwareInterface::getSwitchValue(uint32_t index) -> std::optional<double> {
    if (!isConnected() || !validateSwitchIndex(index)) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(switches_mutex_);
    if (index < switches_.size()) {
        return switches_[index].value;
    }
    return std::nullopt;
}

auto HardwareInterface::setSwitchValue(uint32_t index, double value) -> bool {
    if (!isConnected() || !validateSwitchIndex(index)) {
        return false;
    }

    // For now, treat any non-zero value as "true" state
    return setSwitchState(index, value != 0.0);
}

auto HardwareInterface::setClientID(const std::string& clientId) -> bool {
    client_id_ = clientId;
    return true;
}

auto HardwareInterface::getClientID() -> std::optional<std::string> {
    return client_id_;
}

auto HardwareInterface::enablePolling(bool enable, uint32_t intervalMs) -> bool {
    if (enable) {
        polling_interval_ms_.store(intervalMs);
        polling_enabled_.store(true);
        startPolling();
    } else {
        polling_enabled_.store(false);
        stopPolling();
    }
    return true;
}

auto HardwareInterface::isPollingEnabled() const -> bool {
    return polling_enabled_.load();
}

auto HardwareInterface::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto HardwareInterface::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

void HardwareInterface::setStateChangeCallback(std::function<void(uint32_t, bool)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_change_callback_ = std::move(callback);
}

void HardwareInterface::setErrorCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void HardwareInterface::setConnectionCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    connection_callback_ = std::move(callback);
}

// Alpaca discovery and connection
auto HardwareInterface::discoverAlpacaDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    // TODO: Implement Alpaca discovery protocol
    spdlog::warn("Alpaca device discovery not yet implemented");
    return devices;
}

auto HardwareInterface::connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool {
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        connected_.store(true);
        updateSwitchInfo();
        if (polling_enabled_.load()) {
            startPolling();
        }
        notifyConnectionChange(true);
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromAlpacaDevice() -> bool {
    sendAlpacaRequest("PUT", "connected", "Connected=false");
    return true;
}

#ifdef _WIN32
auto HardwareInterface::connectToCOMDriver(const std::string& progId) -> bool {
    com_prog_id_ = progId;

    HRESULT hr = CoCreateInstance(
        CLSID_NULL,  // Would need to resolve ProgID to CLSID
        nullptr,
        CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch,
        reinterpret_cast<void**>(&com_switch_)
    );

    if (SUCCEEDED(hr)) {
        connected_.store(true);
        updateSwitchInfo();
        if (polling_enabled_.load()) {
            startPolling();
        }
        notifyConnectionChange(true);
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromCOMDriver() -> bool {
    if (com_switch_) {
        com_switch_->Release();
        com_switch_ = nullptr;
    }
    return true;
}

auto HardwareInterface::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    spdlog::warn("ASCOM chooser dialog not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::invokeCOMMethod(const std::string& method, VARIANT* params, int paramCount) -> std::optional<VARIANT> {
    // TODO: Implement COM method invocation
    spdlog::warn("COM method invocation not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::getCOMProperty(const std::string& property) -> std::optional<VARIANT> {
    // TODO: Implement COM property getter
    spdlog::warn("COM property getter not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::setCOMProperty(const std::string& property, const VARIANT& value) -> bool {
    // TODO: Implement COM property setter
    spdlog::warn("COM property setter not yet implemented");
    return false;
}
#endif

// Helper methods
auto HardwareInterface::sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                                         const std::string& params) -> std::optional<std::string> {
    // TODO: Implement HTTP request to Alpaca server
    spdlog::warn("Alpaca HTTP request not yet implemented: {} {} {}", method, endpoint, params);
    return std::nullopt;
}

auto HardwareInterface::parseAlpacaResponse(const std::string& response) -> std::optional<std::string> {
    // TODO: Parse JSON response and check for errors
    spdlog::warn("Alpaca response parsing not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::updateSwitchInfo() -> bool {
    if (!isConnected()) {
        return false;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // Get switch count
        auto countResponse = sendAlpacaRequest("GET", "maxswitch");
        if (countResponse) {
            // TODO: Parse JSON to get actual count
            switch_count_ = 0;  // Placeholder
        }

        // Get information for each switch
        std::lock_guard<std::mutex> lock(switches_mutex_);
        switches_.clear();

        for (uint32_t i = 0; i < switch_count_; ++i) {
            ASCOMSwitchInfo info;

            // Get switch name
            auto nameResponse = sendAlpacaRequest("GET", "getswitchname", "Id=" + std::to_string(i));
            if (nameResponse) {
                // TODO: Parse JSON response
                info.name = "Switch " + std::to_string(i);  // Placeholder
            }

            // Get switch description
            auto descResponse = sendAlpacaRequest("GET", "getswitchdescription", "Id=" + std::to_string(i));
            if (descResponse) {
                // TODO: Parse JSON response
                info.description = "Switch " + std::to_string(i) + " description";  // Placeholder
            }

            // Get switch state
            auto stateResponse = sendAlpacaRequest("GET", "getswitch", "Id=" + std::to_string(i));
            if (stateResponse) {
                // TODO: Parse JSON response
                info.state = false;  // Placeholder
            }

            // Get other properties
            info.can_write = true;  // Most switches are writable
            info.min_value = 0.0;
            info.max_value = 1.0;
            info.step_value = 1.0;
            info.value = info.state ? 1.0 : 0.0;

            switches_.push_back(info);
        }
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        // TODO: Implement COM-based switch info retrieval
        spdlog::warn("COM switch info retrieval not yet implemented");
    }
#endif

    return true;
}

auto HardwareInterface::validateSwitchIndex(uint32_t index) const -> bool {
    std::lock_guard<std::mutex> lock(switches_mutex_);
    return index < switches_.size();
}

auto HardwareInterface::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("Hardware Interface Error: {}", error);
}

auto HardwareInterface::startPolling() -> void {
    if (!polling_thread_) {
        stop_polling_.store(false);
        polling_thread_ = std::make_unique<std::thread>(&HardwareInterface::pollingLoop, this);
    }
}

auto HardwareInterface::stopPolling() -> void {
    if (polling_thread_) {
        stop_polling_.store(true);
        polling_cv_.notify_all();
        if (polling_thread_->joinable()) {
            polling_thread_->join();
        }
        polling_thread_.reset();
    }
}

auto HardwareInterface::pollingLoop() -> void {
    spdlog::debug("Hardware interface polling loop started");

    while (!stop_polling_.load()) {
        if (isConnected()) {
            updateSwitchInfo();
        }

        std::unique_lock<std::mutex> lock(polling_mutex_);
        polling_cv_.wait_for(lock, std::chrono::milliseconds(polling_interval_ms_.load()),
                           [this] { return stop_polling_.load(); });
    }

    spdlog::debug("Hardware interface polling loop stopped");
}

auto HardwareInterface::notifyStateChange(uint32_t index, bool state) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_change_callback_) {
        state_change_callback_(index, state);
    }
}

auto HardwareInterface::notifyError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

auto HardwareInterface::notifyConnectionChange(bool connected) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

} // namespace lithium::device::ascom::sw::components
