/*
 * filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM FilterWheel Implementation

*************************************************/

#include "filterwheel.hpp"

#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <curl/curl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "atom/log/loguru.hpp"

ASCOMFilterWheel::ASCOMFilterWheel(std::string name) 
    : AtomFilterWheel(std::move(name)) {
    LOG_F(INFO, "ASCOMFilterWheel constructor called with name: {}", getName());
}

ASCOMFilterWheel::~ASCOMFilterWheel() {
    LOG_F(INFO, "ASCOMFilterWheel destructor called");
    disconnect();
    
#ifdef _WIN32
    if (com_filterwheel_) {
        com_filterwheel_->Release();
        com_filterwheel_ = nullptr;
    }
    CoUninitialize();
#endif
}

auto ASCOMFilterWheel::initialize() -> bool {
    LOG_F(INFO, "Initializing ASCOM FilterWheel");
    
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_F(ERROR, "Failed to initialize COM: {}", hr);
        return false;
    }
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    
    return true;
}

auto ASCOMFilterWheel::destroy() -> bool {
    LOG_F(INFO, "Destroying ASCOM FilterWheel");
    
    stopMonitoring();
    disconnect();
    
#ifndef _WIN32
    curl_global_cleanup();
#endif
    
    return true;
}

auto ASCOMFilterWheel::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    LOG_F(INFO, "Connecting to ASCOM filterwheel device: {}", deviceName);
    
    device_name_ = deviceName;
    
    // Determine connection type
    if (deviceName.find("://") != std::string::npos) {
        // Alpaca REST API
        size_t start = deviceName.find("://") + 3;
        size_t colon = deviceName.find(":", start);
        size_t slash = deviceName.find("/", start);
        
        if (colon != std::string::npos) {
            alpaca_host_ = deviceName.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1, slash - colon - 1));
            } else {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1));
            }
        }
        
        connection_type_ = ConnectionType::ALPACA_REST;
        return connectToAlpacaDevice(alpaca_host_, alpaca_port_, alpaca_device_number_);
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

auto ASCOMFilterWheel::disconnect() -> bool {
    LOG_F(INFO, "Disconnecting ASCOM FilterWheel");
    
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

auto ASCOMFilterWheel::scan() -> std::vector<std::string> {
    LOG_F(INFO, "Scanning for ASCOM filterwheel devices");
    
    std::vector<std::string> devices;
    
    // Discover Alpaca devices
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());
    
    return devices;
}

auto ASCOMFilterWheel::isConnected() const -> bool {
    return is_connected_.load();
}

auto ASCOMFilterWheel::isMoving() const -> bool {
    return is_moving_.load();
}

// Position control methods
auto ASCOMFilterWheel::getPosition() -> std::optional<int> {
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

auto ASCOMFilterWheel::setPosition(int position) -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }
    
    if (position < 0 || position >= filter_count_) {
        LOG_F(ERROR, "Invalid filter position: {}", position);
        return false;
    }
    
    LOG_F(INFO, "Moving filter wheel to position: {}", position);
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Position=" + std::to_string(position);
        auto response = sendAlpacaRequest("PUT", "position", params);
        if (response) {
            is_moving_.store(true);
            current_filter_.store(position);
            return true;
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_I4;
        param.intVal = position;
        
        auto result = setCOMProperty("Position", param);
        if (result) {
            is_moving_.store(true);
            current_filter_.store(position);
            return true;
        }
    }
#endif
    
    return false;
}

auto ASCOMFilterWheel::getFilterCount() -> int {
    if (!isConnected()) {
        return 0;
    }
    
    if (filter_count_ > 0) {
        return filter_count_;
    }
    
    // Get filter count from device
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "names");
        if (response) {
            // TODO: Parse JSON array to get count
            filter_count_ = 8; // Default assumption
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Names");
        if (result && result->vt == (VT_ARRAY | VT_BSTR)) {
            SAFEARRAY* pArray = result->parray;
            if (pArray) {
                long lBound, uBound;
                SafeArrayGetLBound(pArray, 1, &lBound);
                SafeArrayGetUBound(pArray, 1, &uBound);
                filter_count_ = uBound - lBound + 1;
            }
        }
    }
#endif
    
    return filter_count_;
}

auto ASCOMFilterWheel::isValidPosition(int position) -> bool {
    return position >= 0 && position < getFilterCount();
}

// Filter names and information
auto ASCOMFilterWheel::getSlotName(int slot) -> std::optional<std::string> {
    if (!isConnected() || !isValidPosition(slot)) {
        return std::nullopt;
    }
    
    if (slot < filter_names_.size()) {
        return filter_names_[slot];
    }
    
    // Get from device
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "names");
        if (response) {
            // TODO: Parse JSON array and extract slot name
            return "Filter " + std::to_string(slot + 1);
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Names");
        if (result && result->vt == (VT_ARRAY | VT_BSTR)) {
            SAFEARRAY* pArray = result->parray;
            if (pArray) {
                BSTR* names;
                SafeArrayAccessData(pArray, (void**)&names);
                if (names && slot < filter_count_) {
                    std::string name = _bstr_t(names[slot]);
                    SafeArrayUnaccessData(pArray);
                    return name;
                }
                SafeArrayUnaccessData(pArray);
            }
        }
    }
