/**
 * @file types.hpp
 * @brief Common types and enumerations for camera tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_CAMERA_COMMON_TYPES_HPP
#define LITHIUM_TASK_CAMERA_COMMON_TYPES_HPP

#include "atom/type/json.hpp"
#include <string>

namespace lithium::task::camera {

using json = nlohmann::json;

/**
 * @brief Exposure frame type enumeration
 */
enum class ExposureType {
    Light,     ///< Light frame - main science exposure
    Dark,      ///< Dark frame - noise calibration
    Bias,      ///< Bias frame - readout noise calibration
    Flat,      ///< Flat frame - optical system response
    Snapshot   ///< Quick preview exposure
};

NLOHMANN_JSON_SERIALIZE_ENUM(ExposureType, {
    {ExposureType::Light, "light"},
    {ExposureType::Dark, "dark"},
    {ExposureType::Bias, "bias"},
    {ExposureType::Flat, "flat"},
    {ExposureType::Snapshot, "snapshot"},
})

/**
 * @brief Focus method enumeration
 */
enum class FocusMethod {
    HFD,       ///< Half-Flux Diameter
    FWHM,      ///< Full Width at Half Maximum
    Contrast,  ///< Contrast-based focus
    Bahtinov   ///< Bahtinov mask focus
};

NLOHMANN_JSON_SERIALIZE_ENUM(FocusMethod, {
    {FocusMethod::HFD, "hfd"},
    {FocusMethod::FWHM, "fwhm"},
    {FocusMethod::Contrast, "contrast"},
    {FocusMethod::Bahtinov, "bahtinov"},
})

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
 * @brief Guiding state enumeration
 */
enum class GuidingState {
    Idle,       ///< Not guiding
    Calibrating,///< Calibrating guider
    Guiding,    ///< Actively guiding
    Settling,   ///< Settling after dither
    Paused,     ///< Guiding paused
    Error       ///< Guiding error
};

NLOHMANN_JSON_SERIALIZE_ENUM(GuidingState, {
    {GuidingState::Idle, "idle"},
    {GuidingState::Calibrating, "calibrating"},
    {GuidingState::Guiding, "guiding"},
    {GuidingState::Settling, "settling"},
    {GuidingState::Paused, "paused"},
    {GuidingState::Error, "error"},
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

/**
 * @brief Focus result structure
 */
struct FocusResult {
    int position = 0;
    double metric = 0.0;
    double temperature = 0.0;
    bool success = false;
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_COMMON_TYPES_HPP
