/*
 * guider.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Guider data models for HTTP/WebSocket responses

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_GUIDER_HPP
#define LITHIUM_SERVER_MODELS_GUIDER_HPP

#include <optional>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::models::guider {

using json = nlohmann::json;

/**
 * @brief Guider connection status
 */
enum class GuiderState {
    Disconnected,
    Connected,
    Looping,
    Calibrating,
    Guiding,
    Settling,
    Paused,
    Error
};

/**
 * @brief Convert GuiderState to string
 */
inline auto stateToString(GuiderState state) -> std::string {
    switch (state) {
        case GuiderState::Disconnected:
            return "disconnected";
        case GuiderState::Connected:
            return "connected";
        case GuiderState::Looping:
            return "looping";
        case GuiderState::Calibrating:
            return "calibrating";
        case GuiderState::Guiding:
            return "guiding";
        case GuiderState::Settling:
            return "settling";
        case GuiderState::Paused:
            return "paused";
        case GuiderState::Error:
            return "error";
    }
    return "unknown";
}

/**
 * @brief Guider connection info
 */
struct ConnectionInfo {
    bool connected;
    std::string host;
    int port;
    std::string phd2Version;
    std::optional<std::string> error;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["connected"] = connected;
        j["host"] = host;
        j["port"] = port;

        if (!phd2Version.empty()) {
            j["phd2Version"] = phd2Version;
        }
        if (error) {
            j["error"] = *error;
        }

        return j;
    }
};

/**
 * @brief Guider status
 */
struct GuiderStatus {
    GuiderState state;
    bool isCalibrated;
    bool isGuiding;
    bool isLooping;
    bool isSettling;
    std::optional<double> rmsError;
    std::optional<double> snr;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["state"] = stateToString(state);
        j["isCalibrated"] = isCalibrated;
        j["isGuiding"] = isGuiding;
        j["isLooping"] = isLooping;
        j["isSettling"] = isSettling;

        if (rmsError) {
            j["rmsError"] = *rmsError;
        }
        if (snr) {
            j["snr"] = *snr;
        }

        return j;
    }
};

/**
 * @brief Guide star info
 */
struct StarInfo {
    double x;
    double y;
    double snr;
    double mass;
    bool isLocked;

    [[nodiscard]] auto toJson() const -> json {
        return {{"x", x},
                {"y", y},
                {"snr", snr},
                {"mass", mass},
                {"isLocked", isLocked}};
    }
};

/**
 * @brief Guide statistics
 */
struct GuideStats {
    double rmsRa;
    double rmsDec;
    double rmsTotal;
    double peakRa;
    double peakDec;
    int sampleCount;
    double raOscillation;
    double decOscillation;

    [[nodiscard]] auto toJson() const -> json {
        return {{"rmsRa", rmsRa},
                {"rmsDec", rmsDec},
                {"rmsTotal", rmsTotal},
                {"peakRa", peakRa},
                {"peakDec", peakDec},
                {"sampleCount", sampleCount},
                {"raOscillation", raOscillation},
                {"decOscillation", decOscillation}};
    }
};

/**
 * @brief Calibration data
 */
struct CalibrationData {
    bool isCalibrated;
    double raAngle;
    double decAngle;
    double raRate;
    double decRate;
    double xAngle;
    double yAngle;
    bool decFlipped;

    [[nodiscard]] auto toJson() const -> json {
        return {{"isCalibrated", isCalibrated},
                {"raAngle", raAngle},
                {"decAngle", decAngle},
                {"raRate", raRate},
                {"decRate", decRate},
                {"xAngle", xAngle},
                {"yAngle", yAngle},
                {"decFlipped", decFlipped}};
    }
};

/**
 * @brief Dither settings
 */
struct DitherSettings {
    double amount;
    bool raOnly;
    double settlePixels;
    double settleTime;
    double settleTimeout;

    [[nodiscard]] auto toJson() const -> json {
        return {{"amount", amount},
                {"raOnly", raOnly},
                {"settlePixels", settlePixels},
                {"settleTime", settleTime},
                {"settleTimeout", settleTimeout}};
    }

    static auto fromJson(const json& j) -> DitherSettings {
        DitherSettings s;
        s.amount = j.value("amount", 5.0);
        s.raOnly = j.value("raOnly", false);
        s.settlePixels = j.value("settlePixels", 1.5);
        s.settleTime = j.value("settleTime", 10.0);
        s.settleTimeout = j.value("settleTimeout", 60.0);
        return s;
    }
};

/**
 * @brief Guider event for WebSocket broadcast
 */
struct GuiderEvent {
    std::string eventType;  // guiding_started, guiding_stopped, dither_started, etc.
    json data;
    int64_t timestamp;

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", "event"},
                {"event", "guider." + eventType},
                {"data", data},
                {"timestamp", timestamp}};
    }
};

}  // namespace lithium::models::guider

#endif  // LITHIUM_SERVER_MODELS_GUIDER_HPP
