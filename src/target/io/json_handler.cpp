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

#include "json_handler.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lithium::target::io {

/**
 * @brief Internal implementation of JSON handler
 */
class JsonHandler::Impl {
public:
    /**
     * @brief Get required field from JSON with validation
     *
     * @tparam T Type of the field value
     * @param obj JSON object
     * @param key Field key
     * @return Optional value if field exists and has correct type
     */
    template <typename T>
    static auto getField(const json& obj, const std::string& key)
        -> std::optional<T> {
        if (!obj.contains(key)) {
            return std::nullopt;
        }

        try {
            if constexpr (std::is_same_v<T, std::string>) {
                if (obj[key].is_string() || obj[key].is_null()) {
                    return obj[key].is_null() ? "" : obj[key].get<std::string>();
                }
                // Try to convert to string
                return obj[key].dump();
            } else if constexpr (std::is_same_v<T, double>) {
                if (obj[key].is_number()) {
                    return obj[key].get<double>();
                } else if (obj[key].is_string()) {
                    try {
                        return std::stod(obj[key].get<std::string>());
                    } catch (...) {
                        return 0.0;
                    }
                }
                return 0.0;
            } else if constexpr (std::is_same_v<T, int>) {
                if (obj[key].is_number_integer()) {
                    return obj[key].get<int>();
                } else if (obj[key].is_string()) {
                    try {
                        return std::stoi(obj[key].get<std::string>());
                    } catch (...) {
                        return 0;
                    }
                }
                return 0;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                if (obj[key].is_number_integer()) {
                    return obj[key].get<int64_t>();
                } else if (obj[key].is_string()) {
                    try {
                        return std::stoll(obj[key].get<std::string>());
                    } catch (...) {
                        return 0LL;
                    }
                }
                return 0LL;
            } else {
                return std::nullopt;
            }
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Read file with UTF-8 validation
     *
     * @param filename Path to file
     * @return Expected file contents or error
     */
    static auto readFileWithUtf8(const std::string& filename)
        -> std::expected<std::string, std::string> {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(std::string("Failed to open file: ") +
                                   filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        if (content.empty()) {
            return std::unexpected("File is empty");
        }

        // Basic UTF-8 validation (check for valid UTF-8 sequences)
        for (size_t i = 0; i < content.length(); ++i) {
            unsigned char c = content[i];
            if (c < 0x80) {
                // ASCII character
                continue;
            } else if ((c & 0xE0) == 0xC0) {
                // 2-byte sequence
                if (i + 1 >= content.length() ||
                    (content[i + 1] & 0xC0) != 0x80) {
                    return std::unexpected("Invalid UTF-8 sequence at byte " +
                                         std::to_string(i));
                }
                ++i;
            } else if ((c & 0xF0) == 0xE0) {
                // 3-byte sequence
                if (i + 2 >= content.length() ||
                    (content[i + 1] & 0xC0) != 0x80 ||
                    (content[i + 2] & 0xC0) != 0x80) {
                    return std::unexpected("Invalid UTF-8 sequence at byte " +
                                         std::to_string(i));
                }
                i += 2;
            } else if ((c & 0xF8) == 0xF0) {
                // 4-byte sequence
                if (i + 3 >= content.length() ||
                    (content[i + 1] & 0xC0) != 0x80 ||
                    (content[i + 2] & 0xC0) != 0x80 ||
                    (content[i + 3] & 0xC0) != 0x80) {
                    return std::unexpected("Invalid UTF-8 sequence at byte " +
                                         std::to_string(i));
                }
                i += 3;
            } else {
                return std::unexpected("Invalid UTF-8 sequence at byte " +
                                       std::to_string(i));
            }
        }

        return content;
    }

    /**
     * @brief Check if string looks like JSON array
     *
     * @param str String to check
     * @return True if string starts with '['
     */
    static auto looksLikeJsonArray(const std::string& str) -> bool {
        for (char c : str) {
            if (!std::isspace(c)) {
                return c == '[';
            }
        }
        return false;
    }

    /**
     * @brief Get required string field
     *
     * @param obj JSON object
     * @param key Field key
     * @return String value, or empty string if not found
     */
    static auto getString(const json& obj, const std::string& key)
        -> std::string {
        auto opt = getField<std::string>(obj, key);
        return opt.value_or("");
    }

    /**
     * @brief Get optional double field
     *
     * @param obj JSON object
     * @param key Field key
     * @return Double value, or 0.0 if not found
     */
    static auto getDouble(const json& obj, const std::string& key) -> double {
        auto opt = getField<double>(obj, key);
        return opt.value_or(0.0);
    }

    /**
     * @brief Get optional integer field
     *
     * @param obj JSON object
     * @param key Field key
     * @return Integer value, or 0 if not found
     */
    static auto getInt(const json& obj, const std::string& key) -> int {
        auto opt = getField<int>(obj, key);
        return opt.value_or(0);
    }

    /**
     * @brief Get optional int64_t field
     *
     * @param obj JSON object
     * @param key Field key
     * @return Integer value, or 0 if not found
     */
    static auto getInt64(const json& obj, const std::string& key) -> int64_t {
        auto opt = getField<int64_t>(obj, key);
        return opt.value_or(0LL);
    }
};

// ============================================================================
// JsonHandler Implementation
// ============================================================================

JsonHandler::JsonHandler() : impl_(std::make_unique<Impl>()) {}

JsonHandler::~JsonHandler() = default;

auto JsonHandler::read(const std::string& filename)
    -> std::expected<json, std::string> {
    auto contentResult = Impl::readFileWithUtf8(filename);
    if (!contentResult) {
        return std::unexpected(contentResult.error());
    }

    try {
        return json::parse(contentResult.value());
    } catch (const json::exception& ex) {
        return std::unexpected(std::string("JSON parse error: ") + ex.what());
    }
}

auto JsonHandler::write(const std::string& filename, const json& data,
                       int indent) -> std::expected<void, std::string> {
    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(std::string("Failed to create file: ") +
                                   filename);
        }

        if (indent == 0) {
            file << data.dump();
        } else {
            file << data.dump(indent);
        }

        if (!file.good()) {
            return std::unexpected("Error writing to JSON file");
        }

        file.close();
        return {};
    } catch (const std::exception& ex) {
        return std::unexpected(std::string("Write error: ") + ex.what());
    }
}

auto JsonHandler::validateCelestialObjectJson(const json& data)
    -> std::expected<void, std::string> {
    if (!data.is_object()) {
        return std::unexpected("Expected JSON object for celestial object");
    }

    // Check for required fields
    if (!data.contains("identifier") || !data["identifier"].is_string() ||
        data["identifier"].get<std::string>().empty()) {
        return std::unexpected("Missing or invalid required field: identifier");
    }

    return {};
}

auto JsonHandler::jsonToCelestialObject(const json& jsonObj,
                                       ImportResult& result)
    -> std::expected<CelestialObjectModel, std::string> {
    // Validate JSON structure
    auto validationResult = validateCelestialObjectJson(jsonObj);
    if (!validationResult) {
        return std::unexpected(validationResult.error());
    }

    CelestialObjectModel obj;

    // Extract fields
    obj.id = Impl::getInt64(jsonObj, "id");
    obj.identifier = Impl::getString(jsonObj, "identifier");
    obj.mIdentifier = Impl::getString(jsonObj, "mIdentifier");
    obj.extensionName = Impl::getString(jsonObj, "extensionName");
    obj.component = Impl::getString(jsonObj, "component");
    obj.className = Impl::getString(jsonObj, "className");
    obj.amateurRank = Impl::getInt(jsonObj, "amateurRank");
    obj.chineseName = Impl::getString(jsonObj, "chineseName");
    obj.type = Impl::getString(jsonObj, "type");
    obj.duplicateType = Impl::getString(jsonObj, "duplicateType");
    obj.morphology = Impl::getString(jsonObj, "morphology");
    obj.constellationZh = Impl::getString(jsonObj, "constellationZh");
    obj.constellationEn = Impl::getString(jsonObj, "constellationEn");

    // Coordinate fields
    obj.raJ2000 = Impl::getString(jsonObj, "raJ2000");
    obj.radJ2000 = Impl::getDouble(jsonObj, "radJ2000");
    obj.decJ2000 = Impl::getString(jsonObj, "decJ2000");
    obj.decDJ2000 = Impl::getDouble(jsonObj, "decDJ2000");

    // Magnitude fields
    obj.visualMagnitudeV = Impl::getDouble(jsonObj, "visualMagnitudeV");
    obj.photographicMagnitudeB =
        Impl::getDouble(jsonObj, "photographicMagnitudeB");
    obj.bMinusV = Impl::getDouble(jsonObj, "bMinusV");
    obj.surfaceBrightness = Impl::getDouble(jsonObj, "surfaceBrightness");

    // Size fields
    obj.majorAxis = Impl::getDouble(jsonObj, "majorAxis");
    obj.minorAxis = Impl::getDouble(jsonObj, "minorAxis");
    obj.positionAngle = Impl::getDouble(jsonObj, "positionAngle");

    // Description fields
    obj.detailedDescription = Impl::getString(jsonObj, "detailedDescription");
    obj.briefDescription = Impl::getString(jsonObj, "briefDescription");
    obj.aliases = Impl::getString(jsonObj, "aliases");
    obj.clickCount = Impl::getInt(jsonObj, "clickCount");

    return obj;
}

auto JsonHandler::celestialObjectToJson(const CelestialObjectModel& object)
    -> json {
    json result;

    result["id"] = object.id;
    result["identifier"] = object.identifier;
    result["mIdentifier"] = object.mIdentifier;
    result["extensionName"] = object.extensionName;
    result["component"] = object.component;
    result["className"] = object.className;
    result["amateurRank"] = object.amateurRank;
    result["chineseName"] = object.chineseName;
    result["type"] = object.type;
    result["duplicateType"] = object.duplicateType;
    result["morphology"] = object.morphology;
    result["constellationZh"] = object.constellationZh;
    result["constellationEn"] = object.constellationEn;

    result["raJ2000"] = object.raJ2000;
    result["radJ2000"] = object.radJ2000;
    result["decJ2000"] = object.decJ2000;
    result["decDJ2000"] = object.decDJ2000;

    result["visualMagnitudeV"] = object.visualMagnitudeV;
    result["photographicMagnitudeB"] = object.photographicMagnitudeB;
    result["bMinusV"] = object.bMinusV;
    result["surfaceBrightness"] = object.surfaceBrightness;

    result["majorAxis"] = object.majorAxis;
    result["minorAxis"] = object.minorAxis;
    result["positionAngle"] = object.positionAngle;

    result["detailedDescription"] = object.detailedDescription;
    result["briefDescription"] = object.briefDescription;
    result["aliases"] = object.aliases;
    result["clickCount"] = object.clickCount;

    return result;
}

auto JsonHandler::importCelestialObject(const std::string& filename)
    -> std::expected<CelestialObjectModel, std::string> {
    auto readResult = read(filename);
    if (!readResult) {
        return std::unexpected(readResult.error());
    }

    auto jsonData = readResult.value();
    ImportResult dummyStats;

    // Handle single object or first element in array
    if (jsonData.is_array() && !jsonData.empty()) {
        return jsonToCelestialObject(jsonData[0], dummyStats);
    } else if (jsonData.is_object()) {
        return jsonToCelestialObject(jsonData, dummyStats);
    } else {
        return std::unexpected("Invalid JSON format for celestial object");
    }
}

auto JsonHandler::isJsonLFormat(const std::string& filename)
    -> std::expected<bool, std::string> {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(std::string("Failed to open file: ") +
                               filename);
    }

