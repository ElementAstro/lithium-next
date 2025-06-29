/*
 * switch_stats.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Stats - Statistics Tracking Implementation

*************************************************/

#include "switch_stats.hpp"
#include "switch_client.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

SwitchStats::SwitchStats(INDISwitchClient* client) : client_(client) {}

[[nodiscard]] auto SwitchStats::getSwitchOperationCount(uint32_t index)
    -> uint64_t {
    std::scoped_lock lock(stats_mutex_);
    if (index >= switch_operation_counts_.size()) {
        return 0;
    }
    return switch_operation_counts_[index];
}

[[nodiscard]] auto SwitchStats::getSwitchOperationCount(const std::string& name)
    -> uint64_t {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return getSwitchOperationCount(*indexOpt);
        }
    }
    return 0;
}

[[nodiscard]] auto SwitchStats::getSwitchUptime(uint32_t index) -> uint64_t {
    std::scoped_lock lock(stats_mutex_);
    if (index >= switch_uptimes_.size()) {
        return 0;
    }
    uint64_t uptime = switch_uptimes_[index];
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto state = switchManager->getSwitchState(index);
            state && *state == SwitchState::ON &&
            index < switch_on_times_.size()) {
            auto now = std::chrono::steady_clock::now();
            auto sessionTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - switch_on_times_[index])
                    .count();
            uptime += static_cast<uint64_t>(sessionTime);
        }
    }
    return uptime;
}

[[nodiscard]] auto SwitchStats::getSwitchUptime(const std::string& name)
    -> uint64_t {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return getSwitchUptime(*indexOpt);
        }
    }
    return 0;
}

[[nodiscard]] auto SwitchStats::getTotalOperationCount() -> uint64_t {
    std::scoped_lock lock(stats_mutex_);
    return total_operation_count_;
}

[[nodiscard]] auto SwitchStats::resetStatistics() -> bool {
    std::scoped_lock lock(stats_mutex_);
    try {
        std::ranges::fill(switch_operation_counts_, 0);
        std::ranges::fill(switch_uptimes_, 0);
        total_operation_count_ = 0;
        auto now = std::chrono::steady_clock::now();
        if (auto switchManager = client_->getSwitchManager()) {
            for (size_t i = 0; i < switch_on_times_.size(); ++i) {
                if (auto state =
                        switchManager->getSwitchState(static_cast<uint32_t>(i));
                    state && *state == SwitchState::ON) {
                    switch_on_times_[i] = now;
                }
            }
        }
        spdlog::info("[SwitchStats] All statistics reset");
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[SwitchStats] Failed to reset statistics: {}",
                      ex.what());
        return false;
    }
}

[[nodiscard]] auto SwitchStats::resetSwitchStatistics(uint32_t index) -> bool {
    std::scoped_lock lock(stats_mutex_);
    try {
        ensureVectorSize(index);
        if (switch_operation_counts_[index] > 0) {
            total_operation_count_ -= switch_operation_counts_[index];
            switch_operation_counts_[index] = 0;
        }
        switch_uptimes_[index] = 0;
        if (auto switchManager = client_->getSwitchManager()) {
            if (auto state = switchManager->getSwitchState(index);
                state && *state == SwitchState::ON) {
                switch_on_times_[index] = std::chrono::steady_clock::now();
            }
        }
        spdlog::info("[SwitchStats] Statistics reset for switch index: {}",
                     index);
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[SwitchStats] Failed to reset switch statistics: {}",
                      ex.what());
        return false;
    }
}

[[nodiscard]] auto SwitchStats::resetSwitchStatistics(const std::string& name)
    -> bool {
    if (auto switchManager = client_->getSwitchManager()) {
        if (auto indexOpt = switchManager->getSwitchIndex(name)) {
            return resetSwitchStatistics(*indexOpt);
        }
        spdlog::error("[SwitchStats] Switch not found: {}", name);
    }
    return false;
}

void SwitchStats::updateStatistics(uint32_t index, bool switchedOn) {
    std::scoped_lock lock(stats_mutex_);
    ensureVectorSize(index);
    trackSwitchOperation(index);
    if (switchedOn) {
        startSwitchUptime(index);
    } else {
        stopSwitchUptime(index);
    }
}

void SwitchStats::trackSwitchOperation(uint32_t index) {
    ensureVectorSize(index);
    ++switch_operation_counts_[index];
    ++total_operation_count_;
    spdlog::debug("[SwitchStats] Switch {} operation count: {}", index,
                  switch_operation_counts_[index]);
}

void SwitchStats::startSwitchUptime(uint32_t index) {
    ensureVectorSize(index);
    switch_on_times_[index] = std::chrono::steady_clock::now();
    spdlog::debug("[SwitchStats] Started uptime tracking for switch {}", index);
}

void SwitchStats::stopSwitchUptime(uint32_t index) {
    ensureVectorSize(index);
    auto now = std::chrono::steady_clock::now();
    auto sessionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - switch_on_times_[index])
                           .count();
    switch_uptimes_[index] += static_cast<uint64_t>(sessionTime);
    spdlog::debug(
        "[SwitchStats] Stopped uptime tracking for switch {} (session: {}ms, "
        "total: {}ms)",
        index, sessionTime, switch_uptimes_[index]);
}

void SwitchStats::ensureVectorSize(uint32_t index) {
    if (index >= switch_operation_counts_.size()) {
        switch_operation_counts_.resize(index + 1, 0);
        switch_on_times_.resize(index + 1);
        switch_uptimes_.resize(index + 1, 0);
    }
}
