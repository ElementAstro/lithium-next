/**
 * @file meridian.hpp
 * @brief Meridian flip state and information for equatorial mounts.
 *
 * This file defines types for tracking meridian state and flip timing,
 * essential for German equatorial mount operation.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_MERIDIAN_HPP
#define LITHIUM_TOOLS_ASTRONOMY_MERIDIAN_HPP

#include <chrono>
#include <cmath>

#include "atom/type/json.hpp"
#include "constants.hpp"

namespace lithium::tools::astronomy {

using json = nlohmann::json;

// ============================================================================
// Meridian State Enumeration
// ============================================================================

/**
 * @enum MeridianState
 * @brief Current meridian state for a target.
 *
 * Indicates the position of a target relative to the meridian,
 * which is important for German equatorial mount operation.
 */
enum class MeridianState {
    East,           ///< Target is east of meridian
    West,           ///< Target is west of meridian
    NearMeridian,   ///< Target is near meridian (within offset)
    Unknown         ///< State unknown or not calculated
};

/**
 * @brief Convert MeridianState to string representation.
 * @param state The meridian state.
 * @return String representation of the state.
 */
[[nodiscard]] inline const char* meridianStateToString(MeridianState state) noexcept {
    switch (state) {
        case MeridianState::East: return "East";
        case MeridianState::West: return "West";
        case MeridianState::NearMeridian: return "NearMeridian";
        case MeridianState::Unknown: return "Unknown";
        default: return "Invalid";
    }
}

/**
 * @brief Convert string to MeridianState.
 * @param str String representation.
 * @return Corresponding MeridianState, or Unknown if invalid.
 */
[[nodiscard]] inline MeridianState stringToMeridianState(const std::string& str) noexcept {
    if (str == "East") return MeridianState::East;
    if (str == "West") return MeridianState::West;
    if (str == "NearMeridian") return MeridianState::NearMeridian;
    return MeridianState::Unknown;
}

// ============================================================================
// Meridian Flip Information
// ============================================================================

/**
 * @struct MeridianFlipInfo
 * @brief Information about meridian flip timing and state.
 *
 * Tracks the current meridian state, expected flip time, and whether
 * a flip has been completed. Essential for automated observation scheduling.
 */
struct MeridianFlipInfo {
    MeridianState currentState{MeridianState::Unknown};  ///< Current meridian state
    std::chrono::system_clock::time_point flipTime;      ///< Expected flip time
    bool flipRequired{false};     ///< Whether flip is required
    bool flipCompleted{false};    ///< Whether flip has been completed
    double hourAngle{0.0};        ///< Current hour angle (hours, -12 to +12)

    // ========================================================================
    // Constructors
    // ========================================================================

    MeridianFlipInfo() = default;

    // ========================================================================
    // Status Queries
    // ========================================================================

    /**
     * @brief Get time until flip (negative if already passed).
     * @return Seconds until flip, or 0 if not required.
     */
    [[nodiscard]] int64_t secondsToFlip() const {
        if (!flipRequired) return 0;
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(flipTime - now)
            .count();
    }

    /**
     * @brief Check if flip is imminent (within threshold).
     * @param thresholdSeconds Threshold in seconds.
     * @return true if flip is required and within threshold.
     */
    [[nodiscard]] bool isFlipImminent(int64_t thresholdSeconds = 300) const {
        if (!flipRequired || flipCompleted) return false;
        auto seconds = secondsToFlip();
        return seconds > 0 && seconds <= thresholdSeconds;
    }

    /**
     * @brief Check if flip is overdue.
     * @return true if flip was required but time has passed.
     */
    [[nodiscard]] bool isFlipOverdue() const {
        if (!flipRequired || flipCompleted) return false;
        return secondsToFlip() < 0;
    }

    /**
     * @brief Check if target is currently trackable.
     * @return true if no flip required or flip completed.
     */
    [[nodiscard]] bool isTrackable() const noexcept {
        return !flipRequired || flipCompleted;
    }

    // ========================================================================
    // Hour Angle Utilities
    // ========================================================================

