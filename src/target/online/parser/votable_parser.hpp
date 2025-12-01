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

#ifndef LITHIUM_TARGET_ONLINE_PARSER_VOTABLE_PARSER_HPP
#define LITHIUM_TARGET_ONLINE_PARSER_VOTABLE_PARSER_HPP

#include "response_parser.hpp"
#include <unordered_map>
#include <functional>
#include <optional>

namespace lithium::target::online {

/**
 * @brief Field mapping for VOTable columns to CelestialObjectModel fields
 */
struct VotableFieldMapping {
    std::string votableField;
    std::string modelField;
    std::optional<std::function<std::string(const std::string&)>> transform;
};

/**
 * @brief VOTable XML parser
 *
 * Parses IVOA VOTable format responses from SIMBAD, VizieR, etc.
 * Supports VOTable 1.3 specification.
 */
class VotableParser : public IResponseParser {
public:
    VotableParser();
    ~VotableParser() override;

    // Non-copyable, movable
    VotableParser(const VotableParser&) = delete;
    VotableParser& operator=(const VotableParser&) = delete;
    VotableParser(VotableParser&&) noexcept;
    VotableParser& operator=(VotableParser&&) noexcept;

    [[nodiscard]] auto parse(std::string_view content)
        -> atom::type::Expected<std::vector<CelestialObjectModel>,
                               ParseError> override;

    [[nodiscard]] auto parseEphemeris(std::string_view content)
        -> atom::type::Expected<std::vector<EphemerisPoint>,
                               ParseError> override;

    [[nodiscard]] auto format() const noexcept -> ResponseFormat override {
        return ResponseFormat::VOTable;
    }

    /**
     * @brief Set custom field mappings
     */
    void setFieldMappings(const std::vector<VotableFieldMapping>& mappings);

    /**
     * @brief Get default SIMBAD field mappings
     */
    [[nodiscard]] static auto simbadMappings()
        -> std::vector<VotableFieldMapping>;

    /**
     * @brief Get default VizieR NGC mappings
     */
    [[nodiscard]] static auto vizierNgcMappings()
        -> std::vector<VotableFieldMapping>;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PARSER_VOTABLE_PARSER_HPP
