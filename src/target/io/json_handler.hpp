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

#ifndef LITHIUM_TARGET_IO_JSON_HANDLER_HPP
#define LITHIUM_TARGET_IO_JSON_HANDLER_HPP

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

#include "../celestial_model.hpp"
#include "csv_handler.hpp"

using json = nlohmann::json;

namespace lithium::target::io {

/**
 * @brief JSON handler for importing and exporting celestial objects
 *
 * Provides functionality to read/write JSON files with single objects,
 * arrays of objects, and stream processing for large files with UTF-8 support.
 */
class JsonHandler {
public:
    /**
     * @brief Default constructor
     */
    JsonHandler();

    /**
     * @brief Destructor
     */
    ~JsonHandler();

    /**
     * @brief Read JSON data from file
     *
     * @param filename Path to the JSON file
     * @return Expected JSON object, or error string
     */
    [[nodiscard]] auto read(const std::string& filename)
        -> std::expected<json, std::string>;

    /**
     * @brief Write JSON data to file
     *
     * @param filename Output file path
     * @param data JSON object to write
     * @param indent Number of spaces for indentation (0 for compact output)
     * @return Expected void, or error string
     */
    [[nodiscard]] auto write(const std::string& filename, const json& data,
                            int indent = 2) -> std::expected<void, std::string>;

    /**
     * @brief Import single celestial object from JSON file
     *
     * Expects the file to contain a single JSON object or the first object
     * in an array.
     *
     * @param filename Path to the JSON file
     * @return Expected CelestialObjectModel, or error string
     */
    [[nodiscard]] auto importCelestialObject(const std::string& filename)
        -> std::expected<CelestialObjectModel, std::string>;

    /**
     * @brief Import multiple celestial objects from JSON file
     *
     * Supports both JSON array format and JSONL (JSON Lines) format.
     * JSONL format: one JSON object per line.
     *
     * @param filename Path to the JSON or JSONL file
     * @return Expected pair of imported objects and import statistics, or error
     * string
     */
    [[nodiscard]] auto importCelestialObjects(const std::string& filename)
        -> std::expected<
            std::pair<std::vector<CelestialObjectModel>, ImportResult>,
            std::string>;

    /**
     * @brief Export single celestial object to JSON file
     *
     * @param filename Output file path
     * @param object Celestial object to export
     * @param indent Number of spaces for indentation
     * @return Expected void, or error string
     */
    [[nodiscard]] auto exportCelestialObject(
        const std::string& filename, const CelestialObjectModel& object,
        int indent = 2) -> std::expected<void, std::string>;

    /**
     * @brief Export multiple celestial objects to JSON file
     *
     * @param filename Output file path
     * @param objects Vector of celestial objects to export
     * @param asArray If true, export as JSON array; if false, use JSONL format
     * @param indent Number of spaces for indentation
     * @return Expected count of exported objects, or error string
     */
    [[nodiscard]] auto exportCelestialObjects(
        const std::string& filename,
        const std::vector<CelestialObjectModel>& objects, bool asArray = true,
        int indent = 2) -> std::expected<int, std::string>;

    /**
     * @brief Stream process large JSON file line by line
     *
     * Processes JSONL format (one JSON object per line) with a callback
     * function for each object.
     *
     * @param filename Path to the JSONL file
     * @param callback Function to call for each object
     * @return Expected count of processed objects, or error string
     */
    [[nodiscard]] auto streamProcess(
        const std::string& filename,
        std::function<std::expected<void, std::string>(const json&)> callback)
        -> std::expected<int, std::string>;

    /**
     * @brief Validate JSON against expected schema
     *
     * Checks that all required fields for CelestialObjectModel are present
     * and of correct types.
     *
     * @param data JSON object to validate
     * @return Expected void, or error string with validation details
     */
    [[nodiscard]] static auto validateCelestialObjectJson(const json& data)
        -> std::expected<void, std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Convert JSON object to CelestialObjectModel
     *
     * @param jsonObj JSON object to convert
     * @param result Statistics to update
     * @return Expected CelestialObjectModel, or error message
     */
    [[nodiscard]] static auto jsonToCelestialObject(
        const json& jsonObj, ImportResult& result)
        -> std::expected<CelestialObjectModel, std::string>;

    /**
     * @brief Convert CelestialObjectModel to JSON object
     *
     * @param object Celestial object to convert
     * @return JSON object representation
     */
    [[nodiscard]] static auto celestialObjectToJson(
        const CelestialObjectModel& object) -> json;

    /**
     * @brief Detect if file is JSONL format
     *
     * @param filename Path to the file
     * @return True if file appears to be JSONL format
     */
    [[nodiscard]] static auto isJsonLFormat(const std::string& filename)
        -> std::expected<bool, std::string>;
};

}  // namespace lithium::target::io

#endif  // LITHIUM_TARGET_IO_JSON_HANDLER_HPP
