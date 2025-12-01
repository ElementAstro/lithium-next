// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "votable_parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace lithium::target::online {

// Helper functions for XML parsing
namespace {

/**
 * @brief Trim whitespace from string
 */
std::string_view trimWhitespace(std::string_view str) {
    const char* start = str.data();
    const char* end = str.data() + str.size();

    while (start != end && std::isspace(*start)) {
        ++start;
    }
    while (start != end && std::isspace(*(end - 1))) {
        --end;
    }

    return {start, static_cast<size_t>(end - start)};
}

/**
 * @brief Extract XML tag content between <tag>content</tag>
 */
std::string extractTagContent(std::string_view xml, std::string_view tag) {
    std::string openTag = std::string("<") + std::string(tag) + ">";
    std::string closeTag = std::string("</") + std::string(tag) + ">";

    size_t startPos = xml.find(openTag);
    if (startPos == std::string_view::npos) {
        return "";
    }

    startPos += openTag.length();
    size_t endPos = xml.find(closeTag, startPos);
    if (endPos == std::string_view::npos) {
        return "";
    }

    return std::string(xml.substr(startPos, endPos - startPos));
}

/**
 * @brief Extract XML attribute value
 */
std::string extractAttribute(std::string_view tag, std::string_view attrName) {
    std::string pattern = std::string(attrName) + "=\"([^\"]*)\"";
    std::regex re(pattern);
    std::match_results<std::string_view::const_iterator> match;

    if (std::regex_search(tag.begin(), tag.end(), match, re)) {
        return match[1].str();
    }
    return "";
}

/**
 * @brief Parse sexagesimal coordinates to decimal degrees
 * Format: "HH:MM:SS.SSS" or "±DD:MM:SS.SSS"
 */
std::optional<double> parseSexagesimal(std::string_view coords) {
    // Remove whitespace
    std::string str(coords);
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());

    if (str.empty()) {
        return std::nullopt;
    }

    // Check for sign
    bool negative = str[0] == '-';
    if (str[0] == '+' || str[0] == '-') {
        str = str.substr(1);
    }

    // Split by colon
    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos < str.length()) {
        size_t nextPos = str.find(':', pos);
        if (nextPos == std::string::npos) {
            parts.push_back(str.substr(pos));
            break;
        }
        parts.push_back(str.substr(pos, nextPos - pos));
        pos = nextPos + 1;
    }

    if (parts.empty() || parts.size() > 3) {
        return std::nullopt;
    }

    double degrees = 0.0;
    double minutes = 0.0;
    double seconds = 0.0;

    try {
        if (parts.size() >= 1) {
            degrees = std::stod(parts[0]);
        }
        if (parts.size() >= 2) {
            minutes = std::stod(parts[1]);
        }
        if (parts.size() >= 3) {
            seconds = std::stod(parts[2]);
        }
    } catch (...) {
        return std::nullopt;
    }

    double result = degrees + minutes / 60.0 + seconds / 3600.0;
    return negative ? -result : result;
}

/**
 * @brief Try to parse as decimal number first, then sexagesimal
 */
std::optional<double> parseCoordinate(std::string_view coord) {
    std::string str(coord);
    str = std::string(trimWhitespace(str));

    if (str.empty()) {
        return std::nullopt;
    }

    // Try decimal first
    try {
        return std::stod(str);
    } catch (...) {
        // Try sexagesimal
        return parseSexagesimal(str);
    }
}

/**
 * @brief Parse magnitude value with band
 * Can be "12.34", "12.34V", "12.34B", etc.
 */
