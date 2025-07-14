/*
 * timer_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Timer Manager Component Implementation

This component manages timer functionality for automatic switch operations,
delayed operations, and scheduled tasks.

*************************************************/

#include "timer_manager.hpp"
#include "switch_manager.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::ascom::sw::components {

TimerManager::TimerManager(std::shared_ptr<SwitchManager> switch_manager)
    : switch_manager_(std::move(switch_manager)) {
    spdlog::debug("TimerManager component created");
}

TimerManager::~TimerManager() {
    destroy();
}

auto TimerManager::initialize() -> bool {
    spdlog::info("Initializing Timer Manager");

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    return startTimerThread();
}

auto TimerManager::destroy() -> bool {
    spdlog::info("Destroying Timer Manager");
    stopTimerThread();

    std::lock_guard<std::mutex> lock(timers_mutex_);
    active_timers_.clear();

    return true;
}

auto TimerManager::reset() -> bool {
    if (!destroy()) {
        return false;
    }
    return initialize();
}

auto TimerManager::setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool {
    if (!validateSwitchIndex(index) || !validateTimerDuration(durationMs)) {
        return false;
    }

    auto current_state = switch_manager_->getSwitchState(index);
    if (!current_state) {
        setLastError("Failed to get current switch state for index " + std::to_string(index));
        return false;
    }

    SwitchState restore_state = (*current_state == SwitchState::ON) ? SwitchState::OFF : SwitchState::ON;
    return setSwitchTimerWithRestore(index, durationMs, restore_state);
}

auto TimerManager::setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return setSwitchTimer(*index, durationMs);
}

auto TimerManager::cancelSwitchTimer(uint32_t index) -> bool {
    std::lock_guard<std::mutex> lock(timers_mutex_);

    auto it = active_timers_.find(index);
    if (it == active_timers_.end()) {
        return true; // No timer to cancel
    }

    uint32_t remaining = calculateRemainingTime(it->second);
    active_timers_.erase(it);

    notifyTimerCancelled(index, remaining);
    spdlog::debug("Cancelled timer for switch {}", index);

    return true;
}

auto TimerManager::cancelSwitchTimer(const std::string& name) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return cancelSwitchTimer(*index);
}

auto TimerManager::getRemainingTime(uint32_t index) -> std::optional<uint32_t> {
    std::lock_guard<std::mutex> lock(timers_mutex_);

    auto it = active_timers_.find(index);
    if (it == active_timers_.end()) {
        return std::nullopt;
    }

    return calculateRemainingTime(it->second);
}

auto TimerManager::getRemainingTime(const std::string& name) -> std::optional<uint32_t> {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        return std::nullopt;
    }
    return getRemainingTime(*index);
}

auto TimerManager::setSwitchTimerWithRestore(uint32_t index, uint32_t durationMs, SwitchState restoreState) -> bool {
    if (!validateSwitchIndex(index) || !validateTimerDuration(durationMs)) {
        return false;
    }

    auto current_state = switch_manager_->getSwitchState(index);
    if (!current_state) {
        setLastError("Failed to get current switch state for index " + std::to_string(index));
        return false;
    }

    SwitchState target_state = (*current_state == SwitchState::ON) ? SwitchState::OFF : SwitchState::ON;
    TimerEntry timer = createTimerEntry(index, durationMs, target_state, restoreState);

    // Set initial state
    if (!switch_manager_->setSwitchState(index, target_state)) {
        setLastError("Failed to set switch state for index " + std::to_string(index));
        return false;
    }

    std::lock_guard<std::mutex> lock(timers_mutex_);
    active_timers_[index] = timer;

    notifyTimerStarted(index, durationMs);
    spdlog::debug("Started timer for switch {}: {}ms", index, durationMs);

    return true;
}

auto TimerManager::setSwitchTimerWithRestore(const std::string& name, uint32_t durationMs, SwitchState restoreState) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return setSwitchTimerWithRestore(*index, durationMs, restoreState);
}

