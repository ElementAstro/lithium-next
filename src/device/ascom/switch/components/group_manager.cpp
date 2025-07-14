/*
 * group_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Group Manager Component Implementation

This component manages switch groups, exclusive operations,
and group-based control for ASCOM switch devices.

*************************************************/

#include "group_manager.hpp"
#include "switch_manager.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::ascom::sw::components {

GroupManager::GroupManager(std::shared_ptr<SwitchManager> switch_manager)
    : switch_manager_(std::move(switch_manager)) {
    spdlog::debug("GroupManager component created");
}

auto GroupManager::initialize() -> bool {
    spdlog::info("Initializing Group Manager");

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    return true;
}

auto GroupManager::destroy() -> bool {
    spdlog::info("Destroying Group Manager");

    std::lock_guard<std::mutex> lock(groups_mutex_);
    groups_.clear();
    name_to_index_.clear();

    return true;
}

auto GroupManager::reset() -> bool {
    spdlog::info("Resetting Group Manager");
    return destroy() && initialize();
}

auto GroupManager::addGroup(const SwitchGroup& group) -> bool {
    if (!validateGroupInfo(group)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(groups_mutex_);

    // Check if group already exists
    if (findGroupByName(group.name).has_value()) {
        setLastError("Group already exists: " + group.name);
        return false;
    }

    // Validate that all switches exist
    if (switch_manager_) {
        for (uint32_t switchIndex : group.switchIndices) {
            if (!switch_manager_->isValidSwitchIndex(switchIndex)) {
                setLastError("Invalid switch index in group: " + std::to_string(switchIndex));
                return false;
            }
        }
    }

    uint32_t newIndex = static_cast<uint32_t>(groups_.size());
    groups_.push_back(group);
    name_to_index_[group.name] = newIndex;

    spdlog::info("Added group '{}' with {} switches", group.name, group.switchIndices.size());
    return true;
}

auto GroupManager::removeGroup(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto indexOpt = findGroupByName(name);
    if (!indexOpt) {
        setLastError("Group not found: " + name);
        return false;
    }

    uint32_t index = *indexOpt;

    // Remove from vector (this will invalidate indices, so we need to rebuild the map)
    groups_.erase(groups_.begin() + index);

    // Rebuild name to index map
    name_to_index_.clear();
    for (size_t i = 0; i < groups_.size(); ++i) {
        name_to_index_[groups_[i].name] = static_cast<uint32_t>(i);
    }

    spdlog::info("Removed group '{}'", name);
    return true;
}

auto GroupManager::getGroupCount() -> uint32_t {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    return static_cast<uint32_t>(groups_.size());
}

auto GroupManager::getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto indexOpt = findGroupByName(name);
    if (indexOpt && *indexOpt < groups_.size()) {
        return groups_[*indexOpt];
    }

    return std::nullopt;
}

auto GroupManager::getAllGroups() -> std::vector<SwitchGroup> {
    std::lock_guard<std::mutex> lock(groups_mutex_);
    return groups_;  // Return a copy
}

auto GroupManager::addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    if (!switch_manager_ || !switch_manager_->isValidSwitchIndex(switchIndex)) {
        setLastError("Invalid switch index: " + std::to_string(switchIndex));
        return false;
    }

    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto indexOpt = findGroupByName(groupName);
    if (!indexOpt) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    auto& group = groups_[*indexOpt];
    auto& switches = group.switchIndices;

    if (std::find(switches.begin(), switches.end(), switchIndex) != switches.end()) {
        setLastError("Switch already in group: " + std::to_string(switchIndex));
        return false;
    }

    switches.push_back(switchIndex);

    spdlog::info("Added switch {} to group '{}'", switchIndex, groupName);
    return true;
}

auto GroupManager::removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    std::lock_guard<std::mutex> lock(groups_mutex_);

    auto indexOpt = findGroupByName(groupName);
    if (!indexOpt) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    auto& group = groups_[*indexOpt];
    auto& switches = group.switchIndices;
    auto switchIt = std::find(switches.begin(), switches.end(), switchIndex);

    if (switchIt == switches.end()) {
        setLastError("Switch not in group: " + std::to_string(switchIndex));
        return false;
    }

    switches.erase(switchIt);

    spdlog::info("Removed switch {} from group '{}'", switchIndex, groupName);
    return true;
}

