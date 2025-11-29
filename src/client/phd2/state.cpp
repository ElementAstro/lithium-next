/*
 * state.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: PHD2 state management implementation

*************************************************/

#include "state.hpp"

#include <cmath>

namespace phd2 {

// ==================== Application State ====================

auto StateManager::getAppState() const noexcept -> AppStateType {
    return appState_.load();
}

void StateManager::setAppState(AppStateType state) noexcept {
    appState_.store(state);
}

void StateManager::setAppState(std::string_view stateStr) noexcept {
    appState_.store(appStateFromString(stateStr));
}

auto StateManager::isGuiding() const noexcept -> bool {
    return appState_.load() == AppStateType::Guiding;
}

auto StateManager::isCalibrating() const noexcept -> bool {
    return appState_.load() == AppStateType::Calibrating;
}

auto StateManager::isPaused() const noexcept -> bool {
    return appState_.load() == AppStateType::Paused;
}

auto StateManager::isLooping() const noexcept -> bool {
    return appState_.load() == AppStateType::Looping;
}

// ==================== Star Information ====================

auto StateManager::getStar() const -> StarInfo {
    std::lock_guard lock(mutex_);
    return star_;
}

void StateManager::updateStar(const StarInfo& star) {
    std::lock_guard lock(mutex_);
    star_ = star;
}

void StateManager::updateStarFromGuideStep(const GuideStepEvent& event) {
    std::lock_guard lock(mutex_);
    star_.snr = event.snr;
    star_.mass = event.starMass;
    star_.hfd = event.hfd;
    star_.valid = true;
}

void StateManager::clearStar() {
    std::lock_guard lock(mutex_);
    star_.clear();
}

// ==================== Guide Statistics ====================

auto StateManager::getStats() const -> GuideStatistics {
    std::lock_guard lock(mutex_);
    return stats_;
}

void StateManager::updateStats(double raError, double decError) {
    std::lock_guard lock(mutex_);
    stats_.update(raError, decError);
}

void StateManager::resetStats() {
    std::lock_guard lock(mutex_);
    stats_.clear();
}

// ==================== Calibration ====================

auto StateManager::getCalibration() const -> CalibrationInfo {
    std::lock_guard lock(mutex_);
    return calibration_;
}

void StateManager::setCalibration(const CalibrationInfo& cal) {
    std::lock_guard lock(mutex_);
    calibration_ = cal;
}

void StateManager::setCalibrated(bool calibrated) {
    std::lock_guard lock(mutex_);
    calibration_.calibrated = calibrated;
}

void StateManager::clearCalibration() {
    std::lock_guard lock(mutex_);
    calibration_.clear();
}

// ==================== Settle State ====================

auto StateManager::isSettling() const noexcept -> bool {
    return settling_.load();
}

void StateManager::setSettling(bool settling) noexcept {
    settling_.store(settling);
}

// ==================== Connection State ====================

auto StateManager::isEquipmentConnected() const noexcept -> bool {
    return equipmentConnected_.load();
}

void StateManager::setEquipmentConnected(bool connected) noexcept {
    equipmentConnected_.store(connected);
}

// ==================== Event Processing ====================

void StateManager::processEvent(const Event& event) {
    std::visit(
        [this](auto&& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, AppStateEvent>) {
                setAppState(e.state);
            } else if constexpr (std::is_same_v<T, GuideStepEvent>) {
                updateStarFromGuideStep(e);
                updateStats(e.raDistanceRaw, e.decDistanceRaw);
            } else if constexpr (std::is_same_v<T, SettleBeginEvent>) {
                setSettling(true);
            } else if constexpr (std::is_same_v<T, SettleDoneEvent>) {
                setSettling(false);
            } else if constexpr (std::is_same_v<T, StarLostEvent>) {
                setAppState(AppStateType::LostLock);
                clearStar();
            } else if constexpr (std::is_same_v<T, CalibrationCompleteEvent>) {
                setCalibrated(true);
            } else if constexpr (std::is_same_v<T, CalibrationFailedEvent>) {
                setCalibrated(false);
            } else if constexpr (std::is_same_v<T, StartGuidingEvent>) {
                setAppState(AppStateType::Guiding);
                resetStats();
            } else if constexpr (std::is_same_v<T, GuidingStoppedEvent>) {
                setAppState(AppStateType::Stopped);
            } else if constexpr (std::is_same_v<T, PausedEvent>) {
                setAppState(AppStateType::Paused);
            } else if constexpr (std::is_same_v<T, ResumedEvent>) {
                setAppState(AppStateType::Guiding);
            }
        },
        event);
}

// ==================== Reset ====================

void StateManager::reset() {
    appState_.store(AppStateType::Stopped);
    settling_.store(false);
    equipmentConnected_.store(false);

    std::lock_guard lock(mutex_);
    star_.clear();
    stats_.clear();
    calibration_.clear();
}

}  // namespace phd2
