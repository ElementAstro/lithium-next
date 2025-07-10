/*
 * switch_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Manager Component Implementation

This component manages individual switch operations, state tracking,
and validation for ASCOM switch devices.

*************************************************/

#include "switch_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>

namespace lithium::device::ascom::sw::components {

SwitchManager::SwitchManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    spdlog::debug("SwitchManager component created");
    
    // Set up callbacks from hardware interface
    if (hardware_) {
        hardware_->setStateChangeCallback(
            [this](uint32_t index, bool state) {
                updateCachedState(index, state ? SwitchState::ON : SwitchState::OFF);
            }
        );
    }
}

auto SwitchManager::initialize() -> bool {
    spdlog::info("Initializing Switch Manager");
    
    if (!hardware_) {
        setLastError("Hardware interface not available");
        return false;
    }
    
    return syncWithHardware();
}

auto SwitchManager::destroy() -> bool {
    spdlog::info("Destroying Switch Manager");
    
    std::lock_guard<std::mutex> switches_lock(switches_mutex_);
    std::lock_guard<std::mutex> state_lock(state_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    switches_.clear();
    name_to_index_.clear();
    cached_states_.clear();
    operation_counts_.clear();
    on_times_.clear();
    uptimes_.clear();
    last_state_changes_.clear();
    
    total_operations_.store(0);
    
    return true;
}

auto SwitchManager::reset() -> bool {
    spdlog::info("Resetting Switch Manager");
    
    if (!destroy()) {
        return false;
    }
    
    return initialize();
}

auto SwitchManager::addSwitch(const SwitchInfo& switchInfo) -> bool {
    spdlog::warn("Adding switches not supported for ASCOM devices");
    setLastError("Adding switches not supported for ASCOM devices");
    return false;
}

auto SwitchManager::removeSwitch(uint32_t index) -> bool {
    spdlog::warn("Removing switches not supported for ASCOM devices");
    setLastError("Removing switches not supported for ASCOM devices");
    return false;
}

auto SwitchManager::removeSwitch(const std::string& name) -> bool {
    spdlog::warn("Removing switches not supported for ASCOM devices");
    setLastError("Removing switches not supported for ASCOM devices");
    return false;
}

auto SwitchManager::getSwitchCount() -> uint32_t {
    std::lock_guard<std::mutex> lock(switches_mutex_);
    return static_cast<uint32_t>(switches_.size());
}

auto SwitchManager::getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> {
    std::lock_guard<std::mutex> lock(switches_mutex_);
    
    if (index >= switches_.size()) {
        return std::nullopt;
    }
    
    return switches_[index];
}

auto SwitchManager::getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchInfo(*index);
    }
    return std::nullopt;
}

auto SwitchManager::getSwitchIndex(const std::string& name) -> std::optional<uint32_t> {
    std::lock_guard<std::mutex> lock(switches_mutex_);
    auto it = name_to_index_.find(name);
    if (it != name_to_index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto SwitchManager::getAllSwitches() -> std::vector<SwitchInfo> {
    std::lock_guard<std::mutex> lock(switches_mutex_);
    return switches_;
}

auto SwitchManager::setSwitchState(uint32_t index, SwitchState state) -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }
    
    SwitchState oldState;
    {
        std::lock_guard<std::mutex> lock(switches_mutex_);
        if (index >= switches_.size()) {
            setLastError("Invalid switch index");
            return false;
        }
        
        if (!switches_[index].enabled) {
            setLastError("Switch is not writable");
            return false;
        }
        
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        if (index < cached_states_.size()) {
            oldState = cached_states_[index];
        }
    }
    
    // Convert to boolean
    bool boolState = (state == SwitchState::ON);
    
    // Send to hardware
    if (hardware_->setSwitchState(index, boolState)) {
        updateCachedState(index, state);
        updateStatistics(index, state);
        logOperation(index, "setState", true);
        notifyStateChange(index, oldState, state);
        notifyOperation(index, "setState", true);
        return true;
    } else {
        logOperation(index, "setState", false);
        notifyOperation(index, "setState", false);
        setLastError("Failed to set switch state in hardware");
        return false;
    }
}

