/*
 * rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Rotator Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/rotator.hpp"

class ASCOMRotator : public AtomRotator {
public:
    explicit ASCOMRotator(std::string name);
    ~ASCOMRotator() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Rotator state
    auto isMoving() const -> bool override;

    // Position control
    auto getPosition() -> std::optional<double> override;
    auto setPosition(double angle) -> bool override;
    auto moveToAngle(double angle) -> bool override;
    auto rotateByAngle(double angle) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(double angle) -> bool override;

    // Direction control
    auto getDirection() -> std::optional<RotatorDirection> override;
    auto setDirection(RotatorDirection direction) -> bool override;
    auto isReversed() -> bool override;
    auto setReversed(bool reversed) -> bool override;

    // Speed control
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Limits
    auto getMinPosition() -> double override;
    auto getMaxPosition() -> double override;
    auto setLimits(double min, double max) -> bool override;

    // Backlash compensation
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Presets
    auto savePreset(int slot, double angle) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getLastMoveAngle() -> double override;
    auto getLastMoveDuration() -> int override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // ASCOM Rotator-specific properties
    auto canReverse() -> bool;

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
    std::atomic<double> current_position_{0.0};
    std::atomic<double> target_position_{0.0};

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
    IDispatch* com_rotator_{nullptr};
    std::string com_prog_id_;
#endif

    // Rotator properties
    struct ASCOMRotatorInfo {
        bool can_reverse{false};
        double step_size{1.0};
        bool is_reversed{false};
        double mechanical_position{0.0};
    } ascom_rotator_info_;

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateRotatorInfo() -> bool;
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