auto GroupManager::setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    // Get group info
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    // Check if switch is in the group
    const auto& switches = groupInfo->switchIndices;
    if (!isSwitchIndexInGroup(*groupInfo, switchIndex)) {
        setLastError("Switch " + std::to_string(switchIndex) + " not in group: " + groupName);
        return false;
    }

    // If this is an exclusive group and we're turning ON, turn others OFF first
    if (groupInfo->exclusive && state == SwitchState::ON) {
        for (uint32_t otherIndex : switches) {
            if (otherIndex != switchIndex) {
                if (!switch_manager_->setSwitchState(otherIndex, SwitchState::OFF)) {
                    spdlog::warn("Failed to turn off switch {} in exclusive group '{}'",
                               otherIndex, groupName);
                }
            }
        }
    }

    // Set the target switch state
    bool result = switch_manager_->setSwitchState(switchIndex, state);

    if (result) {
        spdlog::debug("Set switch {} to {} in group '{}'",
                     switchIndex, (state == SwitchState::ON ? "ON" : "OFF"), groupName);
        notifyStateChange(groupName, switchIndex, state);
        notifyOperation(groupName, "setState", true);
    } else {
        setLastError("Failed to set switch state");
        notifyOperation(groupName, "setState", false);
    }

    return result;
}

auto GroupManager::setGroupAllOff(const std::string& groupName) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    // Get group info
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    bool allSuccess = true;

    // Turn off all switches in the group
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        if (!switch_manager_->setSwitchState(switchIndex, SwitchState::OFF)) {
            spdlog::warn("Failed to turn off switch {} in group '{}'", switchIndex, groupName);
            allSuccess = false;
        }
    }

    if (allSuccess) {
        spdlog::info("Turned off all switches in group '{}'", groupName);
        notifyOperation(groupName, "setAllOff", true);
    } else {
        setLastError("Failed to turn off some switches in group");
        notifyOperation(groupName, "setAllOff", false);
    }

    return allSuccess;
}

auto GroupManager::setGroupExclusiveOn(const std::string& groupName, uint32_t switchIndex) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    // Get group info
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    // Check if switch is in the group
    if (!isSwitchIndexInGroup(*groupInfo, switchIndex)) {
        setLastError("Switch " + std::to_string(switchIndex) + " not in group: " + groupName);
        return false;
    }

    bool allSuccess = true;

    // Turn off all other switches first
    for (uint32_t otherIndex : groupInfo->switchIndices) {
        if (otherIndex != switchIndex) {
            if (!switch_manager_->setSwitchState(otherIndex, SwitchState::OFF)) {
                spdlog::warn("Failed to turn off switch {} in exclusive group '{}'",
                           otherIndex, groupName);
                allSuccess = false;
            }
        }
    }

    // Turn on the target switch
    if (!switch_manager_->setSwitchState(switchIndex, SwitchState::ON)) {
        spdlog::error("Failed to turn on switch {} in exclusive group '{}'",
                     switchIndex, groupName);
        setLastError("Failed to turn on target switch");
        notifyOperation(groupName, "setExclusiveOn", false);
        return false;
    }

    if (allSuccess) {
        spdlog::info("Set exclusive ON for switch {} in group '{}'", switchIndex, groupName);
    } else {
        spdlog::warn("Set exclusive ON for switch {} in group '{}' with some failures",
                    switchIndex, groupName);
    }

    notifyOperation(groupName, "setExclusiveOn", allSuccess);
    return allSuccess;
}

auto GroupManager::getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::vector<std::pair<uint32_t, SwitchState>> result;

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return result;
    }

    // Get group info
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return result;
    }

    // Get states for all switches in the group
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        auto state = switch_manager_->getSwitchState(switchIndex);
        if (state) {
            result.emplace_back(switchIndex, *state);
        }
    }

    return result;
}

