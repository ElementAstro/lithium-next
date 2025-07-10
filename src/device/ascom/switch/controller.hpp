/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Modular ASCOM Switch Controller

This modular controller orchestrates the switch components to provide
a clean, maintainable, and testable interface for ASCOM switch control.

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "./components/hardware_interface.hpp"
#include "./components/switch_manager.hpp"
#include "./components/group_manager.hpp"
#include "./components/timer_manager.hpp"
#include "./components/power_manager.hpp"
#include "./components/state_manager.hpp"
#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw {

// Forward declarations
namespace components {
class HardwareInterface;
class SwitchManager;
class GroupManager;
class TimerManager;
class PowerManager;
class StateManager;
}

/**
 * @brief Modular ASCOM Switch Controller
 *
 * This controller provides a clean interface to ASCOM switch functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of switch operation, promoting separation of concerns and
 * testability.
 */
class ASCOMSwitchController : public AtomSwitch {
public:
    explicit ASCOMSwitchController(const std::string& name);
    ~ASCOMSwitchController() override;

    // Non-copyable and non-movable
    ASCOMSwitchController(const ASCOMSwitchController&) = delete;
    ASCOMSwitchController& operator=(const ASCOMSwitchController&) = delete;
    ASCOMSwitchController(ASCOMSwitchController&&) = delete;
    ASCOMSwitchController& operator=(ASCOMSwitchController&&) = delete;

    // =========================================================================
    // AtomDriver Interface Implementation
    // =========================================================================
    
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Switch Management
    // =========================================================================
    
    auto addSwitch(const SwitchInfo& switchInfo) -> bool override;
    auto removeSwitch(uint32_t index) -> bool override;
    auto removeSwitch(const std::string& name) -> bool override;
    auto getSwitchCount() -> uint32_t override;
    auto getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> override;
    auto getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> override;
    auto getSwitchIndex(const std::string& name) -> std::optional<uint32_t> override;
    auto getAllSwitches() -> std::vector<SwitchInfo> override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Switch Control
    // =========================================================================
    
    auto setSwitchState(uint32_t index, SwitchState state) -> bool override;
    auto setSwitchState(const std::string& name, SwitchState state) -> bool override;
    auto getSwitchState(uint32_t index) -> std::optional<SwitchState> override;
    auto getSwitchState(const std::string& name) -> std::optional<SwitchState> override;
    auto toggleSwitch(uint32_t index) -> bool override;
    auto toggleSwitch(const std::string& name) -> bool override;
    auto setAllSwitches(SwitchState state) -> bool override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Batch Operations
    // =========================================================================
    
    auto setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool override;
    auto setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool override;
    auto getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Group Management
    // =========================================================================
    
    auto addGroup(const SwitchGroup& group) -> bool override;
    auto removeGroup(const std::string& name) -> bool override;
    auto getGroupCount() -> uint32_t override;
    auto getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> override;
    auto getAllGroups() -> std::vector<SwitchGroup> override;
    auto addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool override;
    auto removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Group Control
    // =========================================================================
    
    auto setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool override;
    auto setGroupAllOff(const std::string& groupName) -> bool override;
    auto getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Timer Functionality
    // =========================================================================
    
    auto setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool override;
    auto setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool override;
    auto cancelSwitchTimer(uint32_t index) -> bool override;
    auto cancelSwitchTimer(const std::string& name) -> bool override;
    auto getRemainingTime(uint32_t index) -> std::optional<uint32_t> override;
    auto getRemainingTime(const std::string& name) -> std::optional<uint32_t> override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Power Management
    // =========================================================================
    
    auto getTotalPowerConsumption() -> double override;
    auto getSwitchPowerConsumption(uint32_t index) -> std::optional<double> override;
    auto getSwitchPowerConsumption(const std::string& name) -> std::optional<double> override;
    auto setPowerLimit(double maxWatts) -> bool override;
    auto getPowerLimit() -> double override;

    // =========================================================================
    // AtomSwitch Interface Implementation - State Management
    // =========================================================================
    
    auto saveState() -> bool override;
    auto loadState() -> bool override;
    auto resetToDefaults() -> bool override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Safety Features
    // =========================================================================
    
    auto enableSafetyMode(bool enable) -> bool override;
    auto isSafetyModeEnabled() -> bool override;
    auto setEmergencyStop() -> bool override;
    auto clearEmergencyStop() -> bool override;
    auto isEmergencyStopActive() -> bool override;

    // =========================================================================
    // AtomSwitch Interface Implementation - Statistics
    // =========================================================================
    
    auto getSwitchOperationCount(uint32_t index) -> uint64_t override;
    auto getSwitchOperationCount(const std::string& name) -> uint64_t override;
    auto getTotalOperationCount() -> uint64_t override;
    auto getSwitchUptime(uint32_t index) -> uint64_t override;
    auto getSwitchUptime(const std::string& name) -> uint64_t override;
    auto resetStatistics() -> bool override;

    // =========================================================================
    // ASCOM-specific methods
    // =========================================================================
    
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // =========================================================================
    // Error handling and diagnostics
    // =========================================================================
    
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;
    auto enableVerboseLogging(bool enable) -> void;
    auto isVerboseLoggingEnabled() const -> bool;

    // =========================================================================
    // Component access for testing
    // =========================================================================
    
    auto getHardwareInterface() const -> std::shared_ptr<components::HardwareInterface>;
    auto getSwitchManager() const -> std::shared_ptr<components::SwitchManager>;
    auto getGroupManager() const -> std::shared_ptr<components::GroupManager>;
    auto getTimerManager() const -> std::shared_ptr<components::TimerManager>;
    auto getPowerManager() const -> std::shared_ptr<components::PowerManager>;
    auto getStateManager() const -> std::shared_ptr<components::StateManager>;

private:
    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_interface_;
    std::shared_ptr<components::SwitchManager> switch_manager_;
    std::shared_ptr<components::GroupManager> group_manager_;
    std::shared_ptr<components::TimerManager> timer_manager_;
    std::shared_ptr<components::PowerManager> power_manager_;
    std::shared_ptr<components::StateManager> state_manager_;

    // Control flow
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    mutable std::mutex controller_mutex_;
    
    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;
    std::atomic<bool> verbose_logging_{false};

    // Internal helper methods
    auto validateConfiguration() const -> bool;
    auto initializeComponents() -> bool;
    auto cleanupComponents() -> void;
    auto setLastError(const std::string& error) const -> void;
    auto logOperation(const std::string& operation, bool success) const -> void;
    
    // Component coordination
    auto notifyComponentsOfConnection(bool connected) -> void;
    auto synchronizeComponentStates() -> bool;
};

// Exception classes for ASCOM Switch specific errors
class ASCOMSwitchException : public std::runtime_error {
public:
    explicit ASCOMSwitchException(const std::string& message) : std::runtime_error(message) {}
};

class ASCOMSwitchConnectionException : public ASCOMSwitchException {
public:
    explicit ASCOMSwitchConnectionException(const std::string& message) 
        : ASCOMSwitchException("Connection error: " + message) {}
};

class ASCOMSwitchConfigurationException : public ASCOMSwitchException {
public:
    explicit ASCOMSwitchConfigurationException(const std::string& message) 
        : ASCOMSwitchException("Configuration error: " + message) {}
};

} // namespace lithium::device::ascom::sw
