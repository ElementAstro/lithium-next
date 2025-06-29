/*
 * switch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/switch.hpp"

class ASCOMSwitch : public AtomSwitch {
public:
    explicit ASCOMSwitch(std::string name);
    ~ASCOMSwitch() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Switch management
    auto addSwitch(const ::SwitchInfo& switchInfo) -> bool override;
    auto removeSwitch(uint32_t index) -> bool override;
    auto removeSwitch(const std::string& name) -> bool override;
    auto getSwitchCount() -> uint32_t override;
    auto getSwitchInfo(uint32_t index) -> std::optional<::SwitchInfo> override;
    auto getSwitchInfo(const std::string& name) -> std::optional<::SwitchInfo> override;
    auto getSwitchIndex(const std::string& name) -> std::optional<uint32_t> override;
    auto getAllSwitches() -> std::vector<::SwitchInfo> override;

    // Switch control
    auto setSwitchState(uint32_t index, SwitchState state) -> bool override;
    auto setSwitchState(const std::string& name, SwitchState state) -> bool override;
    auto getSwitchState(uint32_t index) -> std::optional<SwitchState> override;
    auto getSwitchState(const std::string& name) -> std::optional<SwitchState> override;
    auto toggleSwitch(uint32_t index) -> bool override;
    auto toggleSwitch(const std::string& name) -> bool override;
    auto setAllSwitches(SwitchState state) -> bool override;

    // Batch operations
    auto setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool override;
    auto setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool override;
    auto getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> override;

    // Group management
    auto addGroup(const SwitchGroup& group) -> bool override;
    auto removeGroup(const std::string& name) -> bool override;
    auto getGroupCount() -> uint32_t override;
    auto getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> override;
    auto getAllGroups() -> std::vector<SwitchGroup> override;
    auto addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool override;
    auto removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool override;

    // Group control
    auto setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool override;
    auto setGroupAllOff(const std::string& groupName) -> bool override;
    auto getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> override;

    // Timer functionality
    auto setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool override;
    auto setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool override;
    auto cancelSwitchTimer(uint32_t index) -> bool override;
    auto cancelSwitchTimer(const std::string& name) -> bool override;
    auto getRemainingTime(uint32_t index) -> std::optional<uint32_t> override;
    auto getRemainingTime(const std::string& name) -> std::optional<uint32_t> override;

    // Power monitoring
    auto getTotalPowerConsumption() -> double override;

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
    IDispatch* com_switch_{nullptr};
    std::string com_prog_id_;
#endif

    // Switch properties
    int switch_count_{0};
    struct SwitchInfo {
        std::string name;
        std::string description;
        bool can_write{false};
        double min_value{0.0};
        double max_value{1.0};
        double step_value{1.0};
        bool state{false};
        double value{0.0};
    };
    std::vector<SwitchInfo> switches_;

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateSwitchInfo() -> bool;
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