auto SwitchManager::setSwitchState(const std::string& name, SwitchState state) -> bool {
    auto index = getSwitchIndex(name);
    if (index) {
        return setSwitchState(*index, state);
    }
    setLastError("Switch name not found: " + name);
    return false;
}

auto SwitchManager::getSwitchState(uint32_t index) -> std::optional<SwitchState> {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (index >= cached_states_.size()) {
        return std::nullopt;
    }
    
    return cached_states_[index];
}

auto SwitchManager::getSwitchState(const std::string& name) -> std::optional<SwitchState> {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchState(*index);
    }
    return std::nullopt;
}

auto SwitchManager::toggleSwitch(uint32_t index) -> bool {
    auto currentState = getSwitchState(index);
    if (currentState) {
        SwitchState newState = (*currentState == SwitchState::ON) 
                              ? SwitchState::OFF 
                              : SwitchState::ON;
        return setSwitchState(index, newState);
    }
    setLastError("Failed to get current switch state for toggle");
    return false;
}

auto SwitchManager::toggleSwitch(const std::string& name) -> bool {
    auto index = getSwitchIndex(name);
    if (index) {
        return toggleSwitch(*index);
    }
    setLastError("Switch name not found: " + name);
    return false;
}

auto SwitchManager::setAllSwitches(SwitchState state) -> bool {
    bool allSuccess = true;
    uint32_t count = getSwitchCount();
    
    for (uint32_t i = 0; i < count; ++i) {
        if (!setSwitchState(i, state)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

auto SwitchManager::setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool {
    bool allSuccess = true;
    
    for (const auto& [index, state] : states) {
        if (!setSwitchState(index, state)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

auto SwitchManager::setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool {
    bool allSuccess = true;
    
    for (const auto& [name, state] : states) {
        if (!setSwitchState(name, state)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

auto SwitchManager::getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::vector<std::pair<uint32_t, SwitchState>> states;
    
    uint32_t count = getSwitchCount();
    for (uint32_t i = 0; i < count; ++i) {
        auto state = getSwitchState(i);
        if (state) {
            states.emplace_back(i, *state);
        }
    }
    
    return states;
}

auto SwitchManager::getSwitchOperationCount(uint32_t index) -> uint64_t {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (index >= operation_counts_.size()) {
        return 0;
    }
    
    return operation_counts_[index];
}

auto SwitchManager::getSwitchOperationCount(const std::string& name) -> uint64_t {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchOperationCount(*index);
    }
    return 0;
}

auto SwitchManager::getTotalOperationCount() -> uint64_t {
    return total_operations_.load();
}

auto SwitchManager::getSwitchUptime(uint32_t index) -> uint64_t {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (index >= uptimes_.size()) {
        return 0;
    }
    
    return uptimes_[index];
}

auto SwitchManager::getSwitchUptime(const std::string& name) -> uint64_t {
    auto index = getSwitchIndex(name);
    if (index) {
        return getSwitchUptime(*index);
    }
    return 0;
}

auto SwitchManager::resetStatistics() -> bool {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::fill(operation_counts_.begin(), operation_counts_.end(), 0);
    std::fill(uptimes_.begin(), uptimes_.end(), 0);
    
    auto now = std::chrono::steady_clock::now();
    std::fill(on_times_.begin(), on_times_.end(), now);
    
    total_operations_.store(0);
    
    spdlog::info("Switch statistics reset");
    return true;
}

auto SwitchManager::isValidSwitchIndex(uint32_t index) -> bool {
    return index < getSwitchCount();
}

auto SwitchManager::isValidSwitchName(const std::string& name) -> bool {
    return getSwitchIndex(name).has_value();
}

auto SwitchManager::refreshSwitchStates() -> bool {
    return syncWithHardware();
}

void SwitchManager::setSwitchStateCallback(SwitchStateCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_callback_ = std::move(callback);
}

void SwitchManager::setSwitchOperationCallback(SwitchOperationCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    operation_callback_ = std::move(callback);
}

auto SwitchManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto SwitchManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private methods
auto SwitchManager::updateNameToIndexMap() -> void {
    name_to_index_.clear();
    for (size_t i = 0; i < switches_.size(); ++i) {
        name_to_index_[switches_[i].name] = static_cast<uint32_t>(i);
    }
}

auto SwitchManager::updateStatistics(uint32_t index, SwitchState state) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (index < operation_counts_.size()) {
        operation_counts_[index]++;
        total_operations_.fetch_add(1);
        
        auto now = std::chrono::steady_clock::now();
        
        if (index < on_times_.size() && index < uptimes_.size()) {
            if (state == SwitchState::ON) {
                on_times_[index] = now;
            } else if (state == SwitchState::OFF) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - on_times_[index]).count();
                uptimes_[index] += static_cast<uint64_t>(duration);
            }
        }
    }
}

auto SwitchManager::validateSwitchInfo(const SwitchInfo& info) -> bool {
    if (info.name.empty()) {
        setLastError("Switch name cannot be empty");
        return false;
    }
    
    if (info.description.empty()) {
        setLastError("Switch description cannot be empty");
        return false;
    }
    
    return true;
}

auto SwitchManager::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("SwitchManager Error: {}", error);
}

auto SwitchManager::logOperation(uint32_t index, const std::string& operation, bool success) -> void {
    spdlog::debug("Switch {} operation '{}': {}", index, operation, success ? "success" : "failed");
}

auto SwitchManager::notifyStateChange(uint32_t index, SwitchState oldState, SwitchState newState) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_callback_) {
        state_callback_(index, oldState, newState);
    }
}

auto SwitchManager::notifyOperation(uint32_t index, const std::string& operation, bool success) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (operation_callback_) {
        operation_callback_(index, operation, success);
    }
}

