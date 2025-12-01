/*
 * yaml_parser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: YAML parser for configuration system

**************************************************/

#ifndef LITHIUM_CONFIG_COMPONENTS_YAML_PARSER_HPP
#define LITHIUM_CONFIG_COMPONENTS_YAML_PARSER_HPP

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "atom/type/json.hpp"

namespace fs = std::filesystem;

namespace lithium::config {

using json = nlohmann::json;

/**
 * @brief YAML parsing options
 */
struct YamlParseOptions {
    bool allowDuplicateKeys{false};  ///< Allow duplicate keys (last wins)
    bool preserveOrder{true};        ///< Preserve key order in objects
    bool convertNullToEmpty{false};  ///< Convert null values to empty strings
    size_t maxDepth{100};            ///< Maximum nesting depth
};

/**
 * @brief YAML output options
 */
struct YamlOutputOptions {
    size_t indent{2};               ///< Indentation spaces
    size_t lineWidth{80};           ///< Line width for wrapping
    bool emitNulls{true};           ///< Emit null values
    bool flowStyle{false};          ///< Use flow style for short arrays/objects
    bool sortKeys{false};           ///< Sort object keys alphabetically
};

/**
 * @brief YAML parser and emitter utilities
 *
 * Provides functions to parse YAML content into JSON and emit JSON as YAML.
 * This serves as a bridge between YAML configuration files and the JSON-based
 * ConfigManager.
 *
 * @note The actual YAML parsing is performed by yaml-cpp library when available.
 *       When yaml-cpp is not available, a fallback implementation provides
 *       basic YAML support (JSON-compatible subset only).
 */
class YamlParser {
public:
    /**
     * @brief Parse YAML string to JSON
     *
     * @param content YAML content string
     * @param options Parsing options
     * @return JSON object or nullopt on error
     */
    [[nodiscard]] static std::optional<json> parse(
        std::string_view content,
        const YamlParseOptions& options = {});

    /**
     * @brief Parse YAML file to JSON
     *
     * @param path Path to YAML file
     * @param options Parsing options
     * @return JSON object or nullopt on error
     */
    [[nodiscard]] static std::optional<json> parseFile(
        const fs::path& path,
        const YamlParseOptions& options = {});

    /**
     * @brief Convert JSON to YAML string
     *
     * @param data JSON data to convert
     * @param options Output formatting options
     * @return YAML string
     */
    [[nodiscard]] static std::string emit(
        const json& data,
        const YamlOutputOptions& options = {});

    /**
     * @brief Save JSON as YAML file
     *
     * @param path Output file path
     * @param data JSON data to save
     * @param options Output formatting options
     * @return true if successful
     */
    [[nodiscard]] static bool saveFile(
        const fs::path& path,
        const json& data,
        const YamlOutputOptions& options = {});

    /**
     * @brief Check if yaml-cpp library is available
     *
     * @return true if yaml-cpp is available
     */
    [[nodiscard]] static bool isYamlCppAvailable();

    /**
     * @brief Get the last error message
     *
     * @return Error message or empty string
     */
    [[nodiscard]] static std::string getLastError();

private:
    static thread_local std::string lastError_;
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_COMPONENTS_YAML_PARSER_HPP