auto TimerManager::setDelayedOperation(uint32_t index, uint32_t delayMs, SwitchState targetState) -> bool {
    if (!validateSwitchIndex(index) || !validateTimerDuration(delayMs)) {
        return false;
    }

    auto current_state = switch_manager_->getSwitchState(index);
    if (!current_state) {
        setLastError("Failed to get current switch state for index " + std::to_string(index));
        return false;
    }

    TimerEntry timer = createTimerEntry(index, delayMs, targetState, *current_state);
    timer.auto_restore = false; // Don't restore for delayed operations

    std::lock_guard<std::mutex> lock(timers_mutex_);
    active_timers_[index] = timer;

    notifyTimerStarted(index, delayMs);
    spdlog::debug("Started delayed operation for switch {}: {}ms to {}",
                  index, delayMs, static_cast<int>(targetState));

    return true;
}

auto TimerManager::setDelayedOperation(const std::string& name, uint32_t delayMs, SwitchState targetState) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return setDelayedOperation(*index, delayMs, targetState);
}

auto TimerManager::setRepeatingTimer(uint32_t index, uint32_t intervalMs, uint32_t repeatCount) -> bool {
    // For now, implement as single timer - could be extended for true repeating functionality
    return setSwitchTimer(index, intervalMs);
}

auto TimerManager::setRepeatingTimer(const std::string& name, uint32_t intervalMs, uint32_t repeatCount) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return setRepeatingTimer(*index, intervalMs, repeatCount);
}

auto TimerManager::getActiveTimers() -> std::vector<uint32_t> {
    std::lock_guard<std::mutex> lock(timers_mutex_);

    std::vector<uint32_t> active;
    for (const auto& [index, timer] : active_timers_) {
        active.push_back(index);
    }

    return active;
}

auto TimerManager::getTimerInfo(uint32_t index) -> std::optional<TimerEntry> {
    std::lock_guard<std::mutex> lock(timers_mutex_);

    auto it = active_timers_.find(index);
    if (it != active_timers_.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto TimerManager::getAllTimerInfo() -> std::vector<TimerEntry> {
    std::lock_guard<std::mutex> lock(timers_mutex_);

    std::vector<TimerEntry> timers;
    for (const auto& [index, timer] : active_timers_) {
        timers.push_back(timer);
    }

    return timers;
}

auto TimerManager::isTimerActive(uint32_t index) -> bool {
    std::lock_guard<std::mutex> lock(timers_mutex_);
    return active_timers_.find(index) != active_timers_.end();
}

auto TimerManager::isTimerActive(const std::string& name) -> bool {
    auto index = switch_manager_->getSwitchIndex(name);
    if (!index) {
        return false;
    }
    return isTimerActive(*index);
}

auto TimerManager::setDefaultTimerDuration(uint32_t durationMs) -> bool {
    if (!validateTimerDuration(durationMs)) {
        return false;
    }

    default_duration_ms_ = durationMs;
    return true;
}

auto TimerManager::getDefaultTimerDuration() -> uint32_t {
    return default_duration_ms_.load();
}

auto TimerManager::setMaxTimerDuration(uint32_t maxDurationMs) -> bool {
    if (maxDurationMs == 0) {
        setLastError("Maximum timer duration must be greater than 0");
        return false;
    }

    max_duration_ms_ = maxDurationMs;
    return true;
}

auto TimerManager::getMaxTimerDuration() -> uint32_t {
    return max_duration_ms_.load();
}

auto TimerManager::enableAutoRestore(bool enable) -> bool {
    auto_restore_enabled_ = enable;
    return true;
}

auto TimerManager::isAutoRestoreEnabled() -> bool {
    return auto_restore_enabled_.load();
}

void TimerManager::setTimerCallback(TimerCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    timer_callback_ = std::move(callback);
}

void TimerManager::setTimerStartCallback(TimerStartCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    timer_start_callback_ = std::move(callback);
}

void TimerManager::setTimerCancelCallback(TimerCancelCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    timer_cancel_callback_ = std::move(callback);
}

auto TimerManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto TimerManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// =========================================================================
// Internal Methods
// =========================================================================

auto TimerManager::startTimerThread() -> bool {
    std::lock_guard<std::mutex> lock(timer_thread_mutex_);

    if (timer_running_.load()) {
        return true;
    }

    timer_running_ = true;
    timer_thread_ = std::make_unique<std::thread>(&TimerManager::timerLoop, this);

    spdlog::debug("Timer thread started");
    return true;
}

auto TimerManager::stopTimerThread() -> void {
    {
        std::lock_guard<std::mutex> lock(timer_thread_mutex_);
        if (!timer_running_.load()) {
            return;
        }
        timer_running_ = false;
    }

    timer_cv_.notify_all();

    if (timer_thread_ && timer_thread_->joinable()) {
        timer_thread_->join();
    }

    timer_thread_.reset();
    spdlog::debug("Timer thread stopped");
}

auto TimerManager::timerLoop() -> void {
    spdlog::debug("Timer loop started");

    while (timer_running_.load()) {
        processExpiredTimers();

        // Sleep for 100ms
        std::unique_lock<std::mutex> lock(timer_thread_mutex_);
        timer_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return !timer_running_.load();
        });
    }

    spdlog::debug("Timer loop stopped");
}

