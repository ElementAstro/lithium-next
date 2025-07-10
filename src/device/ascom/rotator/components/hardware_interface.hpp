/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Hardware Interface Component

This component provides a clean interface to ASCOM Rotator APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <future>

#include <boost/asio/io_context.hpp>

// TODO: Fix C++20 compatibility issue with alpaca_client.hpp
// #include "../../alpaca_client.hpp"

// Forward declaration to avoid include dependency
namespace lithium::device::ascom {
    class AlpacaClient;
}

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
// clang-format on
#endif

namespace lithium::device::ascom::rotator::components {

/**
 * @brief Connection type enumeration
 */
enum class ConnectionType {
    COM_DRIVER,    // Windows COM/ASCOM driver
    ALPACA_REST    // ASCOM Alpaca REST protocol
};

/**
 * @brief ASCOM device information structure
 */
struct ASCOMDeviceInfo {
    std::string name;
    std::string description;
    std::string driverInfo;
    std::string driverVersion;
    std::string interfaceVersion;
    bool connected{false};
};

/**
 * @brief Rotator hardware capabilities
 */
struct RotatorCapabilities {
    bool canReverse{false};
    bool hasTemperatureSensor{false};
    bool canSetPosition{true};
    bool canSyncPosition{true};
    bool canAbort{true};
    double stepSize{1.0};
    double minPosition{0.0};
    double maxPosition{360.0};
};

/**
 * @brief Hardware Interface for ASCOM Rotator
 * 
 * This component handles low-level communication with ASCOM rotator devices,
 * supporting both Windows COM drivers and cross-platform Alpaca REST API.
 * It provides a clean interface that abstracts the underlying protocol.
 */
class HardwareInterface {
public:
    HardwareInterface();
    ~HardwareInterface();

    // Lifecycle management
    auto initialize() -> bool;
    auto destroy() -> bool;

    // Connection management
    auto connect(const std::string& deviceIdentifier, 
                ConnectionType type = ConnectionType::ALPACA_REST) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto reconnect() -> bool;

    // Device discovery
    auto scanDevices() -> std::vector<std::string>;
    auto discoverAlpacaDevices(const std::string& host = "localhost", 
                              int port = 11111) -> std::vector<ASCOMDeviceInfo>;

    // Device information
    auto getDeviceInfo() -> std::optional<ASCOMDeviceInfo>;
    auto getCapabilities() -> RotatorCapabilities;
    auto updateDeviceInfo() -> bool;

    // Low-level property access
    auto getProperty(const std::string& propertyName) -> std::optional<std::string>;
    auto setProperty(const std::string& propertyName, const std::string& value) -> bool;
    auto invokeMethod(const std::string& methodName, 
                     const std::vector<std::string>& parameters = {}) -> std::optional<std::string>;

    // Connection configuration
    auto setAlpacaConnection(const std::string& host, int port, int deviceNumber) -> void;
    auto getAlpacaConnection() const -> std::tuple<std::string, int, int>;
    auto setClientId(const std::string& clientId) -> bool;
    auto getClientId() const -> std::string;

#ifdef _WIN32
    // COM-specific methods
    auto connectCOMDriver(const std::string& progId) -> bool;
    auto disconnectCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
    auto getCOMInterface() -> IDispatch*;
#endif

    // Async operation support
    auto executeAsync(std::function<void()> operation) -> std::future<void>;
    auto getIOContext() -> boost::asio::io_context&;

    // Error handling
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Connection state
    ConnectionType connection_type_{ConnectionType::ALPACA_REST};
    std::atomic<bool> is_connected_{false};
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Device information
    ASCOMDeviceInfo device_info_;
    RotatorCapabilities capabilities_;
    std::string client_id_{"Lithium-Next"};
    mutable std::mutex device_mutex_;

    // Alpaca connection
    std::string alpaca_host_{"localhost"};
    int alpaca_port_{11111};
    int alpaca_device_number_{0};
    std::unique_ptr<lithium::device::ascom::AlpacaClient> alpaca_client_;

#ifdef _WIN32
    // COM interface
    IDispatch* com_rotator_{nullptr};
    std::string com_prog_id_;
#endif

    // Async operations
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::io_context::work> work_guard_;

    // Helper methods
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                          const std::string& params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;
    auto validateConnection() -> bool;
    auto setLastError(const std::string& error) -> void;

#ifdef _WIN32
    auto invokeCOMMethod(const std::string& method, VARIANT* params = nullptr, 
                        int param_count = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string& property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string& property, const VARIANT& value) -> bool;
    auto initializeCOM() -> bool;
    auto cleanupCOM() -> void;
#endif
};

} // namespace lithium::device::ascom::rotator::components
