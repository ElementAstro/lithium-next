/**
 * @file convert.hpp
 * @brief Aggregated header for conversion utilities.
 *
 * This is the main include file for the conversion submodule.
 * Include this file to get access to all conversion functions.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CONVERSION_CONVERT_HPP
#define LITHIUM_TOOLS_CONVERSION_CONVERT_HPP

// Angle conversions
#include "angle.hpp"

// Coordinate conversions
#include "coordinate.hpp"

// Formatting utilities
#include "format.hpp"

namespace lithium::tools::conversion {

/**
 * @brief Conversion module version.
 */
inline constexpr const char* CONVERSION_VERSION = "1.0.0";

}  // namespace lithium::tools::conversion

#endif  // LITHIUM_TOOLS_CONVERSION_CONVERT_HPP
