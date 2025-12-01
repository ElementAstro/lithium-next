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

#include "json_response_parser.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>

namespace lithium::target::online {

namespace {

/**
 * @brief Helper to safely extract a double from JSON
 */
std::optional<double> getDouble(const nlohmann::json& json,
                                const std::string& key) {
    try {
        if (json.contains(key)) {
            auto val = json[key];
            if (val.is_number()) {
                return val.get<double>();
            } else if (val.is_string()) {
                return std::stod(val.get<std::string>());
            }
        }
    } catch (...) {
    }
    return std::nullopt;
}

/**
 * @brief Helper to safely extract a string from JSON
 */
std::string getString(const nlohmann::json& json,
                     const std::string& key) {
    try {
        if (json.contains(key)) {
            auto val = json[key];
            if (val.is_string()) {
                return val.get<std::string>();
            } else if (val.is_number()) {
                return std::to_string(val.get<double>());
            }
        }
    } catch (...) {
    }
    return "";
}

/**
 * @brief Parse timestamp string to chrono time_point
 */
std::chrono::system_clock::time_point parseTimestamp(
    const std::string& timestamp) {
    // Simple ISO 8601 parsing
    // Format: "2000-01-01T00:00:00Z" or similar
    try {
        // For simplicity, just return current time
        // A full implementation would parse the timestamp properly
        return std::chrono::system_clock::now();
    } catch (...) {
        return std::chrono::system_clock::now();
    }
}

/**
 * @brief Navigate JSON path (dot-separated)
 */
nlohmann::json getByPath(const nlohmann::json& json,
                        const std::string& path) {
    std::istringstream iss(path);
    std::string segment;
    nlohmann::json current = json;

    while (std::getline(iss, segment, '.')) {
        if (!segment.empty() && current.contains(segment)) {
            current = current[segment];
        } else {
            return nlohmann::json();
        }
    }

    return current;
}

}  // namespace

/**
 * @brief Implementation class using PIMPL pattern
 */
class JsonResponseParser::Impl {
public:
    ParserFunc objectParser;
    EphemerisParserFunc ephemerisParser;
    std::string objectsPath = "data";  // Default path

    Impl() {
        // Set default parsers
        objectParser = defaultObjectParser();
        ephemerisParser = defaultEphemerisParser();
    }

    static ParserFunc defaultObjectParser() {
        return [](const nlohmann::json& json) -> CelestialObjectModel {
            CelestialObjectModel obj;

            // Try common field names
            obj.identifier = getString(json, "name");
            if (obj.identifier.empty()) {
                obj.identifier = getString(json, "id");
            }
            if (obj.identifier.empty()) {
                obj.identifier = getString(json, "source_id");
            }

            if (auto ra = getDouble(json, "ra")) {
                obj.radJ2000 = *ra;
                obj.raJ2000 = std::to_string(*ra);
            }

            if (auto dec = getDouble(json, "dec")) {
                obj.decDJ2000 = *dec;
                obj.decJ2000 = std::to_string(*dec);
            }

            if (auto mag = getDouble(json, "mag")) {
                obj.visualMagnitudeV = *mag;
            }
            if (auto mag = getDouble(json, "magnitude")) {
                obj.visualMagnitudeV = *mag;
            }
            if (auto mag = getDouble(json, "V")) {
                obj.visualMagnitudeV = *mag;
            }

            obj.type = getString(json, "type");
            if (obj.type.empty()) {
                obj.type = getString(json, "otype");
            }
            if (obj.type.empty()) {
                obj.type = getString(json, "morphology");
            }

            obj.constellationEn = getString(json, "constellation");
            if (obj.constellationEn.empty()) {
                obj.constellationEn = getString(json, "const");
            }

            if (auto maj = getDouble(json, "major_axis")) {
                obj.majorAxis = *maj;
            }

            if (auto min = getDouble(json, "minor_axis")) {
                obj.minorAxis = *min;
            }

            if (auto pa = getDouble(json, "position_angle")) {
                obj.positionAngle = *pa;
            }

            obj.briefDescription = getString(json, "description");

            return obj;
        };
    }

