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

#ifndef LITHIUM_TARGET_ONLINE_PARSER_RESPONSE_PARSER_HPP
#define LITHIUM_TARGET_ONLINE_PARSER_RESPONSE_PARSER_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include "atom/type/expected.hpp"
#include "../provider/provider_interface.hpp"
#include "../../celestial_model.hpp"

namespace lithium::target::online {

/**
 * @brief Response format enumeration
 */
enum class ResponseFormat {
    VOTable,    ///< IVOA VOTable XML format
    JSON,       ///< JSON format
    CSV,        ///< Comma-separated values
    TSV,        ///< Tab-separated values
    Unknown
};

/**
 * @brief Parse error information
 */
struct ParseError {
    std::string message;
    std::optional<size_t> line;
    std::optional<size_t> column;
    std::string context;
};

/**
 * @brief Base interface for response parsers
 */
class IResponseParser {
public:
    virtual ~IResponseParser() = default;

    [[nodiscard]] virtual auto parse(std::string_view content)
        -> atom::type::Expected<std::vector<CelestialObjectModel>,
                               ParseError> = 0;

    [[nodiscard]] virtual auto parseEphemeris(std::string_view content)
        -> atom::type::Expected<std::vector<EphemerisPoint>, ParseError> = 0;

    [[nodiscard]] virtual auto format() const noexcept -> ResponseFormat = 0;

protected:
    IResponseParser() = default;
};

/**
 * @brief Detect response format from content
 */
[[nodiscard]] auto detectFormat(std::string_view content) -> ResponseFormat;

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_PARSER_RESPONSE_PARSER_HPP
