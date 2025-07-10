/*
 * timer_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Timer Manager Component

This component manages timer functionality for automatic switch operations,
delayed operations, and scheduled tasks.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

// Forward declarations
class SwitchManager;

/**
 * @brief Timer entry for scheduled switch operations
 */
struct TimerEntry {
    uint32_t switch_index;
    uint32_t duration_ms;
    SwitchState target_state;
    SwitchState restore_state;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    bool active{false};
    bool auto_restore{true};
    std::string description;
};

/**
 * @brief Timer Manager Component
 * 
 * This component handles all timer-related functionality for switches
 * including delayed operations, automatic shutoffs, and scheduled tasks.
 */
class TimerManager {
public:
    explicit TimerManager(std::shared_ptr<SwitchManager> switch_manager);
    ~TimerManager();

    // Non-copyable and non-movable
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
    TimerManager(TimerManager&&) = delete;
    TimerManager& operator=(TimerManager&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto reset() -> bool;

    // =========================================================================
    // Timer Operations
    // =========================================================================

    auto setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool;
    auto setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool;
    auto cancelSwitchTimer(uint32_t index) -> bool;
    auto cancelSwitchTimer(const std::string& name) -> bool;
    auto getRemainingTime(uint32_t index) -> std::optional<uint32_t>;
    auto getRemainingTime(const std::string& name) -> std::optional<uint32_t>;

    // =========================================================================
    // Advanced Timer Operations
    // =========================================================================

    auto setSwitchTimerWithRestore(uint32_t index, uint32_t durationMs, SwitchState restoreState) -> bool;
    auto setSwitchTimerWithRestore(const std::string& name, uint32_t durationMs, SwitchState restoreState) -> bool;
    auto setDelayedOperation(uint32_t index, uint32_t delayMs, SwitchState targetState) -> bool;
    auto setDelayedOperation(const std::string& name, uint32_t delayMs, SwitchState targetState) -> bool;
    auto setRepeatingTimer(uint32_t index, uint32_t intervalMs, uint32_t repeatCount = 0) -> bool;
    auto setRepeatingTimer(const std::string& name, uint32_t intervalMs, uint32_t repeatCount = 0) -> bool;

    // =========================================================================
    // Timer Information
    // =========================================================================

    auto getActiveTimers() -> std::vector<uint32_t>;
    auto getTimerInfo(uint32_t index) -> std::optional<TimerEntry>;
    auto getAllTimerInfo() -> std::vector<TimerEntry>;
    auto isTimerActive(uint32_t index) -> bool;
    auto isTimerActive(const std::string& name) -> bool;

    // =========================================================================
    // Timer Configuration
    // =========================================================================

    auto setDefaultTimerDuration(uint32_t durationMs) -> bool;
    auto getDefaultTimerDuration() -> uint32_t;
    auto setMaxTimerDuration(uint32_t maxDurationMs) -> bool;
    auto getMaxTimerDuration() -> uint32_t;
    auto enableAutoRestore(bool enable) -> bool;
    auto isAutoRestoreEnabled() -> bool;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using TimerCallback = std::function<void(uint32_t index, bool expired, bool restored)>;
    using TimerStartCallback = std::function<void(uint32_t index, uint32_t durationMs)>;
    using TimerCancelCallback = std::function<void(uint32_t index, uint32_t remainingMs)>;

    void setTimerCallback(TimerCallback callback);
    void setTimerStartCallback(TimerStartCallback callback);
    void setTimerCancelCallback(TimerCancelCallback callback);

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Switch manager reference
    std::shared_ptr<SwitchManager> switch_manager_;

    // Timer management
    std::unordered_map<uint32_t, TimerEntry> active_timers_;
    mutable std::mutex timers_mutex_;

    // Timer thread management
    std::unique_ptr<std::thread> timer_thread_;
    std::atomic<bool> timer_running_{false};
    std::condition_variable timer_cv_;
    std::mutex timer_thread_mutex_;

    // Configuration
    std::atomic<uint32_t> default_duration_ms_{10000};  // 10 seconds
    std::atomic<uint32_t> max_duration_ms_{3600000};    // 1 hour
    std::atomic<bool> auto_restore_enabled_{true};

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    TimerCallback timer_callback_;
    TimerStartCallback timer_start_callback_;
    TimerCancelCallback timer_cancel_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto startTimerThread() -> bool;
    auto stopTimerThread() -> void;
    auto timerLoop() -> void;
    auto processExpiredTimers() -> void;
    
    auto createTimerEntry(uint32_t index, uint32_t durationMs, SwitchState targetState, SwitchState restoreState) -> TimerEntry;
    auto addTimer(uint32_t index, const TimerEntry& timer) -> bool;
    auto removeTimer(uint32_t index) -> bool;
    auto findTimerByName(const std::string& name) -> std::optional<uint32_t>;
    
    auto validateTimerDuration(uint32_t durationMs) -> bool;
    auto validateSwitchIndex(uint32_t index) -> bool;
    auto setLastError(const std::string& error) const -> void;
    
    // Notification helpers
    auto notifyTimerExpired(uint32_t index, bool restored) -> void;
    auto notifyTimerStarted(uint32_t index, uint32_t durationMs) -> void;
    auto notifyTimerCancelled(uint32_t index, uint32_t remainingMs) -> void;
    
    // Timer execution
    auto executeTimerAction(const TimerEntry& timer) -> bool;
    auto restoreSwitchState(uint32_t index, SwitchState state) -> bool;
    auto calculateRemainingTime(const TimerEntry& timer) -> uint32_t;
};

} // namespace lithium::device::ascom::sw::components
