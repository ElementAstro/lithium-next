/**
 * @file validation.hpp
 * @brief Common validation functions for camera task parameters
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMPONENTS_CAMERA_COMMON_VALIDATION_HPP
#define LITHIUM_TASK_COMPONENTS_CAMERA_COMMON_VALIDATION_HPP

#include <stdexcept>
#include <string>
#include "types.hpp"

namespace lithium::task::camera {

/**
 * @brief Validation exception for camera parameters
 */
class ValidationError : public std::invalid_argument {
public:
    explicit ValidationError(const std::string& msg)
        : std::invalid_argument(msg) {}
};

/**
 * @brief Validate exposure time
 * @param exposure Exposure time in seconds
 * @param minExp Minimum allowed exposure (default 0.0001s)
 * @param maxExp Maximum allowed exposure (default 7200s)
 * @throws ValidationError if exposure is out of range
 */
inline void validateExposure(double exposure, double minExp = 0.0001,
                             double maxExp = 7200.0) {
    if (exposure < minExp || exposure > maxExp) {
        throw ValidationError("Exposure time must be between " +
                              std::to_string(minExp) + " and " +
                              std::to_string(maxExp) + " seconds, got " +
                              std::to_string(exposure));
    }
}

/**
 * @brief Validate gain value
 * @param gain Gain value
 * @param minGain Minimum gain (default 0)
 * @param maxGain Maximum gain (default 1000)
 */
inline void validateGain(int gain, int minGain = 0, int maxGain = 1000) {
    if (gain < minGain || gain > maxGain) {
        throw ValidationError(
            "Gain must be between " + std::to_string(minGain) + " and " +
            std::to_string(maxGain) + ", got " + std::to_string(gain));
    }
}

/**
 * @brief Validate offset value
 * @param offset Offset value
 * @param minOffset Minimum offset (default 0)
 * @param maxOffset Maximum offset (default 500)
 */
inline void validateOffset(int offset, int minOffset = 0, int maxOffset = 500) {
    if (offset < minOffset || offset > maxOffset) {
        throw ValidationError(
            "Offset must be between " + std::to_string(minOffset) + " and " +
            std::to_string(maxOffset) + ", got " + std::to_string(offset));
    }
}

/**
 * @brief Validate binning values
 * @param binning Binning configuration
 * @param maxBin Maximum binning value (default 4)
 */
inline void validateBinning(const BinningConfig& binning, int maxBin = 4) {
    if (binning.x < 1 || binning.x > maxBin || binning.y < 1 ||
        binning.y > maxBin) {
        throw ValidationError("Binning must be between 1 and " +
                              std::to_string(maxBin) + ", got " +
                              std::to_string(binning.x) + "x" +
                              std::to_string(binning.y));
    }
}

/**
 * @brief Validate subframe/ROI coordinates
 * @param subframe Subframe configuration
 * @param maxWidth Maximum sensor width
 * @param maxHeight Maximum sensor height
 */
inline void validateSubframe(const SubframeConfig& subframe, int maxWidth,
                             int maxHeight) {
    if (subframe.x < 0 || subframe.y < 0) {
        throw ValidationError("Subframe coordinates cannot be negative");
    }
    if (subframe.width <= 0 || subframe.height <= 0) {
        throw ValidationError("Subframe dimensions must be positive");
    }
    if (subframe.x + subframe.width > maxWidth) {
        throw ValidationError("Subframe exceeds sensor width");
    }
    if (subframe.y + subframe.height > maxHeight) {
        throw ValidationError("Subframe exceeds sensor height");
    }
}

/**
 * @brief Validate count value (for sequences)
 * @param count Number of items
 * @param maxCount Maximum allowed count
 */
inline void validateCount(int count, int maxCount = 10000) {
    if (count < 1 || count > maxCount) {
        throw ValidationError("Count must be between 1 and " +
                              std::to_string(maxCount) + ", got " +
                              std::to_string(count));
    }
}

/**
 * @brief Validate temperature value
 * @param temp Temperature in Celsius
 * @param minTemp Minimum temperature (default -50)
 * @param maxTemp Maximum temperature (default 50)
 */
inline void validateTemperature(double temp, double minTemp = -50.0,
                                double maxTemp = 50.0) {
    if (temp < minTemp || temp > maxTemp) {
        throw ValidationError(
            "Temperature must be between " + std::to_string(minTemp) + " and " +
            std::to_string(maxTemp) + "°C, got " + std::to_string(temp));
    }
}

/**
 * @brief Validate focus position
 * @param position Focuser position
 * @param maxPosition Maximum focuser position
 */
inline void validateFocusPosition(int position, int maxPosition = 100000) {
    if (position < 0 || position > maxPosition) {
        throw ValidationError("Focus position must be between 0 and " +
                              std::to_string(maxPosition) + ", got " +
                              std::to_string(position));
    }
}

/**
 * @brief Validate coordinates (RA/Dec or Alt/Az)
 * @param ra Right ascension in hours (0-24) or altitude in degrees
 * @param dec Declination in degrees (-90 to 90) or azimuth in degrees
 */
inline void validateCoordinates(double ra, double dec) {
    if (ra < 0.0 || ra > 24.0) {
        throw ValidationError("RA must be between 0 and 24 hours, got " +
                              std::to_string(ra));
    }
    if (dec < -90.0 || dec > 90.0) {
        throw ValidationError("Dec must be between -90 and 90 degrees, got " +
                              std::to_string(dec));
    }
}

/**
 * @brief Validate required parameter exists
 * @param params JSON parameters
 * @param key Parameter key
 */
inline void validateRequired(const json& params, const std::string& key) {
    if (!params.contains(key)) {
        throw ValidationError("Missing required parameter: " + key);
    }
}

/**
 * @brief Validate parameter type
 * @param params JSON parameters
 * @param key Parameter key
 * @param expectedType Expected JSON type name
 */
inline void validateType(const json& params, const std::string& key,
                         const std::string& expectedType) {
    if (!params.contains(key))
        return;

    const auto& val = params[key];
    bool valid = false;

    if (expectedType == "number") {
        valid = val.is_number();
    } else if (expectedType == "integer") {
        valid = val.is_number_integer();
    } else if (expectedType == "string") {
        valid = val.is_string();
    } else if (expectedType == "boolean") {
        valid = val.is_boolean();
    } else if (expectedType == "array") {
        valid = val.is_array();
    } else if (expectedType == "object") {
        valid = val.is_object();
    }

    if (!valid) {
        throw ValidationError("Parameter '" + key + "' must be of type " +
                              expectedType);
    }
}

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_COMPONENTS_CAMERA_COMMON_VALIDATION_HPP
