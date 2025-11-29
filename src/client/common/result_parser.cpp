/*
 * result_parser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: Result parsing utilities implementation

*************************************************/

#include "result_parser.hpp"

#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::client {

// ============================================================================
// IniParser Implementation
// ============================================================================

auto IniParser::parse(const std::filesystem::path& filepath)
    -> atom::type::expected<std::unordered_map<std::string, std::string>, ParseError> {
    if (!std::filesystem::exists(filepath)) {
        return atom::type::unexpected(ParseError::FileNotFound);
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return atom::type::unexpected(ParseError::FileNotFound);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str());
}

auto IniParser::parseString(std::string_view content)
    -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> result;
    std::istringstream stream(std::string(content));
    std::string line;

    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        // Skip section headers
        if (line[0] == '[') {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            // Trim whitespace
            auto trimWs = [](std::string& s) {
                s.erase(0, s.find_first_not_of(" \t\r\n"));
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
            };

            trimWs(key);
            trimWs(value);

            result[key] = value;
        }
    }

    return result;
}

auto IniParser::getDouble(
    const std::unordered_map<std::string, std::string>& data,
    std::string_view key, double defaultValue) -> double {
    auto it = data.find(std::string(key));
    if (it == data.end()) {
        return defaultValue;
    }
    try {
        return std::stod(it->second);
    } catch (...) {
        return defaultValue;
    }
}

auto IniParser::getBool(
    const std::unordered_map<std::string, std::string>& data,
    std::string_view key, bool defaultValue) -> bool {
    auto it = data.find(std::string(key));
    if (it == data.end()) {
        return defaultValue;
    }
    const auto& val = it->second;
    return val == "T" || val == "true" || val == "1" || val == "yes";
}

auto IniParser::getString(
    const std::unordered_map<std::string, std::string>& data,
    std::string_view key, std::string_view defaultValue) -> std::string {
    auto it = data.find(std::string(key));
    if (it == data.end()) {
        return std::string(defaultValue);
    }
    return it->second;
}

// ============================================================================
// FitsHeaderParser Implementation
// ============================================================================

auto FitsHeaderParser::parseWCS(std::string_view headerText)
    -> atom::type::expected<WCSData, ParseError> {
    WCSData wcs;

    // Parse standard WCS keywords
    if (auto val = extractNumericKeyword(headerText, "CRVAL1")) {
        wcs.crval1 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CRVAL2")) {
        wcs.crval2 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CRPIX1")) {
        wcs.crpix1 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CRPIX2")) {
        wcs.crpix2 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CDELT1")) {
        wcs.cdelt1 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CDELT2")) {
        wcs.cdelt2 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CROTA2")) {
        wcs.crota2 = *val;
    }

    // CD matrix
    if (auto val = extractNumericKeyword(headerText, "CD1_1")) {
        wcs.cd1_1 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CD1_2")) {
        wcs.cd1_2 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CD2_1")) {
        wcs.cd2_1 = *val;
    }
    if (auto val = extractNumericKeyword(headerText, "CD2_2")) {
        wcs.cd2_2 = *val;
    }

    // Coordinate types
    if (auto val = extractKeyword(headerText, "CTYPE1")) {
        wcs.ctype1 = *val;
    }
    if (auto val = extractKeyword(headerText, "CTYPE2")) {
        wcs.ctype2 = *val;
    }

    if (!wcs.isValid()) {
        return atom::type::unexpected(ParseError::MissingData);
    }

    return wcs;
}

auto FitsHeaderParser::parseWCSFromFile(const std::filesystem::path& filepath)
    -> atom::type::expected<WCSData, ParseError> {
    if (!std::filesystem::exists(filepath)) {
        return atom::type::unexpected(ParseError::FileNotFound);
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return atom::type::unexpected(ParseError::FileNotFound);
    }

    // Read FITS header (2880-byte blocks)
    std::string header;
    char block[2880];

    while (file.read(block, sizeof(block))) {
        header.append(block, sizeof(block));
        // Check for END keyword
        if (header.find("END") != std::string::npos) {
            break;
        }
    }

    return parseWCS(header);
}