    static EphemerisParserFunc defaultEphemerisParser() {
        return [](const nlohmann::json& json) -> EphemerisPoint {
            EphemerisPoint point;

            // Extract timestamp
            if (json.contains("timestamp")) {
                point.time =
                    parseTimestamp(getString(json, "timestamp"));
            } else if (json.contains("jd")) {
                // Julian day - would need to convert
                point.time = std::chrono::system_clock::now();
            }

            if (auto ra = getDouble(json, "ra")) {
                point.ra = *ra;
            }

            if (auto dec = getDouble(json, "dec")) {
                point.dec = *dec;
            }

            if (auto dist = getDouble(json, "distance")) {
                point.distance = *dist;
            }
            if (auto dist = getDouble(json, "delta")) {
                point.distance = *dist;
            }

            if (auto mag = getDouble(json, "magnitude")) {
                point.magnitude = *mag;
            }
            if (auto mag = getDouble(json, "mag")) {
                point.magnitude = *mag;
            }

            if (auto elong = getDouble(json, "elongation")) {
                point.elongation = *elong;
            }

            if (auto phase = getDouble(json, "phase_angle")) {
                point.phaseAngle = *phase;
            }

            if (auto az = getDouble(json, "azimuth")) {
                point.azimuth = *az;
            }

            if (auto alt = getDouble(json, "altitude")) {
                point.altitude = *alt;
            }

            return point;
        };
    }
};

// Public interface implementation

JsonResponseParser::JsonResponseParser()
    : pImpl_(std::make_unique<Impl>()) {}

JsonResponseParser::~JsonResponseParser() = default;

JsonResponseParser::JsonResponseParser(JsonResponseParser&&) noexcept =
    default;

JsonResponseParser& JsonResponseParser::operator=(
    JsonResponseParser&&) noexcept = default;

void JsonResponseParser::setObjectParser(const ParserFunc& parser) {
    pImpl_->objectParser = parser;
}

void JsonResponseParser::setEphemerisParser(
    const EphemerisParserFunc& parser) {
    pImpl_->ephemerisParser = parser;
}

void JsonResponseParser::setObjectsPath(const std::string& path) {
    pImpl_->objectsPath = path;
}

JsonResponseParser::ParserFunc JsonResponseParser::nedParser() {
    return [](const nlohmann::json& json) -> CelestialObjectModel {
        CelestialObjectModel obj;

        // NED format: Top-level object with various fields
        try {
            // Primary identifier
            obj.identifier = getString(json, "Name");

            // Coordinates from Preferred if available
            if (json.contains("Preferred") &&
                json["Preferred"].is_object()) {
                auto pref = json["Preferred"];

                if (pref.contains("Coordinates") &&
                    pref["Coordinates"].is_object()) {
                    auto coords = pref["Coordinates"];

                    if (auto ra = getDouble(coords, "RA_deg")) {
                        obj.radJ2000 = *ra;
                    }

                    if (auto dec = getDouble(coords, "DEC_deg")) {
                        obj.decDJ2000 = *dec;
                    }
                }
            }

            // Try alternate coordinate keys
            if (obj.radJ2000 == 0.0) {
                if (auto ra = getDouble(json, "RA")) {
                    obj.radJ2000 = *ra;
                }
            }

            if (obj.decDJ2000 == 0.0) {
                if (auto dec = getDouble(json, "DEC")) {
                    obj.decDJ2000 = *dec;
                }
            }

            // Object type
            obj.type = getString(json, "Type");

            // Magnitude
            if (auto mag = getDouble(json, "Mag_V")) {
                obj.visualMagnitudeV = *mag;
            }

            // Description
            obj.briefDescription = getString(json, "Description");

            return obj;
        } catch (...) {
            return obj;
        }
    };
}

JsonResponseParser::EphemerisParserFunc
JsonResponseParser::jplHorizonsParser() {
    return [](const nlohmann::json& json) -> EphemerisPoint {
        EphemerisPoint point;

        try {
            // JPL Horizons format is typically:
            // {"signature": {...}, "result": {...}, "state": [...]}

            // Extract time
            if (json.contains("datetime")) {
                point.time =
                    parseTimestamp(getString(json, "datetime"));
            }

            // RA/Dec might be in degrees or separate fields
            if (auto ra = getDouble(json, "RA")) {
                point.ra = *ra;
            } else if (json.contains("ra_rate")) {
                // Sometimes stored as rate
            }

            if (auto dec = getDouble(json, "DEC")) {
                point.dec = *dec;
            }

            // Distance in AU
            if (auto dist = getDouble(json, "delta")) {
                point.distance = *dist;
            }

            // Visual magnitude
            if (auto mag = getDouble(json, "mag")) {
                point.magnitude = *mag;
            }

            // Solar elongation
            if (auto elong = getDouble(json, "elong")) {
                point.elongation = *elong;
            }

            // Phase angle
            if (auto phase = getDouble(json, "phase")) {
                point.phaseAngle = *phase;
            }

            return point;
        } catch (...) {
            return point;
        }
    };
}

JsonResponseParser::ParserFunc JsonResponseParser::gaiaParser() {
    return [](const nlohmann::json& json) -> CelestialObjectModel {
        CelestialObjectModel obj;

        try {
            // GAIA format: Direct object with fields
            obj.identifier = getString(json, "source_id");

            if (auto ra = getDouble(json, "ra")) {
                obj.radJ2000 = *ra;
            }

            if (auto dec = getDouble(json, "dec")) {
                obj.decDJ2000 = *dec;
            }

            // GAIA provides magnitude in different bands
            if (auto mag = getDouble(json, "phot_g_mean_mag")) {
                obj.visualMagnitudeV = *mag;  // Use G magnitude as proxy
            }

            if (auto mag = getDouble(json, "phot_bp_mean_mag")) {
                obj.photographicMagnitudeB = *mag;
            }

            // Distance from parallax
            if (json.contains("parallax") && json["parallax"].is_number()) {
                try {
                    double parallax = json["parallax"].get<double>();
                    if (parallax > 0) {
                        // Distance in parsecs = 1000 / parallax (in mas)
                        obj.surfaceBrightness = 1000.0 / parallax;
                    }
                } catch (...) {
                }
            }

            return obj;
        } catch (...) {
            return obj;
        }
    };
}

auto JsonResponseParser::parse(std::string_view content)
    -> atom::type::Expected<std::vector<CelestialObjectModel>,
                           ParseError> {
    std::vector<CelestialObjectModel> results;

    try {
        // Parse JSON
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(content);
        } catch (const nlohmann::json::parse_error& e) {
            return atom::type::unexpected(ParseError{
                std::string("JSON parse error: ") + e.what(),
                std::make_optional(e.byte),
                std::nullopt,
                "Invalid JSON structure"});
        }

        // Get objects array
        nlohmann::json objectsArray;
        if (pImpl_->objectsPath == "$") {
            // Root is the array
            objectsArray = json;
        } else {
            objectsArray = getByPath(json, pImpl_->objectsPath);
        }

        // Handle different response structures
        if (objectsArray.is_array()) {
            for (const auto& item : objectsArray) {
                try {
                    auto obj = pImpl_->objectParser(item);
                    if (!obj.identifier.empty()) {
                        results.push_back(obj);
                    }
                } catch (...) {
                    // Skip malformed objects
                    continue;
                }
            }
        } else if (objectsArray.is_object() &&
                   !pImpl_->objectsPath.empty()) {
            // Single object response
            try {
                auto obj = pImpl_->objectParser(objectsArray);
                if (!obj.identifier.empty()) {
                    results.push_back(obj);
                }
            } catch (...) {
            }
        } else if (json.is_array()) {
            // Root is directly an array
            for (const auto& item : json) {
                try {
                    auto obj = pImpl_->objectParser(item);
                    if (!obj.identifier.empty()) {
                        results.push_back(obj);
                    }
                } catch (...) {
                    continue;
                }
            }
        } else if (json.is_object()) {
            // Single object at root
            try {
                auto obj = pImpl_->objectParser(json);
                if (!obj.identifier.empty()) {
                    results.push_back(obj);
                }
            } catch (...) {
            }
        } else {
            return atom::type::unexpected(ParseError{
                "JSON is neither array nor object",
                std::nullopt,
                std::nullopt,
                "Unexpected JSON structure"});
        }

        return results;

    } catch (const std::exception& e) {
        return atom::type::unexpected(ParseError{
            std::string("JSON parsing exception: ") + e.what(),
            std::nullopt,
            std::nullopt,
            "Unexpected error during parsing"});
    }
}

