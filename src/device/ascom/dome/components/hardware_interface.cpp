/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
#include <thread>

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

namespace lithium::ascom::dome::components {

HardwareInterface::HardwareInterface() {
    spdlog::info("Initializing ASCOM Dome Hardware Interface");
}

HardwareInterface::~HardwareInterface() {
    spdlog::info("Destroying ASCOM Dome Hardware Interface");
    disconnect();

#ifdef _WIN32
    if (com_dome_) {
        com_dome_->Release();
        com_dome_ = nullptr;
    }
    CoUninitialize();
#endif
}

auto HardwareInterface::initialize() -> bool {
    spdlog::info("Initializing hardware interface");

#ifdef _WIN32
    // Initialize COM for Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        setError("Failed to initialize COM");
        return false;
    }
#endif

    // Clear any previous errors
    clearLastError();
    hardware_status_ = HardwareStatus::DISCONNECTED;

    spdlog::info("Hardware interface initialized successfully");
    return true;
}

auto HardwareInterface::destroy() -> bool {
    spdlog::info("Destroying hardware interface");

    // Disconnect if connected
    if (is_connected_) {
        disconnect();
    }

#ifdef _WIN32
    // Clean up COM
    CoUninitialize();
#endif

    spdlog::info("Hardware interface destroyed successfully");
    return true;
}

auto HardwareInterface::connect(const std::string& deviceName, ConnectionType type, int timeout) -> bool {
    spdlog::info("Connecting to ASCOM dome device: {}", deviceName);

    device_name_ = deviceName;
    connection_type_ = type;

    if (type == ConnectionType::ALPACA_REST) {
        // Parse Alpaca URL
        if (!parseAlpacaUrl(deviceName)) {
            spdlog::error("Failed to parse Alpaca URL: {}", deviceName);
            return false;
        }
        return connectToAlpacaDevice(alpaca_host_, alpaca_port_, alpaca_device_number_);
    }

#ifdef _WIN32
    if (type == ConnectionType::COM_DRIVER) {
        return connectToCOM(deviceName);
    }
#endif

    spdlog::error("Unsupported connection type");
    return false;
}

auto HardwareInterface::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Dome Hardware Interface");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return disconnectFromAlpacaDevice();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return disconnectFromCOM();
    }
#endif

    return true;
}

auto HardwareInterface::isConnected() const -> bool {
    return is_connected_.load();
}

auto HardwareInterface::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for available dome devices");

    std::vector<std::string> devices;

    // TODO: Implement actual device discovery
    // For now, return some example devices
    devices.push_back("ASCOM.Simulator.Dome");
    devices.push_back("ASCOM.TrueTech.Dome");

    spdlog::info("Found {} dome devices", devices.size());
    return devices;
}

auto HardwareInterface::discoverAlpacaDevices() -> std::vector<std::string> {
    spdlog::info("Discovering Alpaca dome devices");
    std::vector<std::string> devices;

    // TODO: Implement Alpaca discovery protocol
    devices.push_back("http://localhost:11111/api/v1/dome/0");

    return devices;
}

auto HardwareInterface::connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool {
    spdlog::info("Connecting to Alpaca dome device at {}:{} device {}", host, port, deviceNumber);

    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateDomeCapabilities();
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromAlpacaDevice() -> bool {
    spdlog::info("Disconnecting from Alpaca dome device");

    if (is_connected_.load()) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        is_connected_.store(false);
    }

    return true;
}

auto HardwareInterface::sendAlpacaRequest(const std::string& method, const std::string& endpoint, const std::string& params) -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    spdlog::debug("Sending Alpaca request: {} {} {}", method, endpoint, params);
    return std::nullopt;
}

auto HardwareInterface::parseAlpacaResponse(const std::string& response) -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto HardwareInterface::updateDomeCapabilities() -> bool {
    if (!isConnected()) {
        return false;
    }

    // Get dome capabilities
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // TODO: Query actual capabilities
        capabilities_.can_find_home = true;
        capabilities_.can_park = true;
        capabilities_.can_set_azimuth = true;
        capabilities_.can_set_shutter = true;
        capabilities_.can_slave = true;
        capabilities_.can_sync_azimuth = false;
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto canFindHome = getCOMProperty("CanFindHome");
        auto canPark = getCOMProperty("CanPark");
        auto canSetAzimuth = getCOMProperty("CanSetAzimuth");
        auto canSetShutter = getCOMProperty("CanSetShutter");
        auto canSlave = getCOMProperty("CanSlave");
        auto canSyncAzimuth = getCOMProperty("CanSyncAzimuth");

        if (canFindHome)
            ascom_capabilities_.can_find_home = (canFindHome->boolVal == VARIANT_TRUE);
        if (canPark)
            ascom_capabilities_.can_park = (canPark->boolVal == VARIANT_TRUE);
        if (canSetAzimuth)
            ascom_capabilities_.can_set_azimuth = (canSetAzimuth->boolVal == VARIANT_TRUE);
        if (canSetShutter)
            ascom_capabilities_.can_set_shutter = (canSetShutter->boolVal == VARIANT_TRUE);
        if (canSlave)
            ascom_capabilities_.can_slave = (canSlave->boolVal == VARIANT_TRUE);
        if (canSyncAzimuth)
            ascom_capabilities_.can_sync_azimuth = (canSyncAzimuth->boolVal == VARIANT_TRUE);
    }
