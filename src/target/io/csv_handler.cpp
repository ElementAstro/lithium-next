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

#include "csv_handler.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lithium::target::io {

/**
 * @brief Internal implementation of CSV handler
 */
class CsvHandler::Impl {
public:
    /**
     * @brief Parse CSV line according to dialect
     *
     * @param line Input line to parse
     * @param dialect CSV dialect configuration
     * @return Vector of field values
     */
    static auto parseLine(const std::string& line, const CsvDialect& dialect)
        -> std::vector<std::string> {
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;
        bool escapeNext = false;

        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            if (escapeNext) {
                field += c;
                escapeNext = false;
                continue;
            }

            if (c == dialect.escapechar) {
                escapeNext = true;
                continue;
            }

            if (c == dialect.quotechar) {
                if (inQuotes && i + 1 < line.length() &&
                    line[i + 1] == dialect.quotechar && dialect.doublequote) {
                    field += dialect.quotechar;
                    ++i;  // Skip next quote
                } else {
                    inQuotes = !inQuotes;
                }
                continue;
            }

            if (c == dialect.delimiter && !inQuotes) {
                if (!field.empty() || !inQuotes) {
                    // Trim leading space if requested
                    if (dialect.skipinitialspace && !field.empty() &&
                        std::isspace(field.front())) {
                        field.erase(0, 1);
                    }
                    fields.push_back(field);
                    field.clear();
                }
                continue;
            }

            field += c;
        }

        // Add last field
        if (!field.empty() || !inQuotes) {
            if (dialect.skipinitialspace && !field.empty() &&
                std::isspace(field.front())) {
                field.erase(0, 1);
            }
            fields.push_back(field);
        }

        return fields;
    }

    /**
     * @brief Escape a field for CSV output
     *
     * @param field Field value to escape
     * @param dialect CSV dialect configuration
     * @return Escaped field value
     */
    static auto escapeField(const std::string& field, const CsvDialect& dialect)
        -> std::string {
        bool needsQuotes = false;

        // Check if field needs quoting
        if (field.find(dialect.delimiter) != std::string::npos ||
            field.find(dialect.quotechar) != std::string::npos ||
            field.find(dialect.escapechar) != std::string::npos ||
            field.find('\n') != std::string::npos ||
            field.find('\r') != std::string::npos) {
            needsQuotes = true;
        }

        if (!needsQuotes) {
            return field;
        }

        std::string result;
        result += dialect.quotechar;

        for (char c : field) {
            if (c == dialect.quotechar) {
                if (dialect.doublequote) {
                    result += dialect.quotechar;
                    result += dialect.quotechar;
                } else {
                    result += dialect.escapechar;
                    result += dialect.quotechar;
                }
            } else {
                result += c;
            }
        }

        result += dialect.quotechar;
        return result;
    }

    /**
     * @brief Convert string to double with error handling
     *
     * @param str String to convert
     * @return Double value or 0.0 on error
     */
    static auto stringToDouble(const std::string& str) -> double {
        if (str.empty()) {
            return 0.0;
        }
        try {
            size_t idx = 0;
            double val = std::stod(str, &idx);
            // Check if entire string was consumed
            if (idx == str.length()) {
                return val;
            }
        } catch (...) {
            // Fall through to return 0.0
        }
        return 0.0;
    }

    /**
     * @brief Convert string to integer with error handling
     *
     * @param str String to convert
     * @return Integer value or 0 on error
     */
    static auto stringToInt(const std::string& str) -> int {
        if (str.empty()) {
            return 0;
        }
        try {
            size_t idx = 0;
            int val = std::stoi(str, &idx);
            if (idx == str.length()) {
                return val;
            }
        } catch (...) {
            // Fall through to return 0
        }
        return 0;
    }

    /**
     * @brief Convert string to int64_t with error handling
     *
     * @param str String to convert
     * @return int64_t value or 0 on error
     */
    static auto stringToInt64(const std::string& str) -> int64_t {
        if (str.empty()) {
            return 0;
        }
        try {
            size_t idx = 0;
            int64_t val = std::stoll(str, &idx);
            if (idx == str.length()) {
                return val;
            }
        } catch (...) {
            // Fall through to return 0
        }
        return 0;
    }

    /**
     * @brief Convert double to string
     *
     * @param val Double value
     * @return String representation
     */
    static auto doubleToString(double val) -> std::string {
        if (val == 0.0) {
            return "";
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6) << val;
        std::string result = oss.str();
        // Remove trailing zeros
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        if (result.back() == '.') {
            result.pop_back();
        }
        return result;
    }

    /**
     * @brief Convert integer to string
     *
     * @param val Integer value
     * @return String representation
     */
    static auto intToString(int val) -> std::string {
        if (val == 0) {
            return "";
        }
        return std::to_string(val);
    }
};

