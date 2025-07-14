#pragma once

#include <optional>
#include <string>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief Connection management component for INDI telescopes
 *
 * Handles device connection, disconnection, and discovery
 */
class TelescopeConnection {
public:
    explicit TelescopeConnection(const std::string& name);
    ~TelescopeConnection() = default;

    /**
     * @brief Initialize connection component
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy connection component and cleanup resources
     */
    auto destroy() -> bool;

    /**
     * @brief Connect to telescope device
     * @param deviceName Name of the telescope device
     * @param timeout Connection timeout in seconds
     * @param maxRetry Maximum retry attempts
     * @return true if connection successful
     */
    auto connect(const std::string& deviceName, int timeout = 5, int maxRetry = 3) -> bool;

    /**
     * @brief Disconnect from telescope device
     */
    auto disconnect() -> bool;

    /**
     * @brief Scan for available telescope devices
     * @return Vector of available device names
     */
    auto scan() -> std::vector<std::string>;

    /**
     * @brief Check if telescope is connected
     */
    auto isConnected() const -> bool;

    /**
     * @brief Get current device name
     */
    auto getDeviceName() const -> std::string;

    /**
     * @brief Get INDI device object
     */
    auto getDevice() const -> INDI::BaseDevice;

    /**
     * @brief Set connection mode (Serial, TCP, etc.)
     */
    auto setConnectionMode(ConnectionMode mode) -> bool;

    /**
     * @brief Get current connection mode
     */
    auto getConnectionMode() const -> ConnectionMode;

    /**
     * @brief Set device port for serial connections
     */
    auto setDevicePort(const std::string& port) -> bool;

    /**
     * @brief Set baud rate for serial connections
     */
    auto setBaudRate(T_BAUD_RATE rate) -> bool;

    /**
     * @brief Enable/disable auto device search
     */
    auto setAutoSearch(bool enable) -> bool;

    /**
     * @brief Enable/disable debug mode
     */
    auto setDebugMode(bool enable) -> bool;

private:
    std::string name_;
    std::string deviceName_;
    std::atomic_bool isConnected_{false};
    ConnectionMode connectionMode_{ConnectionMode::SERIAL};
    std::string devicePort_;
    T_BAUD_RATE baudRate_{T_BAUD_RATE::B9600};
    bool deviceAutoSearch_{true};
    bool isDebug_{false};

    // INDI device reference
    INDI::BaseDevice device_;

    // Helper methods
    auto watchConnectionProperties() -> void;
    auto watchDriverInfo() -> void;
    auto watchDebugProperty() -> void;
};
