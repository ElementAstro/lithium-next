/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <future>
#include <thread>

#ifdef _WIN32
#include "../../ascom_com_helper.hpp"
#endif

namespace lithium::device::ascom::rotator::components {

HardwareInterface::HardwareInterface() {
    spdlog::debug("HardwareInterface constructor called");
    io_context_ = std::make_unique<boost::asio::io_context>();
    work_guard_ = std::make_unique<boost::asio::io_context::work>(*io_context_);
}

HardwareInterface::~HardwareInterface() {
    spdlog::debug("HardwareInterface destructor called");
    disconnect();
    
    if (work_guard_) {
        work_guard_.reset();
    }
    
    if (io_context_) {
        io_context_->stop();
    }

#ifdef _WIN32
    cleanupCOM();
#endif
}

auto HardwareInterface::initialize() -> bool {
    spdlog::info("Initializing ASCOM Rotator Hardware Interface");
    
    clearLastError();
    
#ifdef _WIN32
    if (!initializeCOM()) {
        setLastError("Failed to initialize COM");
        return false;
    }
#endif

    // Initialize Alpaca client
    try {
        alpaca_client_ = std::make_unique<lithium::device::ascom::AlpacaClient>(
            alpaca_host_, alpaca_port_);
    } catch (const std::exception& e) {
        setLastError("Failed to create Alpaca client: " + std::string(e.what()));
        spdlog::warn("Failed to create Alpaca client: {}", e.what());
        // Continue initialization - we can still try COM connections
    }
    
    spdlog::info("Hardware Interface initialized successfully");
    return true;
}

auto HardwareInterface::destroy() -> bool {
    spdlog::info("Destroying ASCOM Rotator Hardware Interface");
    
    disconnect();
    
    if (alpaca_client_) {
        alpaca_client_.reset();
    }
    
#ifdef _WIN32
    cleanupCOM();
#endif
    
    return true;
}

auto HardwareInterface::connect(const std::string& deviceIdentifier, ConnectionType type) -> bool {
    spdlog::info("Connecting to ASCOM rotator device: {} (type: {})", 
                 deviceIdentifier, static_cast<int>(type));
    
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (is_connected_.load()) {
        spdlog::warn("Already connected to a device");
        return true;
    }
    
    clearLastError();
    connection_type_ = type;
    
    bool success = false;
    
    if (type == ConnectionType::ALPACA_REST) {
        // Parse Alpaca device identifier (format: "host:port/device_number" or just device name)
        if (deviceIdentifier.find("://") != std::string::npos || 
            deviceIdentifier.find(":") != std::string::npos) {
            // Parse URL-like identifier
            // For simplicity, assume localhost:11111/0 format
            success = connectAlpacaDevice(alpaca_host_, alpaca_port_, alpaca_device_number_);
        } else {
            // Treat as device name, use default connection
            success = connectAlpacaDevice(alpaca_host_, alpaca_port_, alpaca_device_number_);
        }
    }
#ifdef _WIN32
    else if (type == ConnectionType::COM_DRIVER) {
        success = connectCOMDriver(deviceIdentifier);
    }
#endif
    else {
        setLastError("Unsupported connection type");
        return false;
    }
    
    if (success) {
        is_connected_.store(true);
        device_info_.name = deviceIdentifier;
        device_info_.connected = true;
        updateDeviceInfo();
        spdlog::info("Successfully connected to rotator device");
    } else {
        spdlog::error("Failed to connect to rotator device: {}", getLastError());
    }
    
    return success;
}

auto HardwareInterface::disconnect() -> bool {
    spdlog::info("Disconnecting from ASCOM rotator device");
    
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!is_connected_.load()) {
        return true;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        disconnectAlpacaDevice();
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        disconnectCOMDriver();
    }
#endif
    
    is_connected_.store(false);
    device_info_.connected = false;
    
    spdlog::info("Disconnected from rotator device");
    return true;
}

auto HardwareInterface::isConnected() const -> bool {
    return is_connected_.load();
}

auto HardwareInterface::reconnect() -> bool {
    spdlog::info("Reconnecting to ASCOM rotator device");
    
    std::string device_name = device_info_.name;
    ConnectionType type = connection_type_;
    
    disconnect();
    
    return connect(device_name, type);
}

