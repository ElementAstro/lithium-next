/*
 * state.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: PHD2 state management with modern C++ features

*************************************************/

#pragma once

#include "types.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>

namespace phd2 {

/**
 * @brief Guide star information
 */
struct StarInfo {
    double x{0.0};
    double y{0.0};
    double snr{0.0};
    double mass{0.0};
    double hfd{0.0};
    bool valid{false};

    void clear() noexcept {
        x = y = snr = mass = hfd = 0.0;
        valid = false;
    }
};

/**
 * @brief Guide statistics
 */
struct GuideStatistics {
    double rmsRA{0.0};      // arcsec
    double rmsDec{0.0};     // arcsec
    double rmsTotal{0.0};   // arcsec
    double peakRA{0.0};     // arcsec
    double peakDec{0.0};    // arcsec
    double avgDist{0.0};    // pixels
    int sampleCount{0};

    void clear() noexcept {
        rmsRA = rmsDec = rmsTotal = peakRA = peakDec = avgDist = 0.0;
        sampleCount = 0;
    }

    void update(double raError, double decError) noexcept {
        // Simple running average (could be improved with proper RMS calculation)
        sampleCount++;
        rmsRA = (rmsRA * (sampleCount - 1) + std::abs(raError)) / sampleCount;
        rmsDec = (rmsDec * (sampleCount - 1) + std::abs(decError)) / sampleCount;
        rmsTotal = std::sqrt(rmsRA * rmsRA + rmsDec * rmsDec);
        peakRA = std::max(peakRA, std::abs(raError));
        peakDec = std::max(peakDec, std::abs(decError));
    }
};

/**
 * @brief Calibration data
 */
struct CalibrationInfo {
    bool calibrated{false};
    double raRate{0.0};     // arcsec/sec
    double decRate{0.0};    // arcsec/sec
    double raAngle{0.0};    // degrees
    double decAngle{0.0};   // degrees
    bool decFlipped{false};
    std::string timestamp;

    void clear() noexcept {
        calibrated = false;
        raRate = decRate = raAngle = decAngle = 0.0;
        decFlipped = false;
        timestamp.clear();
    }
};

/**
 * @brief PHD2 state manager
 *
 * Thread-safe state tracking for PHD2 client
 */
class StateManager {
public:
    StateManager() = default;

    // ==================== Application State ====================

    /**
     * @brief Get current application state
     */
    [[nodiscard]] auto getAppState() const noexcept -> AppStateType;

    /**
     * @brief Set application state
     */
    void setAppState(AppStateType state) noexcept;

    /**
     * @brief Set application state from string
     */
    void setAppState(std::string_view stateStr) noexcept;

    /**
     * @brief Check if guiding
     */
    [[nodiscard]] auto isGuiding() const noexcept -> bool;

    /**
     * @brief Check if calibrating
     */
    [[nodiscard]] auto isCalibrating() const noexcept -> bool;

    /**
     * @brief Check if paused
     */
    [[nodiscard]] auto isPaused() const noexcept -> bool;

    /**
     * @brief Check if looping
     */
    [[nodiscard]] auto isLooping() const noexcept -> bool;

    // ==================== Star Information ====================

    /**
     * @brief Get current star info
     */
    [[nodiscard]] auto getStar() const -> StarInfo;

    /**
     * @brief Update star info
     */
    void updateStar(const StarInfo& star);

    /**
     * @brief Update star from guide step event
     */
    void updateStarFromGuideStep(const GuideStepEvent& event);

    /**
     * @brief Clear star info
     */
    void clearStar();

    // ==================== Guide Statistics ====================

    /**
     * @brief Get guide statistics
     */
    [[nodiscard]] auto getStats() const -> GuideStatistics;

    /**
     * @brief Update statistics from guide step
     */
    void updateStats(double raError, double decError);

    /**
     * @brief Reset statistics
     */
    void resetStats();

    // ==================== Calibration ====================

    /**
     * @brief Get calibration info
     */
    [[nodiscard]] auto getCalibration() const -> CalibrationInfo;

    /**
     * @brief Set calibration info
     */
    void setCalibration(const CalibrationInfo& cal);

    /**
     * @brief Mark as calibrated
     */
    void setCalibrated(bool calibrated);

    /**
     * @brief Clear calibration
     */
    void clearCalibration();

    // ==================== Settle State ====================

    /**
     * @brief Check if settling
     */
    [[nodiscard]] auto isSettling() const noexcept -> bool;

    /**
     * @brief Set settling state
     */
    void setSettling(bool settling) noexcept;

    // ==================== Connection State ====================

    /**
     * @brief Check if equipment connected
     */
    [[nodiscard]] auto isEquipmentConnected() const noexcept -> bool;

    /**
     * @brief Set equipment connection state
     */
    void setEquipmentConnected(bool connected) noexcept;

    // ==================== Event Processing ====================

    /**
     * @brief Process PHD2 event and update state
     */
    void processEvent(const Event& event);

    // ==================== Reset ====================

    /**
     * @brief Reset all state
     */
    void reset();

private:
    mutable std::mutex mutex_;

    std::atomic<AppStateType> appState_{AppStateType::Stopped};
    std::atomic<bool> settling_{false};
    std::atomic<bool> equipmentConnected_{false};

    StarInfo star_;
    GuideStatistics stats_;
    CalibrationInfo calibration_;
};

}  // namespace phd2
