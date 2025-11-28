/**
 * @file astro_types.hpp
 * @brief Astronomical data types for target observation planning.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_ASTRO_TYPES_HPP
#define LITHIUM_TASK_ASTRO_TYPES_HPP

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

// ============================================================================
// Constants
// ============================================================================

constexpr double DEG_TO_RAD = M_PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / M_PI;
constexpr double HOURS_TO_DEG = 15.0;

// ============================================================================
// Celestial Coordinates
// ============================================================================

/**
 * @struct Coordinates
 * @brief Celestial coordinates for an astronomical target.
 */
struct Coordinates {
    double ra{0.0};        ///< Right Ascension in degrees (0-360)
    double dec{0.0};       ///< Declination in degrees (-90 to +90)
    double epoch{2000.0};  ///< Coordinate epoch (default J2000.0)

    /// @brief Convert RA to hours (0-24)
    [[nodiscard]] double raHours() const { return ra / HOURS_TO_DEG; }

    /// @brief Convert RA hours to degrees
    static double hoursToRA(double hours) { return hours * HOURS_TO_DEG; }

    /// @brief Check if coordinates are valid
    [[nodiscard]] bool isValid() const {
        return ra >= 0.0 && ra < 360.0 && dec >= -90.0 && dec <= 90.0;
    }

    /// @brief Calculate angular separation from another coordinate (degrees)
    [[nodiscard]] double separationFrom(const Coordinates& other) const {
        double ra1 = ra * DEG_TO_RAD;
        double dec1 = dec * DEG_TO_RAD;
        double ra2 = other.ra * DEG_TO_RAD;
        double dec2 = other.dec * DEG_TO_RAD;
        double cosAngle = std::sin(dec1) * std::sin(dec2) +
                          std::cos(dec1) * std::cos(dec2) * std::cos(ra1 - ra2);
        return std::acos(std::clamp(cosAngle, -1.0, 1.0)) * RAD_TO_DEG;
    }

    /// @brief Create from RA in hours and Dec in degrees
    static Coordinates fromHMS(double raHours, double decDeg,
                               double epoch = 2000.0) {
        return {raHours * HOURS_TO_DEG, decDeg, epoch};
    }

    [[nodiscard]] json toJson() const {
        return {{"ra", ra}, {"dec", dec}, {"epoch", epoch}};
    }

    static Coordinates fromJson(const json& j) {
        return {j.value("ra", 0.0), j.value("dec", 0.0),
                j.value("epoch", 2000.0)};
    }
};

// ============================================================================
// Observer Location
// ============================================================================

/**
 * @struct ObserverLocation
 * @brief Geographic location of the observer.
 */
struct ObserverLocation {
    double latitude{0.0};    ///< Latitude in degrees (-90 to +90)
    double longitude{0.0};   ///< Longitude in degrees (-180 to +180)
    double elevation{0.0};   ///< Elevation in meters

    [[nodiscard]] bool isValid() const {
        return latitude >= -90.0 && latitude <= 90.0 &&
               longitude >= -180.0 && longitude <= 180.0;
    }

    [[nodiscard]] json toJson() const {
        return {{"latitude", latitude},
                {"longitude", longitude},
                {"elevation", elevation}};
    }

    static ObserverLocation fromJson(const json& j) {
        return {j.value("latitude", 0.0), j.value("longitude", 0.0),
                j.value("elevation", 0.0)};
    }
};

// ============================================================================
// Observability Window
// ============================================================================

/**
 * @struct ObservabilityWindow
 * @brief Time window when target is observable.
 */
struct ObservabilityWindow {
    std::chrono::system_clock::time_point riseTime;     ///< Time when target rises
    std::chrono::system_clock::time_point transitTime;  ///< Time at meridian
    std::chrono::system_clock::time_point setTime;      ///< Time when target sets
    double maxAltitude{0.0};      ///< Maximum altitude during window (degrees)
    double transitAzimuth{0.0};   ///< Azimuth at transit (degrees)
    bool isCircumpolar{false};    ///< True if target never sets
    bool neverRises{false};       ///< True if target never rises

    /// @brief Check if target is currently observable
    [[nodiscard]] bool isObservableNow() const {
        if (neverRises) return false;
        if (isCircumpolar) return true;
        auto now = std::chrono::system_clock::now();
        return now >= riseTime && now <= setTime;
    }

    /// @brief Get remaining observable time in seconds
    [[nodiscard]] int64_t remainingSeconds() const {
        if (neverRises) return 0;
        if (isCircumpolar) return 86400;  // 24 hours
        auto now = std::chrono::system_clock::now();
        if (now > setTime) return 0;
        if (now < riseTime) return 0;
        return std::chrono::duration_cast<std::chrono::seconds>(setTime - now)
            .count();
    }

