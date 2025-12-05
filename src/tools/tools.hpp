/**
 * @file tools.hpp
 * @brief Main aggregated header for Lithium Tools library.
 *
 * This is the primary include file for the tools library.
 * Include this file to get access to all tools functionality.
 *
 * @par Usage Example:
 * @code
 * #include "tools/tools.hpp"
 *
 * using namespace lithium::tools;
 *
 * // Calculate Julian Date
 * double jd = calculation::dateToJulianDay(2024, 12, 26, 12, 0, 0);
 *
 * // Convert coordinates
 * auto spherical = conversion::equatorialToHorizontal(ra, dec, lat, lon, lst);
 *
 * // Parse WCS
 * auto wcs = solver::extractWCSParams(wcsString);
 * @endcode
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_HPP
#define LITHIUM_TOOLS_HPP

#include <chrono>
#include <string>

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

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Lithium Tools library version.
 */
inline constexpr const char* TOOLS_VERSION = "1.1.0";

/**
 * @brief Get tools library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getToolsVersion() noexcept {
    return TOOLS_VERSION;
}

/**
 * @brief Get all submodule versions as a formatted string.
 * @return Formatted string with all module versions.
 */
[[nodiscard]] inline std::string getAllToolsModuleVersions() {
    std::string result;
    result += "Tools: ";
    result += TOOLS_VERSION;
    result += "\n  Astronomy: ";
    result += astronomy::ASTRONOMY_TYPES_VERSION;
    result += "\n  Calculation: ";
    result += calculation::CALCULATION_VERSION;
    result += "\n  Conversion: ";
    result += conversion::CONVERSION_VERSION;
    return result;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Spherical coordinates (RA/Dec or Az/Alt)
using SphericalCoords = conversion::SphericalCoordinates;

/// WCS parameters
using WCS = solver::WCSParams;

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Get current Julian Date.
 * @return Current Julian Date.
 */
[[nodiscard]] inline double getCurrentJulianDate() {
    return calculation::timeToJD(std::chrono::system_clock::now());
}

/**
 * @brief Convert degrees to radians.
 * @param degrees Angle in degrees.
 * @return Angle in radians.
 */
[[nodiscard]] inline double degreesToRadians(double degrees) {
    return conversion::degreeToRad(degrees);
}

/**
 * @brief Convert radians to degrees.
 * @param radians Angle in radians.
 * @return Angle in degrees.
 */
[[nodiscard]] inline double radiansToDegrees(double radians) {
    return conversion::radToDegree(radians);
}

/**
 * @brief Convert hours to degrees.
 * @param hours Angle in hours.
 * @return Angle in degrees.
 */
[[nodiscard]] inline double hoursToDegrees(double hours) {
    return conversion::hourToDegree(hours);
}

/**
 * @brief Convert degrees to hours.
 * @param degrees Angle in degrees.
 * @return Angle in hours.
 */
[[nodiscard]] inline double degreesToHours(double degrees) {
    return conversion::degreeToHour(degrees);
}

}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_HPP