    // Read first line to check format
    std::string firstLine;
    if (!std::getline(file, firstLine)) {
        return std::unexpected("Empty file");
    }

    // Trim whitespace
    firstLine.erase(0, firstLine.find_first_not_of(" \t\n\r"));

    // Check if first line is a JSON object (not an array)
    return !firstLine.empty() && firstLine[0] == '{';
}

auto JsonHandler::importCelestialObjects(const std::string& filename)
    -> std::expected<
        std::pair<std::vector<CelestialObjectModel>, ImportResult>,
        std::string> {
    // First try to detect format
    auto formatResult = isJsonLFormat(filename);
    if (!formatResult) {
        // Fall back to regular JSON
        auto readResult = read(filename);
        if (!readResult) {
            return std::unexpected(readResult.error());
        }

        auto jsonData = readResult.value();
        std::vector<CelestialObjectModel> objects;
        ImportResult stats;

        if (jsonData.is_array()) {
            stats.totalRecords = jsonData.size();
            for (const auto& item : jsonData) {
                auto objResult = jsonToCelestialObject(item, stats);
                if (objResult) {
                    objects.push_back(objResult.value());
                    ++stats.successCount;
                } else {
                    ++stats.errorCount;
                    stats.errors.push_back(objResult.error());
                }
            }
        } else if (jsonData.is_object()) {
            stats.totalRecords = 1;
            auto objResult = jsonToCelestialObject(jsonData, stats);
            if (objResult) {
                objects.push_back(objResult.value());
                stats.successCount = 1;
            } else {
                stats.errorCount = 1;
                stats.errors.push_back(objResult.error());
            }
        } else {
            return std::unexpected("Invalid JSON format");
        }

        return std::make_pair(objects, stats);
    }

    bool isJsonl = formatResult.value();

    if (isJsonl) {
        // Process as JSONL
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return std::unexpected(std::string("Failed to open file: ") +
                                   filename);
        }

        std::vector<CelestialObjectModel> objects;
        ImportResult stats;
        std::string line;
        int lineNum = 0;

        while (std::getline(file, line)) {
            ++lineNum;

            // Skip empty lines and comments
            if (line.empty()) {
                continue;
            }
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            if (line.empty() || line[0] == '#') {
                continue;
            }

            stats.totalRecords++;

            try {
                auto jsonObj = json::parse(line);
                auto objResult = jsonToCelestialObject(jsonObj, stats);
                if (objResult) {
                    objects.push_back(objResult.value());
                    ++stats.successCount;
                } else {
                    ++stats.errorCount;
                    stats.errors.push_back(
                        std::string("Line ") + std::to_string(lineNum) + ": " +
                        objResult.error());
                }
            } catch (const json::exception& ex) {
                ++stats.errorCount;
                stats.errors.push_back(std::string("Line ") +
                                       std::to_string(lineNum) +
                                       ": JSON parse error: " + ex.what());
            }
        }

        return std::make_pair(objects, stats);
    } else {
        // Process as regular JSON array
        auto readResult = read(filename);
        if (!readResult) {
            return std::unexpected(readResult.error());
        }

        auto jsonData = readResult.value();
        std::vector<CelestialObjectModel> objects;
        ImportResult stats;

        if (jsonData.is_array()) {
            stats.totalRecords = jsonData.size();
            for (const auto& item : jsonData) {
                auto objResult = jsonToCelestialObject(item, stats);
                if (objResult) {
                    objects.push_back(objResult.value());
                    ++stats.successCount;
                } else {
                    ++stats.errorCount;
                    stats.errors.push_back(objResult.error());
                }
            }
        } else {
            return std::unexpected("Invalid JSON format");
        }

        return std::make_pair(objects, stats);
    }
}

