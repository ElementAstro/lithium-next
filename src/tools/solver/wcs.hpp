/**
 * @file wcs.hpp
 * @brief World Coordinate System (WCS) utilities for plate solving.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_SOLVER_WCS_HPP
#define LITHIUM_TOOLS_SOLVER_WCS_HPP

#include <string>
#include <vector>

#include "tools/conversion/coordinate.hpp"

namespace lithium::tools::solver {

using conversion::SphericalCoordinates;

// ============================================================================
// WCS Parameters
// ============================================================================

/**
 * @struct WCSParams
 * @brief World Coordinate System parameters from plate solving.
 */
struct alignas(64) WCSParams {
    double crpix0{0.0};  ///< Reference pixel X
    double crpix1{0.0};  ///< Reference pixel Y
    double crval0{0.0};  ///< Reference RA in degrees
    double crval1{0.0};  ///< Reference Dec in degrees
    double cd11{0.0};    ///< CD matrix element [1,1]
    double cd12{0.0};    ///< CD matrix element [1,2]
    double cd21{0.0};    ///< CD matrix element [2,1]
    double cd22{0.0};    ///< CD matrix element [2,2]

    WCSParams() = default;

    /**
     * @brief Calculate plate scale in arcseconds per pixel.
     * @return Plate scale.
     */
    [[nodiscard]] double plateScale() const noexcept {
        double scale = std::sqrt(cd11 * cd11 + cd21 * cd21);
        return scale * 3600.0;  // Convert degrees to arcseconds
    }

    /**
     * @brief Calculate field rotation in degrees.
     * @return Rotation angle.
     */
    [[nodiscard]] double rotation() const noexcept {
        return std::atan2(cd21, cd11) * 180.0 / 3.14159265358979323846;
    }

    /**
     * @brief Check if WCS is valid.
     * @return true if parameters are reasonable.
     */
    [[nodiscard]] bool isValid() const noexcept {
        return crpix0 > 0 && crpix1 > 0 &&
               (cd11 != 0 || cd12 != 0 || cd21 != 0 || cd22 != 0);
    }
};

// ============================================================================
// WCS Functions
// ============================================================================

/**
 * @brief Extract WCS parameters from solver output string.
 * @param wcsInfo WCS information string.
 * @return Parsed WCS parameters.
 */
[[nodiscard]] WCSParams extractWCSParams(const std::string& wcsInfo);

/**
 * @brief Convert pixel coordinates to RA/Dec using WCS.
 * @param x Pixel X coordinate.
 * @param y Pixel Y coordinate.
 * @param wcs WCS parameters.
 * @return Spherical coordinates (RA/Dec in degrees).
 */
[[nodiscard]] SphericalCoordinates pixelToRaDec(double x, double y,
                                                 const WCSParams& wcs);

/**
 * @brief Convert RA/Dec to pixel coordinates using WCS.
 * @param ra Right Ascension in degrees.
 * @param dec Declination in degrees.
 * @param wcs WCS parameters.
 * @param[out] x Pixel X coordinate.
 * @param[out] y Pixel Y coordinate.
 * @return true if conversion successful.
 */
[[nodiscard]] bool raDecToPixel(double ra, double dec, const WCSParams& wcs,
                                 double& x, double& y);

/**
 * @brief Get FOV corner coordinates.
 * @param wcs WCS parameters.
 * @param imageWidth Image width in pixels.
 * @param imageHeight Image height in pixels.
 * @return Vector of corner coordinates (RA/Dec).
 */
[[nodiscard]] std::vector<SphericalCoordinates> getFOVCorners(
    const WCSParams& wcs, int imageWidth, int imageHeight);

/**
 * @brief Calculate FOV dimensions.
 * @param wcs WCS parameters.
 * @param imageWidth Image width in pixels.
 * @param imageHeight Image height in pixels.
 * @param[out] fovWidth FOV width in degrees.
 * @param[out] fovHeight FOV height in degrees.
 */
void calculateFOVDimensions(const WCSParams& wcs, int imageWidth,
                            int imageHeight, double& fovWidth,
                            double& fovHeight);

}  // namespace lithium::tools::solver

#endif  // LITHIUM_TOOLS_SOLVER_WCS_HPP
