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

#ifndef LITHIUM_TARGET_ONLINE_PARSER_JSON_RESPONSE_PARSER_HPP
#define LITHIUM_TARGET_ONLINE_PARSER_JSON_RESPONSE_PARSER_HPP

#include "response_parser.hpp"
#include "atom/type/json.hpp"
#include <functional>
#include <optional>
#include <string>

namespace lithium::target::online {

/**
 * @brief JSON parser adapter for different response formats
 *
 * Parses JSON responses from NED, JPL Horizons, and other
 * modern online databases. Supports both object and array responses
 * with flexible field mapping.
 */
class JsonResponseParser : public IResponseParser {
public:
    /**
     * @brief Parser function type for custom JSON field extraction
     */
    using ParserFunc = std::function<CelestialObjectModel(
        const nlohmann::json& json)>;

    /**
     * @brief Ephemeris parser function type
     */
    using EphemerisParserFunc =
        std::function<EphemerisPoint(const nlohmann::json& json)>;

    JsonResponseParser();
    ~JsonResponseParser() override;

    // Non-copyable, movable
    JsonResponseParser(const JsonResponseParser&) = delete;
    JsonResponseParser& operator=(const JsonResponseParser&) = delete;
    JsonResponseParser(JsonResponseParser&&) noexcept;
    JsonResponseParser& operator=(JsonResponseParser&&) noexcept;

    [[nodiscard]] auto parse(std::string_view content)
        -> atom::type::Expected<std::vector<CelestialObjectModel>,
                               ParseError> override;

    [[nodiscard]] auto parseEphemeris(std::string_view content)
        -> atom::type::Expected<std::vector<EphemerisPoint>,
                               ParseError> override;

    [[nodiscard]] auto format() const noexcept -> ResponseFormat override {
        return ResponseFormat::JSON;
    }

    /**
     * @brief Set custom parser for extracting objects from JSON
     *
     * @param parser Function that converts JSON element to
     * CelestialObjectModel
     */
    void setObjectParser(const ParserFunc& parser);

    /**
     * @brief Set custom parser for extracting ephemeris from JSON
     */
    void setEphemerisParser(const EphemerisParserFunc& parser);

    /**
     * @brief Set the JSON path to array of objects
     *
     * For responses where objects are nested under a key
     * (e.g., "results" in NED responses)
     *
     * @param path Dot-separated path (e.g., "results.data")
     */
    void setObjectsPath(const std::string& path);

    /**
     * @brief Get default NED parser
     *
     * NED returns format like:
     * {
     *   "Preferred": {"Coordinates": {...}, "name": "...", ...},
     *   "Name": "...",
     *   ...
     * }
     */
    [[nodiscard]] static auto nedParser() -> ParserFunc;

    /**
     * @brief Get default JPL Horizons parser
     *
     * JPL returns format like:
     * {
     *   "signature": {...},
     *   "result": {...},
     *   "state": [...]
     * }
     */
    [[nodiscard]] static auto jplHorizonsParser() -> EphemerisParserFunc;

    /**
     * @brief Get default GAIA DR3 parser
     *
     * GAIA returns format like:
     * {
     *   "data": [
     *     {"ra": ..., "dec": ..., "source_id": ..., ...},
     *     ...
     *   ]
     * }
     */
    [[nodiscard]] static auto gaiaParser() -> ParserFunc;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PARSER_JSON_RESPONSE_PARSER_HPP
