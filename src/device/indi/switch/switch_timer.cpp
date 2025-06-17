/*
 * switch_timer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Timer - Timer Management Implementation

*************************************************/

#include "switch_timer.hpp"
#include "switch_client.hpp"

#include <spdlog/spdlog.h>
#include <utility>

SwitchTimer::SwitchTimer(INDISwitchClient* client) : client_(client) {}

SwitchTimer::~SwitchTimer() { stopTimerThread(); }

// Timer operations
[[nodiscard]] auto SwitchTimer::setSwitchTimer(uint32_t index,
                                               uint32_t durationMs) -> bool {
    std::scoped_lock lock(timer_mutex_);
    if (!isValidSwitchIndex(index)) {
        spdlog::error("[SwitchTimer] Invalid switch index: {}", index);
        return false;
    }
    if (durationMs == 0) {
        spdlog::error("[SwitchTimer] Invalid timer duration: {}", durationMs);
        return false;
    }
    cancelSwitchTimer(index);
    TimerInfo timer{.switchIndex = index,
                    .startTime = std::chrono::steady_clock::now(),
                    .duration = durationMs,
                    .active = true};
    active_timers_.insert_or_assign(index, std::move(timer));
    spdlog::info("[SwitchTimer] Set timer for switch {} duration: {}ms", index,
                 durationMs);
    return true;
}

[[nodiscard]] auto SwitchTimer::setSwitchTimer(const std::string& name,
                                               uint32_t durationMs) -> bool {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return setSwitchTimer(*indexOpt, durationMs);
        }
        spdlog::error("[SwitchTimer] Switch not found: {}", name);
    } else {
        spdlog::error("[SwitchTimer] Switch manager not available");
    }
    return false;
}

[[nodiscard]] auto SwitchTimer::cancelSwitchTimer(uint32_t index) -> bool {
    std::scoped_lock lock(timer_mutex_);
    if (active_timers_.erase(index)) {
        spdlog::info("[SwitchTimer] Cancelled timer for switch: {}", index);
    }
    return true;
}

[[nodiscard]] auto SwitchTimer::cancelSwitchTimer(const std::string& name)
    -> bool {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return cancelSwitchTimer(*indexOpt);
        }
    }
    return false;
}

[[nodiscard]] auto SwitchTimer::getRemainingTime(uint32_t index)
    -> std::optional<uint32_t> {
    std::scoped_lock lock(timer_mutex_);
    auto it = active_timers_.find(index);
    if (it == active_timers_.end() || !it->second.active) {
        return std::nullopt;
    }
    const auto& timer = it->second;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - timer.startTime)
                       .count();
    if (elapsed >= timer.duration) {
        return 0;
    }
    return static_cast<uint32_t>(timer.duration - elapsed);
}

[[nodiscard]] auto SwitchTimer::getRemainingTime(const std::string& name)
    -> std::optional<uint32_t> {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return getRemainingTime(*indexOpt);
        }
    }
    return std::nullopt;
}

// Timer management
[[nodiscard]] auto SwitchTimer::hasTimer(uint32_t index) -> bool {
    std::scoped_lock lock(timer_mutex_);
    auto it = active_timers_.find(index);
    return it != active_timers_.end() && it->second.active;
}

[[nodiscard]] auto SwitchTimer::hasTimer(const std::string& name) -> bool {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return hasTimer(*indexOpt);
        }
    }
    return false;
}

[[nodiscard]] auto SwitchTimer::cancelAllTimers() -> bool {
    std::scoped_lock lock(timer_mutex_);
    active_timers_.clear();
    spdlog::info("[SwitchTimer] Cancelled all active timers");
    return true;
}

[[nodiscard]] auto SwitchTimer::getActiveTimerCount() -> uint32_t {
    std::scoped_lock lock(timer_mutex_);
    return static_cast<uint32_t>(active_timers_.size());
}

[[nodiscard]] auto SwitchTimer::hasActiveTimers() -> bool {
    std::scoped_lock lock(timer_mutex_);
    return !active_timers_.empty();
}

// Timer thread control
void SwitchTimer::startTimerThread() {
    if (timer_thread_running_.exchange(true)) {
        return;
    }
    timer_thread_ = std::thread([this] { timerThreadFunction(); });
    spdlog::info("[SwitchTimer] Timer thread started");
}

void SwitchTimer::stopTimerThread() {
    if (!timer_thread_running_.exchange(false)) {
        return;
    }
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
    spdlog::info("[SwitchTimer] Timer thread stopped");
}

[[nodiscard]] auto SwitchTimer::isTimerThreadRunning() -> bool {
    return timer_thread_running_;
}

void SwitchTimer::setTimerCallback(TimerCallback callback) {
    std::scoped_lock lock(timer_mutex_);
    timer_callback_ = std::move(callback);
}

// Internal methods
void SwitchTimer::timerThreadFunction() {
    spdlog::info("[SwitchTimer] Timer monitoring thread started");
    while (timer_thread_running_) {
        try {
            processTimers();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& ex) {
            spdlog::error("[SwitchTimer] Timer thread error: {}", ex.what());
        }
    }
    spdlog::info("[SwitchTimer] Timer monitoring thread stopped");
}

void SwitchTimer::processTimers() {
    std::scoped_lock lock(timer_mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<uint32_t> expiredTimers;
    for (auto& [switchIndex, timer] : active_timers_) {
        if (!timer.active)
            continue;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - timer.startTime)
                           .count();
        if (elapsed >= timer.duration) {
            timer.active = false;
            expiredTimers.push_back(switchIndex);
            spdlog::info("[SwitchTimer] Timer expired for switch: {}",
                         switchIndex);
            notifyTimerEvent(switchIndex, true);
        }
    }
    for (uint32_t switchIndex : expiredTimers) {
        active_timers_.erase(switchIndex);
    }
}

void SwitchTimer::notifyTimerEvent(uint32_t switchIndex, bool expired) {
    if (timer_callback_) {
        try {
            timer_callback_(switchIndex, expired);
        } catch (const std::exception& ex) {
            spdlog::error("[SwitchTimer] Timer callback error: {}", ex.what());
        }
    }
}

[[nodiscard]] auto SwitchTimer::isValidSwitchIndex(uint32_t index) -> bool {
    if (auto switchManager = client_->getSwitchManager()) {
        return index < switchManager->getSwitchCount();
    }
    return false;
}