    /// @brief Check if target has crossed or will cross meridian
    [[nodiscard]] bool hasCrossedMeridian() const {
        auto now = std::chrono::system_clock::now();
        return now > transitTime;
    }

    /// @brief Get time until meridian crossing (negative if already crossed)
    [[nodiscard]] int64_t secondsToMeridian() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
                   transitTime - now)
            .count();
    }

    [[nodiscard]] json toJson() const {
        return {
            {"riseTime",
             std::chrono::system_clock::to_time_t(riseTime)},
            {"transitTime",
             std::chrono::system_clock::to_time_t(transitTime)},
            {"setTime", std::chrono::system_clock::to_time_t(setTime)},
            {"maxAltitude", maxAltitude},
            {"transitAzimuth", transitAzimuth},
            {"isCircumpolar", isCircumpolar},
            {"neverRises", neverRises}};
    }

    static ObservabilityWindow fromJson(const json& j) {
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
// Altitude/Azimuth Constraints
// ============================================================================

/**
 * @struct AltitudeConstraints
 * @brief Altitude constraints for target observation.
 */
struct AltitudeConstraints {
    double minAltitude{15.0};     ///< Minimum altitude to observe (degrees)
    double maxAltitude{85.0};     ///< Maximum altitude to observe (degrees)
    double horizonOffset{0.0};    ///< Additional horizon offset (degrees)

    /// @brief Check if altitude is within constraints
    [[nodiscard]] bool isValid(double altitude) const {
        return altitude >= (minAltitude + horizonOffset) &&
               altitude <= maxAltitude;
    }

    [[nodiscard]] json toJson() const {
        return {{"minAltitude", minAltitude},
                {"maxAltitude", maxAltitude},
                {"horizonOffset", horizonOffset}};
    }

    static AltitudeConstraints fromJson(const json& j) {
        return {j.value("minAltitude", 15.0), j.value("maxAltitude", 85.0),
                j.value("horizonOffset", 0.0)};
    }
};

// ============================================================================
// Exposure Plan
// ============================================================================

/**
 * @struct ExposurePlan
 * @brief Single exposure plan entry for a filter.
 */
struct ExposurePlan {
    std::string filterName;       ///< Filter name (e.g., "L", "R", "Ha")
    double exposureTime{0.0};     ///< Exposure time in seconds
    int count{1};                 ///< Number of exposures planned
    int completedCount{0};        ///< Number of completed exposures
    int binning{1};               ///< Binning (1x1, 2x2, etc.)
    int gain{-1};                 ///< Camera gain (-1 = use default)
    int offset{-1};               ///< Camera offset (-1 = use default)
    bool ditherEnabled{true};     ///< Enable dithering between exposures
    int ditherEvery{1};           ///< Dither every N exposures

    /// @brief Get remaining exposures
    [[nodiscard]] int remaining() const {
        return std::max(0, count - completedCount);
    }

    /// @brief Get progress percentage
    [[nodiscard]] double progress() const {
        if (count == 0) return 100.0;
        return (static_cast<double>(completedCount) / count) * 100.0;
    }

    /// @brief Check if plan is complete
    [[nodiscard]] bool isComplete() const { return completedCount >= count; }

    /// @brief Get total exposure time for this plan (seconds)
    [[nodiscard]] double totalExposureTime() const {
        return exposureTime * count;
    }

    /// @brief Get remaining exposure time (seconds)
    [[nodiscard]] double remainingExposureTime() const {
        return exposureTime * remaining();
    }

    [[nodiscard]] json toJson() const {
        return {{"filterName", filterName},
                {"exposureTime", exposureTime},
                {"count", count},
                {"completedCount", completedCount},
                {"binning", binning},
                {"gain", gain},
                {"offset", offset},
                {"ditherEnabled", ditherEnabled},
                {"ditherEvery", ditherEvery}};
    }

    static ExposurePlan fromJson(const json& j) {
        ExposurePlan p;
        p.filterName = j.value("filterName", "");
        p.exposureTime = j.value("exposureTime", 0.0);
        p.count = j.value("count", 1);
        p.completedCount = j.value("completedCount", 0);
        p.binning = j.value("binning", 1);
        p.gain = j.value("gain", -1);
        p.offset = j.value("offset", -1);
        p.ditherEnabled = j.value("ditherEnabled", true);
        p.ditherEvery = j.value("ditherEvery", 1);
        return p;
    }
};

// ============================================================================
// Target Configuration
// ============================================================================

/**
 * @struct TargetConfig
 * @brief Complete configuration for an astronomical target.
 */
struct TargetConfig {
    // Basic Info
    std::string catalogName;        ///< Catalog name (e.g., "M31", "NGC 7000")
    std::string commonName;         ///< Common name (e.g., "Andromeda Galaxy")
    std::string objectType;         ///< Object type (galaxy, nebula, cluster...)

    // Coordinates
    Coordinates coordinates;        ///< Target coordinates
    double rotation{0.0};           ///< Camera rotation angle (degrees)

    // Constraints
    AltitudeConstraints altConstraints;  ///< Altitude constraints
    double minMoonSeparation{30.0};      ///< Minimum moon separation (degrees)
    bool avoidMeridianFlip{false};       ///< Avoid meridian flip during exposure

    // Meridian flip settings
    double meridianFlipOffset{0.0};      ///< Minutes past meridian before flip
    bool autoMeridianFlip{true};         ///< Allow automatic meridian flip

    // Exposure Plans
    std::vector<ExposurePlan> exposurePlans;  ///< List of exposure plans

    // Timing
    std::chrono::system_clock::time_point startTime;  ///< Earliest start time
    std::chrono::system_clock::time_point endTime;    ///< Latest end time
    bool useTimeConstraints{false};    ///< Whether to use time constraints

    // Priority
    int priority{5};                   ///< Target priority (1-10, higher=more important)

    // Acquisition settings
    bool slewRequired{true};           ///< Whether slew is needed
    bool centeringRequired{true};      ///< Whether plate solve centering is needed
    bool guidingRequired{true};        ///< Whether guiding is needed
    bool focusRequired{true};          ///< Whether focus check is needed

    // Focus settings
    bool autoFocusOnStart{true};       ///< Auto focus when target starts
    bool autoFocusOnFilterChange{false};  ///< Auto focus on filter change
    double focusTempThreshold{1.0};    ///< Temperature change threshold for refocus

    /// @brief Get total planned exposure time across all plans (seconds)
    [[nodiscard]] double totalPlannedExposureTime() const {
        double total = 0.0;
        for (const auto& plan : exposurePlans) {
            total += plan.totalExposureTime();
        }
        return total;
    }

    /// @brief Get total remaining exposure time (seconds)
    [[nodiscard]] double totalRemainingExposureTime() const {
        double total = 0.0;
        for (const auto& plan : exposurePlans) {
            total += plan.remainingExposureTime();
        }
        return total;
    }

    /// @brief Get overall progress percentage
    [[nodiscard]] double overallProgress() const {
        double totalPlanned = 0.0;
        double totalCompleted = 0.0;
        for (const auto& plan : exposurePlans) {
            totalPlanned += plan.count;
            totalCompleted += plan.completedCount;
        }
        if (totalPlanned == 0) return 100.0;
        return (totalCompleted / totalPlanned) * 100.0;
    }

    /// @brief Check if all exposure plans are complete
    [[nodiscard]] bool isComplete() const {
        for (const auto& plan : exposurePlans) {
            if (!plan.isComplete()) return false;
        }
        return !exposurePlans.empty();
    }

    [[nodiscard]] json toJson() const {
        json plansJson = json::array();
        for (const auto& plan : exposurePlans) {
            plansJson.push_back(plan.toJson());
        }

        return {{"catalogName", catalogName},
                {"commonName", commonName},
                {"objectType", objectType},
                {"coordinates", coordinates.toJson()},
                {"rotation", rotation},
                {"altConstraints", altConstraints.toJson()},
                {"minMoonSeparation", minMoonSeparation},
                {"avoidMeridianFlip", avoidMeridianFlip},
                {"meridianFlipOffset", meridianFlipOffset},
                {"autoMeridianFlip", autoMeridianFlip},
                {"exposurePlans", plansJson},
                {"startTime",
                 std::chrono::system_clock::to_time_t(startTime)},
                {"endTime", std::chrono::system_clock::to_time_t(endTime)},
                {"useTimeConstraints", useTimeConstraints},
                {"priority", priority},
                {"slewRequired", slewRequired},
                {"centeringRequired", centeringRequired},
                {"guidingRequired", guidingRequired},
                {"focusRequired", focusRequired},
                {"autoFocusOnStart", autoFocusOnStart},
                {"autoFocusOnFilterChange", autoFocusOnFilterChange},
                {"focusTempThreshold", focusTempThreshold}};
    }

    static TargetConfig fromJson(const json& j) {
        TargetConfig cfg;
        cfg.catalogName = j.value("catalogName", "");
        cfg.commonName = j.value("commonName", "");
        cfg.objectType = j.value("objectType", "");
        if (j.contains("coordinates")) {
            cfg.coordinates = Coordinates::fromJson(j["coordinates"]);
        }
        cfg.rotation = j.value("rotation", 0.0);
        if (j.contains("altConstraints")) {
            cfg.altConstraints = AltitudeConstraints::fromJson(j["altConstraints"]);
        }
        cfg.minMoonSeparation = j.value("minMoonSeparation", 30.0);
        cfg.avoidMeridianFlip = j.value("avoidMeridianFlip", false);
        cfg.meridianFlipOffset = j.value("meridianFlipOffset", 0.0);
        cfg.autoMeridianFlip = j.value("autoMeridianFlip", true);
        if (j.contains("exposurePlans") && j["exposurePlans"].is_array()) {
            for (const auto& p : j["exposurePlans"]) {
                cfg.exposurePlans.push_back(ExposurePlan::fromJson(p));
            }
        }
        cfg.startTime = std::chrono::system_clock::from_time_t(
            j.value("startTime", static_cast<std::time_t>(0)));
        cfg.endTime = std::chrono::system_clock::from_time_t(
            j.value("endTime", static_cast<std::time_t>(0)));
        cfg.useTimeConstraints = j.value("useTimeConstraints", false);
        cfg.priority = j.value("priority", 5);
        cfg.slewRequired = j.value("slewRequired", true);
        cfg.centeringRequired = j.value("centeringRequired", true);
        cfg.guidingRequired = j.value("guidingRequired", true);
        cfg.focusRequired = j.value("focusRequired", true);
        cfg.autoFocusOnStart = j.value("autoFocusOnStart", true);
        cfg.autoFocusOnFilterChange = j.value("autoFocusOnFilterChange", false);
        cfg.focusTempThreshold = j.value("focusTempThreshold", 1.0);
        return cfg;
    }
};

// ============================================================================
// Meridian Flip State
// ============================================================================

/**
 * @enum MeridianState
 * @brief Current meridian state for a target.
 */
enum class MeridianState {
    East,           ///< Target is east of meridian
    West,           ///< Target is west of meridian
    NearMeridian,   ///< Target is near meridian (within offset)
    Unknown         ///< State unknown
};

/**
 * @struct MeridianFlipInfo
 * @brief Information about meridian flip timing.
 */
struct MeridianFlipInfo {
    MeridianState currentState{MeridianState::Unknown};
    std::chrono::system_clock::time_point flipTime;  ///< Expected flip time
    bool flipRequired{false};     ///< Whether flip is required
    bool flipCompleted{false};    ///< Whether flip has been completed
    double hourAngle{0.0};        ///< Current hour angle (hours, -12 to +12)

    /// @brief Get time until flip (negative if already passed)
    [[nodiscard]] int64_t secondsToFlip() const {
        if (!flipRequired) return 0;
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(flipTime - now)
            .count();
    }

    [[nodiscard]] json toJson() const {
        return {
            {"currentState", static_cast<int>(currentState)},
            {"flipTime", std::chrono::system_clock::to_time_t(flipTime)},
            {"flipRequired", flipRequired},
            {"flipCompleted", flipCompleted},
            {"hourAngle", hourAngle}};
    }

    static MeridianFlipInfo fromJson(const json& j) {
        MeridianFlipInfo info;
        info.currentState = static_cast<MeridianState>(j.value("currentState", 3));
        info.flipTime = std::chrono::system_clock::from_time_t(
            j.value("flipTime", static_cast<std::time_t>(0)));
        info.flipRequired = j.value("flipRequired", false);
        info.flipCompleted = j.value("flipCompleted", false);
        info.hourAngle = j.value("hourAngle", 0.0);
        return info;
    }
};

// ============================================================================
// Current Position (Alt/Az)
// ============================================================================

/**
 * @struct HorizontalCoordinates
 * @brief Altitude and azimuth coordinates.
 */
struct HorizontalCoordinates {
    double altitude{0.0};   ///< Altitude in degrees (0-90)
    double azimuth{0.0};    ///< Azimuth in degrees (0-360, N=0, E=90)

    [[nodiscard]] bool isAboveHorizon() const { return altitude > 0.0; }

    [[nodiscard]] json toJson() const {
        return {{"altitude", altitude}, {"azimuth", azimuth}};
    }

    static HorizontalCoordinates fromJson(const json& j) {
        return {j.value("altitude", 0.0), j.value("azimuth", 0.0)};
    }
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_ASTRO_TYPES_HPP
