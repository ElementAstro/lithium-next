/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Hardware Interface Component

This component handles the low-level communication with ASCOM filterwheel
devices, supporting both COM and Alpaca interfaces.

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

namespace lithium::device::ascom::filterwheel::components {

// Connection type enumeration
enum class ConnectionType {
    NONE,
    COM_DRIVER,
    ALPACA_REST
};

// Device information structure
struct DeviceInfo {
    std::string name;
    std::string version;
    std::string description;
    ConnectionType type;
    std::string connection_string;
};

/**
 * @brief Hardware Interface for ASCOM Filter Wheels
 *
 * This component abstracts the communication with ASCOM filterwheel devices,
 * supporting both Windows COM drivers and Alpaca REST API.
 */
class HardwareInterface {
public:
    HardwareInterface();
    ~HardwareInterface();

    // Connection management
    auto initialize() -> bool;
    auto shutdown() -> bool;
    auto connect(const std::string& device_name) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;

    // Device discovery
    auto scanDevices() -> std::vector<std::string>;
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto getDeviceInfo() const -> std::optional<DeviceInfo>;

    // Basic properties
    auto getFilterCount() -> std::optional<int>;
    auto getCurrentPosition() -> std::optional<int>;
    auto setPosition(int position) -> bool;
    auto isMoving() -> std::optional<bool>;
    
    // Filter names
    auto getFilterNames() -> std::optional<std::vector<std::string>>;
    auto getFilterName(int slot) -> std::optional<std::string>;
    auto setFilterName(int slot, const std::string& name) -> bool;

    // Temperature (if supported)
    auto getTemperature() -> std::optional<double>;
    auto hasTemperatureSensor() -> bool;

    // ASCOM specific properties
    auto getDriverInfo() -> std::optional<std::string>;
    auto getDriverVersion() -> std::optional<std::string>;
    auto getInterfaceVersion() -> std::optional<int>;
    auto setClientID(const std::string& client_id) -> bool;

    // Connection type specific methods
    auto connectToCOM(const std::string& prog_id) -> bool;
    auto connectToAlpaca(const std::string& host, int port, int device_number) -> bool;
    auto getConnectionType() const -> ConnectionType;
    auto getConnectionString() const -> std::string;

    // Error handling
    auto getLastError() const -> std::string;
    auto clearError() -> void;

    // Utility methods
    auto sendCommand(const std::string& command, const std::string& parameters = "") -> std::optional<std::string>;
    auto getProperty(const std::string& property) -> std::optional<std::string>;
    auto setProperty(const std::string& property, const std::string& value) -> bool;

private:
    // Connection state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_initialized_{false};
    ConnectionType connection_type_{ConnectionType::NONE};
    
    // Device information
    DeviceInfo device_info_;
    std::string client_id_{"Lithium-Next"};
    
    // Alpaca connection details
    std::string alpaca_host_;
    int alpaca_port_{11111};
    int alpaca_device_number_{0};
    
    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

#ifdef _WIN32
    // COM interface
    IDispatch* com_interface_{nullptr};
    std::string com_prog_id_;
    
    // COM helper methods
    auto initializeCOM() -> bool;
    auto shutdownCOM() -> void;
    auto invokeCOMMethod(const std::string& method, const std::vector<std::string>& params = {}) -> std::optional<std::string>;
    auto getCOMProperty(const std::string& property) -> std::optional<std::string>;
    auto setCOMProperty(const std::string& property, const std::string& value) -> bool;
#endif

    // Alpaca REST API methods
    auto initializeAlpaca() -> bool;
    auto shutdownAlpaca() -> void;
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint, const std::string& params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;
    
    // Utility methods
    auto setError(const std::string& error) -> void;
    auto validateConnection() -> bool;
    auto updateDeviceInfo() -> bool;
};

} // namespace lithium::device::ascom::filterwheel::components