// ============================================================================
// CsvHandler Implementation
// ============================================================================

CsvHandler::CsvHandler() : impl_(std::make_unique<Impl>()) {}

CsvHandler::~CsvHandler() = default;

auto CsvHandler::read(const std::string& filename, const CsvDialect& dialect)
    -> std::expected<std::vector<std::unordered_map<std::string, std::string>>,
                    std::string> {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(std::string("Failed to open file: ") + filename);
    }

    std::vector<std::unordered_map<std::string, std::string>> records;
    std::vector<std::string> fieldnames;
    std::string line;
    size_t lineNum = 0;

    // Read header line
    if (!std::getline(file, line)) {
        return std::unexpected("Empty CSV file");
    }
    ++lineNum;

    // Parse header
    fieldnames = Impl::parseLine(line, dialect);
    if (fieldnames.empty()) {
        return std::unexpected("No field names in CSV header");
    }

    // Read data lines
    while (std::getline(file, line)) {
        ++lineNum;

        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        auto fields = Impl::parseLine(line, dialect);

        // Handle field count mismatch
        if (fields.size() != fieldnames.size()) {
            if (dialect.strict) {
                return std::unexpected(
                    std::string("Field count mismatch at line ") +
                    std::to_string(lineNum) + ": expected " +
                    std::to_string(fieldnames.size()) + ", got " +
                    std::to_string(fields.size()));
            }
            // Pad with empty strings or truncate
            if (fields.size() < fieldnames.size()) {
                fields.resize(fieldnames.size());
            } else {
                fields.resize(fieldnames.size());
            }
        }

        // Create record dictionary
        std::unordered_map<std::string, std::string> record;
        for (size_t i = 0; i < fieldnames.size(); ++i) {
            record[fieldnames[i]] = fields[i];
        }
        records.push_back(record);
    }

    return records;
}

auto CsvHandler::write(
    const std::string& filename,
    const std::vector<std::unordered_map<std::string, std::string>>& data,
    const std::vector<std::string>& fields,
    const CsvDialect& dialect) -> std::expected<int, std::string> {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(std::string("Failed to create file: ") +
                               filename);
    }

    // Write header
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            file << dialect.delimiter;
        }
        file << Impl::escapeField(fields[i], dialect);
    }
    file << dialect.lineterminator;

    // Write data rows
    int rowCount = 0;
    for (const auto& record : data) {
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) {
                file << dialect.delimiter;
            }

            auto it = record.find(fields[i]);
            std::string value = (it != record.end()) ? it->second : "";
            file << Impl::escapeField(value, dialect);
        }
        file << dialect.lineterminator;
        ++rowCount;
    }

    if (!file.good()) {
        return std::unexpected("Error writing to CSV file");
    }

    file.close();
    return rowCount;
}

auto CsvHandler::rowToCelestialObject(
    const std::unordered_map<std::string, std::string>& row,
    ImportResult& result) -> std::expected<CelestialObjectModel, std::string> {
    CelestialObjectModel obj;

    // Extract and validate required fields
    auto getField = [&row](const std::string& key) -> std::string {
        auto it = row.find(key);
        return (it != row.end()) ? it->second : "";
    };

    // Primary fields
    obj.identifier = getField("identifier");
    if (obj.identifier.empty()) {
        return std::unexpected("Missing required field: identifier");
    }

    obj.mIdentifier = getField("mIdentifier");
    obj.extensionName = getField("extensionName");
    obj.component = getField("component");
    obj.className = getField("className");
    obj.chineseName = getField("chineseName");
    obj.type = getField("type");
    obj.duplicateType = getField("duplicateType");
    obj.morphology = getField("morphology");
    obj.constellationZh = getField("constellationZh");
    obj.constellationEn = getField("constellationEn");

    // Coordinate fields
    obj.raJ2000 = getField("raJ2000");
    obj.radJ2000 = Impl::stringToDouble(getField("radJ2000"));
    obj.decJ2000 = getField("decJ2000");
    obj.decDJ2000 = Impl::stringToDouble(getField("decDJ2000"));

    // Magnitude and photometric fields
    obj.visualMagnitudeV = Impl::stringToDouble(getField("visualMagnitudeV"));
    obj.photographicMagnitudeB =
        Impl::stringToDouble(getField("photographicMagnitudeB"));
    obj.bMinusV = Impl::stringToDouble(getField("bMinusV"));
    obj.surfaceBrightness =
        Impl::stringToDouble(getField("surfaceBrightness"));

    // Size fields
    obj.majorAxis = Impl::stringToDouble(getField("majorAxis"));
    obj.minorAxis = Impl::stringToDouble(getField("minorAxis"));
    obj.positionAngle = Impl::stringToDouble(getField("positionAngle"));

    // Description and ranking fields
    obj.detailedDescription = getField("detailedDescription");
    obj.briefDescription = getField("briefDescription");
    obj.aliases = getField("aliases");
    obj.amateurRank = Impl::stringToInt(getField("amateurRank"));
    obj.clickCount = Impl::stringToInt(getField("clickCount"));

    return obj;
}

