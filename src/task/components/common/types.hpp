/**
 * @file types.hpp
 * @brief Common types and enumerations for all device tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMPONENTS_COMMON_TYPES_HPP
#define LITHIUM_TASK_COMPONENTS_COMMON_TYPES_HPP

#include <string>
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

// ============================================================================
// Camera Types
// ============================================================================

namespace camera {

/**
 * @brief Exposure frame type enumeration
 */
enum class ExposureType {
    Light,    ///< Light frame - main science exposure
    Dark,     ///< Dark frame - noise calibration
    Bias,     ///< Bias frame - readout noise calibration
    Flat,     ///< Flat frame - optical system response
    Snapshot  ///< Quick preview exposure
};

NLOHMANN_JSON_SERIALIZE_ENUM(ExposureType,
                             {
                                 {ExposureType::Light, "light"},
                                 {ExposureType::Dark, "dark"},
                                 {ExposureType::Bias, "bias"},
                                 {ExposureType::Flat, "flat"},
                                 {ExposureType::Snapshot, "snapshot"},
                             })

/**
 * @brief Camera binning configuration
 */
struct BinningConfig {
    int x = 1;
    int y = 1;

    bool operator==(const BinningConfig& other) const {
        return x == other.x && y == other.y;
    }
};

inline void to_json(json& j, const BinningConfig& b) {
    j = json{{"x", b.x}, {"y", b.y}};
}

inline void from_json(const json& j, BinningConfig& b) {
    j.at("x").get_to(b.x);
    j.at("y").get_to(b.y);
}

/**
 * @brief Subframe/ROI configuration
 */
struct SubframeConfig {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline void to_json(json& j, const SubframeConfig& s) {
    j = json{{"x", s.x}, {"y", s.y}, {"width", s.width}, {"height", s.height}};
}

inline void from_json(const json& j, SubframeConfig& s) {
    j.at("x").get_to(s.x);
    j.at("y").get_to(s.y);
    j.at("width").get_to(s.width);
    j.at("height").get_to(s.height);
}

/**
 * @brief Exposure parameters structure
 */
struct ExposureParams {
    double duration = 1.0;
    ExposureType type = ExposureType::Light;
    int gain = 100;
    int offset = 10;
    BinningConfig binning;
    SubframeConfig subframe;
    std::string filter = "L";
    std::string outputPath;
};

}  // namespace camera

// ============================================================================
// Focuser Types
// ============================================================================

namespace focuser {

/**
 * @brief Focus method enumeration
 */
enum class FocusMethod {
    HFD,       ///< Half-Flux Diameter
    FWHM,      ///< Full Width at Half Maximum
    Contrast,  ///< Contrast-based focus
    Bahtinov   ///< Bahtinov mask focus
};

NLOHMANN_JSON_SERIALIZE_ENUM(FocusMethod,
                             {
                                 {FocusMethod::HFD, "hfd"},
                                 {FocusMethod::FWHM, "fwhm"},
                                 {FocusMethod::Contrast, "contrast"},
                                 {FocusMethod::Bahtinov, "bahtinov"},
                             })

/**
 * @brief Focus result structure
 */
struct FocusResult {
    int position = 0;
    double metric = 0.0;
    double temperature = 0.0;
    bool success = false;
};

inline void to_json(json& j, const FocusResult& r) {
    j = json{{"position", r.position},
             {"metric", r.metric},
             {"temperature", r.temperature},
             {"success", r.success}};
}

inline void from_json(const json& j, FocusResult& r) {
    j.at("position").get_to(r.position);
    j.at("metric").get_to(r.metric);
    j.at("temperature").get_to(r.temperature);
    j.at("success").get_to(r.success);
}

}  // namespace focuser

// ============================================================================
// Filter Wheel Types
// ============================================================================

namespace filterwheel {

/**
 * @brief Filter type enumeration
 */
enum class FilterType {
    Luminance,  ///< L filter
    Red,        ///< R filter
    Green,      ///< G filter
    Blue,       ///< B filter
    Ha,         ///< Hydrogen-alpha narrowband
    OIII,       ///< Oxygen-III narrowband
    SII,        ///< Sulfur-II narrowband
    Custom      ///< Custom filter
};

NLOHMANN_JSON_SERIALIZE_ENUM(FilterType, {
                                             {FilterType::Luminance, "L"},
                                             {FilterType::Red, "R"},
                                             {FilterType::Green, "G"},
                                             {FilterType::Blue, "B"},
                                             {FilterType::Ha, "Ha"},
                                             {FilterType::OIII, "OIII"},
                                             {FilterType::SII, "SII"},
                                             {FilterType::Custom, "custom"},
                                         })

/**
 * @brief Filter configuration
 */
struct FilterConfig {
    int position = 0;
    std::string name;
    FilterType type = FilterType::Custom;
    double focusOffset = 0.0;
};

inline void to_json(json& j, const FilterConfig& f) {
    j = json{{"position", f.position},
             {"name", f.name},
             {"type", f.type},
             {"focusOffset", f.focusOffset}};
}

inline void from_json(const json& j, FilterConfig& f) {
    j.at("position").get_to(f.position);
    j.at("name").get_to(f.name);
    j.at("type").get_to(f.type);
    j.at("focusOffset").get_to(f.focusOffset);
}

}  // namespace filterwheel

// ============================================================================
// Guider Types
// ============================================================================

