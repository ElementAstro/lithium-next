/*
 * group_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Group Manager Component

This component manages switch groups, exclusive operations,
and group-based control for ASCOM switch devices.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

// Forward declarations
class SwitchManager;

/**
 * @brief Group statistics information
 */
struct GroupStatistics {
    std::string group_name;
    uint32_t total_switches{0};
    uint32_t switches_on{0};
    uint32_t switches_off{0};
    uint32_t total_operations{0};
};

/**
 * @brief Group validation result
 */
struct GroupValidationResult {
    std::string group_name;
    bool is_valid{true};
    std::string error_message;
    std::vector<uint32_t> conflicting_switches;
    std::vector<uint32_t> invalid_switches;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

/**
 * @brief Group Manager Component
 * 
 * This component handles switch grouping functionality including
 * exclusive groups, group operations, and group state management.
 */
class GroupManager {
public:
    explicit GroupManager(std::shared_ptr<SwitchManager> switch_manager);
    ~GroupManager() = default;

    // Non-copyable and non-movable
    GroupManager(const GroupManager&) = delete;
    GroupManager& operator=(const GroupManager&) = delete;
    GroupManager(GroupManager&&) = delete;
    GroupManager& operator=(GroupManager&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto reset() -> bool;

    // =========================================================================
    // Group Management
    // =========================================================================

    auto addGroup(const SwitchGroup& group) -> bool;
    auto removeGroup(const std::string& name) -> bool;
    auto getGroupCount() -> uint32_t;
    auto getGroupInfo(const std::string& name) -> std::optional<SwitchGroup>;
    auto getAllGroups() -> std::vector<SwitchGroup>;
    auto addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool;
    auto removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool;

    // =========================================================================
    // Group Control
    // =========================================================================

    auto setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool;
    auto setGroupAllOff(const std::string& groupName) -> bool;
    auto setGroupExclusiveOn(const std::string& groupName, uint32_t switchIndex) -> bool;
    auto getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>>;

    // =========================================================================
    // Group Validation
    // =========================================================================

    auto validateGroupOperation(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool;
    auto isValidGroupName(const std::string& name) -> bool;
    auto isSwitchInGroup(const std::string& groupName, uint32_t switchIndex) -> bool;
    auto getGroupsContainingSwitch(uint32_t switchIndex) -> std::vector<std::string>;
    auto getGroupStatistics(const std::string& groupName) -> std::optional<GroupStatistics>;
    auto validateGroupOperations() -> std::vector<GroupValidationResult>;

    // =========================================================================
    // Group Policies
    // =========================================================================

    auto setGroupPolicy(const std::string& groupName, SwitchType type, bool exclusive) -> bool;
    auto getGroupPolicy(const std::string& groupName) -> std::optional<std::pair<SwitchType, bool>>;
    auto enforceGroupConstraints(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using GroupStateCallback = std::function<void(const std::string& groupName, uint32_t switchIndex, SwitchState state)>;
    using GroupOperationCallback = std::function<void(const std::string& groupName, const std::string& operation, bool success)>;

    void setGroupStateCallback(GroupStateCallback callback);
    void setGroupOperationCallback(GroupOperationCallback callback);

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Switch manager reference
    std::shared_ptr<SwitchManager> switch_manager_;

    // Group data
    std::vector<SwitchGroup> groups_;
    std::unordered_map<std::string, uint32_t> name_to_index_;
    mutable std::mutex groups_mutex_;

    // Group constraints and policies
    std::unordered_map<std::string, std::pair<SwitchType, bool>> group_policies_;
    mutable std::mutex policy_mutex_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    GroupStateCallback state_callback_;
    GroupOperationCallback operation_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto updateNameToIndexMap() -> void;
    auto validateGroupInfo(const SwitchGroup& group) -> bool;
    auto setLastError(const std::string& error) const -> void;
    auto logOperation(const std::string& groupName, const std::string& operation, bool success) -> void;

    // Group constraint enforcement
    auto enforceExclusiveConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool;
    auto enforceRadioConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool;
    auto enforceSelectorConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool;

    // Notification helpers
    auto notifyStateChange(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> void;
    auto notifyOperation(const std::string& groupName, const std::string& operation, bool success) -> void;

    // Utility methods
    auto findGroupByName(const std::string& name) -> std::optional<uint32_t>;
    auto isSwitchIndexInGroup(const SwitchGroup& group, uint32_t switchIndex) -> bool;
};

} // namespace lithium::device::ascom::sw::components
