/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Hardware Interface Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

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

namespace lithium::device::ascom::filterwheel::components {

HardwareInterface::HardwareInterface() {
    spdlog::debug("HardwareInterface constructor");
}

HardwareInterface::~HardwareInterface() {
    spdlog::debug("HardwareInterface destructor");
    shutdown();
}

auto HardwareInterface::initialize() -> bool {
    spdlog::info("Initializing ASCOM Hardware Interface");

    if (is_initialized_.load()) {
        spdlog::warn("Hardware interface already initialized");
        return true;
    }

#ifdef _WIN32
    if (!initializeCOM()) {
        setError("Failed to initialize COM");
        return false;
    }
#else
    if (!initializeAlpaca()) {
        setError("Failed to initialize Alpaca client");
        return false;
    }
#endif

    is_initialized_.store(true);
    spdlog::info("ASCOM Hardware Interface initialized successfully");
    return true;
}

auto HardwareInterface::shutdown() -> bool {
    spdlog::info("Shutting down ASCOM Hardware Interface");

    if (!is_initialized_.load()) {
        return true;
    }

    disconnect();

#ifdef _WIN32
    shutdownCOM();
#else
    shutdownAlpaca();
#endif

    is_initialized_.store(false);
    spdlog::info("ASCOM Hardware Interface shutdown completed");
    return true;
}

auto HardwareInterface::connect(const std::string& device_name) -> bool {
    spdlog::info("Connecting to ASCOM filterwheel device: {}", device_name);

    if (!is_initialized_.load()) {
        setError("Hardware interface not initialized");
        return false;
    }

    // Determine connection type based on device name format
    if (device_name.find("://") != std::string::npos) {
        // Alpaca REST API format
        size_t start = device_name.find("://") + 3;
        size_t colon = device_name.find(":", start);
        size_t slash = device_name.find("/", start);

        if (colon != std::string::npos) {
            alpaca_host_ = device_name.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ = std::stoi(device_name.substr(colon + 1, slash - colon - 1));
                // Extract device number from path
                size_t last_slash = device_name.find_last_of("/");
                if (last_slash != std::string::npos) {
                    alpaca_device_number_ = std::stoi(device_name.substr(last_slash + 1));
                }
            } else {
                alpaca_port_ = std::stoi(device_name.substr(colon + 1));
            }
        }

        connection_type_ = ConnectionType::ALPACA_REST;
        return connectToAlpaca(alpaca_host_, alpaca_port_, alpaca_device_number_);
    }

#ifdef _WIN32
    // Try as COM ProgID
    connection_type_ = ConnectionType::COM_DRIVER;
    return connectToCOM(device_name);
#else
    setError("COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto HardwareInterface::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Hardware Interface");

    if (!is_connected_.load()) {
        return true;
    }

    bool success = true;

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // Send disconnect command to Alpaca device
        auto response = sendAlpacaRequest("PUT", "connected", "Connected=false");
        success = response.has_value();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        // Disconnect COM interface
        success = setCOMProperty("Connected", "false");

        if (com_interface_) {
            com_interface_->Release();
            com_interface_ = nullptr;
        }
    }
#endif

    is_connected_.store(false);
    connection_type_ = ConnectionType::NONE;

    spdlog::info("ASCOM Hardware Interface disconnected");
    return success;
}

auto HardwareInterface::isConnected() const -> bool {
    return is_connected_.load();
}

auto HardwareInterface::scanDevices() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM filterwheel devices");

    std::vector<std::string> devices;

    // Add Alpaca discovery
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());

#ifdef _WIN32
    // Add COM driver enumeration
    // This would scan the Windows registry for ASCOM filterwheel drivers
    // Implementation would go here
#endif

    return devices;
}

