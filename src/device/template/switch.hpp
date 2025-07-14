/*
 * switch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomSwitch device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SwitchState {
    ON,
    OFF,
    UNKNOWN
};

enum class SwitchType {
    TOGGLE,     // Single switch that can be on/off
    BUTTON,     // Momentary switch
    SELECTOR,   // Multiple switches where only one can be on
    RADIO,      // Multiple switches where multiple can be on
    UNKNOWN
};

// Switch capabilities
struct SwitchCapabilities {
    bool canToggle{true};
    bool canSetAll{false};
    bool hasGroups{false};
    bool hasStateFeedback{true};
    bool canSaveState{false};
    bool hasTimer{false};
    SwitchType type{SwitchType::TOGGLE};
    uint32_t maxSwitches{16};
    uint32_t maxGroups{4};
} ATOM_ALIGNAS(32);

// Individual switch information
struct SwitchInfo {
    std::string name;
    std::string label;
    std::string description;
    SwitchState state{SwitchState::OFF};
    SwitchType type{SwitchType::TOGGLE};
    std::string group;
    bool enabled{true};
    uint32_t index{0};

    // Timer functionality
    bool hasTimer{false};
    uint32_t timerDuration{0}; // in milliseconds
    std::chrono::steady_clock::time_point timerStart;

    // Power consumption (for monitoring)
    double powerConsumption{0.0}; // watts

    SwitchInfo() = default;
    SwitchInfo(std::string n, std::string l, std::string d = "", SwitchType t = SwitchType::TOGGLE)
        : name(std::move(n)), label(std::move(l)), description(std::move(d)), type(t) {}
} ATOM_ALIGNAS(32);

// Switch group information
struct SwitchGroup {
    std::string name;
    std::string label;
    std::string description;
    SwitchType type{SwitchType::RADIO};
    std::vector<uint32_t> switchIndices;
    bool exclusive{false}; // Only one switch can be on at a time

    SwitchGroup() = default;
    SwitchGroup(std::string n, std::string l, SwitchType t = SwitchType::RADIO, bool excl = false)
        : name(std::move(n)), label(std::move(l)), type(t), exclusive(excl) {}
} ATOM_ALIGNAS(32);

class AtomSwitch : public AtomDriver {
public:
    explicit AtomSwitch(std::string name) : AtomDriver(std::move(name)) {
        setType("Switch");
    }

    ~AtomSwitch() override = default;

    // Capabilities
    const SwitchCapabilities& getSwitchCapabilities() const { return switch_capabilities_; }
    void setSwitchCapabilities(const SwitchCapabilities& caps) { switch_capabilities_ = caps; }

    // Switch management
    virtual auto addSwitch(const SwitchInfo& switchInfo) -> bool = 0;
    virtual auto removeSwitch(uint32_t index) -> bool = 0;
    virtual auto removeSwitch(const std::string& name) -> bool = 0;
    virtual auto getSwitchCount() -> uint32_t = 0;
    virtual auto getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> = 0;
    virtual auto getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> = 0;
    virtual auto getSwitchIndex(const std::string& name) -> std::optional<uint32_t> = 0;
    virtual auto getAllSwitches() -> std::vector<SwitchInfo> = 0;

    // Switch control
    virtual auto setSwitchState(uint32_t index, SwitchState state) -> bool = 0;
    virtual auto setSwitchState(const std::string& name, SwitchState state) -> bool = 0;
    virtual auto getSwitchState(uint32_t index) -> std::optional<SwitchState> = 0;
    virtual auto getSwitchState(const std::string& name) -> std::optional<SwitchState> = 0;
    virtual auto toggleSwitch(uint32_t index) -> bool = 0;
    virtual auto toggleSwitch(const std::string& name) -> bool = 0;
    virtual auto setAllSwitches(SwitchState state) -> bool = 0;

    // Batch operations
    virtual auto setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool = 0;
    virtual auto setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool = 0;
    virtual auto getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> = 0;

    // Group management
    virtual auto addGroup(const SwitchGroup& group) -> bool = 0;
    virtual auto removeGroup(const std::string& name) -> bool = 0;
    virtual auto getGroupCount() -> uint32_t = 0;
    virtual auto getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> = 0;
    virtual auto getAllGroups() -> std::vector<SwitchGroup> = 0;
    virtual auto addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool = 0;
    virtual auto removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool = 0;

    // Group control
    virtual auto setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool = 0;
    virtual auto setGroupAllOff(const std::string& groupName) -> bool = 0;
    virtual auto getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> = 0;

    // Timer functionality
    virtual auto setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool = 0;
    virtual auto setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool = 0;
    virtual auto cancelSwitchTimer(uint32_t index) -> bool = 0;
    virtual auto cancelSwitchTimer(const std::string& name) -> bool = 0;
    virtual auto getRemainingTime(uint32_t index) -> std::optional<uint32_t> = 0;
    virtual auto getRemainingTime(const std::string& name) -> std::optional<uint32_t> = 0;

    // Power monitoring
    virtual auto getTotalPowerConsumption() -> double = 0;
    virtual auto getSwitchPowerConsumption(uint32_t index) -> std::optional<double> = 0;
    virtual auto getSwitchPowerConsumption(const std::string& name) -> std::optional<double> = 0;
    virtual auto setPowerLimit(double maxWatts) -> bool = 0;
    virtual auto getPowerLimit() -> double = 0;

    // State persistence
    virtual auto saveState() -> bool = 0;
    virtual auto loadState() -> bool = 0;
    virtual auto resetToDefaults() -> bool = 0;

    // Safety features
    virtual auto enableSafetyMode(bool enable) -> bool = 0;
    virtual auto isSafetyModeEnabled() -> bool = 0;
    virtual auto setEmergencyStop() -> bool = 0;
    virtual auto clearEmergencyStop() -> bool = 0;
    virtual auto isEmergencyStopActive() -> bool = 0;

    // Statistics
    virtual auto getSwitchOperationCount(uint32_t index) -> uint64_t = 0;
    virtual auto getSwitchOperationCount(const std::string& name) -> uint64_t = 0;
    virtual auto getTotalOperationCount() -> uint64_t = 0;
    virtual auto getSwitchUptime(uint32_t index) -> uint64_t = 0; // in milliseconds
    virtual auto getSwitchUptime(const std::string& name) -> uint64_t = 0;
    virtual auto resetStatistics() -> bool = 0;

    // Event callbacks
    using SwitchStateCallback = std::function<void(uint32_t index, SwitchState state)>;
    using GroupStateCallback = std::function<void(const std::string& groupName, uint32_t switchIndex, SwitchState state)>;
    using TimerCallback = std::function<void(uint32_t index, bool expired)>;
    using PowerCallback = std::function<void(double totalPower, bool limitExceeded)>;
    using EmergencyCallback = std::function<void(bool active)>;

    virtual void setSwitchStateCallback(SwitchStateCallback callback) { switch_state_callback_ = std::move(callback); }
    virtual void setGroupStateCallback(GroupStateCallback callback) { group_state_callback_ = std::move(callback); }
    virtual void setTimerCallback(TimerCallback callback) { timer_callback_ = std::move(callback); }
    virtual void setPowerCallback(PowerCallback callback) { power_callback_ = std::move(callback); }
    virtual void setEmergencyCallback(EmergencyCallback callback) { emergency_callback_ = std::move(callback); }

    // Utility methods
    virtual auto isValidSwitchIndex(uint32_t index) -> bool;
    virtual auto isValidSwitchName(const std::string& name) -> bool;
    virtual auto isValidGroupName(const std::string& name) -> bool;

protected:
    SwitchCapabilities switch_capabilities_;
    std::vector<SwitchInfo> switches_;
    std::vector<SwitchGroup> groups_;
    std::unordered_map<std::string, uint32_t> switch_name_to_index_;
    std::unordered_map<std::string, uint32_t> group_name_to_index_;

    // Power monitoring
    double power_limit_{1000.0}; // watts
    double total_power_consumption_{0.0};

    // Safety
    bool safety_mode_enabled_{false};
    bool emergency_stop_active_{false};

    // Statistics
    std::vector<uint64_t> switch_operation_counts_;
    std::vector<std::chrono::steady_clock::time_point> switch_on_times_;
    std::vector<uint64_t> switch_uptimes_;
    uint64_t total_operation_count_{0};

    // Callbacks
    SwitchStateCallback switch_state_callback_;
    GroupStateCallback group_state_callback_;
    TimerCallback timer_callback_;
    PowerCallback power_callback_;
    EmergencyCallback emergency_callback_;

    // Utility methods
    virtual void notifySwitchStateChange(uint32_t index, SwitchState state);
    virtual void notifyGroupStateChange(const std::string& groupName, uint32_t switchIndex, SwitchState state);
    virtual void notifyTimerEvent(uint32_t index, bool expired);
    virtual void notifyPowerEvent(double totalPower, bool limitExceeded);
    virtual void notifyEmergencyEvent(bool active);

    virtual void updatePowerConsumption();
    virtual void updateStatistics(uint32_t index, SwitchState state);
    virtual void processTimers();
};

// Inline implementations
inline auto AtomSwitch::isValidSwitchIndex(uint32_t index) -> bool {
    return index < switches_.size();
}

inline auto AtomSwitch::isValidSwitchName(const std::string& name) -> bool {
    return switch_name_to_index_.find(name) != switch_name_to_index_.end();
}

inline auto AtomSwitch::isValidGroupName(const std::string& name) -> bool {
    return group_name_to_index_.find(name) != group_name_to_index_.end();
}

inline void AtomSwitch::notifySwitchStateChange(uint32_t index, SwitchState state) {
    if (switch_state_callback_) {
        switch_state_callback_(index, state);
    }
}

inline void AtomSwitch::notifyGroupStateChange(const std::string& groupName, uint32_t switchIndex, SwitchState state) {
    if (group_state_callback_) {
        group_state_callback_(groupName, switchIndex, state);
    }
}

inline void AtomSwitch::notifyTimerEvent(uint32_t index, bool expired) {
    if (timer_callback_) {
        timer_callback_(index, expired);
    }
}

inline void AtomSwitch::notifyPowerEvent(double totalPower, bool limitExceeded) {
    if (power_callback_) {
        power_callback_(totalPower, limitExceeded);
    }
}

inline void AtomSwitch::notifyEmergencyEvent(bool active) {
    if (emergency_callback_) {
        emergency_callback_(active);
    }
}