auto JsonHandler::exportCelestialObject(const std::string& filename,
                                       const CelestialObjectModel& object,
                                       int indent)
    -> std::expected<void, std::string> {
    auto jsonObj = celestialObjectToJson(object);
    return write(filename, jsonObj, indent);
}

auto JsonHandler::exportCelestialObjects(
    const std::string& filename,
    const std::vector<CelestialObjectModel>& objects, bool asArray, int indent)
    -> std::expected<int, std::string> {
    if (objects.empty()) {
        return std::unexpected("No objects to export");
    }

    try {
        if (asArray) {
            // Export as JSON array
            json arrayData = json::array();
            for (const auto& obj : objects) {
                arrayData.push_back(celestialObjectToJson(obj));
            }

            auto writeResult = write(filename, arrayData, indent);
            if (!writeResult) {
                return std::unexpected(writeResult.error());
            }
        } else {
            // Export as JSONL
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                return std::unexpected(std::string("Failed to create file: ") +
                                       filename);
            }

            for (const auto& obj : objects) {
                auto jsonObj = celestialObjectToJson(obj);
                file << jsonObj.dump() << "\n";
            }

            if (!file.good()) {
                return std::unexpected("Error writing to JSON file");
            }

            file.close();
        }

        return static_cast<int>(objects.size());
    } catch (const std::exception& ex) {
        return std::unexpected(std::string("Export error: ") + ex.what());
    }
}

auto JsonHandler::streamProcess(
    const std::string& filename,
    std::function<std::expected<void, std::string>(const json&)> callback)
    -> std::expected<int, std::string> {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(std::string("Failed to open file: ") +
                               filename);
    }

    std::string line;
    int processedCount = 0;

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        if (line.empty()) {
            continue;
        }

        try {
            auto jsonObj = json::parse(line);
            auto result = callback(jsonObj);
            if (!result) {
                return std::unexpected(result.error());
            }
            ++processedCount;
        } catch (const json::exception& ex) {
            return std::unexpected(std::string("JSON parse error at line ") +
                                   std::to_string(processedCount + 1) + ": " +
                                   ex.what());
        }
    }

    return processedCount;
}

}  // namespace lithium::target::io