auto HardwareInterface::scanDevices() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM rotator devices");
    
    std::vector<std::string> devices;
    
#ifdef _WIN32
    // Scan Windows registry for ASCOM Rotator drivers
    // TODO: Implement registry scanning for COM drivers
    // For now, add some common rotator drivers
    devices.push_back("ASCOM.Simulator.Rotator");
#endif

    // Scan for Alpaca devices
    try {
        auto alpacaDevices = discoverAlpacaDevices();
        for (const auto& device : alpacaDevices) {
            devices.push_back(device.name);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to discover Alpaca devices: {}", e.what());
    }
    
    spdlog::info("Found {} rotator devices", devices.size());
    return devices;
}

auto HardwareInterface::discoverAlpacaDevices(const std::string& host, int port) 
    -> std::vector<ASCOMDeviceInfo> {
    std::vector<ASCOMDeviceInfo> devices;
    
    if (!alpaca_client_) {
        spdlog::warn("Alpaca client not initialized");
        return devices;
    }
    
    // TODO: Implement Alpaca device discovery
    // This would involve querying the management API endpoints
    
    return devices;
}

auto HardwareInterface::getDeviceInfo() -> std::optional<ASCOMDeviceInfo> {
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    if (!is_connected_.load()) {
        return std::nullopt;
    }
    
    return device_info_;
}

auto HardwareInterface::getCapabilities() -> RotatorCapabilities {
    if (!is_connected_.load()) {
        return RotatorCapabilities{};
    }
    
    // Update capabilities from device if needed
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // Query Alpaca properties to update capabilities
        auto canReverse = getProperty("canreverse");
        if (canReverse) {
            capabilities_.canReverse = (*canReverse == "true");
        }
    }
    
    return capabilities_;
}

auto HardwareInterface::updateDeviceInfo() -> bool {
    if (!is_connected_.load()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(device_mutex_);
    
    try {
        // Get basic device information
        auto description = getProperty("description");
        if (description) {
            device_info_.description = *description;
        }
        
        auto driverInfo = getProperty("driverinfo");
        if (driverInfo) {
            device_info_.driverInfo = *driverInfo;
        }
        
        auto driverVersion = getProperty("driverversion");
        if (driverVersion) {
            device_info_.driverVersion = *driverVersion;
        }
        
        auto interfaceVersion = getProperty("interfaceversion");
        if (interfaceVersion) {
            device_info_.interfaceVersion = *interfaceVersion;
        }
        
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to update device info: " + std::string(e.what()));
        return false;
    }
}

auto HardwareInterface::getProperty(const std::string& propertyName) -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return sendAlpacaRequest("GET", propertyName);
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty(propertyName);
        if (result) {
            // Convert VARIANT to string
            // TODO: Implement VARIANT to string conversion
            return std::string(""); // Placeholder
        }
    }
#endif
    
    return std::nullopt;
}

auto HardwareInterface::setProperty(const std::string& propertyName, const std::string& value) -> bool {
    if (!is_connected_.load()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = propertyName + "=" + value;
        auto response = sendAlpacaRequest("PUT", propertyName, params);
        return response.has_value();
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_BSTR;
        var.bstrVal = SysAllocString(std::wstring(value.begin(), value.end()).c_str());
        
        bool result = setCOMProperty(propertyName, var);
        VariantClear(&var);
        return result;
    }
#endif
    
    return false;
}

auto HardwareInterface::invokeMethod(const std::string& methodName, 
                                    const std::vector<std::string>& parameters) -> std::optional<std::string> {
    if (!is_connected_.load()) {
        return std::nullopt;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params;
        for (size_t i = 0; i < parameters.size(); ++i) {
            if (i > 0) params += "&";
            params += "param" + std::to_string(i) + "=" + parameters[i];
        }
        return sendAlpacaRequest("PUT", methodName, params);
    }
#ifdef _WIN32
    else if (connection_type_ == ConnectionType::COM_DRIVER) {
        // TODO: Implement COM method invocation
        return std::nullopt;
    }
#endif
    
    return std::nullopt;
}

auto HardwareInterface::setAlpacaConnection(const std::string& host, int port, int deviceNumber) -> void {
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;
    
    // Recreate Alpaca client with new settings
    if (alpaca_client_) {
        alpaca_client_ = std::make_unique<lithium::device::ascom::AlpacaClient>(host, port);
    }
}

