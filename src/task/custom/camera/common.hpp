#ifndef LITHIUM_TASK_CAMERA_COMMON_HPP
#define LITHIUM_TASK_CAMERA_COMMON_HPP

#include <format>
#include "atom/type/json.hpp"

namespace lithium::task::task {

/**
 * @brief Exposure type enumeration for camera tasks
 */
enum ExposureType { 
    LIGHT,      ///< Light frame - main science exposure
    DARK,       ///< Dark frame - noise calibration
    BIAS,       ///< Bias frame - readout noise calibration
    FLAT,       ///< Flat frame - optical system response calibration
    SNAPSHOT    ///< Quick preview exposure
};

/**
 * @brief JSON serialization for ExposureType enum
 */
NLOHMANN_JSON_SERIALIZE_ENUM(ExposureType, {
    {LIGHT, "light"},
    {DARK, "dark"},
    {BIAS, "bias"},
    {FLAT, "flat"},
    {SNAPSHOT, "snapshot"},
})

// Common utility functions used across camera tasks can be added here
// For example: exposure time validation, camera parameter validation, etc.

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_COMMON_HPP
