#pragma once

#include <optional>
#include <atomic>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief Tracking control component for INDI telescopes
 * 
 * Handles telescope tracking modes, rates, and state management
 */
class TelescopeTracking {
public:
    explicit TelescopeTracking(const std::string& name);
    ~TelescopeTracking() = default;

    /**
     * @brief Initialize tracking component
     */
    auto initialize(INDI::BaseDevice device) -> bool;

    /**
     * @brief Destroy tracking component
     */
    auto destroy() -> bool;

    // Tracking control
    /**
     * @brief Check if tracking is enabled
     */
    auto isTrackingEnabled() -> bool;

    /**
     * @brief Enable or disable tracking
     */
    auto enableTracking(bool enable) -> bool;

    /**
     * @brief Get current track mode
     */
    auto getTrackRate() -> std::optional<TrackMode>;

    /**
     * @brief Set track mode (Sidereal, Solar, Lunar, Custom)
     */
    auto setTrackRate(TrackMode rate) -> bool;

    /**
     * @brief Get motion rates for tracking
     */
    auto getTrackRates() -> MotionRates;

    /**
     * @brief Set custom tracking rates
     */
    auto setTrackRates(const MotionRates& rates) -> bool;

    /**
     * @brief Get current pier side
     */
    auto getPierSide() -> std::optional<PierSide>;

    /**
     * @brief Set pier side (for German equatorial mounts)
     */
    auto setPierSide(PierSide side) -> bool;

    /**
     * @brief Check if telescope can flip sides
     */
    auto canFlipPierSide() -> bool;

    /**
     * @brief Perform meridian flip
     */
    auto flipPierSide() -> bool;

private:
    std::string name_;
    INDI::BaseDevice device_;
    
    // Tracking state
    std::atomic_bool isTrackingEnabled_{false};
    std::atomic_bool isTracking_{false};
    TrackMode trackMode_{TrackMode::SIDEREAL};
    PierSide pierSide_{PierSide::UNKNOWN};
    
    // Tracking rates
    MotionRates trackRates_{};
    std::atomic<double> trackRateRA_{15.041067}; // sidereal rate arcsec/sec
    std::atomic<double> trackRateDEC_{0.0};
    
    // Helper methods
    auto watchTrackingProperties() -> void;
    auto watchPierSideProperties() -> void;
    auto updateTrackingState() -> void;
};