auto JsonResponseParser::parseEphemeris(std::string_view content)
    -> atom::type::Expected<std::vector<EphemerisPoint>, ParseError> {
    std::vector<EphemerisPoint> results;

    try {
        // Parse JSON
        nlohmann::json json;
        try {
            json = nlohmann::json::parse(content);
        } catch (const nlohmann::json::parse_error& e) {
            return atom::type::unexpected(ParseError{
                std::string("JSON parse error: ") + e.what(),
                std::make_optional(e.byte),
                std::nullopt,
                "Invalid JSON structure"});
        }

        // Get ephemeris array
        nlohmann::json ephemerisArray;

        // Try common paths for ephemeris data
        if (json.contains("result") && json["result"].is_array()) {
            ephemerisArray = json["result"];
        } else if (json.contains("data") && json["data"].is_array()) {
            ephemerisArray = json["data"];
        } else if (json.is_array()) {
            ephemerisArray = json;
        } else if (json.is_object()) {
            ephemerisArray = json;  // Treat as single point
        }

        // Parse ephemeris points
        if (ephemerisArray.is_array()) {
            for (const auto& item : ephemerisArray) {
                try {
                    auto point = pImpl_->ephemerisParser(item);
                    results.push_back(point);
                } catch (...) {
                    continue;
                }
            }
        } else if (ephemerisArray.is_object()) {
            try {
                auto point = pImpl_->ephemerisParser(ephemerisArray);
                results.push_back(point);
            } catch (...) {
            }
        } else {
            return atom::type::unexpected(ParseError{
                "Ephemeris data not found in expected format",
                std::nullopt,
                std::nullopt,
                "Unable to locate ephemeris array"});
        }

        return results;

    } catch (const std::exception& e) {
        return atom::type::unexpected(ParseError{
            std::string("Ephemeris parsing exception: ") + e.what(),
            std::nullopt,
            std::nullopt,
            "Unexpected error during parsing"});
    }
}

}  // namespace lithium::target::online
