/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Hardware Interface Component

This component provides a clean interface to ASCOM Focuser APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <boost/asio/io_context.hpp>

#include "../../alpaca_client.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
// clang-format on
#endif

namespace lithium::device::ascom::focuser::components {

/**
 * @brief Connection type enumeration
 */
enum class ConnectionType {
    COM_DRIVER,    // Windows COM/ASCOM driver
    ALPACA_REST    // ASCOM Alpaca REST protocol
};

/**
 * @brief ASCOM Focuser states
 */
enum class ASCOMFocuserState {
    IDLE = 0,
    MOVING = 1,
    ERROR = 2
};

/**
 * @brief Hardware Interface for ASCOM Focuser communication
 *
 * This component encapsulates all direct interaction with ASCOM Focuser APIs,
 * providing a clean C++ interface for hardware operations while managing
 * both COM driver and Alpaca REST communication, device enumeration,
 * connection management, and low-level parameter control.
 */
class HardwareInterface {
public:
    struct FocuserInfo {
        std::string name;
        std::string serialNumber;
        std::string driverInfo;
        std::string driverVersion;
        int maxStep = 10000;
        int maxIncrement = 10000;
        double stepSize = 1.0;
        bool absolute = true;
        bool canHalt = true;
        bool tempCompAvailable = false;
        bool tempComp = false;
        double temperature = 0.0;
        double tempCompCoeff = 0.0;
        int interfaceVersion = 3;
    };

    struct ConnectionInfo {
        ConnectionType type = ConnectionType::ALPACA_REST;
        std::string deviceName;
        std::string progId;  // For COM connections
        std::string host = "localhost";
        int port = 11111;
        int deviceNumber = 0;
        std::string clientId = "Lithium-Next";
        int timeout = 5000;
    };

    // Constructor and destructor
    explicit HardwareInterface(const std::string& name);
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // =========================================================================
    // Connection Management
    // =========================================================================

    /**
     * @brief Initialize the hardware interface
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the hardware interface
     */
    auto destroy() -> bool;

    /**
     * @brief Connect to a focuser device
     */
    auto connect(const ConnectionInfo& info) -> bool;

    /**
     * @brief Disconnect from the focuser device
     */
    auto disconnect() -> bool;

    /**
     * @brief Check if connected to a focuser device
     */
    auto isConnected() const -> bool;

    /**
     * @brief Scan for available focuser devices
     */
    auto scan() -> std::vector<std::string>;

    // =========================================================================
    // Device Information
    // =========================================================================

    /**
     * @brief Get focuser information
     */
    auto getFocuserInfo() const -> FocuserInfo;

    /**
     * @brief Update focuser information from device
     */
    auto updateFocuserInfo() -> bool;

    // =========================================================================
    // Low-level Hardware Operations
    // =========================================================================

    /**
     * @brief Get current focuser position
     */
    auto getPosition() -> std::optional<int>;

    /**
     * @brief Move focuser to absolute position
     */
    auto moveToPosition(int position) -> bool;

    /**
     * @brief Move focuser by relative steps
     */
    auto moveSteps(int steps) -> bool;

    /**
     * @brief Check if focuser is currently moving
     */
    auto isMoving() -> bool;

    /**
     * @brief Halt focuser movement
     */
    auto halt() -> bool;

    /**
     * @brief Get focuser temperature
     */
    auto getTemperature() -> std::optional<double>;

    /**
     * @brief Get temperature compensation setting
     */
    auto getTemperatureCompensation() -> bool;

    /**
     * @brief Set temperature compensation setting
     */
    auto setTemperatureCompensation(bool enable) -> bool;

    /**
     * @brief Check if temperature compensation is available
     */
    auto hasTemperatureCompensation() -> bool;

    // =========================================================================
    // Alpaca-specific Operations
    // =========================================================================

    /**
     * @brief Discover Alpaca devices
     */
    auto discoverAlpacaDevices() -> std::vector<std::string>;

    /**
     * @brief Connect to Alpaca device
     */
    auto connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool;

    /**
     * @brief Disconnect from Alpaca device
     */
    auto disconnectFromAlpacaDevice() -> bool;

    /**
     * @brief Send Alpaca request
     */
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint,
                          const std::string& params = "") -> std::optional<std::string>;

    // =========================================================================
    // COM-specific Operations (Windows only)
    // =========================================================================

#ifdef _WIN32
    /**
     * @brief Connect to COM driver
     */
    auto connectToCOMDriver(const std::string& progId) -> bool;

    /**
     * @brief Disconnect from COM driver
     */
    auto disconnectFromCOMDriver() -> bool;

    /**
     * @brief Show ASCOM chooser dialog
     */
    auto showASCOMChooser() -> std::optional<std::string>;

    /**
     * @brief Invoke COM method
     */
    auto invokeCOMMethod(const std::string& method, VARIANT* params = nullptr,
                        int paramCount = 0) -> std::optional<VARIANT>;

    /**
     * @brief Get COM property
     */
    auto getCOMProperty(const std::string& property) -> std::optional<VARIANT>;

    /**
     * @brief Set COM property
     */
    auto setCOMProperty(const std::string& property, const VARIANT& value) -> bool;
#endif

    // =========================================================================
    // Error Handling
    // =========================================================================

    /**
     * @brief Get last error message
     */
    auto getLastError() const -> std::string;

    /**
     * @brief Clear last error
     */
    auto clearError() -> void;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using ErrorCallback = std::function<void(const std::string& error)>;
    using StateChangeCallback = std::function<void(ASCOMFocuserState newState)>;

    /**
     * @brief Set error callback
     */
    auto setErrorCallback(ErrorCallback callback) -> void;

    /**
     * @brief Set state change callback
     */
    auto setStateChangeCallback(StateChangeCallback callback) -> void;

private:
    // Private members
    std::string name_;
    std::atomic<bool> connected_{false};
    std::atomic<ASCOMFocuserState> state_{ASCOMFocuserState::IDLE};

    FocuserInfo focuser_info_;
    ConnectionInfo connection_info_;
    std::string last_error_;

    mutable std::mutex interface_mutex_;

    // Callbacks
    ErrorCallback error_callback_;
    StateChangeCallback state_change_callback_;

    // Connection-specific data
    std::unique_ptr<lithium::device::ascom::FocuserClient> alpaca_client_;

#ifdef _WIN32
    IDispatch* com_focuser_{nullptr};
#endif

    // Helper methods
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;
    auto setError(const std::string& error) -> void;
    auto setState(ASCOMFocuserState newState) -> void;
    auto validateConnection() -> bool;

    // Alpaca helpers
    auto buildAlpacaUrl(const std::string& endpoint) -> std::string;
    auto executeAlpacaRequest(const std::string& method, const std::string& url,
                             const std::string& params) -> std::optional<std::string>;

#ifdef _WIN32
    // COM helpers
    auto initializeCOM() -> bool;
    auto cleanupCOM() -> void;
    auto variantToString(const VARIANT& var) -> std::string;
    auto stringToVariant(const std::string& str) -> VARIANT;
#endif
};

} // namespace lithium::device::ascom::focuser::components
