/*
 * switch_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Manager Component

This component manages individual switch operations, state tracking,
and validation for ASCOM switch devices.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Switch Manager Component
 * 
 * This component handles all switch-related operations including
 * state management, validation, and coordination with hardware.
 */
class SwitchManager {
public:
    explicit SwitchManager(std::shared_ptr<HardwareInterface> hardware);
    ~SwitchManager() = default;

    // Non-copyable and non-movable
    SwitchManager(const SwitchManager&) = delete;
    SwitchManager& operator=(const SwitchManager&) = delete;
    SwitchManager(SwitchManager&&) = delete;
    SwitchManager& operator=(SwitchManager&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto reset() -> bool;

    // =========================================================================
    // Switch Management
    // =========================================================================

    auto addSwitch(const SwitchInfo& switchInfo) -> bool;
    auto removeSwitch(uint32_t index) -> bool;
    auto removeSwitch(const std::string& name) -> bool;
    auto getSwitchCount() -> uint32_t;
    auto getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo>;
    auto getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo>;
    auto getSwitchIndex(const std::string& name) -> std::optional<uint32_t>;
    auto getAllSwitches() -> std::vector<SwitchInfo>;

    // =========================================================================
    // Switch Control
    // =========================================================================

    auto setSwitchState(uint32_t index, SwitchState state) -> bool;
    auto setSwitchState(const std::string& name, SwitchState state) -> bool;
    auto getSwitchState(uint32_t index) -> std::optional<SwitchState>;
    auto getSwitchState(const std::string& name) -> std::optional<SwitchState>;
    auto toggleSwitch(uint32_t index) -> bool;
    auto toggleSwitch(const std::string& name) -> bool;
    auto setAllSwitches(SwitchState state) -> bool;

    // =========================================================================
    // Batch Operations
    // =========================================================================

    auto setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool;
    auto setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool;
    auto getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>>;

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    auto getSwitchOperationCount(uint32_t index) -> uint64_t;
    auto getSwitchOperationCount(const std::string& name) -> uint64_t;
    auto getTotalOperationCount() -> uint64_t;
    auto getSwitchUptime(uint32_t index) -> uint64_t;
    auto getSwitchUptime(const std::string& name) -> uint64_t;
    auto resetStatistics() -> bool;

    // =========================================================================
    // Validation and Utility
    // =========================================================================

    auto isValidSwitchIndex(uint32_t index) -> bool;
    auto isValidSwitchName(const std::string& name) -> bool;
    auto refreshSwitchStates() -> bool;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using SwitchStateCallback = std::function<void(uint32_t index, SwitchState oldState, SwitchState newState)>;
    using SwitchOperationCallback = std::function<void(uint32_t index, const std::string& operation, bool success)>;

    void setSwitchStateCallback(SwitchStateCallback callback);
    void setSwitchOperationCallback(SwitchOperationCallback callback);

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // Switch data
    std::vector<SwitchInfo> switches_;
    std::unordered_map<std::string, uint32_t> name_to_index_;
    mutable std::mutex switches_mutex_;

    // Statistics
    std::vector<uint64_t> operation_counts_;
    std::vector<std::chrono::steady_clock::time_point> on_times_;
    std::vector<uint64_t> uptimes_;
    std::atomic<uint64_t> total_operations_{0};
    mutable std::mutex stats_mutex_;

    // State tracking
    std::vector<SwitchState> cached_states_;
    std::vector<std::chrono::steady_clock::time_point> last_state_changes_;
    mutable std::mutex state_mutex_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    SwitchStateCallback state_callback_;
    SwitchOperationCallback operation_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto updateNameToIndexMap() -> void;
    auto updateStatistics(uint32_t index, SwitchState state) -> void;
    auto validateSwitchInfo(const SwitchInfo& info) -> bool;
    auto setLastError(const std::string& error) const -> void;
    auto logOperation(uint32_t index, const std::string& operation, bool success) -> void;

    // Notification helpers
    auto notifyStateChange(uint32_t index, SwitchState oldState, SwitchState newState) -> void;
    auto notifyOperation(uint32_t index, const std::string& operation, bool success) -> void;

    // Hardware synchronization
    auto syncWithHardware() -> bool;
    auto updateCachedState(uint32_t index, SwitchState state) -> void;
};

} // namespace lithium::device::ascom::sw::components
