/*
 * switch_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Manager - Core Switch Control Implementation

*************************************************/

#include "switch_manager.hpp"
#include "switch_client.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

SwitchManager::SwitchManager(INDISwitchClient* client)
    : client_(client) {
    setupPropertyMappings();
}

// Basic switch operations
auto SwitchManager::addSwitch(const SwitchInfo& switchInfo) -> bool {
    std::scoped_lock lock(state_mutex_);
    if (switch_name_to_index_.contains(switchInfo.name)) [[unlikely]] {
        spdlog::error("[SwitchManager] Switch with name '{}' already exists", switchInfo.name);
        return false;
    }
    uint32_t index = static_cast<uint32_t>(switches_.size());
    switches_.push_back(switchInfo);
    switch_name_to_index_[switchInfo.name] = index;
    spdlog::info("[SwitchManager] Added switch: {} at index {}", switchInfo.name, index);
    return true;
}

auto SwitchManager::removeSwitch(uint32_t index) -> bool {
    std::scoped_lock lock(state_mutex_);
    if (!isValidSwitchIndex(index)) [[unlikely]] {
        spdlog::error("[SwitchManager] Invalid switch index: {}", index);
        return false;
    }
    std::string name = switches_[index].name;
    switch_name_to_index_.erase(name);
    switches_.erase(switches_.begin() + index);
    for (auto& pair : switch_name_to_index_) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    spdlog::info("[SwitchManager] Removed switch: {} from index {}", name, index);
    return true;
}

auto SwitchManager::removeSwitch(const std::string& name) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) [[unlikely]] {
        spdlog::error("[SwitchManager] Switch not found: {}", name);
        return false;
    }
    return removeSwitch(*indexOpt);
}

auto SwitchManager::getSwitchCount() const noexcept -> uint32_t {
    std::scoped_lock lock(state_mutex_);
    return static_cast<uint32_t>(switches_.size());
}

auto SwitchManager::getSwitchInfo(uint32_t index) const -> std::optional<SwitchInfo> {
    std::scoped_lock lock(state_mutex_);
    if (!isValidSwitchIndex(index)) [[unlikely]] {
        return std::nullopt;
    }
    return switches_[index];
}

auto SwitchManager::getSwitchInfo(const std::string& name) const -> std::optional<SwitchInfo> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) [[unlikely]] {
        return std::nullopt;
    }
    return getSwitchInfo(*indexOpt);
}

auto SwitchManager::getSwitchIndex(const std::string& name) const -> std::optional<uint32_t> {
    std::scoped_lock lock(state_mutex_);
    auto it = switch_name_to_index_.find(name);
    if (it != switch_name_to_index_.end()) [[likely]] {
        return it->second;
    }
    return std::nullopt;
}

auto SwitchManager::getAllSwitches() const -> std::vector<SwitchInfo> {
    std::scoped_lock lock(state_mutex_);
    return switches_;
}

// Switch state management
auto SwitchManager::setSwitchState(uint32_t index, SwitchState state) -> bool {
    std::scoped_lock lock(state_mutex_);
    if (!isValidSwitchIndex(index)) [[unlikely]] {
        spdlog::error("[SwitchManager] Invalid switch index: {}", index);
        return false;
    }
    auto& switchInfo = switches_[index];
    if (switchInfo.state == state) [[unlikely]] {
        return true;
    }
    switchInfo.state = state;
    if (client_->isConnected()) [[likely]] {
        auto property = findSwitchProperty(switchInfo.name);
        if (property) [[likely]] {
            property->reset();
            auto widget = property->findWidgetByName(switchInfo.name.c_str());
            if (widget) [[likely]] {
                widget->setState(createINDIState(state));
                client_->sendNewProperty(property);
            }
        }
    }
    if (auto stats = client_->getStatsManager()) [[likely]] {
        stats->updateStatistics(index, state == SwitchState::ON);
    }
    notifySwitchStateChange(index, state);
    spdlog::info("[SwitchManager] Switch {} state changed to {}",
                switchInfo.name, (state == SwitchState::ON ? "ON" : "OFF"));
    return true;
}

auto SwitchManager::setSwitchState(const std::string& name, SwitchState state) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) [[unlikely]] {
        spdlog::error("[SwitchManager] Switch not found: {}", name);
        return false;
    }
    return setSwitchState(*indexOpt, state);
}

auto SwitchManager::getSwitchState(uint32_t index) const -> std::optional<SwitchState> {
    std::scoped_lock lock(state_mutex_);
    if (!isValidSwitchIndex(index)) [[unlikely]] {
        return std::nullopt;
    }
    return switches_[index].state;
}

auto SwitchManager::getSwitchState(const std::string& name) const -> std::optional<SwitchState> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) [[unlikely]] {
        return std::nullopt;
    }
    return getSwitchState(*indexOpt);
}

