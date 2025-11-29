/*
 * astro_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file astro_ops.hpp
 * @brief Astronomical Data and FITS Image Operations
 * @date 2024
 * @version 1.0.0
 *
 * This module provides utilities for astronomical data processing:
 * - Star catalog creation and parsing
 * - FITS image loading and saving
 * - Image statistics calculation
 * - World Coordinate System (WCS) transformations
 */

#ifndef LITHIUM_SCRIPT_ASTRO_OPS_HPP
#define LITHIUM_SCRIPT_ASTRO_OPS_HPP

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <concepts>
#include <expected>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

// Forward declarations
enum class NumpyError;
template<typename T>
concept NumpyCompatible = requires {
    { 0 } -> std::convertible_to<T>;
};
struct StarData;
struct ImageStats;
template<typename T>
using NumpyResult = std::expected<T, NumpyError>;

/**
 * @brief Astronomical operations on star catalogs and FITS images
 */
class AstroOps {
public:
    // =========================================================================
    // Star Catalog Operations
    // =========================================================================

    /**
     * @brief Create a star catalog DataFrame from StarData vector
     * @param stars Vector of star data structures
     * @return DataFrame with columns: ra, dec, magnitude, bv_color, name,
     *         hip_id, pm_ra, pm_dec, parallax
     */
    [[nodiscard]] static NumpyResult<py::object> createStarCatalog(
        const std::vector<StarData>& stars);

    /**
     * @brief Parse star catalog DataFrame to StarData vector
     * @param df Pandas DataFrame with star data
     * @return Vector of StarData structures on success, error on failure
     */
    [[nodiscard]] static NumpyResult<std::vector<StarData>> parseStarCatalog(
        const py::object& df);

    // =========================================================================
    // FITS Image Operations
    // =========================================================================

    /**
     * @brief Load FITS image as NumPy array
     * @param fitsPath Path to FITS file
     * @param hdu Header-Data Unit index (default: 0 for primary)
     * @return NumPy array containing image data on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> loadFitsImage(
        const std::filesystem::path& fitsPath, int hdu = 0);

    /**
     * @brief Save NumPy array as FITS image
     * @tparam T NumPy-compatible data type
     * @param arr NumPy array to save
     * @param fitsPath Output FITS file path
     * @param header Optional FITS header keywords (key-value pairs)
     * @return Success or error result
     */
    template<typename T>
    [[nodiscard]] static NumpyResult<void> saveFitsImage(
        const py::array_t<T>& arr,
        const std::filesystem::path& fitsPath,
        const std::unordered_map<std::string, std::string>& header = {});

    /**
     * @brief Calculate image statistics
     * @tparam T NumPy-compatible data type
     * @param arr NumPy array (image data)
     * @return ImageStats structure with min, max, mean, median, stddev, etc.
     */
    template<typename T>
    [[nodiscard]] static ImageStats calculateImageStats(const py::array_t<T>& arr);

    // =========================================================================
    // World Coordinate System (WCS) Operations
    // =========================================================================

    /**
     * @brief Convert pixel coordinates to world coordinates using WCS
     * @param wcs WCS object (from astropy.wcs)
     * @param x Pixel X coordinate
     * @param y Pixel Y coordinate
     * @return Pair of (RA, Dec) in degrees on success, error on failure
     */
    [[nodiscard]] static NumpyResult<std::pair<double, double>> pixelToWorld(
        const py::object& wcs, double x, double y);

    /**
     * @brief Convert world coordinates to pixel coordinates using WCS
     * @param wcs WCS object (from astropy.wcs)
     * @param ra Right Ascension (degrees)
     * @param dec Declination (degrees)
     * @return Pair of (pixel_x, pixel_y) on success, error on failure
     */
    [[nodiscard]] static NumpyResult<std::pair<double, double>> worldToPixel(
        const py::object& wcs, double ra, double dec);
};

// =========================================================================
// Template Implementations
// =========================================================================

template<typename T>
NumpyResult<void> AstroOps::saveFitsImage(
    const py::array_t<T>& arr,
    const std::filesystem::path& fitsPath,
    const std::unordered_map<std::string, std::string>& header) {

    try {
        py::module fits = py::module::import("astropy.io.fits");

        // Create primary HDU
        py::object hdu = fits.attr("PrimaryHDU")(arr);

        // Add header cards
        py::object hduHeader = hdu.attr("header");
        for (const auto& [key, value] : header) {
            hduHeader[py::cast(key)] = py::cast(value);
        }

        // Write to file
        hdu.attr("writeto")(fitsPath.string(), py::arg("overwrite") = true);

        return {};

    } catch (const py::error_already_set& e) {
        return std::unexpected(NumpyError::FitsError);
    }
}

template<typename T>
ImageStats AstroOps::calculateImageStats(const py::array_t<T>& arr) {
    ImageStats stats;

    try {
        py::module np = py::module::import("numpy");

        // Mask invalid values (NaN, Inf)
        py::array validData = np.attr("ma").attr("masked_invalid")(arr);

        stats.min = np.attr("min")(validData).cast<double>();
        stats.max = np.attr("max")(validData).cast<double>();
        stats.mean = np.attr("mean")(validData).cast<double>();
        stats.median = np.attr("median")(validData).cast<double>();
        stats.stddev = np.attr("std")(validData).cast<double>();
        stats.sum = np.attr("sum")(validData).cast<double>();
        stats.validPixels = np.attr("count")(validData).cast<size_t>();

    } catch (const py::error_already_set&) {
        // Return default stats on error
    }

    return stats;
}

}  // namespace lithium::numpy

#endif  // LITHIUM_SCRIPT_ASTRO_OPS_HPP
