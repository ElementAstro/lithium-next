/*
 * focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Focuser Implementation

*************************************************/

#include "focuser.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <spdlog/spdlog.h>

ASCOMFocuser::ASCOMFocuser(std::string name) : AtomFocuser(std::move(name)) {
    spdlog::info("ASCOMFocuser constructor called with name: {}", getName());
}

ASCOMFocuser::~ASCOMFocuser() {
    spdlog::info("ASCOMFocuser destructor called");
    disconnect();

#ifdef _WIN32
    if (com_focuser_) {
        com_focuser_->Release();
        com_focuser_ = nullptr;
    }
    CoUninitialize();
#endif
}

auto ASCOMFocuser::initialize() -> bool {
    spdlog::info("Initializing ASCOM Focuser");

#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("Failed to initialize COM: {}", hr);
        return false;
    }
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    return true;
}

auto ASCOMFocuser::destroy() -> bool {
    spdlog::info("Destroying ASCOM Focuser");

    stopMonitoring();
    disconnect();

#ifndef _WIN32
    curl_global_cleanup();
#endif

    return true;
}

auto ASCOMFocuser::connect(const std::string &deviceName, int timeout,
                           int maxRetry) -> bool {
    spdlog::info("Connecting to ASCOM focuser device: {}", deviceName);

    device_name_ = deviceName;

    // Try to determine if this is a COM ProgID or Alpaca device
    if (deviceName.find("://") != std::string::npos) {
        // Alpaca REST API
        size_t start = deviceName.find("://") + 3;
        size_t colon = deviceName.find(":", start);
        size_t slash = deviceName.find("/", start);

        if (colon != std::string::npos) {
            alpaca_host_ = deviceName.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ =
                    std::stoi(deviceName.substr(colon + 1, slash - colon - 1));
            } else {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1));
            }
        }

        connection_type_ = ConnectionType::ALPACA_REST;
        return connectToAlpacaDevice(alpaca_host_, alpaca_port_,
                                     alpaca_device_number_);
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

auto ASCOMFocuser::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Focuser");

    stopMonitoring();

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return disconnectFromAlpacaDevice();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return disconnectFromCOMDriver();
    }
#endif

    return true;
}

auto ASCOMFocuser::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM focuser devices");

    std::vector<std::string> devices;

    // Discover Alpaca devices
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());

#ifdef _WIN32
    // TODO: Scan Windows registry for ASCOM COM drivers
#endif

    return devices;
}

auto ASCOMFocuser::isConnected() const -> bool { return is_connected_.load(); }

auto ASCOMFocuser::isMoving() const -> bool { return is_moving_.load(); }

// Position control methods
auto ASCOMFocuser::getPosition() -> std::optional<int> {
    if (!isConnected()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "position");
        if (response) {
            return std::stoi(*response);
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Position");
        if (result) {
            return result->intVal;
        }
    }
#endif

    return std::nullopt;
}

