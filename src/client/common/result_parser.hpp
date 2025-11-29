/*
 * result_parser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Common result parsing utilities for solver clients

*************************************************/

#pragma once

#include "atom/type/expected.hpp"

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace lithium::client {

/**
 * @brief WCS (World Coordinate System) data structure
 */
struct WCSData {
    double crval1{0.0};  // Reference RA (degrees)
    double crval2{0.0};  // Reference Dec (degrees)
    double crpix1{0.0};  // Reference pixel X
    double crpix2{0.0};  // Reference pixel Y
    double cdelt1{0.0};  // Pixel scale X (degrees/pixel)
    double cdelt2{0.0};  // Pixel scale Y (degrees/pixel)
    double crota2{0.0};  // Rotation angle (degrees)
    double cd1_1{0.0};   // CD matrix element
    double cd1_2{0.0};   // CD matrix element
    double cd2_1{0.0};   // CD matrix element
    double cd2_2{0.0};   // CD matrix element

    std::string ctype1;  // Coordinate type X
    std::string ctype2;  // Coordinate type Y

    [[nodiscard]] auto getRaDeg() const noexcept -> double { return crval1; }
    [[nodiscard]] auto getDecDeg() const noexcept -> double { return crval2; }

    [[nodiscard]] auto getPixelScaleArcsec() const noexcept -> double {
        // Use CDELT2 if available, otherwise compute from CD matrix
        if (cdelt2 != 0.0) {
            return std::abs(cdelt2) * 3600.0;
        }
        // Compute from CD matrix
        double scale = std::sqrt(cd2_1 * cd2_1 + cd2_2 * cd2_2);
        return scale * 3600.0;
    }

    [[nodiscard]] auto getRotationDeg() const noexcept -> double {
        if (crota2 != 0.0) {
            return crota2;
        }
        // Compute from CD matrix
        return std::atan2(cd2_1, cd2_2) * 180.0 / 3.14159265358979323846;
    }

    [[nodiscard]] auto isValid() const noexcept -> bool {
        return crval1 != 0.0 || crval2 != 0.0;
    }
};

/**
 * @brief Parse error types
 */
enum class ParseError {
    FileNotFound,
    InvalidFormat,
    MissingData,
    ParseFailed
};

/**
 * @brief INI file parser for ASTAP output
 */
class IniParser {
public:
    /**
     * @brief Parse INI file
     * @param filepath Path to INI file
     * @return Map of key-value pairs or error
     */
    [[nodiscard]] static auto parse(const std::filesystem::path& filepath)
        -> atom::type::expected<std::unordered_map<std::string, std::string>,
                         ParseError>;

    /**
     * @brief Parse INI content string
     * @param content INI content
     * @return Map of key-value pairs
     */
    [[nodiscard]] static auto parseString(std::string_view content)
        -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Get value as double
     */
    [[nodiscard]] static auto getDouble(
        const std::unordered_map<std::string, std::string>& data,
        std::string_view key, double defaultValue = 0.0) -> double;

    /**
     * @brief Get value as bool
     */
    [[nodiscard]] static auto getBool(
        const std::unordered_map<std::string, std::string>& data,
        std::string_view key, bool defaultValue = false) -> bool;

    /**
     * @brief Get value as string
     */
    [[nodiscard]] static auto getString(
        const std::unordered_map<std::string, std::string>& data,
        std::string_view key, std::string_view defaultValue = "")
        -> std::string;
};

/**
 * @brief FITS header keyword parser
 */
class FitsHeaderParser {
public:
    /**
     * @brief Parse WCS from FITS header text
     * @param headerText FITS header as text
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseWCS(std::string_view headerText)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Parse WCS from FITS file
     * @param filepath Path to FITS file
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseWCSFromFile(
        const std::filesystem::path& filepath)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Extract keyword value from header
     * @param headerText FITS header text
     * @param keyword Keyword name
     * @return Value as string or nullopt
     */
    [[nodiscard]] static auto extractKeyword(std::string_view headerText,
                                             std::string_view keyword)
        -> std::optional<std::string>;

    /**
     * @brief Extract numeric keyword value
     */
    [[nodiscard]] static auto extractNumericKeyword(std::string_view headerText,
                                                    std::string_view keyword)
        -> std::optional<double>;
};

/**
 * @brief Astrometry.net output parser
 */
class AstrometryOutputParser {
public:
    /**
     * @brief Parse solve-field console output
     * @param output Console output text
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseConsoleOutput(std::string_view output)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Parse .wcs file
     * @param filepath Path to WCS file
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseWcsFile(const std::filesystem::path& filepath)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Check if output indicates success
     */
    [[nodiscard]] static auto isSuccessful(std::string_view output) -> bool;

    /**
     * @brief Extract error message from output
     */
    [[nodiscard]] static auto extractError(std::string_view output)
        -> std::optional<std::string>;
};

/**
 * @brief ASTAP output parser
 */
class AstapOutputParser {
public:
    /**
     * @brief Parse ASTAP INI output file
     * @param filepath Path to INI file
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseIniFile(const std::filesystem::path& filepath)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Parse ASTAP console output
     * @param output Console output text
     * @return WCS data or error
     */
    [[nodiscard]] static auto parseConsoleOutput(std::string_view output)
        -> atom::type::expected<WCSData, ParseError>;

    /**
     * @brief Check if output indicates success
     */
    [[nodiscard]] static auto isSuccessful(std::string_view output) -> bool;
};

/**
 * @brief Coordinate conversion utilities
 */
struct CoordinateUtils {
    /**
     * @brief Convert RA from HMS string to degrees
     */
    [[nodiscard]] static auto raHmsToDegs(std::string_view hms)
        -> std::optional<double>;

    /**
     * @brief Convert Dec from DMS string to degrees
     */
    [[nodiscard]] static auto decDmsToDegs(std::string_view dms)
        -> std::optional<double>;

    /**
     * @brief Convert RA from degrees to HMS string
     */
    [[nodiscard]] static auto raDegsToHms(double degrees) -> std::string;

    /**
     * @brief Convert Dec from degrees to DMS string
     */
    [[nodiscard]] static auto decDegsToHms(double degrees) -> std::string;

    /**
     * @brief Normalize RA to [0, 360) range
     */
    [[nodiscard]] static constexpr auto normalizeRa(double ra) noexcept
        -> double {
        while (ra < 0.0)
            ra += 360.0;
        while (ra >= 360.0)
            ra -= 360.0;
        return ra;
    }

    /**
     * @brief Clamp Dec to [-90, 90] range
     */
    [[nodiscard]] static constexpr auto clampDec(double dec) noexcept
        -> double {
        if (dec < -90.0)
            return -90.0;
        if (dec > 90.0)
            return 90.0;
        return dec;
    }
};

}  // namespace lithium::client
