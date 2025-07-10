/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Hardware Interface Component

*************************************************/

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <atomic>
#include <vector>

namespace lithium::ascom::dome::components {

/**
 * @brief Hardware Interface for ASCOM Dome
 * 
 * This component provides a low-level hardware abstraction layer for 
 * communicating with the physical dome device through either ASCOM COM 
 * drivers or Alpaca REST API.
 */
class HardwareInterface {
public:
    enum class ConnectionType {
        COM_DRIVER,
        ALPACA_REST
    };

    enum class HardwareStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };

    explicit HardwareInterface();
    virtual ~HardwareInterface();

    // === Lifecycle Management ===
    auto initialize() -> bool;
    auto destroy() -> bool;
    auto scan() -> std::vector<std::string>;

    // === Connection Management ===
    auto connect(const std::string& deviceName, ConnectionType type, int timeout = 30) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto getConnectionType() const -> ConnectionType;
    auto getHardwareStatus() const -> HardwareStatus;

    // === Raw Hardware Commands ===
    auto sendRawCommand(const std::string& command, const std::string& parameters = "") -> std::optional<std::string>;
    auto getRawProperty(const std::string& property) -> std::optional<std::string>;
    auto setRawProperty(const std::string& property, const std::string& value) -> bool;

    // === Dome Hardware Capabilities ===
    auto updateCapabilities() -> bool;
    auto getDomeCapabilities() -> std::optional<std::string>;
    auto canFindHome() const -> bool;
    auto canPark() const -> bool;
    auto canSetAzimuth() const -> bool;
    auto canSetPark() const -> bool;
    auto canSetShutter() const -> bool;
    auto canSlave() const -> bool;
    auto canSyncAzimuth() const -> bool;

    // === Basic Dome Properties ===
    auto getAzimuthRaw() -> std::optional<double>;
    auto setAzimuthRaw(double azimuth) -> bool;
    auto getIsMoving() -> std::optional<bool>;
    auto getIsParked() -> std::optional<bool>;
    auto getIsSlewing() -> std::optional<bool>;

    // === Shutter Hardware Interface ===
    auto getShutterStatus() -> std::optional<int>;
    auto openShutterRaw() -> bool;
    auto closeShutterRaw() -> bool;
    auto abortShutterRaw() -> bool;

    // === Motion Control ===
    auto slewToAzimuthRaw(double azimuth) -> bool;
    auto abortSlewRaw() -> bool;
    auto syncAzimuthRaw(double azimuth) -> bool;
    auto parkRaw() -> bool;
    auto unparkRaw() -> bool;
    auto findHomeRaw() -> bool;

    // === Device Information ===
    auto getDriverInfo() -> std::optional<std::string>;
    auto getDriverVersion() -> std::optional<std::string>;
    auto getInterfaceVersion() -> std::optional<int>;
    auto getDeviceName() -> std::optional<std::string>;
    
    // === Alpaca Connection Info ===
    auto getAlpacaHost() const -> std::string;
    auto getAlpacaPort() const -> int;
    auto getAlpacaDeviceNumber() const -> int;

    // === Error Handling ===
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;
    auto hasError() const -> bool;

protected:
    // === Internal State ===
    std::atomic<bool> is_connected_{false};
    std::atomic<ConnectionType> connection_type_{ConnectionType::ALPACA_REST};
    std::atomic<HardwareStatus> hardware_status_{HardwareStatus::DISCONNECTED};
    
    // === Capability Cache ===
    struct Capabilities {
        bool can_find_home{false};
        bool can_park{false};
        bool can_set_azimuth{false};
        bool can_set_park{false};
        bool can_set_shutter{false};
        bool can_slave{false};
        bool can_sync_azimuth{false};
        bool capabilities_loaded{false};
    } capabilities_;

    // === Error State ===
    mutable std::string last_error_;
    std::atomic<bool> has_error_{false};

    // === Device Information ===
    std::string device_name_;
    std::string driver_info_;
    std::string driver_version_;
    int interface_version_{2};

    // === Alpaca Connection Details ===
    std::string alpaca_host_;
    int alpaca_port_{11111};
    int alpaca_device_number_{0};

    // === Connection-specific implementations ===
    virtual auto connectToAlpaca(const std::string& host, int port, int deviceNumber) -> bool;
    virtual auto connectToCOM(const std::string& progId) -> bool;
    virtual auto disconnectFromAlpaca() -> bool;
    virtual auto disconnectFromCOM() -> bool;

    // === Error handling helpers ===
    auto setError(const std::string& error) -> void;
    auto validateConnection() const -> bool;
    auto parseAlpacaUrl(const std::string& url) -> bool;

    // === Hardware-specific command implementations ===
    virtual auto sendAlpacaCommand(const std::string& endpoint, const std::string& method, 
                                  const std::string& params = "") -> std::optional<std::string>;
    virtual auto sendCOMCommand(const std::string& method, const std::string& params = "") -> std::optional<std::string>;
    
    // === Alpaca-specific helpers ===
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint, const std::string& params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto connectToAlpacaDevice(const std::string& host, int port, int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;
    auto updateDomeCapabilities() -> bool;
};

} // namespace lithium::ascom::dome::components