auto FitsHeaderParser::extractKeyword(std::string_view headerText,
                                      std::string_view keyword)
    -> std::optional<std::string> {
    // FITS keyword format: KEYWORD = 'value' / comment
    // or: KEYWORD = value / comment
    std::string pattern =
        std::string(keyword) + R"(\s*=\s*(?:'([^']*)'|([^\s/]+)))";
    std::regex re(pattern);
    std::smatch match;
    std::string text(headerText);

    if (std::regex_search(text, match, re)) {
        if (match[1].matched) {
            return match[1].str();
        }
        if (match[2].matched) {
            return match[2].str();
        }
    }
    return std::nullopt;
}

auto FitsHeaderParser::extractNumericKeyword(std::string_view headerText,
                                             std::string_view keyword)
    -> std::optional<double> {
    auto strVal = extractKeyword(headerText, keyword);
    if (!strVal) {
        return std::nullopt;
    }
    try {
        return std::stod(*strVal);
    } catch (...) {
        return std::nullopt;
    }
}

// ============================================================================
// AstrometryOutputParser Implementation
// ============================================================================

auto AstrometryOutputParser::parseConsoleOutput(std::string_view output)
    -> atom::type::expected<WCSData, ParseError> {
    WCSData wcs;

    // Parse field center from output
    // Format: Field center: (RA,Dec) = (123.456, -45.678) deg.
    std::regex centerRegex(
        R"(Field center:\s*\(RA,Dec\)\s*=\s*\(([^,]+),\s*([^)]+)\))");
    std::smatch match;
    std::string text(output);

    if (std::regex_search(text, match, centerRegex)) {
        try {
            wcs.crval1 = std::stod(match[1].str());
            wcs.crval2 = std::stod(match[2].str());
        } catch (...) {
            return atom::type::unexpected(ParseError::ParseFailed);
        }
    }

    // Parse pixel scale
    // Format: pixel scale 1.23 arcsec/pix
    std::regex scaleRegex(R"(pixel scale\s*([0-9.]+)\s*arcsec/pix)");
    if (std::regex_search(text, match, scaleRegex)) {
        try {
            wcs.cdelt2 = std::stod(match[1].str()) / 3600.0;
        } catch (...) {
        }
    }

    // Parse rotation
    // Format: Field rotation angle: up is 123.45 degrees
    std::regex rotRegex(
        R"(Field rotation angle:\s*up is\s*([0-9.-]+)\s*degrees)");
    if (std::regex_search(text, match, rotRegex)) {
        try {
            wcs.crota2 = std::stod(match[1].str());
        } catch (...) {
        }
    }

    if (!wcs.isValid()) {
        return atom::type::unexpected(ParseError::MissingData);
    }

    return wcs;
}

auto AstrometryOutputParser::parseWcsFile(const std::filesystem::path& filepath)
    -> atom::type::expected<WCSData, ParseError> {
    return FitsHeaderParser::parseWCSFromFile(filepath);
}

auto AstrometryOutputParser::isSuccessful(std::string_view output) -> bool {
    return output.find("Field center") != std::string_view::npos ||
           output.find("solved") != std::string_view::npos;
}

auto AstrometryOutputParser::extractError(std::string_view output)
    -> std::optional<std::string> {
    if (output.find("Did not solve") != std::string_view::npos) {
        return "Did not solve - no matching stars found";
    }
    if (output.find("failed") != std::string_view::npos) {
        return "Solve failed";
    }
    return std::nullopt;
}

// ============================================================================
// AstapOutputParser Implementation
// ============================================================================