auto ASCOMFocuser::moveSteps(int steps) -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }

    spdlog::info("Moving focuser {} steps", steps);

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Position=" + std::to_string(steps);
        auto response = sendAlpacaRequest("PUT", "move", params);
        if (response) {
            is_moving_.store(true);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_I4;
        param.intVal = steps;

        auto result = invokeCOMMethod("Move", &param, 1);
        if (result) {
            is_moving_.store(true);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMFocuser::moveToPosition(int position) -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }

    spdlog::info("Moving focuser to position: {}", position);

    target_position_.store(position);

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Position=" + std::to_string(position);
        auto response = sendAlpacaRequest("PUT", "move", params);
        if (response) {
            is_moving_.store(true);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_I4;
        param.intVal = position;

        auto result = invokeCOMMethod("Move", &param, 1);
        if (result) {
            is_moving_.store(true);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMFocuser::moveInward(int steps) -> bool { return moveSteps(-steps); }

auto ASCOMFocuser::moveOutward(int steps) -> bool { return moveSteps(steps); }

auto ASCOMFocuser::abortMove() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Aborting focuser movement");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "halt");
        if (response) {
            is_moving_.store(false);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("Halt");
        if (result) {
            is_moving_.store(false);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMFocuser::syncPosition(int position) -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Syncing focuser position to: {}", position);

    // ASCOM focusers don't typically support sync, but some do
    current_position_.store(position);
    return true;
}

// Speed control
auto ASCOMFocuser::getSpeed() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    // ASCOM doesn't have a standard speed property, return cached value
    return ascom_focuser_info_.current_speed;
}

auto ASCOMFocuser::setSpeed(double speed) -> bool {
    if (!isConnected()) {
        return false;
    }

    ascom_focuser_info_.current_speed = static_cast<int>(speed);
    spdlog::info("Set focuser speed to: {}", speed);
    return true;
}

auto ASCOMFocuser::getMaxSpeed() -> int {
    return ascom_focuser_info_.max_speed;
}

auto ASCOMFocuser::getSpeedRange() -> std::pair<int, int> {
    return {1, ascom_focuser_info_.max_speed};
}

// Temperature
auto ASCOMFocuser::getExternalTemperature() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "temperature");
        if (response) {
            return std::stod(*response);
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Temperature");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return std::nullopt;
}

auto ASCOMFocuser::hasTemperatureSensor() -> bool {
    return ascom_focuser_info_.temp_comp_available;
}

// Temperature compensation
auto ASCOMFocuser::getTemperatureCompensation() -> TemperatureCompensation {
    TemperatureCompensation comp;
    comp.enabled = ascom_focuser_info_.temp_comp;
    comp.coefficient = ascom_focuser_info_.temperature_coefficient;

    auto temp = getExternalTemperature();
    if (temp) {
        comp.temperature = *temp;
    }

    return comp;
}

auto ASCOMFocuser::setTemperatureCompensation(
    const TemperatureCompensation &comp) -> bool {
    if (!isConnected()) {
        return false;
    }

    ascom_focuser_info_.temp_comp = comp.enabled;
    ascom_focuser_info_.temperature_coefficient = comp.coefficient;

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params =
            "TempComp=" + std::string(comp.enabled ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "tempcomp", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = comp.enabled ? VARIANT_TRUE : VARIANT_FALSE;
        return setCOMProperty("TempComp", value);
    }
#endif

    return false;
}

auto ASCOMFocuser::enableTemperatureCompensation(bool enable) -> bool {
    TemperatureCompensation comp = getTemperatureCompensation();
    comp.enabled = enable;
    return setTemperatureCompensation(comp);
}

// Backlash compensation
auto ASCOMFocuser::getBacklash() -> int { return ascom_focuser_info_.backlash; }

auto ASCOMFocuser::setBacklash(int backlash) -> bool {
    ascom_focuser_info_.backlash = backlash;
    spdlog::info("Set focuser backlash to: {}", backlash);
    return true;
}

auto ASCOMFocuser::enableBacklashCompensation(bool enable) -> bool {
    ascom_focuser_info_.has_backlash = enable;
    return true;
}

auto ASCOMFocuser::isBacklashCompensationEnabled() -> bool {
    return ascom_focuser_info_.has_backlash;
}

// Alpaca discovery and connection methods
auto ASCOMFocuser::discoverAlpacaDevices() -> std::vector<std::string> {
    spdlog::info("Discovering Alpaca focuser devices");
    std::vector<std::string> devices;

    // TODO: Implement Alpaca discovery protocol
    devices.push_back("http://localhost:11111/api/v1/focuser/0");

    return devices;
}

auto ASCOMFocuser::connectToAlpacaDevice(const std::string &host, int port,
                                         int deviceNumber) -> bool {
    spdlog::info("Connecting to Alpaca focuser device at {}:{} device {}", host,
                 port, deviceNumber);

    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateFocuserInfo();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMFocuser::disconnectFromAlpacaDevice() -> bool {
    spdlog::info("Disconnecting from Alpaca focuser device");

    if (is_connected_.load()) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        is_connected_.store(false);
    }

    return true;
}

// Helper methods
auto ASCOMFocuser::sendAlpacaRequest(const std::string &method,
                                     const std::string &endpoint,
                                     const std::string &params)
    -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    spdlog::debug("Sending Alpaca request: {} {}", method, endpoint);
    return std::nullopt;
}

auto ASCOMFocuser::parseAlpacaResponse(const std::string &response)
    -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto ASCOMFocuser::updateFocuserInfo() -> bool {
    if (!isConnected()) {
        return false;
    }

    // Get focuser properties
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // TODO: Get actual properties from device
        ascom_focuser_info_.is_absolute = true;
        ascom_focuser_info_.max_step = 10000;
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto absolute_result = getCOMProperty("Absolute");
        auto maxstep_result = getCOMProperty("MaxStep");

        if (absolute_result) {
            ascom_focuser_info_.is_absolute =
                (absolute_result->boolVal == VARIANT_TRUE);
        }

        if (maxstep_result) {
            ascom_focuser_info_.max_step = maxstep_result->intVal;
        }
    }
#endif

    return true;
}

auto ASCOMFocuser::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ =
            std::make_unique<std::thread>(&ASCOMFocuser::monitoringLoop, this);
    }
}

