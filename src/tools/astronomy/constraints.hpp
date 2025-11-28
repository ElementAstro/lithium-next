/**
 * @file constraints.hpp
 * @brief Observation constraints for astronomical targets.
 *
 * This file defines constraint types used to determine when and how
 * astronomical targets can be observed, including altitude limits,
 * moon separation, and time windows.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_CONSTRAINTS_HPP
#define LITHIUM_TOOLS_ASTRONOMY_CONSTRAINTS_HPP

#include <chrono>

#include "atom/type/json.hpp"
#include "constants.hpp"

namespace lithium::tools::astronomy {

using json = nlohmann::json;

// ============================================================================
// Altitude Constraints
// ============================================================================

/**
 * @struct AltitudeConstraints
 * @brief Altitude constraints for target observation.
 *
 * Defines the minimum and maximum altitude angles at which a target
 * can be observed, along with any horizon offset adjustments.
 */
struct AltitudeConstraints {
    double minAltitude{15.0};     ///< Minimum altitude to observe (degrees)
    double maxAltitude{85.0};     ///< Maximum altitude to observe (degrees)
    double horizonOffset{0.0};    ///< Additional horizon offset (degrees)

    // ========================================================================
    // Constructors
    // ========================================================================

    AltitudeConstraints() = default;

    AltitudeConstraints(double minAlt, double maxAlt, double offset = 0.0)
        : minAltitude(minAlt), maxAltitude(maxAlt), horizonOffset(offset) {}

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if altitude is within constraints.
     * @param altitude Current altitude in degrees.
     * @return true if altitude is within valid observation range.
     */
    [[nodiscard]] bool isValid(double altitude) const noexcept {
        return altitude >= (minAltitude + horizonOffset) &&
               altitude <= maxAltitude;
    }

    /**
     * @brief Check if constraints themselves are valid.
     * @return true if constraint values are reasonable.
     */
    [[nodiscard]] bool areConstraintsValid() const noexcept {
        return minAltitude >= -90.0 && minAltitude <= 90.0 &&
               maxAltitude >= -90.0 && maxAltitude <= 90.0 &&
               minAltitude < maxAltitude;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {{"minAltitude", minAltitude},
                {"maxAltitude", maxAltitude},
                {"horizonOffset", horizonOffset}};
    }

    [[nodiscard]] static AltitudeConstraints fromJson(const json& j) {
        return {j.value("minAltitude", 15.0),
                j.value("maxAltitude", 85.0),
                j.value("horizonOffset", 0.0)};
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const AltitudeConstraints& other) const noexcept {
        return std::abs(minAltitude - other.minAltitude) < EPSILON &&
               std::abs(maxAltitude - other.maxAltitude) < EPSILON &&
               std::abs(horizonOffset - other.horizonOffset) < EPSILON;
    }

    bool operator!=(const AltitudeConstraints& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// Observability Window
// ============================================================================

/**
 * @struct ObservabilityWindow
 * @brief Time window when target is observable.
 *
 * Defines the rise, transit, and set times for a target, along with
 * information about circumpolar status and maximum altitude.
 */
struct ObservabilityWindow {
    std::chrono::system_clock::time_point riseTime;     ///< Time when target rises
    std::chrono::system_clock::time_point transitTime;  ///< Time at meridian
    std::chrono::system_clock::time_point setTime;      ///< Time when target sets
    double maxAltitude{0.0};      ///< Maximum altitude during window (degrees)
    double transitAzimuth{0.0};   ///< Azimuth at transit (degrees)
    bool isCircumpolar{false};    ///< True if target never sets
    bool neverRises{false};       ///< True if target never rises

    // ========================================================================
    // Constructors
    // ========================================================================

    ObservabilityWindow() = default;

    // ========================================================================
    // Status Queries
    // ========================================================================

    /**
     * @brief Check if target is currently observable.
     * @return true if target is above horizon now.
     */
    [[nodiscard]] bool isObservableNow() const {
        if (neverRises) return false;
        if (isCircumpolar) return true;
        auto now = std::chrono::system_clock::now();
        return now >= riseTime && now <= setTime;
    }

    /**
     * @brief Get remaining observable time in seconds.
     * @return Seconds until target sets, or 0 if not observable.
     */
    [[nodiscard]] int64_t remainingSeconds() const {
        if (neverRises) return 0;
        if (isCircumpolar) return 86400;  // 24 hours
        auto now = std::chrono::system_clock::now();
        if (now > setTime) return 0;
        if (now < riseTime) return 0;
        return std::chrono::duration_cast<std::chrono::seconds>(setTime - now)
            .count();
    }

    /**
     * @brief Get total observable duration in seconds.
     * @return Total seconds target is observable.
     */
    [[nodiscard]] int64_t totalDurationSeconds() const {
        if (neverRises) return 0;
        if (isCircumpolar) return 86400;
        return std::chrono::duration_cast<std::chrono::seconds>(setTime - riseTime)
            .count();
    }

    // ========================================================================
    // Meridian Queries
    // ========================================================================

    /**
     * @brief Check if target has crossed or will cross meridian.
     * @return true if current time is past transit time.
     */
    [[nodiscard]] bool hasCrossedMeridian() const {
        auto now = std::chrono::system_clock::now();
        return now > transitTime;
    }

    /**
     * @brief Get time until meridian crossing.
     * @return Seconds to meridian (negative if already crossed).
     */
    [[nodiscard]] int64_t secondsToMeridian() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
                   transitTime - now)
            .count();
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {
            {"riseTime", std::chrono::system_clock::to_time_t(riseTime)},
            {"transitTime", std::chrono::system_clock::to_time_t(transitTime)},
            {"setTime", std::chrono::system_clock::to_time_t(setTime)},
            {"maxAltitude", maxAltitude},
            {"transitAzimuth", transitAzimuth},
            {"isCircumpolar", isCircumpolar},
            {"neverRises", neverRises}
        };
    }

