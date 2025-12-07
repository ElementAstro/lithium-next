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

#ifndef LITHIUM_TARGET_IO_CSV_HANDLER_HPP
#define LITHIUM_TARGET_IO_CSV_HANDLER_HPP

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../model/celestial_model.hpp"

namespace lithium::target::io {

/**
 * @brief CSV dialect configuration for handling different CSV formats
 */
struct CsvDialect {
    char delimiter = ',';               ///< Field separator character
    char quotechar = '"';               ///< Quote character
    char escapechar = '\\';             ///< Escape character
    bool doublequote = true;            ///< Whether to double quote characters
    bool skipinitialspace = false;      ///< Skip spaces after delimiter
    std::string lineterminator = "\n";  ///< Line ending string
    bool strict = false;                ///< Strict mode validation

    /**
     * @brief Parameterized constructor
     *
     * @param delim Field delimiter character
     * @param quote Quote character
     * @param escape Escape character
     * @param dquote Whether to double quotes in fields
     * @param skipspace Whether to skip spaces after delimiter
     * @param lineterm Line terminator string
     * @param strict_mode Strict mode validation
     */
    CsvDialect(char delim, char quote, char escape, bool dquote, bool skipspace,
               std::string lineterm, bool strict_mode)
        : delimiter(delim),
          quotechar(quote),
          escapechar(escape),
          doublequote(dquote),
          skipinitialspace(skipspace),
          lineterminator(lineterm),
          strict(strict_mode) {}

    CsvDialect() = default;
};

/**
 * @brief Result statistics for import operations
 */
struct ImportResult {
    int totalRecords = 0;             ///< Total records encountered
    int successCount = 0;             ///< Successfully imported
    int errorCount = 0;               ///< Records with errors
    int duplicateCount = 0;           ///< Duplicate records skipped
    std::vector<std::string> errors;  ///< Detailed error messages
};

/**
 * @brief CSV handler for reading and writing CSV files with celestial objects
 *
 * Provides functionality to read/write CSV files with configurable dialects,
 * import/export celestial objects with error tracking, and stream processing
 * of large files.
 */
class CsvHandler {
public:
    /**
     * @brief Default constructor
     */
    CsvHandler();

    /**
     * @brief Destructor
     */
    ~CsvHandler();

    /**
     * @brief Read CSV file and return raw data as records
     *
     * @param filename Path to the CSV file
     * @param dialect CSV dialect configuration
     * @return Expected vector of field dictionaries, or error string
     */
    [[nodiscard]] auto read(const std::string& filename,
                            const CsvDialect& dialect = {})
        -> std::expected<
            std::vector<std::unordered_map<std::string, std::string>>,
            std::string>;

    /**
     * @brief Write raw data to CSV file
     *
     * @param filename Output file path
     * @param data Vector of field dictionaries
     * @param fields Column names
     * @param dialect CSV dialect configuration
     * @return Expected count of written rows, or error string
     */
    [[nodiscard]] auto write(
        const std::string& filename,
        const std::vector<std::unordered_map<std::string, std::string>>& data,
        const std::vector<std::string>& fields, const CsvDialect& dialect = {})
        -> std::expected<int, std::string>;

    /**
     * @brief Import celestial objects from CSV file
     *
     * Converts CSV records to CelestialObjectModel instances with validation
     * and error handling.
     *
     * @param filename Path to the CSV file
     * @param dialect CSV dialect configuration
     * @return Expected pair of imported objects and import statistics, or error
     * string
     */
    [[nodiscard]] auto importCelestialObjects(const std::string& filename,
                                              const CsvDialect& dialect = {})
        -> std::expected<
            std::pair<std::vector<CelestialObjectModel>, ImportResult>,
            std::string>;

    /**
     * @brief Export celestial objects to CSV file
     *
     * Exports a vector of CelestialObjectModel instances to CSV format.
     *
     * @param filename Output file path
     * @param objects Vector of celestial objects to export
     * @param dialect CSV dialect configuration
     * @return Expected count of exported objects, or error string
     */
    [[nodiscard]] auto exportCelestialObjects(
        const std::string& filename,
        const std::vector<CelestialObjectModel>& objects,
        const CsvDialect& dialect = {}) -> std::expected<int, std::string>;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    /**
     * @brief Convert field dictionary to CelestialObjectModel
     *
     * @param row Field dictionary from CSV
     * @param result Statistics to update
     * @return Expected CelestialObjectModel, or error message
     */
    [[nodiscard]] static auto rowToCelestialObject(
        const std::unordered_map<std::string, std::string>& row,
        ImportResult& result)
        -> std::expected<CelestialObjectModel, std::string>;

    /**
     * @brief Convert CelestialObjectModel to field dictionary
     *
     * @param object Celestial object to convert
     * @return Field dictionary for CSV output
     */
    [[nodiscard]] static auto celestialObjectToRow(
        const CelestialObjectModel& object)
        -> std::unordered_map<std::string, std::string>;
};

}  // namespace lithium::target::io

#endif  // LITHIUM_TARGET_IO_CSV_HANDLER_HPP