auto SwitchManager::setAllSwitches(SwitchState state) -> bool {
    std::scoped_lock lock(state_mutex_);
    bool success = true;
    for (uint32_t i = 0; i < switches_.size(); ++i) {
        if (!setSwitchState(i, state)) [[unlikely]] {
            success = false;
        }
    }
    spdlog::info("[SwitchManager] Set all switches to {}",
                (state == SwitchState::ON ? "ON" : "OFF"));
    return success;
}

auto SwitchManager::toggleSwitch(uint32_t index) -> bool {
    auto currentState = getSwitchState(index);
    if (!currentState) [[unlikely]] {
        return false;
    }
    SwitchState newState = (*currentState == SwitchState::ON) ? SwitchState::OFF : SwitchState::ON;
    return setSwitchState(index, newState);
}

auto SwitchManager::toggleSwitch(const std::string& name) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) [[unlikely]] {
        return false;
    }
    return toggleSwitch(*indexOpt);
}

// Group management implementations (similar to original INDISwitch)
auto SwitchManager::addGroup(const SwitchGroup& group) -> bool {
    std::scoped_lock lock(state_mutex_);
    if (group_name_to_index_.contains(group.name)) [[unlikely]] {
        spdlog::error("[SwitchManager] Group with name '{}' already exists", group.name);
        return false;
    }
    uint32_t index = static_cast<uint32_t>(groups_.size());
    groups_.push_back(group);
    group_name_to_index_[group.name] = index;
    spdlog::info("[SwitchManager] Added group: {} at index {}", group.name, index);
    return true;
}

auto SwitchManager::removeGroup(const std::string& name) -> bool {
    std::scoped_lock lock(state_mutex_);
    auto it = group_name_to_index_.find(name);
    if (it == group_name_to_index_.end()) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", name);
        return false;
    }
    uint32_t index = it->second;
    group_name_to_index_.erase(name);
    groups_.erase(groups_.begin() + index);
    for (auto& pair : group_name_to_index_) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    spdlog::info("[SwitchManager] Removed group: {} from index {}", name, index);
    return true;
}

auto SwitchManager::getGroupCount() const noexcept -> uint32_t {
    std::scoped_lock lock(state_mutex_);
    return static_cast<uint32_t>(groups_.size());
}

auto SwitchManager::getGroupInfo(const std::string& name) const -> std::optional<SwitchGroup> {
    std::scoped_lock lock(state_mutex_);
    auto it = group_name_to_index_.find(name);
    if (it == group_name_to_index_.end()) [[unlikely]] {
        return std::nullopt;
    }
    return groups_[it->second];
}

auto SwitchManager::getAllGroups() const -> std::vector<SwitchGroup> {
    std::scoped_lock lock(state_mutex_);
    return groups_;
}

auto SwitchManager::addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    std::scoped_lock lock(state_mutex_);
    if (!isValidSwitchIndex(switchIndex)) [[unlikely]] {
        spdlog::error("[SwitchManager] Invalid switch index: {}", switchIndex);
        return false;
    }
    auto it = group_name_to_index_.find(groupName);
    if (it == group_name_to_index_.end()) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", groupName);
        return false;
    }
    uint32_t groupIndex = it->second;
    auto& group = groups_[groupIndex];
    if (std::find(group.switchIndices.begin(), group.switchIndices.end(), switchIndex) != group.switchIndices.end()) [[unlikely]] {
        spdlog::warn("[SwitchManager] Switch {} already in group {}", switchIndex, groupName);
        return true;
    }
    group.switchIndices.push_back(switchIndex);
    switches_[switchIndex].group = groupName;
    spdlog::info("[SwitchManager] Added switch {} to group {}", switchIndex, groupName);
    return true;
}

auto SwitchManager::removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    std::scoped_lock lock(state_mutex_);
    auto it = group_name_to_index_.find(groupName);
    if (it == group_name_to_index_.end()) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", groupName);
        return false;
    }
    uint32_t groupIndex = it->second;
    auto& group = groups_[groupIndex];
    auto switchIt = std::find(group.switchIndices.begin(), group.switchIndices.end(), switchIndex);
    if (switchIt == group.switchIndices.end()) [[unlikely]] {
        spdlog::warn("[SwitchManager] Switch {} not found in group {}", switchIndex, groupName);
        return true;
    }
    group.switchIndices.erase(switchIt);
    if (isValidSwitchIndex(switchIndex)) [[likely]] {
        switches_[switchIndex].group.clear();
    }
    spdlog::info("[SwitchManager] Removed switch {} from group {}", switchIndex, groupName);
    return true;
}

