/**
 * @file tools.hpp
 * @brief Main aggregated header for Lithium Tools library.
 *
 * This is the primary include file for the tools library.
 * Include this file to get access to all tools functionality.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_HPP
#define LITHIUM_TOOLS_HPP

// ============================================================================
// Astronomy Types Module
// ============================================================================
// Astronomical data types for observation planning

#include "astronomy/types.hpp"

// ============================================================================
// Calculation Module
// ============================================================================
// Astronomical calculations (Julian date, sidereal time, transformations)

#include "calculation/calculate.hpp"

// ============================================================================
// Conversion Module
// ============================================================================
// Unit conversions and coordinate transformations

#include "conversion/convert.hpp"

// ============================================================================
// Solver Module
// ============================================================================
// Plate solving utilities

#include "solver/wcs.hpp"

namespace lithium::tools {

/**
 * @brief Lithium Tools library version.
 */
inline constexpr const char* TOOLS_VERSION = "1.0.0";

/**
 * @brief Get tools library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getToolsVersion() noexcept {
    return TOOLS_VERSION;
}

}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_HPP
