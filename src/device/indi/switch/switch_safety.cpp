/*
 * switch_safety.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Safety - Safety Management Implementation

*************************************************/

#include "switch_safety.hpp"
#include "switch_client.hpp"

#include <spdlog/spdlog.h>

SwitchSafety::SwitchSafety(INDISwitchClient* client) : client_(client) {}

// Safety features
auto SwitchSafety::enableSafetyMode(bool enable) noexcept -> bool {
    std::scoped_lock lock(safety_mutex_);
    safety_mode_enabled_.store(enable, std::memory_order_release);
    if (enable) [[likely]] {
        spdlog::info("[SwitchSafety] Safety mode ENABLED");
        // Perform immediate safety checks
        performSafetyChecks();
    } else {
        spdlog::info("[SwitchSafety] Safety mode DISABLED");
    }
    return true;
}

auto SwitchSafety::isSafetyModeEnabled() const noexcept -> bool {
    return safety_mode_enabled_.load(std::memory_order_acquire);
}

auto SwitchSafety::setEmergencyStop() noexcept -> bool {
    std::scoped_lock lock(safety_mutex_);
    emergency_stop_active_.store(true, std::memory_order_release);
    spdlog::critical("[SwitchSafety] EMERGENCY STOP ACTIVATED");

    // Execute immediate safety shutdown
    executeSafetyShutdown();

    notifyEmergencyEvent(true);

    return true;
}

auto SwitchSafety::clearEmergencyStop() noexcept -> bool {
    std::scoped_lock lock(safety_mutex_);
    emergency_stop_active_.store(false, std::memory_order_release);
    spdlog::info("[SwitchSafety] Emergency stop CLEARED");
    notifyEmergencyEvent(false);

    return true;
}

auto SwitchSafety::isEmergencyStopActive() const noexcept -> bool {
    return emergency_stop_active_.load(std::memory_order_acquire);
}

// Safety checks
auto SwitchSafety::isSafeToOperate() const noexcept -> bool {
    if (emergency_stop_active_.load(std::memory_order_acquire)) [[unlikely]] {
        return false;
    }
    if (safety_mode_enabled_.load(std::memory_order_acquire)) {
        // Additional safety checks when in safety mode
        auto powerManager = client_->getPowerManager();
        if (powerManager && powerManager->isPowerLimitExceeded()) {
            return false;
        }
    }
    return true;
}

void SwitchSafety::performSafetyChecks() noexcept {
    if (!safety_mode_enabled_.load(std::memory_order_acquire)) {
        return;
    }

    std::scoped_lock lock(safety_mutex_);

    // Check emergency stop
    if (emergency_stop_active_.load(std::memory_order_acquire)) {
        return;
    }

    // Check power limits
    auto powerManager = client_->getPowerManager();
    if (powerManager && powerManager->isPowerLimitExceeded()) {
        spdlog::critical(
            "[SwitchSafety] Power limit exceeded in safety mode - executing "
            "shutdown");
        executeSafetyShutdown();
        return;
    }

    // Additional safety checks can be added here
    // - Temperature monitoring
    // - Voltage monitoring
    // - Current monitoring
    // - External safety signals
}

void SwitchSafety::setEmergencyCallback(EmergencyCallback&& callback) noexcept {
    std::scoped_lock lock(safety_mutex_);
    emergency_callback_ = std::move(callback);
}

// Internal methods
void SwitchSafety::notifyEmergencyEvent(bool active) {
    if (emergency_callback_) {
        try {
            emergency_callback_(active);
        } catch (const std::exception& ex) {
            spdlog::error("[SwitchSafety] Emergency callback error: {}",
                          ex.what());
        }
    }
}

void SwitchSafety::executeSafetyShutdown() {
    auto switchManager = client_->getSwitchManager();
    if (!switchManager) {
        spdlog::error(
            "[SwitchSafety] Switch manager not available for safety shutdown");
        return;
    }

    // Turn off all switches immediately
    bool success = switchManager->setAllSwitches(SwitchState::OFF);

    if (success) {
        spdlog::info(
            "[SwitchSafety] Safety shutdown completed - all switches turned "
            "OFF");
    } else {
        spdlog::error(
            "[SwitchSafety] Safety shutdown failed - some switches may still "
            "be ON");
    }

    // Cancel all timers for safety
    auto timerManager = client_->getTimerManager();
    if (timerManager) {
        timerManager->cancelAllTimers();
        spdlog::info("[SwitchSafety] All timers cancelled for safety");
    }
}
