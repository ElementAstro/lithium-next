/*
 * switch_timer.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Timer - Timer Management Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_TIMER_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_TIMER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>

// Forward declarations
class INDISwitchClient;

/**
 * @brief Switch timer management component
 *
 * Handles automatic switch timers and time-based operations
 */
class SwitchTimer {
public:
    explicit SwitchTimer(INDISwitchClient* client);
    ~SwitchTimer();

    // Timer operations
    auto setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool;
    auto setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool;
    auto cancelSwitchTimer(uint32_t index) -> bool;
    auto cancelSwitchTimer(const std::string& name) -> bool;
    auto getRemainingTime(uint32_t index) -> std::optional<uint32_t>;
    auto getRemainingTime(const std::string& name) -> std::optional<uint32_t>;
    auto hasTimer(uint32_t index) -> bool;
    auto hasTimer(const std::string& name) -> bool;

    // Timer management
    auto cancelAllTimers() -> bool;
    auto getActiveTimerCount() -> uint32_t;
    auto hasActiveTimers() -> bool;
    void startTimerThread();
    void stopTimerThread();
    auto isTimerThreadRunning() -> bool;
    void processTimers();

    // Timer callback registration
    using TimerCallback = std::function<void(uint32_t switchIndex, bool expired)>;
    void setTimerCallback(TimerCallback callback);

private:
    struct TimerInfo {
        uint32_t switchIndex;
        std::chrono::steady_clock::time_point startTime;
        uint32_t duration;
        bool active{true};
    };

    INDISwitchClient* client_;
    mutable std::mutex timer_mutex_;

    // Timer data
    std::unordered_map<uint32_t, TimerInfo> active_timers_;

    // Timer thread
    std::thread timer_thread_;
    std::atomic<bool> timer_active_{false};
    std::atomic<bool> timer_thread_running_{false};

    // Timer callback
    TimerCallback timer_callback_;

    // Timer processing
    void timerThreadFunction();
    void handleTimerExpired(uint32_t switchIndex);
    void notifyTimerEvent(uint32_t switchIndex, bool expired);
    auto isValidSwitchIndex(uint32_t index) -> bool;
};

#endif // LITHIUM_DEVICE_INDI_SWITCH_TIMER_HPP
