/*
 * switch.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Implementation

*************************************************/

#include "switch.hpp"

#include <chrono>

#ifdef _WIN32
#include "ascom_com_helper.hpp"
#else
#include "ascom_alpaca_client.hpp"
#endif

#include <spdlog/spdlog.h>

ASCOMSwitch::ASCOMSwitch(std::string name) : AtomSwitch(std::move(name)) {
    spdlog::info("ASCOMSwitch constructor called with name: {}", getName());
}

ASCOMSwitch::~ASCOMSwitch() {
    spdlog::info("ASCOMSwitch destructor called");
    disconnect();

#ifdef _WIN32
    if (com_switch_) {
        com_switch_->Release();
        com_switch_ = nullptr;
    }
#endif
}

auto ASCOMSwitch::initialize() -> bool {
    spdlog::info("Initializing ASCOM Switch");

    // Initialize COM on Windows
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("Failed to initialize COM");
        return false;
    }
#endif

    return true;
}

auto ASCOMSwitch::destroy() -> bool {
    spdlog::info("Destroying ASCOM Switch");

    stopMonitoring();
    disconnect();

#ifdef _WIN32
    CoUninitialize();
#endif

    return true;
}

auto ASCOMSwitch::connect(const std::string& deviceName, int timeout,
                          int maxRetry) -> bool {
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
    spdlog::error("COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto ASCOMSwitch::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Switch");

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

auto ASCOMSwitch::scan() -> std::vector<std::string> {
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

auto ASCOMSwitch::isConnected() const -> bool { return is_connected_.load(); }

// Switch management methods
auto ASCOMSwitch::addSwitch(const ::SwitchInfo& switchInfo) -> bool {
    // ASCOM switches are typically predefined by the driver
    spdlog::warn("Adding switches not supported for ASCOM devices");
    return false;
}

auto ASCOMSwitch::removeSwitch(uint32_t index) -> bool {
    spdlog::warn("Removing switches not supported for ASCOM devices");
    return false;
}

auto ASCOMSwitch::removeSwitch(const std::string& name) -> bool {
    spdlog::warn("Removing switches not supported for ASCOM devices");
    return false;
}

auto ASCOMSwitch::getSwitchCount() -> uint32_t {
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

auto ASCOMSwitch::getSwitchInfo(uint32_t index) -> std::optional<::SwitchInfo> {
    if (index >= switches_.size()) {
        return std::nullopt;
    }

    // Convert internal format to interface format
    const auto& internal = switches_[index];
    ::SwitchInfo info;
    info.name = internal.name;
    info.description = internal.description;
    info.label = internal.name;  // Use name as label
    info.state = internal.state ? SwitchState::ON : SwitchState::OFF;
    info.type = SwitchType::TOGGLE;
    info.enabled = internal.can_write;
    info.index = index;
    info.powerConsumption = 0.0;  // Not supported by ASCOM

    return info;
}

auto ASCOMSwitch::getSwitchInfo(const std::string& name)
    -> std::optional<::SwitchInfo> {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchInfo(*index);
    }
    return std::nullopt;
}

auto ASCOMSwitch::getSwitchIndex(const std::string& name)
    -> std::optional<uint32_t> {
    for (size_t i = 0; i < switches_.size(); ++i) {
        if (switches_[i].name == name) {
            return static_cast<uint32_t>(i);
        }
    }
    return std::nullopt;
}

auto ASCOMSwitch::getAllSwitches() -> std::vector<::SwitchInfo> {
    std::vector<::SwitchInfo> result;

    for (uint32_t i = 0; i < getSwitchCount(); ++i) {
        auto info = getSwitchInfo(i);
        if (info) {
            result.push_back(*info);
        }
    }

    return result;
}

// Switch control methods
auto ASCOMSwitch::setSwitchState(uint32_t index, SwitchState state) -> bool {
    if (!isConnected() || index >= switches_.size()) {
        return false;
    }

    bool boolState = (state == SwitchState::ON);

    // Send command to ASCOM device
    std::string params = "Id=" + std::to_string(index) +
                         "&State=" + (boolState ? "true" : "false");
    auto response = sendAlpacaRequest("PUT", "setswitch", params);

    if (response) {
        switches_[index].state = boolState;
        return true;
    }

    return false;
}

auto ASCOMSwitch::setSwitchState(const std::string& name, SwitchState state)
    -> bool {
    auto index = getSwitchIndex(name);
    if (index) {
        return setSwitchState(*index, state);
    }
    return false;
}

auto ASCOMSwitch::getSwitchState(uint32_t index) -> std::optional<SwitchState> {
    if (!isConnected() || index >= switches_.size()) {
        return std::nullopt;
    }

    // Update from device
    updateSwitchInfo();

    return switches_[index].state ? SwitchState::ON : SwitchState::OFF;
}

auto ASCOMSwitch::getSwitchState(const std::string& name)
    -> std::optional<SwitchState> {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchState(*index);
    }
    return std::nullopt;
}

auto ASCOMSwitch::toggleSwitch(uint32_t index) -> bool {
    auto currentState = getSwitchState(index);
    if (currentState) {
        SwitchState newState = (*currentState == SwitchState::ON)
                                   ? SwitchState::OFF
                                   : SwitchState::ON;
        return setSwitchState(index, newState);
    }
    return false;
}

auto ASCOMSwitch::toggleSwitch(const std::string& name) -> bool {
    auto index = getSwitchIndex(name);
    if (index) {
        return toggleSwitch(*index);
    }
    return false;
}

auto ASCOMSwitch::setAllSwitches(SwitchState state) -> bool {
    bool allSuccess = true;

    for (uint32_t i = 0; i < getSwitchCount(); ++i) {
        if (!setSwitchState(i, state)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

// Batch operations
auto ASCOMSwitch::setSwitchStates(
    const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool {
    bool allSuccess = true;

    for (const auto& [index, state] : states) {
        if (!setSwitchState(index, state)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

auto ASCOMSwitch::setSwitchStates(
    const std::vector<std::pair<std::string, SwitchState>>& states) -> bool {
    bool allSuccess = true;

    for (const auto& [name, state] : states) {
        if (!setSwitchState(name, state)) {
            allSuccess = false;
        }
    }

    return allSuccess;
}

auto ASCOMSwitch::getAllSwitchStates()
    -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::vector<std::pair<uint32_t, SwitchState>> states;

    for (uint32_t i = 0; i < getSwitchCount(); ++i) {
        auto state = getSwitchState(i);
        if (state) {
            states.emplace_back(i, *state);
        }
    }

    return states;
}

// Group management - placeholder implementations
auto ASCOMSwitch::addGroup(const SwitchGroup& group) -> bool {
    spdlog::warn("Switch groups not implemented");
    return false;
}

auto ASCOMSwitch::removeGroup(const std::string& name) -> bool {
    spdlog::warn("Switch groups not implemented");
    return false;
}

auto ASCOMSwitch::getGroupCount() -> uint32_t { return 0; }

auto ASCOMSwitch::getGroupInfo(const std::string& name)
    -> std::optional<SwitchGroup> {
    return std::nullopt;
}

auto ASCOMSwitch::getAllGroups() -> std::vector<SwitchGroup> { return {}; }

auto ASCOMSwitch::addSwitchToGroup(const std::string& groupName,
                                   uint32_t switchIndex) -> bool {
    return false;
}

auto ASCOMSwitch::removeSwitchFromGroup(const std::string& groupName,
                                        uint32_t switchIndex) -> bool {
    return false;
}

// Group control - placeholder implementations
auto ASCOMSwitch::setGroupState(const std::string& groupName,
                                uint32_t switchIndex, SwitchState state)
    -> bool {
    return false;
}

auto ASCOMSwitch::setGroupAllOff(const std::string& groupName) -> bool {
    return false;
}

auto ASCOMSwitch::getGroupStates(const std::string& groupName)
    -> std::vector<std::pair<uint32_t, SwitchState>> {
    return {};
}

// Timer functionality - placeholder implementations
auto ASCOMSwitch::setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool {
    spdlog::warn("Switch timers not implemented");
    return false;
}

auto ASCOMSwitch::setSwitchTimer(const std::string& name, uint32_t durationMs)
    -> bool {
    return false;
}

auto ASCOMSwitch::cancelSwitchTimer(uint32_t index) -> bool { return false; }

auto ASCOMSwitch::cancelSwitchTimer(const std::string& name) -> bool {
    return false;
}

auto ASCOMSwitch::getRemainingTime(uint32_t index) -> std::optional<uint32_t> {
    return std::nullopt;
}

auto ASCOMSwitch::getRemainingTime(const std::string& name)
    -> std::optional<uint32_t> {
    return std::nullopt;
}

// Power monitoring
auto ASCOMSwitch::getTotalPowerConsumption() -> double { return 0.0; }

// ASCOM-specific methods
auto ASCOMSwitch::getASCOMDriverInfo() -> std::optional<std::string> {
    return driver_info_;
}

auto ASCOMSwitch::getASCOMVersion() -> std::optional<std::string> {
    return driver_version_;
}

auto ASCOMSwitch::getASCOMInterfaceVersion() -> std::optional<int> {
    return interface_version_;
}

auto ASCOMSwitch::setASCOMClientID(const std::string& clientId) -> bool {
    client_id_ = clientId;
    return true;
}

auto ASCOMSwitch::getASCOMClientID() -> std::optional<std::string> {
    return client_id_;
}

// Alpaca discovery and connection
auto ASCOMSwitch::discoverAlpacaDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    // TODO: Implement Alpaca discovery
    return devices;
}

auto ASCOMSwitch::connectToAlpacaDevice(const std::string& host, int port,
                                        int deviceNumber) -> bool {
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateSwitchInfo();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMSwitch::disconnectFromAlpacaDevice() -> bool {
    sendAlpacaRequest("PUT", "connected", "Connected=false");
    return true;
}

#ifdef _WIN32
auto ASCOMSwitch::connectToCOMDriver(const std::string& progId) -> bool {
    com_prog_id_ = progId;

    HRESULT hr =
        CoCreateInstance(CLSID_NULL,  // Would need to resolve ProgID to CLSID
                         nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                         IID_IDispatch, reinterpret_cast<void**>(&com_switch_));

    if (SUCCEEDED(hr)) {
        is_connected_.store(true);
        updateSwitchInfo();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMSwitch::disconnectFromCOMDriver() -> bool {
    if (com_switch_) {
        com_switch_->Release();
        com_switch_ = nullptr;
    }
    return true;
}

auto ASCOMSwitch::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    return std::nullopt;
}
#endif

// Helper methods
auto ASCOMSwitch::sendAlpacaRequest(const std::string& method,
                                    const std::string& endpoint,
                                    const std::string& params)
    -> std::optional<std::string> {
    // TODO: Implement HTTP request to Alpaca server
    return std::nullopt;
}

auto ASCOMSwitch::parseAlpacaResponse(const std::string& response)
    -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto ASCOMSwitch::updateSwitchInfo() -> bool {
    if (!isConnected()) {
        return false;
    }

    // Get switch count and information from device
    switch_count_ = 0;  // Default, would query from device

    return true;
}

auto ASCOMSwitch::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ =
            std::make_unique<std::thread>(&ASCOMSwitch::monitoringLoop, this);
    }
}

auto ASCOMSwitch::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMSwitch::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            updateSwitchInfo();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

#ifdef _WIN32
auto ASCOMSwitch::invokeCOMMethod(const std::string& method, VARIANT* params,
                                  int param_count) -> std::optional<VARIANT> {
    // TODO: Implement COM method invocation
    return std::nullopt;
}

auto ASCOMSwitch::getCOMProperty(const std::string& property)
    -> std::optional<VARIANT> {
    // TODO: Implement COM property getter
    return std::nullopt;
}

auto ASCOMSwitch::setCOMProperty(const std::string& property,
                                 const VARIANT& value) -> bool {
    // TODO: Implement COM property setter
    return false;
}
#endif