auto ASCOMFocuser::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMFocuser::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            // Update focuser state
            auto position = getPosition();
            if (position) {
                current_position_.store(*position);
            }

            // Check if movement completed
            if (is_moving_.load()) {
                bool moving = false;

                if (connection_type_ == ConnectionType::ALPACA_REST) {
                    auto response = sendAlpacaRequest("GET", "ismoving");
                    if (response && *response == "false") {
                        moving = false;
                    }
                }

#ifdef _WIN32
                if (connection_type_ == ConnectionType::COM_DRIVER) {
                    auto result = getCOMProperty("IsMoving");
                    if (result && result->boolVal == VARIANT_FALSE) {
                        moving = false;
                    }
                }
#endif

                if (!moving) {
                    is_moving_.store(false);
                    notifyMoveComplete(true, "Movement completed");
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#ifdef _WIN32
auto ASCOMFocuser::connectToCOMDriver(const std::string &progId) -> bool {
    spdlog::info("Connecting to COM focuser driver: {}", progId);

    com_prog_id_ = progId;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get CLSID from ProgID: {}", hr);
        return false;
    }

    hr = CoCreateInstance(
        clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch, reinterpret_cast<void **>(&com_focuser_));
    if (FAILED(hr)) {
        spdlog::error("Failed to create COM instance: {}", hr);
        return false;
    }

    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;

    if (setCOMProperty("Connected", value)) {
        is_connected_.store(true);
        updateFocuserInfo();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMFocuser::disconnectFromCOMDriver() -> bool {
    spdlog::info("Disconnecting from COM focuser driver");

    if (com_focuser_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);

        com_focuser_->Release();
        com_focuser_ = nullptr;
    }

    is_connected_.store(false);
    return true;
}

// COM helper methods (similar to camera implementation)
auto ASCOMFocuser::invokeCOMMethod(const std::string &method, VARIANT *params,
                                   int param_count) -> std::optional<VARIANT> {
    if (!com_focuser_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR method_name(method.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &method_name, 1,
                                             LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {params, nullptr, param_count, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_METHOD, &dispparams, &result, nullptr,
                              nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }

    return result;
}

auto ASCOMFocuser::getCOMProperty(const std::string &property)
    -> std::optional<VARIANT> {
    if (!com_focuser_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &property_name, 1,
                                             LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_PROPERTYGET, &dispparams, &result,
                              nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }

    return result;
}

auto ASCOMFocuser::setCOMProperty(const std::string &property,
                                  const VARIANT &value) -> bool {
    if (!com_focuser_) {
        return false;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &property_name, 1,
                                             LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return false;
    }

    VARIANT params[] = {value};
    DISPID dispid_put = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = {params, &dispid_put, 1, 1};

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_PROPERTYPUT, &dispparams, nullptr,
                              nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to set property {}: {}", property, hr);
        return false;
    }

    return true;
}
#endif

// Stub implementations for remaining virtual methods
auto ASCOMFocuser::getDirection() -> std::optional<FocusDirection> {
    return std::nullopt;
}
auto ASCOMFocuser::setDirection(FocusDirection direction) -> bool {
    return false;
}
auto ASCOMFocuser::isReversed() -> std::optional<bool> { return false; }
auto ASCOMFocuser::setReversed(bool reversed) -> bool { return false; }
auto ASCOMFocuser::getMaxLimit() -> std::optional<int> {
    return ascom_focuser_info_.max_step;
}
auto ASCOMFocuser::setMaxLimit(int maxLimit) -> bool { return false; }
auto ASCOMFocuser::getMinLimit() -> std::optional<int> { return std::nullopt; }
auto ASCOMFocuser::setMinLimit(int minLimit) -> bool { return false; }
auto ASCOMFocuser::moveForDuration(int durationMs) -> bool { return false; }
auto ASCOMFocuser::getChipTemperature() -> std::optional<double> {
    return std::nullopt;
}
auto ASCOMFocuser::startAutoFocus() -> bool { return false; }
auto ASCOMFocuser::stopAutoFocus() -> bool { return false; }
auto ASCOMFocuser::isAutoFocusing() -> bool { return false; }
auto ASCOMFocuser::getAutoFocusProgress() -> double { return 0.0; }
auto ASCOMFocuser::savePreset(int slot, int position) -> bool { return false; }
auto ASCOMFocuser::loadPreset(int slot) -> bool { return false; }
auto ASCOMFocuser::getPreset(int slot) -> std::optional<int> {
    return std::nullopt;
}
auto ASCOMFocuser::deletePreset(int slot) -> bool { return false; }
auto ASCOMFocuser::getTotalSteps() -> uint64_t { return 0; }
auto ASCOMFocuser::resetTotalSteps() -> bool { return false; }
auto ASCOMFocuser::getLastMoveSteps() -> int { return 0; }
auto ASCOMFocuser::getLastMoveDuration() -> int { return 0; }