    /**
     * @brief Determine meridian state from hour angle.
     * @param ha Hour angle in hours (-12 to +12).
     * @param nearThreshold Threshold for "near meridian" in hours.
     * @return Corresponding MeridianState.
     */
    [[nodiscard]] static MeridianState stateFromHourAngle(
        double ha, double nearThreshold = 0.5) noexcept {
        if (std::abs(ha) <= nearThreshold) {
            return MeridianState::NearMeridian;
        }
        return ha < 0 ? MeridianState::East : MeridianState::West;
    }

    /**
     * @brief Update state based on current hour angle.
     * @param nearThreshold Threshold for "near meridian" in hours.
     */
    void updateStateFromHourAngle(double nearThreshold = 0.5) {
        currentState = stateFromHourAngle(hourAngle, nearThreshold);
    }

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {
            {"currentState", static_cast<int>(currentState)},
            {"flipTime", std::chrono::system_clock::to_time_t(flipTime)},
            {"flipRequired", flipRequired},
            {"flipCompleted", flipCompleted},
            {"hourAngle", hourAngle}
        };
    }

    [[nodiscard]] static MeridianFlipInfo fromJson(const json& j) {
        MeridianFlipInfo info;
        info.currentState = static_cast<MeridianState>(j.value("currentState", 3));
        info.flipTime = std::chrono::system_clock::from_time_t(
            j.value("flipTime", static_cast<std::time_t>(0)));
        info.flipRequired = j.value("flipRequired", false);
        info.flipCompleted = j.value("flipCompleted", false);
        info.hourAngle = j.value("hourAngle", 0.0);
        return info;
    }

    // ========================================================================
    // Operators
    // ========================================================================

    bool operator==(const MeridianFlipInfo& other) const noexcept {
        return currentState == other.currentState &&
               flipRequired == other.flipRequired &&
               flipCompleted == other.flipCompleted &&
               std::abs(hourAngle - other.hourAngle) < EPSILON;
    }

    bool operator!=(const MeridianFlipInfo& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================================
// Meridian Flip Settings
// ============================================================================

/**
 * @struct MeridianFlipSettings
 * @brief Configuration settings for meridian flip behavior.
 *
 * Defines how the system should handle meridian flips, including
 * timing offsets and automation preferences.
 */
struct MeridianFlipSettings {
    double flipOffset{0.0};       ///< Minutes past meridian before flip
    bool autoFlip{true};          ///< Allow automatic meridian flip
    bool avoidFlipDuringExposure{false};  ///< Avoid flip during exposure
    double pauseBeforeFlip{30.0}; ///< Seconds to pause before flip
    double pauseAfterFlip{30.0};  ///< Seconds to pause after flip
    bool recenterAfterFlip{true}; ///< Re-center target after flip
    bool refocusAfterFlip{false}; ///< Refocus after flip

    // ========================================================================
    // Constructors
    // ========================================================================

    MeridianFlipSettings() = default;

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json toJson() const {
        return {
            {"flipOffset", flipOffset},
            {"autoFlip", autoFlip},
            {"avoidFlipDuringExposure", avoidFlipDuringExposure},
            {"pauseBeforeFlip", pauseBeforeFlip},
            {"pauseAfterFlip", pauseAfterFlip},
            {"recenterAfterFlip", recenterAfterFlip},
            {"refocusAfterFlip", refocusAfterFlip}
        };
    }

    [[nodiscard]] static MeridianFlipSettings fromJson(const json& j) {
        MeridianFlipSettings s;
        s.flipOffset = j.value("flipOffset", 0.0);
        s.autoFlip = j.value("autoFlip", true);
        s.avoidFlipDuringExposure = j.value("avoidFlipDuringExposure", false);
        s.pauseBeforeFlip = j.value("pauseBeforeFlip", 30.0);
        s.pauseAfterFlip = j.value("pauseAfterFlip", 30.0);
        s.recenterAfterFlip = j.value("recenterAfterFlip", true);
        s.refocusAfterFlip = j.value("refocusAfterFlip", false);
        return s;
    }
};

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_MERIDIAN_HPP