auto HardwareInterface::getAlpacaConnection() const -> std::tuple<std::string, int, int> {
    return std::make_tuple(alpaca_host_, alpaca_port_, alpaca_device_number_);
}

auto HardwareInterface::setClientId(const std::string& clientId) -> bool {
    client_id_ = clientId;
    return true;
}

auto HardwareInterface::getClientId() const -> std::string {
    return client_id_;
}

auto HardwareInterface::executeAsync(std::function<void()> operation) -> std::future<void> {
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();
    
    io_context_->post([operation, promise]() {
        try {
            operation();
            promise->set_value();
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    });
    
    return future;
}

auto HardwareInterface::getIOContext() -> boost::asio::io_context& {
    return *io_context_;
}

auto HardwareInterface::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto HardwareInterface::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private helper methods

auto HardwareInterface::sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                                         const std::string& params) -> std::optional<std::string> {
    if (!alpaca_client_) {
        setLastError("Alpaca client not initialized");
        return std::nullopt;
    }
    
    try {
        // Construct the full URL path
        std::string path = "/api/v1/rotator/" + std::to_string(alpaca_device_number_) + "/" + endpoint;
        
        // TODO: Use actual Alpaca client implementation
        // For now, return a placeholder
        return std::string("{}"); // Empty JSON response
    } catch (const std::exception& e) {
        setLastError("Alpaca request failed: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto HardwareInterface::parseAlpacaResponse(const std::string& response) -> std::optional<std::string> {
    // TODO: Implement JSON response parsing
    // Should check for errors and extract the value
    return response;
}

auto HardwareInterface::validateConnection() -> bool {
    if (!is_connected_.load()) {
        return false;
    }
    
    // Try to get a basic property to validate connection
    auto connected = getProperty("connected");
    return connected && (*connected == "true");
}

auto HardwareInterface::setLastError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
}

auto HardwareInterface::connectAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool {
    try {
        if (!alpaca_client_) {
            alpaca_client_ = std::make_unique<lithium::device::ascom::AlpacaClient>(host, port);
        }
        
        // Test connection by setting connected property
        if (!setProperty("connected", "true")) {
            setLastError("Failed to connect to Alpaca device");
            return false;
        }
        
        // Verify connection
        auto connected = getProperty("connected");
        if (!connected || *connected != "true") {
            setLastError("Device connection verification failed");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        setLastError("Alpaca connection failed: " + std::string(e.what()));
        return false;
    }
}

auto HardwareInterface::disconnectAlpacaDevice() -> bool {
    try {
        setProperty("connected", "false");
        return true;
    } catch (const std::exception& e) {
        setLastError("Alpaca disconnection failed: " + std::string(e.what()));
        return false;
    }
}

#ifdef _WIN32

auto HardwareInterface::connectCOMDriver(const std::string& progId) -> bool {
    com_prog_id_ = progId;
    
    // TODO: Implement COM driver connection
    // This involves creating COM instance and connecting
    setLastError("COM driver connection not yet implemented");
    return false;
}

auto HardwareInterface::disconnectCOMDriver() -> bool {
    if (com_rotator_) {
        com_rotator_->Release();
        com_rotator_ = nullptr;
    }
    return true;
}

auto HardwareInterface::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    setLastError("ASCOM chooser not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::getCOMInterface() -> IDispatch* {
    return com_rotator_;
}

auto HardwareInterface::invokeCOMMethod(const std::string& method, VARIANT* params, int param_count) 
    -> std::optional<VARIANT> {
    // TODO: Implement COM method invocation
    return std::nullopt;
}

auto HardwareInterface::getCOMProperty(const std::string& property) -> std::optional<VARIANT> {
    // TODO: Implement COM property getter
    return std::nullopt;
}

auto HardwareInterface::setCOMProperty(const std::string& property, const VARIANT& value) -> bool {
    // TODO: Implement COM property setter
    return false;
}

auto HardwareInterface::initializeCOM() -> bool {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("Failed to initialize COM: 0x{:08x}", hr);
        return false;
    }
    return true;
}

auto HardwareInterface::cleanupCOM() -> void {
    if (com_rotator_) {
        com_rotator_->Release();
        com_rotator_ = nullptr;
    }
    CoUninitialize();
}

#endif

} // namespace lithium::device::ascom::rotator::components
