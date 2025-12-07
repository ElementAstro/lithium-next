/*
 * mount.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Mount data models for HTTP/WebSocket responses

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_MOUNT_HPP
#define LITHIUM_SERVER_MODELS_MOUNT_HPP

#include <optional>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::models::mount {

using json = nlohmann::json;

/**
 * @brief Mount tracking state
 */
enum class TrackingState { Off, Sidereal, Lunar, Solar, Custom };

/**
 * @brief Convert TrackingState to string
 */
inline auto trackingStateToString(TrackingState state) -> std::string {
    switch (state) {
        case TrackingState::Off:
            return "off";
        case TrackingState::Sidereal:
            return "sidereal";
        case TrackingState::Lunar:
            return "lunar";
        case TrackingState::Solar:
            return "solar";
        case TrackingState::Custom:
            return "custom";
    }
    return "unknown";
}

/**
 * @brief Mount pier side
 */
enum class PierSide { East, West, Unknown };

/**
 * @brief Convert PierSide to string
 */
inline auto pierSideToString(PierSide side) -> std::string {
    switch (side) {
        case PierSide::East:
            return "east";
        case PierSide::West:
            return "west";
        case PierSide::Unknown:
            return "unknown";
    }
    return "unknown";
}

/**
 * @brief Equatorial coordinates
 */
struct EquatorialCoords {
    double ra;   ///< Right Ascension in hours
    double dec;  ///< Declination in degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"ra", ra}, {"dec", dec}};
    }

    /**
     * @brief Convert RA to sexagesimal string (HH:MM:SS.ss)
     */
    [[nodiscard]] auto raToString() const -> std::string {
        int hours = static_cast<int>(ra);
        int minutes = static_cast<int>((ra - hours) * 60);
        double seconds = ((ra - hours) * 60 - minutes) * 60;
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d:%02d:%05.2f", hours, minutes, seconds);
        return buf;
    }

    /**
     * @brief Convert Dec to sexagesimal string (DD:MM:SS.s)
     */
    [[nodiscard]] auto decToString() const -> std::string {
        char sign = dec >= 0 ? '+' : '-';
        double absDec = std::abs(dec);
        int degrees = static_cast<int>(absDec);
        int minutes = static_cast<int>((absDec - degrees) * 60);
        double seconds = ((absDec - degrees) * 60 - minutes) * 60;
        char buf[32];
        snprintf(buf, sizeof(buf), "%c%02d:%02d:%04.1f", sign, degrees, minutes,
                 seconds);
        return buf;
    }
};

/**
 * @brief Horizontal coordinates
 */
struct HorizontalCoords {
    double alt;  ///< Altitude in degrees
    double az;   ///< Azimuth in degrees

    [[nodiscard]] auto toJson() const -> json {
        return {{"alt", alt}, {"az", az}};
    }
};

/**
 * @brief Mount status
 */
struct MountStatus {
    bool connected;
    bool tracking;
    bool slewing;
    bool parked;
    bool atHome;
    TrackingState trackingState;
    PierSide pierSide;
    EquatorialCoords position;
    std::optional<HorizontalCoords> altAz;
    std::optional<double> siderealTime;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["connected"] = connected;
        j["tracking"] = tracking;
        j["slewing"] = slewing;
        j["parked"] = parked;
        j["atHome"] = atHome;
        j["trackingState"] = trackingStateToString(trackingState);
        j["pierSide"] = pierSideToString(pierSide);
        j["position"] = position.toJson();

        if (altAz) {
            j["altAz"] = altAz->toJson();
        }
        if (siderealTime) {
            j["siderealTime"] = *siderealTime;
        }

        return j;
    }
};

/**
 * @brief Mount capabilities
 */
struct MountCapabilities {
    bool canSlew;
    bool canSlewAsync;
    bool canSync;
    bool canPark;
    bool canUnpark;
    bool canSetTracking;
    bool canSetPierSide;
    bool canPulseGuide;
    bool canSetGuideRates;
    std::vector<std::string> trackingModes;

    [[nodiscard]] auto toJson() const -> json {
        return {{"canSlew", canSlew},
                {"canSlewAsync", canSlewAsync},
                {"canSync", canSync},
                {"canPark", canPark},
                {"canUnpark", canUnpark},
                {"canSetTracking", canSetTracking},
                {"canSetPierSide", canSetPierSide},
                {"canPulseGuide", canPulseGuide},
                {"canSetGuideRates", canSetGuideRates},
                {"trackingModes", trackingModes}};
    }
};

/**
 * @brief Slew target
 */
struct SlewTarget {
    std::string ra;   ///< RA in sexagesimal format
    std::string dec;  ///< Dec in sexagesimal format
    std::optional<std::string> targetName;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["ra"] = ra;
        j["dec"] = dec;
        if (targetName) {
            j["targetName"] = *targetName;
        }
        return j;
    }

    static auto fromJson(const json& j) -> SlewTarget {
        SlewTarget t;
        t.ra = j.value("ra", "");
        t.dec = j.value("dec", "");
        if (j.contains("targetName")) {
            t.targetName = j["targetName"].get<std::string>();
        }
        return t;
    }
};

/**
 * @brief Mount event for WebSocket broadcast
 */
struct MountEvent {
    std::string
        eventType;  // slew_started, slew_finished, tracking_changed, etc.
    json data;
    int64_t timestamp;

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", "event"},
                {"event", "mount." + eventType},
                {"data", data},
                {"timestamp", timestamp}};
    }
};

}  // namespace lithium::models::mount

#endif  // LITHIUM_SERVER_MODELS_MOUNT_HPP