auto GroupManager::getGroupStatistics(const std::string& groupName) -> std::optional<GroupStatistics> {
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo || !switch_manager_) {
        return std::nullopt;
    }

    GroupStatistics stats;
    stats.group_name = groupName;
    stats.total_switches = static_cast<uint32_t>(groupInfo->switchIndices.size());
    stats.switches_on = 0;
    stats.switches_off = 0;
    stats.total_operations = 0;

    // Count switch states and operations
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        auto state = switch_manager_->getSwitchState(switchIndex);
        if (state) {
            if (*state == SwitchState::ON) {
                stats.switches_on++;
            } else {
                stats.switches_off++;
            }
        }

        stats.total_operations += switch_manager_->getSwitchOperationCount(switchIndex);
    }

    return stats;
}

auto GroupManager::validateGroupOperations() -> std::vector<GroupValidationResult> {
    std::vector<GroupValidationResult> results;

    if (!switch_manager_) {
        return results;
    }

    std::lock_guard<std::mutex> lock(groups_mutex_);

    for (const auto& group : groups_) {
        GroupValidationResult result;
        result.group_name = group.name;
        result.is_valid = true;

        // Check exclusive group constraints
        if (group.exclusive) {
            uint32_t onCount = 0;
            std::vector<uint32_t> onSwitches;

            for (uint32_t switchIndex : group.switchIndices) {
                auto state = switch_manager_->getSwitchState(switchIndex);
                if (state && *state == SwitchState::ON) {
                    onCount++;
                    onSwitches.push_back(switchIndex);
                }
            }

            if (onCount > 1) {
                result.is_valid = false;
                result.error_message = "Exclusive group has multiple switches ON: " +
                                     std::to_string(onCount);
                result.conflicting_switches = onSwitches;
            }
        }

        // Check if all switches in group still exist
        for (uint32_t switchIndex : group.switchIndices) {
            if (!switch_manager_->isValidSwitchIndex(switchIndex)) {
                result.is_valid = false;
                if (!result.error_message.empty()) {
                    result.error_message += "; ";
                }
                result.error_message += "Invalid switch index: " + std::to_string(switchIndex);
                result.invalid_switches.push_back(switchIndex);
            }
        }

        results.push_back(result);
    }

    return results;
}

auto GroupManager::validateGroupOperation(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    if (!isSwitchIndexInGroup(*groupInfo, switchIndex)) {
        setLastError("Switch " + std::to_string(switchIndex) + " not in group " + groupName);
        return false;
    }

    return enforceGroupConstraints(groupName, switchIndex, state);
}

auto GroupManager::isValidGroupName(const std::string& name) -> bool {
    if (name.empty()) {
        return false;
    }

    // Check for valid characters (alphanumeric, underscore, hyphen)
    for (char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }

    return true;
}

auto GroupManager::isSwitchInGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        return false;
    }

    return isSwitchIndexInGroup(*groupInfo, switchIndex);
}

auto GroupManager::getGroupsContainingSwitch(uint32_t switchIndex) -> std::vector<std::string> {
    std::vector<std::string> groupNames;
    std::lock_guard<std::mutex> lock(groups_mutex_);

    for (const auto& group : groups_) {
        if (isSwitchIndexInGroup(group, switchIndex)) {
            groupNames.push_back(group.name);
        }
    }

    return groupNames;
}

auto GroupManager::setGroupPolicy(const std::string& groupName, SwitchType type, bool exclusive) -> bool {
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        setLastError("Group not found: " + groupName);
        return false;
    }

    std::lock_guard<std::mutex> lock(policy_mutex_);
    group_policies_[groupName] = std::make_pair(type, exclusive);

    spdlog::debug("Set policy for group {}: type={}, exclusive={}",
                  groupName, static_cast<int>(type), exclusive);
    return true;
}

