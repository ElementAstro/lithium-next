/*
 * switch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: INDI Switch Client Implementation

*************************************************/

#ifndef LITHIUM_CLIENT_INDI_SWITCH_HPP
#define LITHIUM_CLIENT_INDI_SWITCH_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

#include "device/template/switch.hpp"

class INDISwitch : public INDI::BaseClient, public AtomSwitch {
public:
    explicit INDISwitch(std::string name);
    ~INDISwitch() override = default;

    // Non-copyable, non-movable due to atomic members
    INDISwitch(const INDISwitch& other) = delete;
    INDISwitch& operator=(const INDISwitch& other) = delete;
    INDISwitch(INDISwitch&& other) = delete;
    INDISwitch& operator=(INDISwitch&& other) = delete;

    // Base device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto reconnect(int timeout, int maxRetry) -> bool;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    virtual auto watchAdditionalProperty() -> bool;

    // Switch management
    auto addSwitch(const SwitchInfo& switchInfo) -> bool override;
    auto removeSwitch(uint32_t index) -> bool override;
    auto removeSwitch(const std::string& name) -> bool override;
    auto getSwitchCount() -> uint32_t override;
    auto getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> override;
    auto getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> override;
    auto getSwitchIndex(const std::string& name) -> std::optional<uint32_t> override;
    auto getAllSwitches() -> std::vector<SwitchInfo> override;

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
    auto getSwitchPowerConsumption(uint32_t index) -> std::optional<double> override;
    auto getSwitchPowerConsumption(const std::string& name) -> std::optional<double> override;
    auto setPowerLimit(double maxWatts) -> bool override;
    auto getPowerLimit() -> double override;

    // State persistence
    auto saveState() -> bool override;
    auto loadState() -> bool override;
    auto resetToDefaults() -> bool override;

    // Safety features
    auto enableSafetyMode(bool enable) -> bool override;
    auto isSafetyModeEnabled() -> bool override;
    auto setEmergencyStop() -> bool override;
    auto clearEmergencyStop() -> bool override;
    auto isEmergencyStopActive() -> bool override;

    // Statistics
    auto getSwitchOperationCount(uint32_t index) -> uint64_t override;
    auto getSwitchOperationCount(const std::string& name) -> uint64_t override;
    auto getTotalOperationCount() -> uint64_t override;
    auto getSwitchUptime(uint32_t index) -> uint64_t override;
    auto getSwitchUptime(const std::string& name) -> uint64_t override;
    auto resetStatistics() -> bool override;

protected:
    // INDI BaseClient virtual methods
    void newDevice(INDI::BaseDevice baseDevice) override;
    void removeDevice(INDI::BaseDevice baseDevice) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;

private:
    // Internal state
    std::string device_name_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> server_connected_{false};
    
    // Device reference
    INDI::BaseDevice base_device_;
    
    // Thread safety
    mutable std::recursive_mutex state_mutex_;
    mutable std::recursive_mutex device_mutex_;
    
    // Timer thread for timer functionality
    std::thread timer_thread_;
    std::atomic<bool> timer_thread_running_{false};
    
    // INDI property mappings
    std::unordered_map<std::string, std::string> property_mappings_;
    std::unordered_map<std::string, uint32_t> property_to_switch_index_;
    
    // Internal methods
    void timerThreadFunction();
    auto findSwitchProperty(const std::string& switchName) -> INDI::PropertySwitch;
    auto createINDIState(SwitchState state) -> ISState;
    auto parseINDIState(ISState state) -> SwitchState;
    void updateSwitchFromProperty(const INDI::PropertySwitch& property);
    void handleSwitchProperty(const INDI::PropertySwitch& property);
    void setupPropertyMappings();
    void synchronizeWithDevice();
    
    // Helper methods
    void updatePowerConsumption() override;
    void updateStatistics(uint32_t index, SwitchState state) override;
    void processTimers() override;
    
    // Utility methods
    auto waitForConnection(int timeout) -> bool;
    auto waitForProperty(const std::string& propertyName, int timeout) -> bool;
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
};

#endif  // LITHIUM_CLIENT_INDI_SWITCH_HPP
