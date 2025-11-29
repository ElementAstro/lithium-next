/**
 * @file format.hpp
 * @brief Coordinate formatting utilities.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_CONVERSION_FORMAT_HPP
#define LITHIUM_TOOLS_CONVERSION_FORMAT_HPP

#include <cmath>
#include <concepts>
#include <format>
#include <string>

#include "angle.hpp"

namespace lithium::tools::conversion {

// ============================================================================
// RA/Dec Formatting
// ============================================================================

/**
 * @brief Format Right Ascension as HMS string.
 * @tparam T Floating-point type.
 * @param ra Right Ascension in hours.
 * @return Formatted string "HHh MMm SS.SSs".
 */
template <std::floating_point T>
[[nodiscard]] auto formatRa(T ra) -> std::string {
    int hours, minutes;
    double seconds;
    hoursToHms(static_cast<double>(ra), hours, minutes, seconds);
    return std::format("{:02d}h {:02d}m {:.2f}s", hours, minutes, seconds);
}

/**
 * @brief Format Declination as DMS string.
 * @tparam T Floating-point type.
 * @param dec Declination in degrees.
 * @return Formatted string "+DD° MM' SS.SS\"".
 */
template <std::floating_point T>
[[nodiscard]] auto formatDec(T dec) -> std::string {
    char sign = (dec >= 0) ? '+' : '-';
    dec = std::abs(dec);
    int degrees = static_cast<int>(dec);
    int minutes = static_cast<int>((dec - degrees) * 60);
    double seconds = ((dec - degrees) * 60 - minutes) * 60;
    return std::format("{}{:02d}° {:02d}' {:.2f}\"", sign, degrees, minutes,
                       seconds);
}

/**
 * @brief Format coordinates as RA/Dec string.
 * @tparam T Floating-point type.
 * @param ra Right Ascension in hours.
 * @param dec Declination in degrees.
 * @return Formatted string "RA: HHh MMm SS.SSs, Dec: +DD° MM' SS.SS\"".
 */
template <std::floating_point T>
[[nodiscard]] auto formatCoordinates(T ra, T dec) -> std::string {
    return std::format("RA: {}, Dec: {}", formatRa(ra), formatDec(dec));
}

// ============================================================================
// Angle Formatting
// ============================================================================

/**
 * @brief Format angle as DMS string.
 * @param angle Angle in degrees.
 * @return Formatted string "DD° MM' SS.SS\"".
 */
[[nodiscard]] inline std::string formatAngleDMS(double angle) {
    int degrees, minutes;
    double seconds;
    degreeToDms(angle, degrees, minutes, seconds);
    char sign = (degrees < 0) ? '-' : '+';
    return std::format("{}{:02d}° {:02d}' {:.2f}\"", sign, std::abs(degrees),
                       minutes, seconds);
}

/**
 * @brief Format angle as decimal degrees string.
 * @param angle Angle in degrees.
 * @param precision Decimal places (default 4).
 * @return Formatted string.
 */
[[nodiscard]] inline std::string formatAngleDegrees(double angle,
                                                    int precision = 4) {
    return std::format("{:.{}f}°", angle, precision);
}

// ============================================================================
// Radians to String Conversions
// ============================================================================

/**
 * @brief Convert radians to DMS string.
 * @param radians Angle in radians.
 * @return DMS formatted string.
 */
[[nodiscard]] inline std::string radToDmsStr(double radians) {
    return formatAngleDMS(radToDegree(radians));
}

/**
 * @brief Convert radians to HMS string.
 * @param radians Angle in radians.
 * @return HMS formatted string.
 */
[[nodiscard]] inline std::string radToHmsStr(double radians) {
    return formatRa(radToHour(radians));
}

// ============================================================================
// Time Formatting
// ============================================================================

/**
 * @brief Format time duration as string.
 * @param seconds Duration in seconds.
 * @return Formatted string "HH:MM:SS".
 */
[[nodiscard]] inline std::string formatDuration(double seconds) {
    int totalSeconds = static_cast<int>(seconds);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;
    return std::format("{:02d}:{:02d}:{:02d}", hours, minutes, secs);
}

/**
 * @brief Format time duration with fractional seconds.
 * @param seconds Duration in seconds.
 * @return Formatted string "HH:MM:SS.ss".
 */
[[nodiscard]] inline std::string formatDurationPrecise(double seconds) {
    int totalSeconds = static_cast<int>(seconds);
    double frac = seconds - totalSeconds;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;
    return std::format("{:02d}:{:02d}:{:05.2f}", hours, minutes, secs + frac);
}

}  // namespace lithium::tools::conversion

#endif  // LITHIUM_TOOLS_CONVERSION_FORMAT_HPP