auto GroupManager::getGroupPolicy(const std::string& groupName) -> std::optional<std::pair<SwitchType, bool>> {
    std::lock_guard<std::mutex> lock(policy_mutex_);
    auto it = group_policies_.find(groupName);
    if (it != group_policies_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto GroupManager::enforceGroupConstraints(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        return false;
    }

    // Check group policy
    auto policy = getGroupPolicy(groupName);
    if (policy) {
        SwitchType type = policy->first;
        bool exclusive = policy->second;

        if (exclusive && state == SwitchState::ON) {
            // For exclusive groups, only one switch can be on
            for (uint32_t idx : groupInfo->switchIndices) {
                if (idx != switchIndex && switch_manager_) {
                    auto currentState = switch_manager_->getSwitchState(idx);
                    if (currentState && *currentState == SwitchState::ON) {
                        // Turn off other switches
                        switch_manager_->setSwitchState(idx, SwitchState::OFF);
                    }
                }
            }
        }

        // Apply type-specific constraints
        switch (type) {
            case SwitchType::RADIO:
                return enforceRadioConstraint(*groupInfo, switchIndex, state);
            case SwitchType::SELECTOR:
                return enforceSelectorConstraint(*groupInfo, switchIndex, state);
            default:
                break;
        }
    }

    return true;
}

// Private methods
auto GroupManager::findGroupByName(const std::string& name) -> std::optional<uint32_t> {
    auto it = name_to_index_.find(name);
    if (it != name_to_index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto GroupManager::isSwitchIndexInGroup(const SwitchGroup& group, uint32_t switchIndex) -> bool {
    const auto& switches = group.switchIndices;
    return std::find(switches.begin(), switches.end(), switchIndex) != switches.end();
}

auto GroupManager::validateGroupInfo(const SwitchGroup& group) -> bool {
    if (group.name.empty()) {
        setLastError("Group name cannot be empty");
        return false;
    }

    if (group.switchIndices.empty()) {
        setLastError("Group must contain at least one switch");
        return false;
    }

    // Check for duplicate switches in the group
    std::vector<uint32_t> sorted_switches = group.switchIndices;
    std::sort(sorted_switches.begin(), sorted_switches.end());
    auto it = std::adjacent_find(sorted_switches.begin(), sorted_switches.end());
    if (it != sorted_switches.end()) {
        setLastError("Group contains duplicate switch index: " + std::to_string(*it));
        return false;
    }

    return true;
}

auto GroupManager::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("GroupManager Error: {}", error);
}

auto GroupManager::notifyStateChange(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_callback_) {
        state_callback_(groupName, switchIndex, state);
    }
}

auto GroupManager::notifyOperation(const std::string& groupName, const std::string& operation,
                                  bool success) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (operation_callback_) {
        operation_callback_(groupName, operation, success);
    }
}

auto GroupManager::updateNameToIndexMap() -> void {
    name_to_index_.clear();
    for (uint32_t i = 0; i < groups_.size(); ++i) {
        name_to_index_[groups_[i].name] = i;
    }
}

auto GroupManager::logOperation(const std::string& groupName, const std::string& operation, bool success) -> void {
    if (success) {
        spdlog::debug("Group operation succeeded: {} on group {}", operation, groupName);
    } else {
        spdlog::warn("Group operation failed: {} on group {}", operation, groupName);
    }

    notifyOperation(groupName, operation, success);
}

auto GroupManager::enforceExclusiveConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool {
    if (!switch_manager_) {
        return false;
    }

    if (state == SwitchState::ON && group.exclusive) {
        // Turn off all other switches in the group
        for (uint32_t idx : group.switchIndices) {
            if (idx != switchIndex) {
                auto currentState = switch_manager_->getSwitchState(idx);
                if (currentState && *currentState == SwitchState::ON) {
                    if (!switch_manager_->setSwitchState(idx, SwitchState::OFF)) {
                        spdlog::warn("Failed to turn off switch {} for exclusive constraint", idx);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

auto GroupManager::enforceRadioConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool {
    // Radio groups allow multiple switches to be on
    // No special constraints needed
    return true;
}

auto GroupManager::enforceSelectorConstraint(const SwitchGroup& group, uint32_t switchIndex, SwitchState state) -> bool {
    if (!switch_manager_) {
        return false;
    }

    if (state == SwitchState::ON) {
        // For selector groups, only one switch should be on at a time
        for (uint32_t idx : group.switchIndices) {
            if (idx != switchIndex) {
                auto currentState = switch_manager_->getSwitchState(idx);
                if (currentState && *currentState == SwitchState::ON) {
                    if (!switch_manager_->setSwitchState(idx, SwitchState::OFF)) {
                        spdlog::warn("Failed to turn off switch {} for selector constraint", idx);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

} // namespace lithium::device::ascom::sw::components