namespace guider {

/**
 * @brief Guiding state enumeration
 */
enum class GuidingState {
    Idle,         ///< Not guiding
    Calibrating,  ///< Calibrating guider
    Guiding,      ///< Actively guiding
    Settling,     ///< Settling after dither
    Paused,       ///< Guiding paused
    Error         ///< Guiding error
};

NLOHMANN_JSON_SERIALIZE_ENUM(GuidingState,
                             {
                                 {GuidingState::Idle, "idle"},
                                 {GuidingState::Calibrating, "calibrating"},
                                 {GuidingState::Guiding, "guiding"},
                                 {GuidingState::Settling, "settling"},
                                 {GuidingState::Paused, "paused"},
                                 {GuidingState::Error, "error"},
                             })

/**
 * @brief Guiding statistics
 */
struct GuidingStats {
    double rmsRA = 0.0;
    double rmsDec = 0.0;
    double rmsTotal = 0.0;
    double peakRA = 0.0;
    double peakDec = 0.0;
    int sampleCount = 0;
};

inline void to_json(json& j, const GuidingStats& s) {
    j = json{{"rmsRA", s.rmsRA},       {"rmsDec", s.rmsDec},
             {"rmsTotal", s.rmsTotal}, {"peakRA", s.peakRA},
             {"peakDec", s.peakDec},   {"sampleCount", s.sampleCount}};
}

inline void from_json(const json& j, GuidingStats& s) {
    j.at("rmsRA").get_to(s.rmsRA);
    j.at("rmsDec").get_to(s.rmsDec);
    j.at("rmsTotal").get_to(s.rmsTotal);
    j.at("peakRA").get_to(s.peakRA);
    j.at("peakDec").get_to(s.peakDec);
    j.at("sampleCount").get_to(s.sampleCount);
}

}  // namespace guider

// ============================================================================
// Astrometry Types
// ============================================================================

namespace astrometry {

/**
 * @brief Plate solve result
 */
struct PlateSolveResult {
    double ra = 0.0;          ///< Right ascension in degrees
    double dec = 0.0;         ///< Declination in degrees
    double rotation = 0.0;    ///< Field rotation in degrees
    double pixelScale = 0.0;  ///< Pixel scale in arcsec/pixel
    double fovWidth = 0.0;    ///< Field of view width in degrees
    double fovHeight = 0.0;   ///< Field of view height in degrees
    bool success = false;
    std::string solver;      ///< Solver used
    double solveTime = 0.0;  ///< Time to solve in seconds
};

inline void to_json(json& j, const PlateSolveResult& r) {
    j = json{{"ra", r.ra},
             {"dec", r.dec},
             {"rotation", r.rotation},
             {"pixelScale", r.pixelScale},
             {"fovWidth", r.fovWidth},
             {"fovHeight", r.fovHeight},
             {"success", r.success},
             {"solver", r.solver},
             {"solveTime", r.solveTime}};
}

inline void from_json(const json& j, PlateSolveResult& r) {
    j.at("ra").get_to(r.ra);
    j.at("dec").get_to(r.dec);
    j.at("rotation").get_to(r.rotation);
    j.at("pixelScale").get_to(r.pixelScale);
    j.at("fovWidth").get_to(r.fovWidth);
    j.at("fovHeight").get_to(r.fovHeight);
    j.at("success").get_to(r.success);
    j.at("solver").get_to(r.solver);
    j.at("solveTime").get_to(r.solveTime);
}

}  // namespace astrometry

// ============================================================================
// Observatory Types
// ============================================================================

namespace observatory {

/**
 * @brief Weather condition
 */
enum class WeatherCondition {
    Clear,
    Cloudy,
    PartlyCloudy,
    Overcast,
    Rain,
    Snow,
    Fog,
    Windy,
    Unknown
};

NLOHMANN_JSON_SERIALIZE_ENUM(WeatherCondition,
                             {
                                 {WeatherCondition::Clear, "clear"},
                                 {WeatherCondition::Cloudy, "cloudy"},
                                 {WeatherCondition::PartlyCloudy,
                                  "partly_cloudy"},
                                 {WeatherCondition::Overcast, "overcast"},
                                 {WeatherCondition::Rain, "rain"},
                                 {WeatherCondition::Snow, "snow"},
                                 {WeatherCondition::Fog, "fog"},
                                 {WeatherCondition::Windy, "windy"},
                                 {WeatherCondition::Unknown, "unknown"},
                             })

/**
 * @brief Safety status
 */
struct SafetyStatus {
    bool isSafe = false;
    WeatherCondition weather = WeatherCondition::Unknown;
    double temperature = 0.0;
    double humidity = 0.0;
    double windSpeed = 0.0;
    double cloudCover = 0.0;
    std::string reason;
};

inline void to_json(json& j, const SafetyStatus& s) {
    j = json{{"isSafe", s.isSafe},
             {"weather", s.weather},
             {"temperature", s.temperature},
             {"humidity", s.humidity},
             {"windSpeed", s.windSpeed},
             {"cloudCover", s.cloudCover},
             {"reason", s.reason}};
}

inline void from_json(const json& j, SafetyStatus& s) {
    j.at("isSafe").get_to(s.isSafe);
    j.at("weather").get_to(s.weather);
    j.at("temperature").get_to(s.temperature);
    j.at("humidity").get_to(s.humidity);
    j.at("windSpeed").get_to(s.windSpeed);
    j.at("cloudCover").get_to(s.cloudCover);
    j.at("reason").get_to(s.reason);
}

}  // namespace observatory

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMPONENTS_COMMON_TYPES_HPP
