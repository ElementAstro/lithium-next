/**
 * @file calculate.hpp
 * @brief Aggregated header for astronomical calculations.
 *
 * This is the main include file for the calculation submodule.
 * Include this file to get access to all calculation functions.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CALCULATION_CALCULATE_HPP
#define LITHIUM_TOOLS_CALCULATION_CALCULATE_HPP

// Julian Date calculations
#include "julian.hpp"

// Sidereal time calculations
#include "sidereal.hpp"

// Coordinate transformations
#include "transform.hpp"

// Precession, nutation, aberration
#include "precession.hpp"

namespace lithium::tools::calculation {

/**
 * @brief Calculation module version.
 */
inline constexpr const char* CALCULATION_VERSION = "1.0.0";

}  // namespace lithium::tools::calculation

#endif  // LITHIUM_TOOLS_CALCULATION_CALCULATE_HPP
