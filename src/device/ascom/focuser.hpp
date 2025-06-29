/*
 * focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Focuser Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/focuser.hpp"

class ASCOMFocuser : public AtomFocuser {
public:
    explicit ASCOMFocuser(std::string name);
    ~ASCOMFocuser() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Focuser state
    auto isMoving() const -> bool override;

    // Speed control
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;

    // Direction control
    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;

    // Limits
    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;

    // Reverse control
    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;

    // Movement control
    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;

    // Relative movement
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;

    // Backlash compensation
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature
    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Temperature compensation
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp) -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;

    // Auto focus
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;

    // Presets
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // ASCOM Focuser-specific properties
    auto isAbsolute() -> bool;
    auto getMaxIncrement() -> int;
    auto getMaxStep() -> int;
    auto getStepCount() -> int;
    auto getTempCompAvailable() -> bool;
    auto getTempComp() -> bool;
    auto setTempComp(bool enable) -> bool;

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
    std::atomic<int> current_position_{0};
    std::atomic<int> target_position_{0};

    // ASCOM device information
    std::string device_name_;
    std::string driver_info_;
    std::string driver_version_;
    std::string client_id_{"Lithium-Next"};
    int interface_version_{3};

    // Alpaca connection details
    std::string alpaca_host_{"localhost"};
    int alpaca_port_{11111};
    int alpaca_device_number_{0};

#ifdef _WIN32
    // COM object for Windows ASCOM drivers
    IDispatch* com_focuser_{nullptr};
    std::string com_prog_id_;
#endif

    // Focuser properties cache
    struct ASCOMFocuserInfo {
        bool is_absolute{true};
        int max_increment{10000};
        int max_step{10000};
        bool temp_comp_available{false};
        bool temp_comp{false};
        double step_size{1.0};
        int max_position{10000};
        int min_position{0};
        int max_speed{100};
        int current_speed{50};
        bool has_backlash{false};
        int backlash{0};
        double temperature_coefficient{0.0};
    } ascom_focuser_info_;

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateFocuserInfo() -> bool;
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