#endif
    
    return std::nullopt;
}

auto ASCOMFilterWheel::setSlotName(int slot, const std::string& name) -> bool {
    if (!isConnected() || !isValidPosition(slot)) {
        return false;
    }
    
    // Ensure vector is large enough
    if (slot >= filter_names_.size()) {
        filter_names_.resize(slot + 1);
    }
    
    filter_names_[slot] = name;
    LOG_F(INFO, "Set filter slot {} name to: {}", slot, name);
    
    // ASCOM filter wheels typically don't support setting names
    // Names are usually configured in the driver
    return true;
}

auto ASCOMFilterWheel::getAllSlotNames() -> std::vector<std::string> {
    std::vector<std::string> names;
    
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto name = getSlotName(i);
        names.push_back(name ? *name : ("Filter " + std::to_string(i + 1)));
    }
    
    return names;
}

auto ASCOMFilterWheel::getCurrentFilterName() -> std::string {
    auto position = getPosition();
    if (!position) {
        return "Unknown";
    }
    
    auto name = getSlotName(*position);
    return name ? *name : ("Filter " + std::to_string(*position + 1));
}

// Enhanced filter management
auto ASCOMFilterWheel::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    if (!isValidPosition(slot)) {
        return std::nullopt;
    }
    
    FilterInfo info;
    auto name = getSlotName(slot);
    if (name) {
        info.name = *name;
    } else {
        info.name = "Filter " + std::to_string(slot + 1);
    }
    
    info.type = "Unknown";
    info.description = "ASCOM Filter " + std::to_string(slot + 1);
    
    return info;
}

auto ASCOMFilterWheel::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    if (!isValidPosition(slot)) {
        return false;
    }
    
    return setSlotName(slot, info.name);
}

auto ASCOMFilterWheel::getAllFilterInfo() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> filters;
    
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto info = getFilterInfo(i);
        if (info) {
            filters.push_back(*info);
        }
    }
    
    return filters;
}

// Filter search and selection
auto ASCOMFilterWheel::findFilterByName(const std::string& name) -> std::optional<int> {
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto slotName = getSlotName(i);
        if (slotName && *slotName == name) {
            return i;
        }
    }
    return std::nullopt;
}

auto ASCOMFilterWheel::findFilterByType(const std::string& type) -> std::vector<int> {
    std::vector<int> matches;
    
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto info = getFilterInfo(i);
        if (info && info->type == type) {
            matches.push_back(i);
        }
    }
    
    return matches;
}

auto ASCOMFilterWheel::selectFilterByName(const std::string& name) -> bool {
    auto position = findFilterByName(name);
    if (position) {
        return setPosition(*position);
    }
    return false;
}

auto ASCOMFilterWheel::selectFilterByType(const std::string& type) -> bool {
    auto matches = findFilterByType(type);
    if (!matches.empty()) {
        return setPosition(matches[0]);
    }
    return false;
}

// Motion control
auto ASCOMFilterWheel::abortMotion() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Aborting filter wheel motion");
    
    // ASCOM filter wheels typically don't support abort
    // Movement is usually fast and atomic
    is_moving_.store(false);
    return true;
}

auto ASCOMFilterWheel::homeFilterWheel() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Homing filter wheel");
    
    // Move to position 0
    return setPosition(0);
}

auto ASCOMFilterWheel::calibrateFilterWheel() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    LOG_F(INFO, "Calibrating filter wheel");
    
    // ASCOM filter wheels typically auto-calibrate on connection
    return true;
}

// Temperature (if supported)
auto ASCOMFilterWheel::getTemperature() -> std::optional<double> {
    // Most ASCOM filter wheels don't have temperature sensors
    return std::nullopt;
}

auto ASCOMFilterWheel::hasTemperatureSensor() -> bool {
    return false;
}

// Statistics
auto ASCOMFilterWheel::getTotalMoves() -> uint64_t {
    return 0; // Not typically tracked by ASCOM filter wheels
}

auto ASCOMFilterWheel::resetTotalMoves() -> bool {
    return true;
}

auto ASCOMFilterWheel::getLastMoveTime() -> int {
    return 0;
}

// Configuration presets (not supported by standard ASCOM)
auto ASCOMFilterWheel::saveFilterConfiguration(const std::string& name) -> bool {
    return false;
}

auto ASCOMFilterWheel::loadFilterConfiguration(const std::string& name) -> bool {
    return false;
}