auto CsvHandler::celestialObjectToRow(const CelestialObjectModel& object)
    -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> row;

    row["identifier"] = object.identifier;
    row["mIdentifier"] = object.mIdentifier;
    row["extensionName"] = object.extensionName;
    row["component"] = object.component;
    row["className"] = object.className;
    row["chineseName"] = object.chineseName;
    row["type"] = object.type;
    row["duplicateType"] = object.duplicateType;
    row["morphology"] = object.morphology;
    row["constellationZh"] = object.constellationZh;
    row["constellationEn"] = object.constellationEn;

    // Coordinate fields
    row["raJ2000"] = object.raJ2000;
    row["radJ2000"] = Impl::doubleToString(object.radJ2000);
    row["decJ2000"] = object.decJ2000;
    row["decDJ2000"] = Impl::doubleToString(object.decDJ2000);

    // Magnitude and photometric fields
    row["visualMagnitudeV"] = Impl::doubleToString(object.visualMagnitudeV);
    row["photographicMagnitudeB"] =
        Impl::doubleToString(object.photographicMagnitudeB);
    row["bMinusV"] = Impl::doubleToString(object.bMinusV);
    row["surfaceBrightness"] = Impl::doubleToString(object.surfaceBrightness);

    // Size fields
    row["majorAxis"] = Impl::doubleToString(object.majorAxis);
    row["minorAxis"] = Impl::doubleToString(object.minorAxis);
    row["positionAngle"] = Impl::doubleToString(object.positionAngle);

    // Description and ranking fields
    row["detailedDescription"] = object.detailedDescription;
    row["briefDescription"] = object.briefDescription;
    row["aliases"] = object.aliases;
    row["amateurRank"] = Impl::intToString(object.amateurRank);
    row["clickCount"] = Impl::intToString(object.clickCount);

    return row;
}

auto CsvHandler::importCelestialObjects(const std::string& filename,
                                       const CsvDialect& dialect)
    -> std::expected<
        std::pair<std::vector<CelestialObjectModel>, ImportResult>,
        std::string> {
    auto readResult = read(filename, dialect);
    if (!readResult) {
        return std::unexpected(readResult.error());
    }

    auto rawData = readResult.value();
    std::vector<CelestialObjectModel> objects;
    ImportResult stats;
    stats.totalRecords = rawData.size();

    for (const auto& row : rawData) {
        auto objResult = rowToCelestialObject(row, stats);
        if (objResult) {
            objects.push_back(objResult.value());
            ++stats.successCount;
        } else {
            ++stats.errorCount;
            stats.errors.push_back(objResult.error());
        }
    }

    return std::make_pair(objects, stats);
}

auto CsvHandler::exportCelestialObjects(
    const std::string& filename,
    const std::vector<CelestialObjectModel>& objects,
    const CsvDialect& dialect) -> std::expected<int, std::string> {
    if (objects.empty()) {
        return std::unexpected("No objects to export");
    }

    // Get all field names from first object
    auto firstRow = celestialObjectToRow(objects[0]);
    std::vector<std::string> fieldnames;
    fieldnames.reserve(firstRow.size());

    // Use a consistent field order
    const std::vector<std::string> fieldOrder = {
        "identifier",
        "mIdentifier",
        "extensionName",
        "component",
        "className",
        "chineseName",
        "type",
        "duplicateType",
        "morphology",
        "constellationZh",
        "constellationEn",
        "raJ2000",
        "radJ2000",
        "decJ2000",
        "decDJ2000",
        "visualMagnitudeV",
        "photographicMagnitudeB",
        "bMinusV",
        "surfaceBrightness",
        "majorAxis",
        "minorAxis",
        "positionAngle",
        "detailedDescription",
        "briefDescription",
        "aliases",
        "amateurRank",
        "clickCount",
    };

    for (const auto& field : fieldOrder) {
        if (firstRow.find(field) != firstRow.end()) {
            fieldnames.push_back(field);
        }
    }

    // Convert objects to rows
    std::vector<std::unordered_map<std::string, std::string>> rows;
    rows.reserve(objects.size());
    for (const auto& obj : objects) {
        rows.push_back(celestialObjectToRow(obj));
    }

    return write(filename, rows, fieldnames, dialect);
}

}  // namespace lithium::target::io