#endif

    return true;
}

auto HardwareInterface::getDomeCapabilities() -> std::optional<std::string> {
    if (!capabilities_.capabilities_loaded) {
        return std::nullopt;
    }

    // Return capabilities as a formatted string
    std::string caps;
    if (capabilities_.can_find_home) caps += "home,";
    if (capabilities_.can_park) caps += "park,";
    if (capabilities_.can_set_azimuth) caps += "azimuth,";
    if (capabilities_.can_set_shutter) caps += "shutter,";
    if (capabilities_.can_slave) caps += "slave,";
    if (capabilities_.can_sync_azimuth) caps += "sync,";

    if (!caps.empty()) {
        caps.pop_back(); // Remove trailing comma
    }

    return caps;
}

auto HardwareInterface::getDriverInfo() -> std::optional<std::string> {
    return driver_info_.empty() ? std::nullopt : std::optional<std::string>(driver_info_);
}

auto HardwareInterface::getDriverVersion() -> std::optional<std::string> {
    return driver_version_.empty() ? std::nullopt : std::optional<std::string>(driver_version_);
}

auto HardwareInterface::getInterfaceVersion() -> std::optional<int> {
    return interface_version_;
}

auto HardwareInterface::getConnectionType() const -> ConnectionType {
    return connection_type_;
}

auto HardwareInterface::getDeviceName() -> std::optional<std::string> {
    if (device_name_.empty()) {
        return std::nullopt;
    }
    return device_name_;
}

auto HardwareInterface::getAlpacaHost() const -> std::string {
    return alpaca_host_;
}

auto HardwareInterface::getAlpacaPort() const -> int {
    return alpaca_port_;
}

auto HardwareInterface::getAlpacaDeviceNumber() const -> int {
    return alpaca_device_number_;
}

auto HardwareInterface::parseAlpacaUrl(const std::string& url) -> bool {
    // Parse URL format: http://host:port/api/v1/dome/deviceNumber
    if (url.find("://") != std::string::npos) {
        size_t start = url.find("://") + 3;
        size_t colon = url.find(":", start);
        size_t slash = url.find("/", start);

        if (colon != std::string::npos) {
            alpaca_host_ = url.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ = std::stoi(url.substr(colon + 1, slash - colon - 1));
            } else {
                alpaca_port_ = std::stoi(url.substr(colon + 1));
            }
        }
        return true;
    }
    return false;
}

#ifdef _WIN32
auto HardwareInterface::connectToCOMDriver(const std::string& progId) -> bool {
    spdlog::info("Connecting to COM dome driver: {}", progId);

    com_prog_id_ = progId;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get CLSID from ProgID: {}", hr);
        return false;
    }

    hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                          IID_IDispatch, reinterpret_cast<void**>(&com_dome_));
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
        updateDomeCapabilities();
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromCOMDriver() -> bool {
    spdlog::info("Disconnecting from COM dome driver");

    if (com_dome_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);

        com_dome_->Release();
        com_dome_ = nullptr;
    }

    is_connected_.store(false);
    return true;
}

auto HardwareInterface::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM Chooser dialog
    return std::nullopt;
}

auto HardwareInterface::invokeCOMMethod(const std::string& method, VARIANT* params, int param_count) -> std::optional<VARIANT> {
    if (!com_dome_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR method_name(method.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &method_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {params, nullptr, param_count, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                           &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::getCOMProperty(const std::string& property) -> std::optional<VARIANT> {
    if (!com_dome_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET,
                           &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::setCOMProperty(const std::string& property, const VARIANT& value) -> bool {
    if (!com_dome_) {
        return false;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return false;
    }

    VARIANT params[] = {value};
    DISPID dispid_put = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = {params, &dispid_put, 1, 1};

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT,
                           &dispparams, nullptr, nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to set property {}: {}", property, hr);
        return false;
    }

    return true;
}
#endif

} // namespace lithium::ascom::dome::components