auto AstapOutputParser::parseIniFile(const std::filesystem::path& filepath)
    -> atom::type::expected<WCSData, ParseError> {
    auto iniResult = IniParser::parse(filepath);
    if (!iniResult.has_value()) {
        return iniResult.error();
    }

    const auto& data = iniResult.value();

    // Check for solution
    if (!IniParser::getBool(data, "PLTSOLVD", false)) {
        return atom::type::unexpected(ParseError::MissingData);
    }

    WCSData wcs;
    wcs.crval1 = IniParser::getDouble(data, "CRVAL1", 0.0);
    wcs.crval2 = IniParser::getDouble(data, "CRVAL2", 0.0);
    wcs.crpix1 = IniParser::getDouble(data, "CRPIX1", 0.0);
    wcs.crpix2 = IniParser::getDouble(data, "CRPIX2", 0.0);
    wcs.cdelt1 = IniParser::getDouble(data, "CDELT1", 0.0);
    wcs.cdelt2 = IniParser::getDouble(data, "CDELT2", 0.0);
    wcs.crota2 = IniParser::getDouble(data, "CROTA2", 0.0);
    wcs.cd1_1 = IniParser::getDouble(data, "CD1_1", 0.0);
    wcs.cd1_2 = IniParser::getDouble(data, "CD1_2", 0.0);
    wcs.cd2_1 = IniParser::getDouble(data, "CD2_1", 0.0);
    wcs.cd2_2 = IniParser::getDouble(data, "CD2_2", 0.0);

    if (!wcs.isValid()) {
        return atom::type::unexpected(ParseError::MissingData);
    }

    return wcs;
}

auto AstapOutputParser::parseConsoleOutput(std::string_view output)
    -> atom::type::expected<WCSData, ParseError> {
    // ASTAP console output parsing
    if (!isSuccessful(output)) {
        return atom::type::unexpected(ParseError::MissingData);
    }

    // ASTAP typically outputs to INI file, console parsing is limited
    return atom::type::unexpected(ParseError::MissingData);
}

auto AstapOutputParser::isSuccessful(std::string_view output) -> bool {
    return output.find("Solution found") != std::string_view::npos ||
           output.find("Solved") != std::string_view::npos;
}

// ============================================================================
// CoordinateUtils Implementation
// ============================================================================

auto CoordinateUtils::raHmsToDegs(std::string_view hms)
    -> std::optional<double> {
    // Format: HH:MM:SS.ss or HH MM SS.ss
    std::regex re(R"((\d+)[:\s]+(\d+)[:\s]+([0-9.]+))");
    std::smatch match;
    std::string text(hms);

    if (std::regex_search(text, match, re)) {
        try {
            double h = std::stod(match[1].str());
            double m = std::stod(match[2].str());
            double s = std::stod(match[3].str());
            return (h + m / 60.0 + s / 3600.0) * 15.0;  // Convert hours to degrees
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

auto CoordinateUtils::decDmsToDegs(std::string_view dms)
    -> std::optional<double> {
    // Format: +DD:MM:SS.ss or DD MM SS.ss
    std::regex re(R"(([+-]?\d+)[:\s]+(\d+)[:\s]+([0-9.]+))");
    std::smatch match;
    std::string text(dms);

    if (std::regex_search(text, match, re)) {
        try {
            double d = std::stod(match[1].str());
            double m = std::stod(match[2].str());
            double s = std::stod(match[3].str());
            double sign = (d < 0 || text.find('-') != std::string::npos) ? -1.0
                                                                         : 1.0;
            return sign * (std::abs(d) + m / 60.0 + s / 3600.0);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

auto CoordinateUtils::raDegsToHms(double degrees) -> std::string {
    degrees = normalizeRa(degrees);
    double hours = degrees / 15.0;
    int h = static_cast<int>(hours);
    double minRem = (hours - h) * 60.0;
    int m = static_cast<int>(minRem);
    double s = (minRem - m) * 60.0;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%05.2f", h, m, s);
    return buf;
}

auto CoordinateUtils::decDegsToHms(double degrees) -> std::string {
    degrees = clampDec(degrees);
    char sign = degrees >= 0 ? '+' : '-';
    degrees = std::abs(degrees);
    int d = static_cast<int>(degrees);
    double minRem = (degrees - d) * 60.0;
    int m = static_cast<int>(minRem);
    double s = (minRem - m) * 60.0;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%c%02d:%02d:%05.2f", sign, d, m, s);
    return buf;
}

}  // namespace lithium::client