    [[nodiscard]] static ObservabilityWindow fromJson(const json& j) {
        ObservabilityWindow w;
        w.riseTime = std::chrono::system_clock::from_time_t(
            j.value("riseTime", static_cast<std::time_t>(0)));
        w.transitTime = std::chrono::system_clock::from_time_t(
            j.value("transitTime", static_cast<std::time_t>(0)));
        w.setTime = std::chrono::system_clock::from_time_t(
            j.value("setTime", static_cast<std::time_t>(0)));
        w.maxAltitude = j.value("maxAltitude", 0.0);
        w.transitAzimuth = j.value("transitAzimuth", 0.0);
        w.isCircumpolar = j.value("isCircumpolar", false);
        w.neverRises = j.value("neverRises", false);
        return w;
    }
};

// ============================================================================
// Time Constraints
// ============================================================================

/**
 * @struct TimeConstraints
 * @brief Time-based constraints for observation scheduling.
 *
 * Defines when observations can start and end, useful for scheduling
 * observations during specific time windows.
 */
struct TimeConstraints {
    std::chrono::system_clock::time_point startTime;  ///< Earliest start time
    std::chrono::system_clock::time_point endTime;    ///< Latest end time
    bool enabled{false};  ///< Whether time constraints are active

    // ========================================================================
    // Constructors
    // ========================================================================

    TimeConstraints() = default;

    TimeConstraints(std::chrono::system_clock::time_point start,
                    std::chrono::system_clock::time_point end,
                    bool enable = true)
        : startTime(start), endTime(end), enabled(enable) {}

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Check if current time is within constraints.
     * @return true if within time window or constraints disabled.
     */
    [[nodiscard]] bool isWithinWindow() const {
        if (!enabled) return true;
        auto now = std::chrono::system_clock::now();
        return now >= startTime && now <= endTime;
    }

    /**
     * @brief Check if a specific time is within constraints.
     * @param time Time to check.
     * @return true if within time window or constraints disabled.
     */
    [[nodiscard]] bool isWithinWindow(
        std::chrono::system_clock::time_point time) const {
        if (!enabled) return true;
        return time >= startTime && time <= endTime;
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {
            {"startTime", std::chrono::system_clock::to_time_t(startTime)},
            {"endTime", std::chrono::system_clock::to_time_t(endTime)},
            {"enabled", enabled}
        };
    }

    [[nodiscard]] static TimeConstraints fromJson(const json& j) {
        TimeConstraints tc;
        tc.startTime = std::chrono::system_clock::from_time_t(
            j.value("startTime", static_cast<std::time_t>(0)));
        tc.endTime = std::chrono::system_clock::from_time_t(
            j.value("endTime", static_cast<std::time_t>(0)));
        tc.enabled = j.value("enabled", false);
        return tc;
    }
};

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_CONSTRAINTS_HPP