auto TimerManager::processExpiredTimers() -> void {
    std::vector<uint32_t> expired_timers;

    {
        std::lock_guard<std::mutex> lock(timers_mutex_);
        auto now = std::chrono::steady_clock::now();

        for (const auto& [index, timer] : active_timers_) {
            if (now >= timer.end_time) {
                expired_timers.push_back(index);
            }
        }
    }

    // Process expired timers outside of lock to avoid deadlock
    for (uint32_t index : expired_timers) {
        auto timer_opt = getTimerInfo(index);
        if (timer_opt) {
            TimerEntry timer = *timer_opt;
            {
                std::lock_guard<std::mutex> lock(timers_mutex_);
                active_timers_.erase(index);
            }

            bool restored = false;
            if (timer.auto_restore && auto_restore_enabled_.load()) {
                restored = restoreSwitchState(timer.switch_index, timer.restore_state);
            }

            notifyTimerExpired(timer.switch_index, restored);
            spdlog::debug("Timer expired for switch {}, restored: {}", timer.switch_index, restored);
        }
    }
}

auto TimerManager::createTimerEntry(uint32_t index, uint32_t durationMs, SwitchState targetState, SwitchState restoreState) -> TimerEntry {
    TimerEntry timer;
    timer.switch_index = index;
    timer.duration_ms = durationMs;
    timer.target_state = targetState;
    timer.restore_state = restoreState;
    timer.start_time = std::chrono::steady_clock::now();
    timer.end_time = timer.start_time + std::chrono::milliseconds(durationMs);
    timer.active = true;
    timer.auto_restore = auto_restore_enabled_.load();

    return timer;
}

auto TimerManager::validateTimerDuration(uint32_t durationMs) -> bool {
    if (durationMs == 0) {
        setLastError("Timer duration must be greater than 0");
        return false;
    }

    if (durationMs > max_duration_ms_.load()) {
        setLastError("Timer duration exceeds maximum allowed: " + std::to_string(max_duration_ms_.load()));
        return false;
    }

    return true;
}

auto TimerManager::validateSwitchIndex(uint32_t index) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    if (index >= switch_manager_->getSwitchCount()) {
        setLastError("Invalid switch index: " + std::to_string(index));
        return false;
    }

    return true;
}

auto TimerManager::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("TimerManager Error: {}", error);
}

auto TimerManager::notifyTimerExpired(uint32_t index, bool restored) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (timer_callback_) {
        timer_callback_(index, true, restored); // expired=true
    }
}

auto TimerManager::notifyTimerStarted(uint32_t index, uint32_t durationMs) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (timer_start_callback_) {
        timer_start_callback_(index, durationMs);
    }
}

auto TimerManager::notifyTimerCancelled(uint32_t index, uint32_t remainingMs) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (timer_cancel_callback_) {
        timer_cancel_callback_(index, remainingMs);
    }
}

auto TimerManager::restoreSwitchState(uint32_t index, SwitchState state) -> bool {
    if (!switch_manager_) {
        return false;
    }

    return switch_manager_->setSwitchState(index, state);
}

auto TimerManager::calculateRemainingTime(const TimerEntry& timer) -> uint32_t {
    auto now = std::chrono::steady_clock::now();
    if (now >= timer.end_time) {
        return 0;
    }

    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(timer.end_time - now);
    return static_cast<uint32_t>(remaining.count());
}

} // namespace lithium::device::ascom::sw::components
