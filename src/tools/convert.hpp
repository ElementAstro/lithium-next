#ifndef LITHIUM_TOOLS_CONVERT_HPP
#define LITHIUM_TOOLS_CONVERT_HPP

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace lithium::tools {

/**
 * @brief Represents Cartesian coordinates.
 */
struct CartesianCoordinates {
    double x;  ///< X coordinate
    double y;  ///< Y coordinate
    double z;  ///< Z coordinate
} ATOM_ALIGNAS(32);

/**
 * @brief Represents Spherical coordinates.
 */
struct SphericalCoordinates {
    double rightAscension;  ///< Right Ascension in degrees
    double declination;     ///< Declination in degrees
} ATOM_ALIGNAS(16);

/**
 * @brief Constrains a value within a specified range with proper wrap-around.
 *
 * @param value The value to constrain.
 * @param maxVal The maximum value of the range.
 * @param minVal The minimum value of the range.
 * @return The constrained value.
 */
auto rangeTo(double value, double maxVal, double minVal) -> double;

/**
 * @brief Converts degrees to radians.
 *
 * @param degree The angle in degrees.
 * @return The angle in radians.
 */
auto degreeToRad(double degree) -> double;

/**
 * @brief Converts radians to degrees.
 *
 * @param radians The angle in radians.
 * @return The angle in degrees.
 */
auto radToDegree(double radians) -> double;

/**
 * @brief Converts hours to degrees.
 *
 * @param hours The angle in hours.
 * @return The angle in degrees.
 */
auto hourToDegree(double hours) -> double;

/**
 * @brief Converts hours to radians.
 *
 * @param hours The angle in hours.
 * @return The angle in radians.
 */
auto hourToRad(double hours) -> double;

/**
 * @brief Converts degrees to hours.
 *
 * @param degrees The angle in degrees.
 * @return The angle in hours.
 */
auto degreeToHour(double degrees) -> double;

/**
 * @brief Converts radians to hours.
 *
 * @param radians The angle in radians.
 * @return The angle in hours.
 */
auto radToHour(double radians) -> double;

/**
 * @brief Calculates the Hour Angle (HA) in degrees.
 *
 * @param rightAscensionRad The Right Ascension in radians.
 * @param lstDegree The Local Sidereal Time in degrees.
 * @return The Hour Angle in degrees.
 */
auto getHaDegree(double rightAscensionRad, double lstDegree) -> double;

/**
 * @brief Converts equatorial coordinates (RA, Dec) to horizontal coordinates
 * (Alt, Az).
 *
 * @param hourAngleRad The Hour Angle in radians.
 * @param declinationRad The Declination in radians.
 * @param latitudeRad The observer's latitude in radians.
 * @return A vector containing Altitude and Azimuth in radians.
 */
auto raDecToAltAz(double hourAngleRad, double declinationRad,
                  double latitudeRad) -> std::vector<double>;

/**
 * @brief Converts horizontal coordinates (Alt, Az) to equatorial coordinates
 * (RA, Dec).
 *
 * @param altRadian The Altitude in radians.
 * @param azRadian The Azimuth in radians.
 * @param hrRadian Reference to store the Hour Angle in radians.
 * @param decRadian Reference to store the Declination in radians.
 * @param latRadian The observer's latitude in radians.
 */
void altAzToRaDec(double altRadian, double azRadian, double& hrRadian,
                  double& decRadian, double latRadian);

/**
 * @brief Converts equatorial coordinates to Cartesian coordinates.
 *
 * @param rightAscension The Right Ascension in degrees.
 * @param declination The Declination in degrees.
 * @param radius The radius (distance).
 * @return The Cartesian coordinates.
 */
auto convertEquatorialToCartesian(double rightAscension, double declination,
                                  double radius) -> CartesianCoordinates;

/**
 * @brief Converts Cartesian coordinates to spherical coordinates.
 *
 * @param cartesianPoint The Cartesian coordinates.
 * @return The spherical coordinates, or std::nullopt if the conversion fails.
 */
auto convertToSphericalCoordinates(const CartesianCoordinates& cartesianPoint)
    -> std::optional<SphericalCoordinates>;

/**
 * @brief Converts degrees, minutes, and seconds to decimal degrees.
 *
 * @param degrees The degrees component.
 * @param minutes The minutes component.
 * @param seconds The seconds component.
 * @return The angle in decimal degrees.
 */
auto dmsToDegree(int degrees, int minutes, double seconds) -> double;

/**
 * @brief Converts radians to a string representation in degrees, minutes, and
 * seconds (DMS).
 *
 * @param radians The angle in radians.
 * @return The string representation in DMS format.
 */
auto radToDmsStr(double radians) -> std::string;

/**
 * @brief Converts radians to a string representation in hours, minutes, and
 * seconds (HMS).
 *
 * @param radians The angle in radians.
 * @return The string representation in HMS format.
 */
auto radToHmsStr(double radians) -> std::string;
}  // namespace lithium::tools

#endif  // LITHIUM_TOOLS_CONVERT_HPP
