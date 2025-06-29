/*
 * dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Dome Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/dome.hpp"

class ASCOMDome : public AtomDome {
public:
    explicit ASCOMDome(std::string name);
    ~ASCOMDome() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Dome state
    auto isMoving() const -> bool override;
    auto isParked() const -> bool override;

    // Azimuth control
    auto getAzimuth() -> std::optional<double> override;
    auto setAzimuth(double azimuth) -> bool override;
    auto moveToAzimuth(double azimuth) -> bool override;
    auto rotateClockwise() -> bool override;
    auto rotateCounterClockwise() -> bool override;
    auto stopRotation() -> bool override;
    auto abortMotion() -> bool override;
    auto syncAzimuth(double azimuth) -> bool override;

    // Parking
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto getParkPosition() -> std::optional<double> override;
    auto setParkPosition(double azimuth) -> bool override;
    auto canPark() -> bool override;

    // Shutter control
    auto openShutter() -> bool override;
    auto closeShutter() -> bool override;
    auto abortShutter() -> bool override;
    auto getShutterState() -> ShutterState override;
    auto hasShutter() -> bool override;

    // Speed control
    auto getRotationSpeed() -> std::optional<double> override;
    auto setRotationSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Telescope coordination
    auto followTelescope(bool enable) -> bool override;
    auto isFollowingTelescope() -> bool override;
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double override;
    auto setTelescopePosition(double az, double alt) -> bool override;

    // Home position
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;
    auto getHomePosition() -> std::optional<double> override;

    // Backlash compensation
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Weather monitoring
    auto canOpenShutter() -> bool override;
    auto isSafeToOperate() -> bool override;
    auto getWeatherStatus() -> std::string override;

    // Statistics
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getShutterOperations() -> uint64_t override;
    auto resetShutterOperations() -> bool override;

    // Presets
    auto savePreset(int slot, double azimuth) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // ASCOM Dome-specific properties
    auto canFindHome() -> bool;
    auto canSetAzimuth() -> bool;
    auto canSetPark() -> bool;
    auto canSetShutter() -> bool;
    auto canSlave() -> bool;
    auto canSyncAzimuth() -> bool;

    // Alpaca discovery and connection
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;

    // ASCOM COM object connection (Windows only)
#ifdef _WIN32
    auto connectToCOMDriver(const std::string &progId) -> bool;
    auto disconnectFromCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

protected:
    // Connection management
    enum class ConnectionType {
        COM_DRIVER,
        ALPACA_REST
    } connection_type_{ConnectionType::ALPACA_REST};

    // Device state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_moving_{false};
    std::atomic<bool> is_parked_{false};
    std::atomic<bool> is_slaved_{false};
    std::atomic<double> current_azimuth_{0.0};

    // ASCOM device information
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
    IDispatch* com_dome_{nullptr};
    std::string com_prog_id_;
#endif

    // Dome capabilities cache
    struct ASCOMDomeCapabilities {
        bool can_find_home{false};
        bool can_park{false};
        bool can_set_azimuth{false};
        bool can_set_park{false};
        bool can_set_shutter{false};
        bool can_slave{false};
        bool can_sync_azimuth{false};
    } ascom_capabilities_;

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateDomeCapabilities() -> bool;
    auto startMonitoring() -> void;
    auto stopMonitoring() -> void;
    auto monitoringLoop() -> void;

#ifdef _WIN32
    auto invokeCOMMethod(const std::string &method, VARIANT* params = nullptr,
                        int param_count = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string &property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string &property, const VARIANT &value) -> bool;
#endif
};