auto HardwareInterface::discoverAlpacaDevices() -> std::vector<std::string> {
    spdlog::info("Discovering Alpaca filterwheel devices");
    std::vector<std::string> devices;

    // TODO: Implement Alpaca discovery protocol
    // For now, add default localhost entry
    devices.push_back("http://localhost:11111/api/v1/filterwheel/0");

    return devices;
}

auto HardwareInterface::getDeviceInfo() const -> std::optional<DeviceInfo> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    return device_info_;
}

auto HardwareInterface::getFilterCount() -> std::optional<int> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "names");
        if (response) {
            // TODO: Parse JSON array to get count
            return 8; // Default assumption
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Names");
        if (result) {
            // TODO: Parse SafeArray to get count
            return 8; // Default assumption
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getCurrentPosition() -> std::optional<int> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "position");
        if (response) {
            try {
                return std::stoi(*response);
            } catch (const std::exception& e) {
                setError("Failed to parse position response: " + std::string(e.what()));
            }
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Position");
        if (result) {
            // TODO: Convert VARIANT to int
            return 0; // Placeholder
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::setPosition(int position) -> bool {
    if (!is_connected_.load()) {
        setError("Not connected to device");
        return false;
    }

    spdlog::info("Setting filterwheel position to: {}", position);

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Position=" + std::to_string(position);
        auto response = sendAlpacaRequest("PUT", "position", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return setCOMProperty("Position", std::to_string(position));
    }
#endif

    return false;
}

auto HardwareInterface::isMoving() -> std::optional<bool> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    // Most ASCOM filterwheels don't have a separate "moving" property
    // Movement is typically fast and synchronous
    return false;
}

auto HardwareInterface::getFilterNames() -> std::optional<std::vector<std::string>> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "names");
        if (response) {
            // TODO: Parse JSON array of names
            return std::vector<std::string>{}; // Placeholder
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Names");
        if (result) {
            // TODO: Parse SafeArray of strings
            return std::vector<std::string>{}; // Placeholder
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getFilterName(int slot) -> std::optional<std::string> {
    auto names = getFilterNames();
    if (names && slot >= 0 && slot < static_cast<int>(names->size())) {
        return (*names)[slot];
    }
    return std::nullopt;
}

auto HardwareInterface::setFilterName(int slot, const std::string& name) -> bool {
    // ASCOM filterwheels typically don't support setting individual names
    // Names are usually set through the driver configuration
    setError("Setting individual filter names not supported by ASCOM standard");
    return false;
}

auto HardwareInterface::getTemperature() -> std::optional<double> {
    // Most ASCOM filterwheels don't have temperature sensors
    return std::nullopt;
}

auto HardwareInterface::hasTemperatureSensor() -> bool {
    return false;
}

auto HardwareInterface::getDriverInfo() -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "driverinfo");
        return response;
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("DriverInfo");
        if (result) {
            // TODO: Convert VARIANT to string
            return "COM Driver"; // Placeholder
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getDriverVersion() -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "driverversion");
        return response;
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("DriverVersion");
        if (result) {
            // TODO: Convert VARIANT to string
            return "1.0"; // Placeholder
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getInterfaceVersion() -> std::optional<int> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "interfaceversion");
        if (response) {
            try {
                return std::stoi(*response);
            } catch (const std::exception& e) {
                setError("Failed to parse interface version: " + std::string(e.what()));
            }
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("InterfaceVersion");
        if (result) {
            // TODO: Convert VARIANT to int
            return 2; // ASCOM standard interface version
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::setClientID(const std::string& client_id) -> bool {
    client_id_ = client_id;

    if (is_connected_.load()) {
        // Update client ID on connected device if supported
        if (connection_type_ == ConnectionType::COM_DRIVER) {
#ifdef _WIN32
            return setCOMProperty("ClientID", client_id);
#endif
        }
    }

    return true;
}

auto HardwareInterface::connectToCOM(const std::string& prog_id) -> bool {
#ifdef _WIN32
    spdlog::info("Connecting to COM filterwheel driver: {}", prog_id);

    // Implementation would use COM helper
    // For now, just set connected state
    is_connected_.store(true);
    device_info_.name = prog_id;
    device_info_.type = ConnectionType::COM_DRIVER;
    return true;
#else
    setError("COM not supported on this platform");
    return false;
#endif
}

auto HardwareInterface::connectToAlpaca(const std::string& host, int port, int device_number) -> bool {
    spdlog::info("Connecting to Alpaca filterwheel at {}:{} device {}", host, port, device_number);

    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = device_number;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        device_info_.name = host + ":" + std::to_string(port);
        device_info_.type = ConnectionType::ALPACA_REST;
        return true;
    }

    return false;
}

auto HardwareInterface::getConnectionType() const -> ConnectionType {
    return connection_type_;
}

auto HardwareInterface::getConnectionString() const -> std::string {
    switch (connection_type_) {
        case ConnectionType::COM_DRIVER:
            return "COM: " + device_info_.name;
        case ConnectionType::ALPACA_REST:
            return "Alpaca: " + alpaca_host_ + ":" + std::to_string(alpaca_port_);
        default:
            return "None";
    }
}

auto HardwareInterface::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto HardwareInterface::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

auto HardwareInterface::sendCommand(const std::string& command, const std::string& parameters) -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return sendAlpacaRequest("PUT", command, parameters);
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        // Use COM helper to invoke method
        return std::nullopt; // Placeholder
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getProperty(const std::string& property) -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return sendAlpacaRequest("GET", property);
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return getCOMProperty(property);
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::setProperty(const std::string& property, const std::string& value) -> bool {
    if (!is_connected_.load()) {
        return false;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", property, property + "=" + value);
        return response.has_value();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return setCOMProperty(property, value);
    }
#endif

    return false;
}

// Private implementation methods

#ifdef _WIN32
auto HardwareInterface::initializeCOM() -> bool {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        setError("Failed to initialize COM: " + std::to_string(hr));
        return false;
    }
    return true;
}

auto HardwareInterface::shutdownCOM() -> void {
    if (com_interface_) {
        com_interface_->Release();
        com_interface_ = nullptr;
    }
    CoUninitialize();
}

auto HardwareInterface::getCOMProperty(const std::string& property) -> std::optional<std::string> {
    // TODO: Implement COM property access
    return std::nullopt;
}

auto HardwareInterface::setCOMProperty(const std::string& property, const std::string& value) -> bool {
    // TODO: Implement COM property setting
    return false;
}
#endif

auto HardwareInterface::initializeAlpaca() -> bool {
#ifndef _WIN32
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    return true;
}

auto HardwareInterface::shutdownAlpaca() -> void {
#ifndef _WIN32
    curl_global_cleanup();
#endif
}

auto HardwareInterface::sendAlpacaRequest(const std::string& method, const std::string& endpoint, const std::string& params) -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    spdlog::debug("Sending Alpaca request: {} {} {}", method, endpoint, params);

    // Placeholder implementation
    if (endpoint == "connected" && method == "GET") {
        return "true";
    }

    return std::nullopt;
}

auto HardwareInterface::parseAlpacaResponse(const std::string& response) -> std::optional<std::string> {
    // TODO: Parse JSON response and extract value
    return response;
}

auto HardwareInterface::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("HardwareInterface error: {}", error);
}

auto HardwareInterface::validateConnection() -> bool {
    return is_connected_.load() && connection_type_ != ConnectionType::NONE;
}

auto HardwareInterface::updateDeviceInfo() -> bool {
    if (!is_connected_.load()) {
        return false;
    }

    // Update device information from connected device
    auto driver_info = getDriverInfo();
    if (driver_info) {
        device_info_.description = *driver_info;
    }

    auto driver_version = getDriverVersion();
    if (driver_version) {
        device_info_.version = *driver_version;
    }

    return true;
}

} // namespace lithium::device::ascom::filterwheel::components