auto SwitchManager::syncWithHardware() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not available or not connected");
        return false;
    }
    
    uint32_t hwSwitchCount = hardware_->getSwitchCount();
    
    std::lock_guard<std::mutex> switches_lock(switches_mutex_);
    std::lock_guard<std::mutex> state_lock(state_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    // Resize containers
    switches_.clear();
    switches_.reserve(hwSwitchCount);
    cached_states_.clear();
    cached_states_.reserve(hwSwitchCount);
    operation_counts_.clear();
    operation_counts_.resize(hwSwitchCount, 0);
    on_times_.clear();
    on_times_.resize(hwSwitchCount, std::chrono::steady_clock::now());
    uptimes_.clear();
    uptimes_.resize(hwSwitchCount, 0);
    last_state_changes_.clear();
    last_state_changes_.resize(hwSwitchCount, std::chrono::steady_clock::now());
    
    // Populate switch info from hardware
    for (uint32_t i = 0; i < hwSwitchCount; ++i) {
        auto hwInfo = hardware_->getSwitchInfo(i);
        if (hwInfo) {
            SwitchInfo info;
            info.name = hwInfo->name;
            info.description = hwInfo->description;
            info.label = hwInfo->name;
            info.state = hwInfo->state ? SwitchState::ON : SwitchState::OFF;
            info.type = SwitchType::TOGGLE;
            info.enabled = hwInfo->can_write;
            info.index = i;
            info.powerConsumption = 0.0;  // Not supported by ASCOM
            
            switches_.push_back(info);
            cached_states_.push_back(info.state);
        } else {
            // Create placeholder info if hardware doesn't provide it
            SwitchInfo info;
            info.name = "Switch " + std::to_string(i);
            info.description = "ASCOM Switch " + std::to_string(i);
            info.label = info.name;
            info.state = SwitchState::OFF;
            info.type = SwitchType::TOGGLE;
            info.enabled = true;
            info.index = i;
            info.powerConsumption = 0.0;
            
            switches_.push_back(info);
            cached_states_.push_back(SwitchState::OFF);
        }
    }
    
    updateNameToIndexMap();
    
    spdlog::info("Synchronized with hardware: {} switches", hwSwitchCount);
    return true;
}

auto SwitchManager::updateCachedState(uint32_t index, SwitchState state) -> void {
    std::lock_guard<std::mutex> state_lock(state_mutex_);
    
    if (index < cached_states_.size()) {
        cached_states_[index] = state;
        
        if (index < last_state_changes_.size()) {
            last_state_changes_[index] = std::chrono::steady_clock::now();
        }
    }
    
    // Also update the switch info state
    std::lock_guard<std::mutex> switches_lock(switches_mutex_);
    if (index < switches_.size()) {
        switches_[index].state = state;
    }
}

} // namespace lithium::device::ascom::sw::components
