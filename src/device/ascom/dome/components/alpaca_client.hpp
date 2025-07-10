/*
 * alpaca_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Alpaca Client for Dome Control

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <map>

namespace lithium::ascom::dome::components {

/**
 * @brief ASCOM Alpaca REST API Client for Dome Control
 * 
 * This class provides a REST client interface for communicating with
 * ASCOM Alpaca-compliant dome devices over HTTP/HTTPS.
 */
class AlpacaClient {
public:
    struct DeviceInfo {
        std::string name;
        std::string unique_id;
        std::string device_type;
        std::string interface_version;
        std::string driver_info;
        std::string driver_version;
        std::vector<std::string> supported_actions;
    };

    struct AlpacaDevice {
        std::string host;
        int port;
        int device_number;
        std::string device_name;
        std::string device_type;
        std::string uuid;
    };

    explicit AlpacaClient();
    ~AlpacaClient();

    // === Connection Management ===
    auto connect(const std::string& host, int port, int device_number) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;

    // === Device Discovery ===
    auto discoverDevices() -> std::vector<AlpacaDevice>;
    auto discoverDevices(const std::string& host, int port) -> std::vector<AlpacaDevice>;

    // === Device Information ===
    auto getDeviceInfo() -> std::optional<DeviceInfo>;
    auto getDriverInfo() -> std::optional<std::string>;
    auto getDriverVersion() -> std::optional<std::string>;
    auto getInterfaceVersion() -> std::optional<int>;
    auto getName() -> std::optional<std::string>;
    auto getUniqueId() -> std::optional<std::string>;

    // === Connection Properties ===
    auto getConnected() -> std::optional<bool>;
    auto setConnected(bool connected) -> bool;

    // === Dome-Specific Properties ===
    auto getAzimuth() -> std::optional<double>;
    auto setAzimuth(double azimuth) -> bool;
    auto getAtHome() -> std::optional<bool>;
    auto getAtPark() -> std::optional<bool>;
    auto getSlewing() -> std::optional<bool>;
    auto getShutterStatus() -> std::optional<int>;

    // === Dome Capabilities ===
    auto getCanFindHome() -> std::optional<bool>;
    auto getCanPark() -> std::optional<bool>;
    auto getCanSetAzimuth() -> std::optional<bool>;
    auto getCanSetPark() -> std::optional<bool>;
    auto getCanSetShutter() -> std::optional<bool>;
    auto getCanSlave() -> std::optional<bool>;
    auto getCanSyncAzimuth() -> std::optional<bool>;

    // === Dome Actions ===
    auto abortSlew() -> bool;
    auto closeShutter() -> bool;
    auto findHome() -> bool;
    auto openShutter() -> bool;
    auto park() -> bool;
    auto setElevation(double elevation) -> bool;
    auto slewToAzimuth(double azimuth) -> bool;
    auto syncToAzimuth(double azimuth) -> bool;

    // === Client Configuration ===
    auto setClientId(const std::string& client_id) -> bool;
    auto getClientId() -> std::optional<std::string>;
    auto setClientTransactionId(uint32_t transaction_id) -> void;
    auto getClientTransactionId() -> uint32_t;

    // === Error Handling ===
    auto getLastError() -> std::optional<std::string>;
    auto clearLastError() -> void;

    // === Advanced Operations ===
    auto sendCustomCommand(const std::string& action, const std::map<std::string, std::string>& parameters) -> std::optional<std::string>;
    auto getSupportedActions() -> std::vector<std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lithium::ascom::dome::components
