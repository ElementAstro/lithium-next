/*
 * filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM FilterWheel Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/filterwheel.hpp"

class ASCOMFilterWheel : public AtomFilterWheel {
public:
    explicit ASCOMFilterWheel(std::string name);
    ~ASCOMFilterWheel() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Filter wheel state
    auto isMoving() const -> bool override;

    // Position control
    auto getPosition() -> std::optional<int> override;
    auto setPosition(int position) -> bool override;
    auto getFilterCount() -> int override;
    auto isValidPosition(int position) -> bool override;

    // Filter names and information
    auto getSlotName(int slot) -> std::optional<std::string> override;
    auto setSlotName(int slot, const std::string& name) -> bool override;
    auto getAllSlotNames() -> std::vector<std::string> override;
    auto getCurrentFilterName() -> std::string override;

    // Enhanced filter management
    auto getFilterInfo(int slot) -> std::optional<FilterInfo> override;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool override;
    auto getAllFilterInfo() -> std::vector<FilterInfo> override;

    // Filter search and selection
    auto findFilterByName(const std::string& name) -> std::optional<int> override;
    auto findFilterByType(const std::string& type) -> std::vector<int> override;
    auto selectFilterByName(const std::string& name) -> bool override;
    auto selectFilterByType(const std::string& type) -> bool override;

    // Motion control
    auto abortMotion() -> bool override;
    auto homeFilterWheel() -> bool override;
    auto calibrateFilterWheel() -> bool override;

    // Temperature (if supported)
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Statistics
    auto getTotalMoves() -> uint64_t override;
    auto resetTotalMoves() -> bool override;
    auto getLastMoveTime() -> int override;

    // Configuration presets
    auto saveFilterConfiguration(const std::string& name) -> bool override;
    auto loadFilterConfiguration(const std::string& name) -> bool override;
    auto deleteFilterConfiguration(const std::string& name) -> bool override;
    auto getAvailableConfigurations() -> std::vector<std::string> override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

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
    std::atomic<int> current_filter_{0};

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
    IDispatch* com_filterwheel_{nullptr};
    std::string com_prog_id_;
#endif

    // Filter wheel properties
    int filter_count_{0};
    std::vector<std::string> filter_names_;

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateFilterWheelInfo() -> bool;
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
