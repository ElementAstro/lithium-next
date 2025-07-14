/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
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

namespace lithium::device::ascom::focuser::components {

HardwareInterface::HardwareInterface(const std::string& name)
    : name_(name) {
    spdlog::info("HardwareInterface constructor called with name: {}", name);
}

HardwareInterface::~HardwareInterface() {
    spdlog::info("HardwareInterface destructor called");
    disconnect();

#ifdef _WIN32
    cleanupCOM();
#endif
}

auto HardwareInterface::initialize() -> bool {
    spdlog::info("Initializing ASCOM Focuser Hardware Interface");

#ifdef _WIN32
    if (!initializeCOM()) {
        setError("Failed to initialize COM");
        return false;
    }
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    return true;
}

auto HardwareInterface::destroy() -> bool {
    spdlog::info("Destroying ASCOM Focuser Hardware Interface");

    disconnect();

#ifndef _WIN32
    curl_global_cleanup();
#endif

    return true;
}

auto HardwareInterface::connect(const ConnectionInfo& info) -> bool {
    std::lock_guard<std::mutex> lock(interface_mutex_);

    spdlog::info("Connecting to ASCOM focuser device: {}", info.deviceName);

    connection_info_ = info;

    bool result = false;

    if (info.type == ConnectionType::ALPACA_REST) {
        result = connectToAlpacaDevice(info.host, info.port, info.deviceNumber);
    }
#ifdef _WIN32
    else if (info.type == ConnectionType::COM_DRIVER) {
        result = connectToCOMDriver(info.progId);
    }
#endif

    if (result) {
        connected_.store(true);
        updateFocuserInfo();
        setState(ASCOMFocuserState::IDLE);
        spdlog::info("Successfully connected to focuser device");
    } else {
        setError("Failed to connect to focuser device");
    }

    return result;
}

auto HardwareInterface::disconnect() -> bool {
    std::lock_guard<std::mutex> lock(interface_mutex_);

    if (!connected_.load()) {
        return true;
    }

    spdlog::info("Disconnecting from ASCOM focuser device");

    bool result = true;

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        result = disconnectFromAlpacaDevice();
    }
#ifdef _WIN32
    else if (connection_info_.type == ConnectionType::COM_DRIVER) {
        result = disconnectFromCOMDriver();
    }
#endif

    connected_.store(false);
    setState(ASCOMFocuserState::IDLE);

    return result;
}

auto HardwareInterface::isConnected() const -> bool {
    return connected_.load();
}

auto HardwareInterface::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM focuser devices");

    std::vector<std::string> devices;

    // Discover Alpaca devices
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());

#ifdef _WIN32
    // TODO: Scan Windows registry for ASCOM COM drivers
    // This would involve enumerating registry keys under HKEY_LOCAL_MACHINE\SOFTWARE\ASCOM\Focuser Drivers
#endif

    return devices;
}

auto HardwareInterface::getFocuserInfo() const -> FocuserInfo {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    return focuser_info_;
}