auto ASCOMFilterWheel::deleteFilterConfiguration(const std::string& name) -> bool {
    return false;
}

auto ASCOMFilterWheel::getAvailableConfigurations() -> std::vector<std::string> {
    return {};
}

// Alpaca discovery and connection methods
auto ASCOMFilterWheel::discoverAlpacaDevices() -> std::vector<std::string> {
    LOG_F(INFO, "Discovering Alpaca filterwheel devices");
    std::vector<std::string> devices;
    
    // TODO: Implement Alpaca discovery protocol
    devices.push_back("http://localhost:11111/api/v1/filterwheel/0");
    
    return devices;
}

auto ASCOMFilterWheel::connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool {
    LOG_F(INFO, "Connecting to Alpaca filterwheel device at {}:{} device {}", host, port, deviceNumber);
    
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;
    
    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateFilterWheelInfo();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMFilterWheel::disconnectFromAlpacaDevice() -> bool {
    LOG_F(INFO, "Disconnecting from Alpaca filterwheel device");
    
    if (is_connected_.load()) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        is_connected_.store(false);
    }
    
    return true;
}

// Helper methods
auto ASCOMFilterWheel::sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                                        const std::string &params) -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    LOG_F(DEBUG, "Sending Alpaca request: {} {}", method, endpoint);
    return std::nullopt;
}

auto ASCOMFilterWheel::parseAlpacaResponse(const std::string &response) -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto ASCOMFilterWheel::updateFilterWheelInfo() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    // Get filter wheel properties
    filter_count_ = getFilterCount();
    filter_names_ = getAllSlotNames();
    
    return true;
}

auto ASCOMFilterWheel::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ = std::make_unique<std::thread>(&ASCOMFilterWheel::monitoringLoop, this);
    }
}

auto ASCOMFilterWheel::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMFilterWheel::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            // Update filter position
            auto position = getPosition();
            if (position) {
                current_filter_.store(*position);
            }
            
            // Check if movement completed
            if (is_moving_.load()) {
                // Filter wheels typically move quickly, so check for completion
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                is_moving_.store(false);
                
                auto filterName = getCurrentFilterName();
                notifyPositionChange(current_filter_.load(), filterName);
                notifyMoveComplete(true, "Filter change completed");
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

#ifdef _WIN32
auto ASCOMFilterWheel::connectToCOMDriver(const std::string &progId) -> bool {
    LOG_F(INFO, "Connecting to COM filterwheel driver: {}", progId);
    
    com_prog_id_ = progId;
    
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get CLSID from ProgID: {}", hr);
        return false;
    }
    
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                         IID_IDispatch, reinterpret_cast<void**>(&com_filterwheel_));
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to create COM instance: {}", hr);
        return false;
    }
    
    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;
    
    if (setCOMProperty("Connected", value)) {
        is_connected_.store(true);
        updateFilterWheelInfo();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMFilterWheel::disconnectFromCOMDriver() -> bool {
    LOG_F(INFO, "Disconnecting from COM filterwheel driver");
    
    if (com_filterwheel_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);
        
        com_filterwheel_->Release();
        com_filterwheel_ = nullptr;
    }
    
    is_connected_.store(false);
    return true;
}

// COM helper methods
auto ASCOMFilterWheel::invokeCOMMethod(const std::string &method, VARIANT* params, int param_count) -> std::optional<VARIANT> {
    if (!com_filterwheel_) {
        return std::nullopt;
    }
    
    DISPID dispid;
    CComBSTR method_name(method.c_str());
    HRESULT hr = com_filterwheel_->GetIDsOfNames(IID_NULL, &method_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }
    
    DISPPARAMS dispparams = { params, nullptr, param_count, 0 };
    VARIANT result;
    VariantInit(&result);
    
    hr = com_filterwheel_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                                 &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }
    
    return result;
}

auto ASCOMFilterWheel::getCOMProperty(const std::string &property) -> std::optional<VARIANT> {
    if (!com_filterwheel_) {
        return std::nullopt;
    }
    
    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_filterwheel_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }
    
    DISPPARAMS dispparams = { nullptr, nullptr, 0, 0 };
    VARIANT result;
    VariantInit(&result);
    
    hr = com_filterwheel_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET,
                                 &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }
    
    return result;
}

auto ASCOMFilterWheel::setCOMProperty(const std::string &property, const VARIANT &value) -> bool {
    if (!com_filterwheel_) {
        return false;
    }
    
    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_filterwheel_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property ID for {}: {}", property, hr);
        return false;
    }
    
    VARIANT params[] = { value };
    DISPID dispid_put = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = { params, &dispid_put, 1, 1 };
    
    hr = com_filterwheel_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT,
                                 &dispparams, nullptr, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to set property {}: {}", property, hr);
        return false;
    }
    
    return true;
}

auto ASCOMFilterWheel::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM Chooser dialog
    return std::nullopt;
}
#endif
