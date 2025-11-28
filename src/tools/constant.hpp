#ifndef LITHIUM_TOOLS_CONSTANT_HPP
#define LITHIUM_TOOLS_CONSTANT_HPP

#include <numbers>

namespace lithium::tools {

// ============================================================================
// Mathematical Constants
// ============================================================================

/// Pi constant using std::numbers for precision
inline constexpr double K_PI = std::numbers::pi;
/// Two times Pi
inline constexpr double K_TWO_PI = 2.0 * K_PI;
/// Half of Pi
inline constexpr double K_HALF_PI = K_PI / 2.0;

// ============================================================================
// Angular Conversion Constants
// ============================================================================

/// Conversion factor: degrees to radians
inline constexpr double K_DEGREES_TO_RADIANS = K_PI / 180.0;
/// Conversion factor: radians to degrees
inline constexpr double K_RADIANS_TO_DEGREES = 180.0 / K_PI;
/// Conversion factor: hours to degrees (1 hour = 15 degrees)
inline constexpr double K_HOURS_TO_DEGREES = 15.0;
/// Conversion factor: degrees to hours
inline constexpr double K_DEGREES_TO_HOURS = 1.0 / K_HOURS_TO_DEGREES;
/// Degrees in a full circle
inline constexpr double K_DEGREES_IN_CIRCLE = 360.0;
/// Arcseconds per degree
inline constexpr double K_ARCSEC_PER_DEGREE = 3600.0;

// ============================================================================
// Time Constants
// ============================================================================

/// Hours in a day
inline constexpr double K_HOURS_IN_DAY = 24.0;
/// Minutes in an hour
inline constexpr double K_MINUTES_IN_HOUR = 60.0;
/// Seconds in a minute
inline constexpr double K_SECONDS_IN_MINUTE = 60.0;
/// Seconds in an hour
inline constexpr double K_SECONDS_IN_HOUR = 3600.0;
/// Seconds in a day
inline constexpr double K_SECONDS_IN_DAY = 86400.0;

// ============================================================================
// Numerical Constants
// ============================================================================

/// Small epsilon for floating-point comparisons
inline constexpr double K_EPSILON_VALUE = 1e-5;
/// High precision epsilon for numerical stability
inline constexpr double K_EPSILON_HIGH_PRECISION = 1e-10;

// ============================================================================
// Astronomical Constants
// ============================================================================

/// Julian Date of Unix epoch (1970-01-01 00:00:00 UTC)
inline constexpr double K_JD_EPOCH = 2440587.5;
/// Offset between Julian Date and Modified Julian Date
inline constexpr double K_MJD_OFFSET = 2400000.5;
/// Earth equatorial radius in meters
inline constexpr double K_EARTH_RADIUS_EQUATORIAL = 6378137.0;
/// Earth polar radius in meters
inline constexpr double K_EARTH_RADIUS_POLAR = 6356752.0;
/// Astronomical Unit in meters
inline constexpr double K_ASTRONOMICAL_UNIT = 1.495978707e11;
/// Speed of light in m/s
inline constexpr double K_SPEED_OF_LIGHT = 299792458.0;
/// Airy disk constant
inline constexpr double K_AIRY_CONSTANT = 1.21966;
/// Solar mass in kg
inline constexpr double K_SOLAR_MASS = 1.98847e30;
/// Solar radius in meters
inline constexpr double K_SOLAR_RADIUS = 6.957e8;
/// Parsec in meters
inline constexpr double K_PARSEC = 3.0857e16;

}  // namespace lithium::tools

// ============================================================================
// Legacy Global Constants (for backward compatibility)
// ============================================================================

// These are kept for backward compatibility with existing code
// New code should use the namespaced versions above

inline constexpr double K_DEGREES_TO_RADIANS = lithium::tools::K_DEGREES_TO_RADIANS;
inline constexpr double K_RADIANS_TO_DEGREES = lithium::tools::K_RADIANS_TO_DEGREES;
inline constexpr double K_HOURS_TO_DEGREES = lithium::tools::K_HOURS_TO_DEGREES;
inline constexpr double K_HOURS_IN_DAY = lithium::tools::K_HOURS_IN_DAY;
inline constexpr double K_MINUTES_IN_HOUR = lithium::tools::K_MINUTES_IN_HOUR;
inline constexpr double K_DEGREES_IN_CIRCLE = lithium::tools::K_DEGREES_IN_CIRCLE;
inline constexpr double K_EPSILON_VALUE = lithium::tools::K_EPSILON_VALUE;
inline constexpr double K_SECONDS_IN_HOUR = lithium::tools::K_SECONDS_IN_HOUR;

#endif  // LITHIUM_TOOLS_CONSTANT_HPP
