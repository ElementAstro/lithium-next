/*
 * switch_power.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Power - Power Management Implementation

*************************************************/

#include "switch_power.hpp"
#include "switch_client.hpp"

#include <spdlog/spdlog.h>

SwitchPower::SwitchPower(INDISwitchClient* client) : client_(client) {}

// Power monitoring
auto SwitchPower::getSwitchPowerConsumption(uint32_t index)
    -> std::optional<double> {
    std::scoped_lock lock(power_mutex_);
    auto switchManager = client_->getSwitchManager();
    if (!switchManager) {
        return std::nullopt;
    }
    auto switchInfo = switchManager->getSwitchInfo(index);
    if (!switchInfo) {
        return std::nullopt;
    }
    auto state = switchManager->getSwitchState(index);
    if (!state || *state != SwitchState::ON) [[unlikely]] {
        return 0.0;
    }
    return switchInfo->powerConsumption;
}

auto SwitchPower::getSwitchPowerConsumption(const std::string& name)
    -> std::optional<double> {
    auto switchManager = client_->getSwitchManager();
    if (!switchManager) {
        return std::nullopt;
    }
    auto indexOpt = switchManager->getSwitchIndex(name);
    if (!indexOpt) {
        return std::nullopt;
    }
    return getSwitchPowerConsumption(*indexOpt);
}

auto SwitchPower::getTotalPowerConsumption() -> double {
    std::scoped_lock lock(power_mutex_);
    return total_power_consumption_;
}

// Power limits
auto SwitchPower::setPowerLimit(double maxWatts) -> bool {
    std::scoped_lock lock(power_mutex_);
    if (maxWatts <= 0.0) [[unlikely]] {
        spdlog::error("[SwitchPower] Invalid power limit: {}", maxWatts);
        return false;
    }
    power_limit_ = maxWatts;
    spdlog::info("[SwitchPower] Set power limit to: {} watts", maxWatts);
    updatePowerConsumption();
    return true;
}

auto SwitchPower::getPowerLimit() -> double {
    std::scoped_lock lock(power_mutex_);
    return power_limit_;
}

auto SwitchPower::isPowerLimitExceeded() -> bool {
    std::scoped_lock lock(power_mutex_);
    return total_power_consumption_ > power_limit_;
}

// Power management
void SwitchPower::updatePowerConsumption() {
    std::scoped_lock lock(power_mutex_);
    auto switchManager = client_->getSwitchManager();
    if (!switchManager) {
        return;
    }
    double totalPower = 0.0;
    const auto& switches = switchManager->getAllSwitches();
    for (size_t i = 0; i < switches.size(); ++i) {
        const auto& switchInfo = switches[i];
        auto state = switchManager->getSwitchState(static_cast<uint32_t>(i));
        if (state && *state == SwitchState::ON) [[likely]] {
            totalPower += switchInfo.powerConsumption;
        }
    }
    total_power_consumption_ = totalPower;
    bool limitExceeded = totalPower > power_limit_;
    if (limitExceeded) {
        spdlog::warn("[SwitchPower] Power limit exceeded: {:.2f}W > {:.2f}W",
                     totalPower, power_limit_);
    }
    notifyPowerEvent(totalPower, limitExceeded);
}

void SwitchPower::checkPowerLimits() { updatePowerConsumption(); }

void SwitchPower::setPowerCallback(PowerCallback callback) {
    std::scoped_lock lock(power_mutex_);
    power_callback_ = std::move(callback);
}

// Internal methods
void SwitchPower::notifyPowerEvent(double totalPower, bool limitExceeded) {
    if (power_callback_) {
        try {
            power_callback_(totalPower, limitExceeded);
        } catch (const std::exception& ex) {
            spdlog::error("[SwitchPower] Power callback error: {}", ex.what());
        }
    }
}