std::optional<double> parseMagnitude(std::string_view magStr) {
    std::string str(magStr);
    str = std::string(trimWhitespace(str));

    if (str.empty()) {
        return std::nullopt;
    }

    // Remove trailing band indicator (V, B, R, I, etc.)
    while (!str.empty() && std::isalpha(str.back())) {
        str.pop_back();
    }

    if (str.empty()) {
        return std::nullopt;
    }

    try {
        return std::stod(str);
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Parse TIME-STAMP format to chrono time_point
 * Formats: "2000-01-01T00:00:00" or "2000-01-01"
 */
std::chrono::system_clock::time_point parseTimeStamp(std::string_view timestamp) {
    // For simplicity, just use epoch if parsing fails
    // A full implementation would use std::get_time or similar
    return std::chrono::system_clock::now();
}

}  // namespace

/**
 * @brief Implementation class using PIMPL pattern
 */
class VotableParser::Impl {
public:
    std::unordered_map<std::string, std::string> fieldMappings;
    std::vector<VotableFieldMapping> customMappings;

    CelestialObjectModel parseRow(
        const std::vector<std::string>& values,
        const std::vector<std::string>& fieldNames) {
        CelestialObjectModel obj;

        // Map field values to object properties
        for (size_t i = 0; i < fieldNames.size() && i < values.size(); ++i) {
            const auto& fieldName = fieldNames[i];
            const auto& value = values[i];

            if (value.empty()) {
                continue;
            }

            // Check custom mappings first
            for (const auto& mapping : customMappings) {
                if (mapping.votableField == fieldName) {
                    std::string processedValue = value;
                    if (mapping.transform) {
                        processedValue = (*mapping.transform)(value);
                    }

                    if (mapping.modelField == "identifier") {
                        obj.identifier = processedValue;
                    } else if (mapping.modelField == "raJ2000") {
                        obj.raJ2000 = processedValue;
                        if (auto raVal = parseCoordinate(processedValue)) {
                            obj.radJ2000 = *raVal;
                        }
                    } else if (mapping.modelField == "decJ2000") {
                        obj.decJ2000 = processedValue;
                        if (auto decVal = parseCoordinate(processedValue)) {
                            obj.decDJ2000 = *decVal;
                        }
                    } else if (mapping.modelField == "type") {
                        obj.type = processedValue;
                    } else if (mapping.modelField == "visualMagnitudeV") {
                        if (auto magVal = parseMagnitude(processedValue)) {
                            obj.visualMagnitudeV = *magVal;
                        }
                    } else if (mapping.modelField == "constellationEn") {
                        obj.constellationEn = processedValue;
                    }
                    break;
                }
            }

            // Fallback to standard mapping
            if (fieldName == "main_id" || fieldName == "name" ||
                fieldName == "Name") {
                obj.identifier = value;
            } else if (fieldName == "ra" || fieldName == "RA" ||
                       fieldName == "RA_ICRS_Angle_alpha") {
                obj.raJ2000 = value;
                if (auto raVal = parseCoordinate(value)) {
                    obj.radJ2000 = *raVal;
                }
            } else if (fieldName == "dec" || fieldName == "DEC" ||
                       fieldName == "DEC_ICRS_Angle_delta") {
                obj.decJ2000 = value;
                if (auto decVal = parseCoordinate(value)) {
                    obj.decDJ2000 = *decVal;
                }
            } else if (fieldName == "otype" || fieldName == "Morphology") {
                obj.type = value;
            } else if (fieldName == "V" || fieldName == "Vmag" ||
                       fieldName == "mag") {
                if (auto magVal = parseMagnitude(value)) {
                    obj.visualMagnitudeV = *magVal;
                }
            } else if (fieldName == "B" || fieldName == "Bmag") {
                if (auto magVal = parseMagnitude(value)) {
                    obj.photographicMagnitudeB = *magVal;
                }
            } else if (fieldName == "Const" || fieldName == "Constellation") {
                obj.constellationEn = value;
            } else if (fieldName == "Major_axis") {
                try {
                    obj.majorAxis = std::stod(value);
                } catch (...) {
                }
            } else if (fieldName == "Minor_axis") {
                try {
                    obj.minorAxis = std::stod(value);
                } catch (...) {
                }
            }
        }

        return obj;
    }

    EphemerisPoint parseEphemerisRow(
        const std::vector<std::string>& values,
        const std::vector<std::string>& fieldNames) {
        EphemerisPoint point;

        for (size_t i = 0; i < fieldNames.size() && i < values.size(); ++i) {
            const auto& fieldName = fieldNames[i];
            const auto& value = values[i];

            if (value.empty()) {
                continue;
            }

            if (fieldName == "DATE__1" || fieldName == "Date_UTC") {
                point.time = parseTimeStamp(value);
            } else if (fieldName == "RA" || fieldName == "RA_ICRS") {
                if (auto raVal = parseCoordinate(value)) {
                    point.ra = *raVal;
                }
            } else if (fieldName == "DEC" || fieldName == "DEC_ICRS") {
                if (auto decVal = parseCoordinate(value)) {
                    point.dec = *decVal;
                }
            } else if (fieldName == "Delta" || fieldName == "Distance") {
                try {
                    point.distance = std::stod(value);
                } catch (...) {
                }
            } else if (fieldName == "Mag" || fieldName == "Mag_total") {
                if (auto magVal = parseMagnitude(value)) {
                    point.magnitude = *magVal;
                }
            } else if (fieldName == "Elong" || fieldName == "Elongation") {
                try {
                    point.elongation = std::stod(value);
                } catch (...) {
                }
            } else if (fieldName == "Phase" || fieldName == "Phase_Angle") {
                try {
                    point.phaseAngle = std::stod(value);
                } catch (...) {
                }
            } else if (fieldName == "AZ" || fieldName == "Azimuth") {
                try {
                    point.azimuth = std::stod(value);
                } catch (...) {
                }
            } else if (fieldName == "EL" || fieldName == "Altitude") {
                try {
                    point.altitude = std::stod(value);
                } catch (...) {
                }
            }
        }

        return point;
    }
};

// Public interface implementation

VotableParser::VotableParser() : pImpl_(std::make_unique<Impl>()) {}

VotableParser::~VotableParser() = default;

VotableParser::VotableParser(VotableParser&&) noexcept = default;

VotableParser& VotableParser::operator=(VotableParser&&) noexcept = default;

void VotableParser::setFieldMappings(
    const std::vector<VotableFieldMapping>& mappings) {
    pImpl_->customMappings = mappings;
}

std::vector<VotableFieldMapping> VotableParser::simbadMappings() {
    return {
        {"main_id", "identifier", std::nullopt},
        {"RA_ICRS_Angle_alpha", "raJ2000", std::nullopt},
        {"DEC_ICRS_Angle_delta", "decJ2000", std::nullopt},
        {"V", "visualMagnitudeV", std::nullopt},
        {"B", "photographicMagnitudeB", std::nullopt},
        {"Const", "constellationEn", std::nullopt},
    };
}

std::vector<VotableFieldMapping> VotableParser::vizierNgcMappings() {
    return {
        {"Name", "identifier", std::nullopt},
        {"RA_ICRS_Angle_alpha", "raJ2000", std::nullopt},
        {"DEC_ICRS_Angle_delta", "decJ2000", std::nullopt},
        {"Morphology", "type", std::nullopt},
        {"V_mag", "visualMagnitudeV", std::nullopt},
        {"Const", "constellationEn", std::nullopt},
        {"Major_axis", "majorAxis", std::nullopt},
        {"Minor_axis", "minorAxis", std::nullopt},
    };
}

auto VotableParser::parse(std::string_view content)
    -> atom::type::Expected<std::vector<CelestialObjectModel>, ParseError> {
    std::vector<CelestialObjectModel> results;

    try {
        std::string contentStr(content);

        // Extract RESOURCE section
        size_t resourceStart = contentStr.find("<RESOURCE");
        if (resourceStart == std::string::npos) {
            return atom::type::unexpected(ParseError{
                "No RESOURCE element found",
                std::nullopt,
                std::nullopt,
                "XML structure is missing RESOURCE tag"});
        }

        size_t resourceEnd =
            contentStr.find("</RESOURCE>", resourceStart);
        if (resourceEnd == std::string::npos) {
            return atom::type::unexpected(ParseError{
                "RESOURCE element not closed",
                std::nullopt,
                std::nullopt,
                "Missing closing </RESOURCE> tag"});
        }

        std::string_view resourceContent(
            contentStr.data() + resourceStart,
            resourceEnd - resourceStart + 11);  // +11 for "</RESOURCE>"

        // Extract TABLE section
        size_t tableStart = resourceContent.find("<TABLE");
        if (tableStart == std::string::npos) {
            return atom::type::unexpected(ParseError{
                "No TABLE element found in RESOURCE",
                std::nullopt,
                std::nullopt,
                "VOTable structure missing TABLE tag"});
        }

        size_t tableEnd = resourceContent.find("</TABLE>", tableStart);
        if (tableEnd == std::string::npos) {
            return atom::type::unexpected(ParseError{
                "TABLE element not closed",
                std::nullopt,
                std::nullopt,
                "Missing closing </TABLE> tag"});
        }

        std::string_view tableContent(
            resourceContent.data() + tableStart,
            tableEnd - tableStart + 8);  // +8 for "</TABLE>"

        // Extract FIELD definitions to get field names and types
        std::vector<std::string> fieldNames;
        size_t fieldPos = 0;
        while ((fieldPos = tableContent.find("<FIELD", fieldPos)) !=
               std::string::npos) {
            size_t fieldEnd = tableContent.find(">", fieldPos);
            if (fieldEnd == std::string::npos) {
                break;
            }

            std::string_view fieldTag(tableContent.data() + fieldPos,
                                      fieldEnd - fieldPos + 1);
            std::string name = extractAttribute(fieldTag, "name");
            if (!name.empty()) {
                fieldNames.push_back(name);
            }

            fieldPos = fieldEnd + 1;
        }

        if (fieldNames.empty()) {
            return atom::type::unexpected(ParseError{
                "No FIELD definitions found",
                std::nullopt,
                std::nullopt,
                "Unable to determine column structure"});
        }

        // Extract TABLEDATA section
        size_t tabledataStart = tableContent.find("<TABLEDATA");
        if (tabledataStart == std::string::npos) {
            // Some VOTables might have DATA instead
            tabledataStart = tableContent.find("<DATA");
            if (tabledataStart == std::string::npos) {
                return atom::type::unexpected(ParseError{
                    "No TABLEDATA or DATA element found",
                    std::nullopt,
                    std::nullopt,
                    "Missing data section in table"});
            }
        }

        size_t tabledataEnd = tableContent.find("</TABLEDATA>", tabledataStart);
        if (tabledataEnd == std::string::npos) {
            tabledataEnd = tableContent.find("</DATA>", tabledataStart);
            if (tabledataEnd == std::string::npos) {
                return atom::type::unexpected(ParseError{
                    "TABLEDATA/DATA element not closed",
                    std::nullopt,
                    std::nullopt,
                    "Missing closing tag for data section"});
            }
        }

        // Find first <TR> after TABLEDATA opening
        size_t trStart = tableContent.find("<TABLEDATA>", tabledataStart);
        if (trStart == std::string::npos) {
            trStart = tabledataStart;
        } else {
            trStart += 11;  // length of "<TABLEDATA>"
        }

        // Parse rows
        size_t rowPos = trStart;
        while ((rowPos = tableContent.find("<TR>", rowPos)) !=
               std::string::npos) {
            size_t rowEnd = tableContent.find("</TR>", rowPos);
            if (rowEnd == std::string::npos) {
                break;
            }

            std::string_view rowContent(tableContent.data() + rowPos + 4,
                                        rowEnd - rowPos - 4);

            // Extract TD values
            std::vector<std::string> values;
            size_t tdPos = 0;
            while ((tdPos = rowContent.find("<TD>", tdPos)) !=
                   std::string::npos) {
                size_t tdEnd = rowContent.find("</TD>", tdPos);
                if (tdEnd == std::string::npos) {
                    break;
                }

                std::string_view cellContent(
                    rowContent.data() + tdPos + 4,
                    tdEnd - tdPos - 4);
                values.push_back(std::string(trimWhitespace(cellContent)));

                tdPos = tdEnd + 5;  // length of "</TD>"
            }

            // Parse row into object
            if (!values.empty()) {
                auto obj = pImpl_->parseRow(values, fieldNames);
                if (!obj.identifier.empty()) {
                    results.push_back(obj);
                }
            }

            rowPos = rowEnd + 5;  // length of "</TR>"
        }

        return results;

    } catch (const std::exception& e) {
        return atom::type::unexpected(ParseError{
            std::string("VOTable parsing exception: ") + e.what(),
            std::nullopt,
            std::nullopt,
            "Unexpected error during parsing"});
    }
}

auto VotableParser::parseEphemeris(std::string_view content)
    -> atom::type::Expected<std::vector<EphemerisPoint>, ParseError> {
    std::vector<EphemerisPoint> results;

    try {
        std::string contentStr(content);

        // Find TABLEDATA section
        size_t tabledataStart = contentStr.find("<TABLEDATA");
        if (tabledataStart == std::string::npos) {
            return atom::type::unexpected(ParseError{
                "No TABLEDATA element found",
                std::nullopt,
                std::nullopt,
                "Ephemeris data section not found"});
        }

        // Extract FIELD definitions
        std::vector<std::string> fieldNames;
        size_t fieldPos = 0;
        while ((fieldPos = contentStr.find("<FIELD", fieldPos)) !=
               std::string::npos) {
            if (fieldPos > tabledataStart) {
                break;  // Only get fields before TABLEDATA
            }

            size_t fieldEnd = contentStr.find(">", fieldPos);
            if (fieldEnd == std::string::npos) {
                break;
            }

            std::string_view fieldTag(contentStr.data() + fieldPos,
                                      fieldEnd - fieldPos + 1);
            std::string name = extractAttribute(fieldTag, "name");
            if (!name.empty()) {
                fieldNames.push_back(name);
            }

            fieldPos = fieldEnd + 1;
        }

        // Parse rows
        size_t rowPos = tabledataStart;
        while ((rowPos = contentStr.find("<TR>", rowPos)) !=
               std::string::npos) {
            size_t rowEnd = contentStr.find("</TR>", rowPos);
            if (rowEnd == std::string::npos) {
                break;
            }

            std::string_view rowContent(contentStr.data() + rowPos + 4,
                                        rowEnd - rowPos - 4);

            // Extract TD values
            std::vector<std::string> values;
            size_t tdPos = 0;
            while ((tdPos = rowContent.find("<TD>", tdPos)) !=
                   std::string::npos) {
                size_t tdEnd = rowContent.find("</TD>", tdPos);
                if (tdEnd == std::string::npos) {
                    break;
                }

                std::string_view cellContent(
                    rowContent.data() + tdPos + 4,
                    tdEnd - tdPos - 4);
                values.push_back(std::string(trimWhitespace(cellContent)));

                tdPos = tdEnd + 5;
            }

            // Parse row into ephemeris point
            if (!values.empty()) {
                auto point =
                    pImpl_->parseEphemerisRow(values, fieldNames);
                results.push_back(point);
            }

            rowPos = rowEnd + 5;
        }

        return results;

    } catch (const std::exception& e) {
        return atom::type::unexpected(ParseError{
            std::string("Ephemeris parsing exception: ") + e.what(),
            std::nullopt,
            std::nullopt,
            "Unexpected error during ephemeris parsing"});
    }
}

// Global function to detect format
auto detectFormat(std::string_view content) -> ResponseFormat {
    if (content.find("<?xml") != std::string_view::npos &&
        content.find("<VOTABLE") != std::string_view::npos) {
        return ResponseFormat::VOTable;
    }

    if (content.find('{') != std::string_view::npos &&
        content.find('"') != std::string_view::npos) {
        return ResponseFormat::JSON;
    }

    if (content.find(',') != std::string_view::npos) {
        return ResponseFormat::CSV;
    }

    return ResponseFormat::Unknown;
}

}  // namespace lithium::target::online
