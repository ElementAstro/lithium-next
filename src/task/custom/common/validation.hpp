/**
 * @file validation.hpp
 * @brief Parameter validation utilities for tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMMON_VALIDATION_HPP
#define LITHIUM_TASK_COMMON_VALIDATION_HPP

#include <functional>
#include <limits>
#include <string>
#include <vector>
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

/**
 * @brief Validation result containing success status and error message
 */
struct ValidationResult {
    bool valid = true;
    std::string error;

    operator bool() const { return valid; }

    static ValidationResult success() { return {true, ""}; }

    static ValidationResult failure(const std::string& msg) {
        return {false, msg};
    }
};

/**
 * @brief Parameter validator class
 */
class ParamValidator {
public:
    using ValidatorFunc = std::function<ValidationResult(const json&)>;

    /**
     * @brief Validate a required parameter exists
     */
    static ValidationResult required(const json& params,
                                     const std::string& key) {
        if (!params.contains(key) || params[key].is_null()) {
            return ValidationResult::failure("Missing required parameter: " +
                                             key);
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a number within range
     */
    template <typename T>
    static ValidationResult numberInRange(const json& params,
                                          const std::string& key, T minVal,
                                          T maxVal) {
        if (!params.contains(key)) {
            return ValidationResult::success();  // Optional if not present
        }

        if (!params[key].is_number()) {
            return ValidationResult::failure(key + " must be a number");
        }

        T value = params[key].get<T>();
        if (value < minVal || value > maxVal) {
            return ValidationResult::failure(key + " must be between " +
                                             std::to_string(minVal) + " and " +
                                             std::to_string(maxVal));
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a positive number
     */
    template <typename T>
    static ValidationResult positive(const json& params,
                                     const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_number()) {
            return ValidationResult::failure(key + " must be a number");
        }

        T value = params[key].get<T>();
        if (value <= 0) {
            return ValidationResult::failure(key + " must be positive");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a non-negative number
     */
    template <typename T>
    static ValidationResult nonNegative(const json& params,
                                        const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_number()) {
            return ValidationResult::failure(key + " must be a number");
        }

        T value = params[key].get<T>();
        if (value < 0) {
            return ValidationResult::failure(key + " must be non-negative");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a non-empty string
     */
    static ValidationResult nonEmptyString(const json& params,
                                           const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_string()) {
            return ValidationResult::failure(key + " must be a string");
        }

        if (params[key].get<std::string>().empty()) {
            return ValidationResult::failure(key + " must not be empty");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is one of allowed values
     */
    template <typename T>
    static ValidationResult oneOf(const json& params, const std::string& key,
                                  const std::vector<T>& allowedValues) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        T value = params[key].get<T>();
        for (const auto& allowed : allowedValues) {
            if (value == allowed) {
                return ValidationResult::success();
            }
        }

        return ValidationResult::failure(key + " has an invalid value");
    }

    /**
     * @brief Validate parameter is a valid boolean
     */
    static ValidationResult isBoolean(const json& params,
                                      const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_boolean()) {
            return ValidationResult::failure(key + " must be a boolean");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a valid array
     */
    static ValidationResult isArray(const json& params,
                                    const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_array()) {
            return ValidationResult::failure(key + " must be an array");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Validate parameter is a valid object
     */
    static ValidationResult isObject(const json& params,
                                     const std::string& key) {
        if (!params.contains(key)) {
            return ValidationResult::success();
        }

        if (!params[key].is_object()) {
            return ValidationResult::failure(key + " must be an object");
        }
        return ValidationResult::success();
    }

    /**
     * @brief Chain multiple validators
     */
    static ValidationResult all(
        std::initializer_list<ValidationResult> results) {
        for (const auto& result : results) {
            if (!result.valid) {
                return result;
            }
        }
        return ValidationResult::success();
    }
};

/**
 * @brief Exposure parameter validator
 */
class ExposureValidator {
public:
    static ValidationResult validate(const json& params) {
        return ParamValidator::all({
            ParamValidator::numberInRange<double>(params, "exposure", 0.0,
                                                  86400.0),
            ParamValidator::numberInRange<int>(params, "gain", 0, 1000),
            ParamValidator::numberInRange<int>(params, "offset", 0, 1000),
            ParamValidator::numberInRange<int>(params, "binning_x", 1, 8),
            ParamValidator::numberInRange<int>(params, "binning_y", 1, 8),
        });
    }
};

/**
 * @brief Focus parameter validator
 */
class FocusValidator {
public:
    static ValidationResult validate(const json& params) {
        return ParamValidator::all({
            ParamValidator::numberInRange<int>(params, "step_size", 1, 10000),
            ParamValidator::numberInRange<int>(params, "num_steps", 3, 100),
            ParamValidator::positive<double>(params, "exposure"),
        });
    }
};

/**
 * @brief Guiding parameter validator
 */
class GuidingValidator {
public:
    static ValidationResult validate(const json& params) {
        return ParamValidator::all({
            ParamValidator::positive<double>(params, "exposure"),
            ParamValidator::nonNegative<double>(params, "settle_time"),
            ParamValidator::nonNegative<double>(params, "settle_threshold"),
        });
    }
};

/**
 * @brief Coordinate validator for RA/Dec
 */
class CoordinateValidator {
public:
    static ValidationResult validateRA(const json& params,
                                       const std::string& key = "ra") {
        return ParamValidator::numberInRange<double>(params, key, 0.0, 360.0);
    }

    static ValidationResult validateDec(const json& params,
                                        const std::string& key = "dec") {
        return ParamValidator::numberInRange<double>(params, key, -90.0, 90.0);
    }

    static ValidationResult validate(const json& params) {
        return ParamValidator::all({validateRA(params), validateDec(params)});
    }
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMMON_VALIDATION_HPP