auto SwitchManager::setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    std::scoped_lock lock(state_mutex_);
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", groupName);
        return false;
    }
    if (std::find(groupInfo->switchIndices.begin(), groupInfo->switchIndices.end(), switchIndex) == groupInfo->switchIndices.end()) [[unlikely]] {
        spdlog::error("[SwitchManager] Switch {} not in group {}", switchIndex, groupName);
        return false;
    }
    if (groupInfo->exclusive && state == SwitchState::ON) [[likely]] {
        for (uint32_t idx : groupInfo->switchIndices) {
            if (idx != switchIndex) {
                [[maybe_unused]] bool result = setSwitchState(idx, SwitchState::OFF);
            }
        }
    }
    bool result = setSwitchState(switchIndex, state);
    if (result) [[likely]] {
        notifyGroupStateChange(groupName, switchIndex, state);
    }
    return result;
}

auto SwitchManager::setGroupAllOff(const std::string& groupName) -> bool {
    std::scoped_lock lock(state_mutex_);
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", groupName);
        return false;
    }
    bool success = true;
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        if (!setSwitchState(switchIndex, SwitchState::OFF)) [[unlikely]] {
            success = false;
        }
    }
    spdlog::info("[SwitchManager] Set all switches OFF in group: {}", groupName);
    return success;
}

auto SwitchManager::getGroupStates(const std::string& groupName) const -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::scoped_lock lock(state_mutex_);
    std::vector<std::pair<uint32_t, SwitchState>> states;
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) [[unlikely]] {
        spdlog::error("[SwitchManager] Group not found: {}", groupName);
        return states;
    }
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        auto state = getSwitchState(switchIndex);
        if (state) [[likely]] {
            states.emplace_back(switchIndex, *state);
        }
    }
    return states;
}

// INDI property handling
void SwitchManager::handleSwitchProperty(const INDI::Property& property) {
    if (property.getType() != INDI_SWITCH) [[unlikely]] {
        return;
    }
    auto switchProperty = property.getSwitch();
    if (switchProperty) [[likely]] {
        updateSwitchFromProperty(switchProperty);
    }
}

void SwitchManager::synchronizeWithDevice() {
    if (!client_->isConnected()) [[unlikely]] {
        return;
    }
    for (size_t i = 0; i < switches_.size(); ++i) {
        const auto& switchInfo = switches_[i];
        auto property = findSwitchProperty(switchInfo.name);
        if (property) [[likely]] {
            updateSwitchFromProperty(property);
        }
    }
}

auto SwitchManager::findSwitchProperty(const std::string& switchName) -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) [[unlikely]] {
        return nullptr;
    }
    std::vector<std::string> propertyNames = {
        switchName,
        "SWITCH_" + switchName,
        switchName + "_SWITCH",
        "OUTPUT_" + switchName,
        switchName + "_OUTPUT"
    };
    for (const auto& propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) [[likely]] {
            return property.getSwitch();
        }
    }
    return nullptr;
}

void SwitchManager::updateSwitchFromProperty(INDI::PropertyViewSwitch* property) {
    if (!property) [[unlikely]] {
        return;
    }
    std::scoped_lock lock(state_mutex_);
    for (int i = 0; i < property->count(); ++i) {
        auto widget = property->at(i);
        std::string widgetName = widget->getName();
        auto indexOpt = getSwitchIndex(widgetName);
        if (indexOpt) [[likely]] {
            uint32_t index = *indexOpt;
            SwitchState newState = parseINDIState(widget->getState());
            if (switches_[index].state != newState) [[unlikely]] {
                switches_[index].state = newState;
                notifySwitchStateChange(index, newState);
                if (auto stats = client_->getStatsManager()) [[likely]] {
                    stats->updateStatistics(index, newState == SwitchState::ON);
                }
            }
        }
    }
}

// Utility methods
auto SwitchManager::isValidSwitchIndex(uint32_t index) const noexcept -> bool {
    return index < switches_.size();
}

void SwitchManager::notifySwitchStateChange(uint32_t index, SwitchState state) {
    spdlog::debug("[SwitchManager] Switch {} state changed to {}",
                 index, (state == SwitchState::ON ? "ON" : "OFF"));
}

void SwitchManager::notifyGroupStateChange(const std::string& groupName, uint32_t switchIndex, SwitchState state) {
    spdlog::debug("[SwitchManager] Group {} switch {} state changed to {}",
                 groupName, switchIndex, (state == SwitchState::ON ? "ON" : "OFF"));
}

// INDI utility methods
auto SwitchManager::createINDIState(SwitchState state) const noexcept -> ISState {
    return (state == SwitchState::ON) ? ISS_ON : ISS_OFF;
}

auto SwitchManager::parseINDIState(ISState state) const noexcept -> SwitchState {
    return (state == ISS_ON) ? SwitchState::ON : SwitchState::OFF;
}

void SwitchManager::setupPropertyMappings() {
    spdlog::info("[SwitchManager] Setting up INDI property mappings");
}
