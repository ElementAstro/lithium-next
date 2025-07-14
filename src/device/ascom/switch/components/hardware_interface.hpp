/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Hardware Interface Component

This component handles low-level communication with ASCOM switch devices,
supporting both COM drivers and Alpaca REST API.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

namespace lithium::device::ascom::sw::components {

enum class ConnectionType {
    COM_DRIVER,
    ALPACA_REST
};

/**
 * @brief Switch information from ASCOM device
 */
struct ASCOMSwitchInfo {
    std::string name;
    std::string description;
    bool can_write{false};
    double min_value{0.0};
    double max_value{1.0};
    double step_value{1.0};
    bool state{false};
    double value{0.0};
};

/**
 * @brief Hardware Interface Component for ASCOM Switch
 *
 * This component encapsulates all hardware communication details,
 * providing a clean interface for the controller to interact with
 * physical switch devices.
 */
class HardwareInterface {
public:
    explicit HardwareInterface();
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // =========================================================================
    // Connection Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto scan() -> std::vector<std::string>;

    // =========================================================================
    // Device Information
    // =========================================================================

    auto getDriverInfo() -> std::optional<std::string>;
    auto getDriverVersion() -> std::optional<std::string>;
    auto getInterfaceVersion() -> std::optional<int>;
    auto getDeviceName() const -> std::string;
    auto getConnectionType() const -> ConnectionType;

    // =========================================================================
    // Switch Operations
    // =========================================================================

    auto getSwitchCount() -> uint32_t;
    auto getSwitchInfo(uint32_t index) -> std::optional<ASCOMSwitchInfo>;
    auto setSwitchState(uint32_t index, bool state) -> bool;
    auto getSwitchState(uint32_t index) -> std::optional<bool>;
    auto getSwitchValue(uint32_t index) -> std::optional<double>;
    auto setSwitchValue(uint32_t index, double value) -> bool;

    // =========================================================================
    // Advanced Features
    // =========================================================================

    auto setClientID(const std::string& clientId) -> bool;
    auto getClientID() -> std::optional<std::string>;
    auto enablePolling(bool enable, uint32_t intervalMs = 1000) -> bool;
    auto isPollingEnabled() const -> bool;

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using StateChangeCallback = std::function<void(uint32_t index, bool state)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    void setStateChangeCallback(std::function<void(uint32_t index, bool state)> callback);
    void setErrorCallback(std::function<void(const std::string& error)> callback);
    void setConnectionCallback(std::function<void(bool connected)> callback);

private:
    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> initialized_{false};
    ConnectionType connection_type_{ConnectionType::ALPACA_REST};

    // Device information
    std::string device_name_;
    std::string driver_info_;
    std::string driver_version_;
    std::string client_id_{"Lithium-Next"};
    int interface_version_{2};

    // Alpaca connection details
    std::string alpaca_host_{"localhost"};
    int alpaca_port_{11111};
    int alpaca_device_number_{0};

#ifdef _WIN32
    // COM object for Windows ASCOM drivers
    IDispatch* com_switch_{nullptr};
    std::string com_prog_id_;
#endif

    // Switch properties cache
    uint32_t switch_count_{0};
    std::vector<ASCOMSwitchInfo> switches_;
    mutable std::mutex switches_mutex_;

    // Polling mechanism
    std::atomic<bool> polling_enabled_{false};
    std::atomic<uint32_t> polling_interval_ms_{1000};
    std::unique_ptr<std::thread> polling_thread_;
    std::atomic<bool> stop_polling_{false};
    std::condition_variable polling_cv_;
    std::mutex polling_mutex_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    StateChangeCallback state_change_callback_;
    ErrorCallback error_callback_;
    ConnectionCallback connection_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods - Connection
    // =========================================================================

    auto connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;
    auto discoverAlpacaDevices() -> std::vector<std::string>;

#ifdef _WIN32
    auto connectToCOMDriver(const std::string& progId) -> bool;
    auto disconnectFromCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

    // =========================================================================
    // Internal Methods - Communication
    // =========================================================================

    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                          const std::string& params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;

#ifdef _WIN32
    auto invokeCOMMethod(const std::string& method, VARIANT* params = nullptr,
                        int param_count = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string& property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string& property, const VARIANT& value) -> bool;
#endif

    // =========================================================================
    // Internal Methods - Data Management
    // =========================================================================

    auto updateSwitchInfo() -> bool;
    auto validateSwitchIndex(uint32_t index) const -> bool;
    auto setLastError(const std::string& error) const -> void;

    // =========================================================================
    // Internal Methods - Polling
    // =========================================================================

    auto startPolling() -> void;
    auto stopPolling() -> void;
    auto pollingLoop() -> void;

    // =========================================================================
    // Internal Methods - Callbacks
    // =========================================================================

    auto notifyStateChange(uint32_t index, bool state) -> void;
    auto notifyError(const std::string& error) -> void;
    auto notifyConnectionChange(bool connected) -> void;
};

// Exception classes for hardware interface
class HardwareInterfaceException : public std::runtime_error {
public:
    explicit HardwareInterfaceException(const std::string& message) : std::runtime_error(message) {}
};

class CommunicationException : public HardwareInterfaceException {
public:
    explicit CommunicationException(const std::string& message)
        : HardwareInterfaceException("Communication error: " + message) {}
};

} // namespace lithium::device::ascom::sw::components