auto HardwareInterface::updateFocuserInfo() -> bool {
    if (!connected_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(interface_mutex_);

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        // Update from Alpaca device
        auto response = sendAlpacaRequest("GET", "absolute");
        if (response) {
            focuser_info_.absolute = (*response == "true");
        }

        response = sendAlpacaRequest("GET", "maxstep");
        if (response) {
            focuser_info_.maxStep = std::stoi(*response);
        }

        response = sendAlpacaRequest("GET", "maxincrement");
        if (response) {
            focuser_info_.maxIncrement = std::stoi(*response);
        }

        response = sendAlpacaRequest("GET", "stepsize");
        if (response) {
            focuser_info_.stepSize = std::stod(*response);
        }

        response = sendAlpacaRequest("GET", "tempcompavailable");
        if (response) {
            focuser_info_.tempCompAvailable = (*response == "true");
        }

        if (focuser_info_.tempCompAvailable) {
            response = sendAlpacaRequest("GET", "tempcomp");
            if (response) {
                focuser_info_.tempComp = (*response == "true");
            }

            response = sendAlpacaRequest("GET", "temperature");
            if (response) {
                focuser_info_.temperature = std::stod(*response);
            }
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        // Update from COM driver
        auto result = getCOMProperty("Absolute");
        if (result) {
            focuser_info_.absolute = (result->boolVal == VARIANT_TRUE);
        }

        result = getCOMProperty("MaxStep");
        if (result) {
            focuser_info_.maxStep = result->intVal;
        }

        result = getCOMProperty("MaxIncrement");
        if (result) {
            focuser_info_.maxIncrement = result->intVal;
        }

        result = getCOMProperty("StepSize");
        if (result) {
            focuser_info_.stepSize = result->dblVal;
        }

        result = getCOMProperty("TempCompAvailable");
        if (result) {
            focuser_info_.tempCompAvailable = (result->boolVal == VARIANT_TRUE);
        }

        if (focuser_info_.tempCompAvailable) {
            result = getCOMProperty("TempComp");
            if (result) {
                focuser_info_.tempComp = (result->boolVal == VARIANT_TRUE);
            }

            result = getCOMProperty("Temperature");
            if (result) {
                focuser_info_.temperature = result->dblVal;
            }
        }
    }
#endif

    return true;
}

auto HardwareInterface::getPosition() -> std::optional<int> {
    if (!connected_.load()) {
        return std::nullopt;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "position");
        if (response) {
            return std::stoi(*response);
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Position");
        if (result) {
            return result->intVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::moveToPosition(int position) -> bool {
    if (!connected_.load()) {
        return false;
    }

    spdlog::info("Moving focuser to position: {}", position);

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        std::string params = "Position=" + std::to_string(position);
        auto response = sendAlpacaRequest("PUT", "move", params);
        if (response) {
            setState(ASCOMFocuserState::MOVING);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_I4;
        param.intVal = position;

        auto result = invokeCOMMethod("Move", &param, 1);
        if (result) {
            setState(ASCOMFocuserState::MOVING);
            return true;
        }
    }
#endif

    return false;
}

auto HardwareInterface::moveSteps(int steps) -> bool {
    if (!connected_.load()) {
        return false;
    }

    spdlog::info("Moving focuser {} steps", steps);

    // For relative moves, we need to get current position first
    auto currentPos = getPosition();
    if (!currentPos) {
        return false;
    }

    return moveToPosition(*currentPos + steps);
}

auto HardwareInterface::isMoving() -> bool {
    if (!connected_.load()) {
        return false;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "ismoving");
        if (response) {
            bool moving = (*response == "true");
            if (!moving && state_.load() == ASCOMFocuserState::MOVING) {
                setState(ASCOMFocuserState::IDLE);
            }
            return moving;
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("IsMoving");
        if (result) {
            bool moving = (result->boolVal == VARIANT_TRUE);
            if (!moving && state_.load() == ASCOMFocuserState::MOVING) {
                setState(ASCOMFocuserState::IDLE);
            }
            return moving;
        }
    }
#endif

    return false;
}

auto HardwareInterface::halt() -> bool {
    if (!connected_.load()) {
        return false;
    }

    spdlog::info("Halting focuser movement");

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "halt");
        if (response) {
            setState(ASCOMFocuserState::IDLE);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("Halt");
        if (result) {
            setState(ASCOMFocuserState::IDLE);
            return true;
        }
    }
#endif

    return false;
}

auto HardwareInterface::getTemperature() -> std::optional<double> {
    if (!connected_.load()) {
        return std::nullopt;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "temperature");
        if (response) {
            return std::stod(*response);
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Temperature");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getTemperatureCompensation() -> bool {
    if (!connected_.load()) {
        return false;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "tempcomp");
        if (response) {
            return (*response == "true");
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("TempComp");
        if (result) {
            return (result->boolVal == VARIANT_TRUE);
        }
    }
#endif

    return false;
}

auto HardwareInterface::setTemperatureCompensation(bool enable) -> bool {
    if (!connected_.load()) {
        return false;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        std::string params = "TempComp=" + std::string(enable ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "tempcomp", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = enable ? VARIANT_TRUE : VARIANT_FALSE;
        return setCOMProperty("TempComp", value);
    }
#endif

    return false;
}

auto HardwareInterface::hasTemperatureCompensation() -> bool {
    if (!connected_.load()) {
        return false;
    }

    if (connection_info_.type == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "tempcompavailable");
        if (response) {
            return (*response == "true");
        }
    }

#ifdef _WIN32
    if (connection_info_.type == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("TempCompAvailable");
        if (result) {
            return (result->boolVal == VARIANT_TRUE);
        }
    }
#endif

    return false;
}

// Alpaca-specific methods
auto HardwareInterface::discoverAlpacaDevices() -> std::vector<std::string> {
    spdlog::info("Discovering Alpaca focuser devices");
    std::vector<std::string> devices;

    // TODO: Implement proper Alpaca discovery protocol
    // For now, return a default device
    devices.push_back("http://localhost:11111/api/v1/focuser/0");

    return devices;
}

auto HardwareInterface::connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool {
    spdlog::info("Connecting to Alpaca focuser device at {}:{} device {}", host, port, deviceNumber);

    alpaca_client_ = std::make_unique<AlpacaClient>(host, port);

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        return true;
    }

    alpaca_client_.reset();
    return false;
}

auto HardwareInterface::disconnectFromAlpacaDevice() -> bool {
    spdlog::info("Disconnecting from Alpaca focuser device");

    if (alpaca_client_) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        alpaca_client_.reset();
    }

    return true;
}

auto HardwareInterface::sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                                         const std::string& params) -> std::optional<std::string> {
    if (!alpaca_client_) {
        return std::nullopt;
    }

    std::string url = buildAlpacaUrl(endpoint);
    return executeAlpacaRequest(method, url, params);
}

// Error handling
auto HardwareInterface::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    return last_error_;
}

auto HardwareInterface::clearError() -> void {
    std::lock_guard<std::mutex> lock(interface_mutex_);
    last_error_.clear();
}

// Callbacks
auto HardwareInterface::setErrorCallback(ErrorCallback callback) -> void {
    error_callback_ = std::move(callback);
}

auto HardwareInterface::setStateChangeCallback(StateChangeCallback callback) -> void {
    state_change_callback_ = std::move(callback);
}

// Private helper methods
auto HardwareInterface::parseAlpacaResponse(const std::string& response) -> std::optional<std::string> {
    // TODO: Implement proper JSON parsing
    return response;
}

auto HardwareInterface::setError(const std::string& error) -> void {
    {
        std::lock_guard<std::mutex> lock(interface_mutex_);
        last_error_ = error;
    }

    spdlog::error("HardwareInterface error: {}", error);

    if (error_callback_) {
        error_callback_(error);
    }
}

auto HardwareInterface::setState(ASCOMFocuserState newState) -> void {
    ASCOMFocuserState oldState = state_.exchange(newState);

    if (oldState != newState && state_change_callback_) {
        state_change_callback_(newState);
    }
}

auto HardwareInterface::validateConnection() -> bool {
    return connected_.load();
}

auto HardwareInterface::buildAlpacaUrl(const std::string& endpoint) -> std::string {
    std::ostringstream oss;
    oss << "http://" << connection_info_.host << ":" << connection_info_.port
        << "/api/v1/focuser/" << connection_info_.deviceNumber << "/" << endpoint;
    return oss.str();
}

auto HardwareInterface::executeAlpacaRequest(const std::string& method, const std::string& url,
                                           const std::string& params) -> std::optional<std::string> {
    if (!alpaca_client_) {
        return std::nullopt;
    }

    // TODO: Implement actual HTTP request using alpaca_client_
    spdlog::debug("Executing Alpaca request: {} {}", method, url);

    return std::nullopt;
}

#ifdef _WIN32
// COM-specific methods
auto HardwareInterface::connectToCOMDriver(const std::string& progId) -> bool {
    spdlog::info("Connecting to COM focuser driver: {}", progId);

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        setError("Failed to get CLSID from ProgID: " + std::to_string(hr));
        return false;
    }

    hr = CoCreateInstance(
        clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch, reinterpret_cast<void**>(&com_focuser_));
    if (FAILED(hr)) {
        setError("Failed to create COM instance: " + std::to_string(hr));
        return false;
    }

    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;

    if (setCOMProperty("Connected", value)) {
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromCOMDriver() -> bool {
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

    return true;
}

auto HardwareInterface::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    return std::nullopt;
}

auto HardwareInterface::invokeCOMMethod(const std::string& method, VARIANT* params,
                                       int paramCount) -> std::optional<VARIANT> {
    if (!com_focuser_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR methodName(method.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &methodName, 1,
                                            LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        setError("Failed to get method ID for " + method + ": " + std::to_string(hr));
        return std::nullopt;
    }

    DISPPARAMS dispparams = {params, nullptr, paramCount, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_METHOD, &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        setError("Failed to invoke method " + method + ": " + std::to_string(hr));
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::getCOMProperty(const std::string& property) -> std::optional<VARIANT> {
    if (!com_focuser_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR propertyName(property.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &propertyName, 1,
                                            LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        setError("Failed to get property ID for " + property + ": " + std::to_string(hr));
        return std::nullopt;
    }

    DISPPARAMS dispparams = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_PROPERTYGET, &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        setError("Failed to get property " + property + ": " + std::to_string(hr));
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::setCOMProperty(const std::string& property, const VARIANT& value) -> bool {
    if (!com_focuser_) {
        return false;
    }

    DISPID dispid;
    CComBSTR propertyName(property.c_str());
    HRESULT hr = com_focuser_->GetIDsOfNames(IID_NULL, &propertyName, 1,
                                            LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        setError("Failed to get property ID for " + property + ": " + std::to_string(hr));
        return false;
    }

    VARIANT params[] = {value};
    DISPID dispidPut = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = {params, &dispidPut, 1, 1};

    hr = com_focuser_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_PROPERTYPUT, &dispparams, nullptr, nullptr, nullptr);
    if (FAILED(hr)) {
        setError("Failed to set property " + property + ": " + std::to_string(hr));
        return false;
    }

    return true;
}

auto HardwareInterface::initializeCOM() -> bool {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        setError("Failed to initialize COM: " + std::to_string(hr));
        return false;
    }
    return true;
}

auto HardwareInterface::cleanupCOM() -> void {
    if (com_focuser_) {
        com_focuser_->Release();
        com_focuser_ = nullptr;
    }
    CoUninitialize();
}

auto HardwareInterface::variantToString(const VARIANT& var) -> std::string {
    // TODO: Implement proper variant to string conversion
    return "";
}

auto HardwareInterface::stringToVariant(const std::string& str) -> VARIANT {
    VARIANT var;
    VariantInit(&var);
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(std::wstring(str.begin(), str.end()).c_str());
    return var;
}

#endif

} // namespace lithium::device::ascom::focuser::components
