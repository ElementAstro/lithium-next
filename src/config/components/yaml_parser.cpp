/*
 * yaml_parser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: YAML parser implementation

**************************************************/

#include "yaml_parser.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <stack>

#include "atom/log/spdlog_logger.hpp"

// Check if yaml-cpp is available
#if __has_include(<yaml-cpp/yaml.h>)
#define LITHIUM_HAS_YAML_CPP 1
#include <yaml-cpp/yaml.h>
#else
#define LITHIUM_HAS_YAML_CPP 0
#endif

namespace lithium::config {

thread_local std::string YamlParser::lastError_;

#if LITHIUM_HAS_YAML_CPP

// yaml-cpp based implementation

namespace {

json yamlNodeToJson(const YAML::Node& node, size_t depth, size_t maxDepth) {
    if (depth > maxDepth) {
        throw std::runtime_error("Maximum nesting depth exceeded");
    }

    switch (node.Type()) {
        case YAML::NodeType::Null:
            return json(nullptr);

        case YAML::NodeType::Scalar: {
            // Try to determine the actual type
            std::string value = node.as<std::string>();

            // Boolean
            if (value == "true" || value == "True" || value == "TRUE" ||
                value == "yes" || value == "Yes" || value == "YES" ||
                value == "on" || value == "On" || value == "ON") {
                return json(true);
            }
            if (value == "false" || value == "False" || value == "FALSE" ||
                value == "no" || value == "No" || value == "NO" ||
                value == "off" || value == "Off" || value == "OFF") {
                return json(false);
            }

            // Null
            if (value == "null" || value == "Null" || value == "NULL" ||
                value == "~" || value.empty()) {
                return json(nullptr);
            }

            // Integer
            try {
                size_t pos;
                long long intVal = std::stoll(value, &pos);
                if (pos == value.size()) {
                    return json(intVal);
                }
            } catch (...) {}

            // Float
            try {
                size_t pos;
                double floatVal = std::stod(value, &pos);
                if (pos == value.size()) {
                    return json(floatVal);
                }
            } catch (...) {}

            // String
            return json(value);
        }

        case YAML::NodeType::Sequence: {
            json arr = json::array();
            for (const auto& item : node) {
                arr.push_back(yamlNodeToJson(item, depth + 1, maxDepth));
            }
            return arr;
        }

        case YAML::NodeType::Map: {
            json obj = json::object();
            for (const auto& pair : node) {
                std::string key = pair.first.as<std::string>();
                obj[key] = yamlNodeToJson(pair.second, depth + 1, maxDepth);
            }
            return obj;
        }

        default:
            return json(nullptr);
    }
}

YAML::Node jsonToYamlNode(const json& j) {
    YAML::Node node;

    if (j.is_null()) {
        node = YAML::Node(YAML::NodeType::Null);
    } else if (j.is_boolean()) {
        node = j.get<bool>();
    } else if (j.is_number_integer()) {
        node = j.get<int64_t>();
    } else if (j.is_number_unsigned()) {
        node = j.get<uint64_t>();
    } else if (j.is_number_float()) {
        node = j.get<double>();
    } else if (j.is_string()) {
        node = j.get<std::string>();
    } else if (j.is_array()) {
        for (const auto& item : j) {
            node.push_back(jsonToYamlNode(item));
        }
    } else if (j.is_object()) {
        for (auto& [key, value] : j.items()) {
            node[key] = jsonToYamlNode(value);
        }
    }

    return node;
}

}  // namespace

std::optional<json> YamlParser::parse(std::string_view content,
                                       const YamlParseOptions& options) {
    lastError_.clear();

    try {
        YAML::Node root = YAML::Load(std::string(content));
        return yamlNodeToJson(root, 0, options.maxDepth);
    } catch (const YAML::Exception& e) {
        lastError_ = std::string("YAML parse error: ") + e.what();
        LOG_ERROR("YamlParser: {}", lastError_);
        return std::nullopt;
    } catch (const std::exception& e) {
        lastError_ = std::string("Parse error: ") + e.what();
        LOG_ERROR("YamlParser: {}", lastError_);
        return std::nullopt;
    }
}

std::optional<json> YamlParser::parseFile(const fs::path& path,
                                           const YamlParseOptions& options) {
    lastError_.clear();

    try {
        YAML::Node root = YAML::LoadFile(path.string());
        return yamlNodeToJson(root, 0, options.maxDepth);
    } catch (const YAML::Exception& e) {
        lastError_ = std::string("YAML parse error: ") + e.what();
        LOG_ERROR("YamlParser: Failed to parse {}: {}", path.string(), lastError_);
        return std::nullopt;
    } catch (const std::exception& e) {
        lastError_ = std::string("Parse error: ") + e.what();
        LOG_ERROR("YamlParser: Failed to parse {}: {}", path.string(), lastError_);
        return std::nullopt;
    }
}

std::string YamlParser::emit(const json& data, const YamlOutputOptions& options) {
    YAML::Node node = jsonToYamlNode(data);

    YAML::Emitter emitter;
    emitter.SetIndent(static_cast<int>(options.indent));
    emitter.SetMapFormat(options.flowStyle ? YAML::Flow : YAML::Block);
    emitter.SetSeqFormat(options.flowStyle ? YAML::Flow : YAML::Block);

    emitter << node;
    return emitter.c_str();
}

bool YamlParser::saveFile(const fs::path& path, const json& data,
                           const YamlOutputOptions& options) {
    lastError_.clear();

    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            lastError_ = "Cannot open file for writing";
            LOG_ERROR("YamlParser: {}: {}", lastError_, path.string());
            return false;
        }

        file << emit(data, options);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Save error: ") + e.what();
        LOG_ERROR("YamlParser: {}", lastError_);
        return false;
    }
}

bool YamlParser::isYamlCppAvailable() {
    return true;
}

#else

// Fallback implementation without yaml-cpp
// Only supports JSON-compatible YAML (no advanced YAML features)

namespace {

// Simple YAML-like parser for JSON-compatible subset
json parseSimpleYaml(std::string_view content, size_t maxDepth) {
    // For now, try to parse as JSON (JSON is valid YAML)
    // This is a basic fallback for when yaml-cpp is not available
    try {
        return json::parse(content, nullptr, true, true);  // Allow comments
    } catch (...) {
        // If JSON parse fails, return empty object
        return json::object();
    }
}

std::string emitSimpleYaml(const json& data, size_t indent, size_t depth = 0) {
    std::string result;
    std::string indentStr(depth * indent, ' ');

    if (data.is_null()) {
        return "null";
    } else if (data.is_boolean()) {
        return data.get<bool>() ? "true" : "false";
    } else if (data.is_number_integer()) {
        return std::to_string(data.get<int64_t>());
    } else if (data.is_number_unsigned()) {
        return std::to_string(data.get<uint64_t>());
    } else if (data.is_number_float()) {
        std::ostringstream oss;
        oss << data.get<double>();
        return oss.str();
    } else if (data.is_string()) {
        std::string value = data.get<std::string>();
        // Check if quoting is needed
        bool needsQuotes = value.empty() ||
                           value.find(':') != std::string::npos ||
                           value.find('#') != std::string::npos ||
                           value.find('\n') != std::string::npos ||
                           value[0] == ' ' || value[0] == '"' ||
                           value[0] == '\'' || value[0] == '[' ||
                           value[0] == '{';
        if (needsQuotes) {
            // Escape and quote
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped += '"';
            for (char c : value) {
                switch (c) {
                    case '"': escaped += "\\\""; break;
                    case '\\': escaped += "\\\\"; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default: escaped += c;
                }
            }
            escaped += '"';
            return escaped;
        }
        return value;
    } else if (data.is_array()) {
        if (data.empty()) {
            return "[]";
        }
        std::string arrayIndent(depth * indent, ' ');
        for (const auto& item : data) {
            result += arrayIndent + "- ";
            if (item.is_object() || item.is_array()) {
                result += "\n" + emitSimpleYaml(item, indent, depth + 1);
            } else {
                result += emitSimpleYaml(item, indent, depth + 1) + "\n";
            }
        }
        return result;
    } else if (data.is_object()) {
        if (data.empty()) {
            return "{}";
        }
        std::string objIndent(depth * indent, ' ');
        for (auto& [key, value] : data.items()) {
            result += objIndent + key + ":";
            if (value.is_object() || value.is_array()) {
                result += "\n" + emitSimpleYaml(value, indent, depth + 1);
            } else {
                result += " " + emitSimpleYaml(value, indent, depth + 1) + "\n";
            }
        }
        return result;
    }

    return "";
}

}  // namespace

std::optional<json> YamlParser::parse(std::string_view content,
                                       const YamlParseOptions& options) {
    lastError_.clear();

    try {
        return parseSimpleYaml(content, options.maxDepth);
    } catch (const std::exception& e) {
        lastError_ = std::string("Parse error: ") + e.what();
        LOG_ERROR("YamlParser: {}", lastError_);
        return std::nullopt;
    }
}

std::optional<json> YamlParser::parseFile(const fs::path& path,
                                           const YamlParseOptions& options) {
    lastError_.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot open file";
        LOG_ERROR("YamlParser: {}: {}", lastError_, path.string());
        return std::nullopt;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse(buffer.str(), options);
}

std::string YamlParser::emit(const json& data, const YamlOutputOptions& options) {
    return emitSimpleYaml(data, options.indent);
}

bool YamlParser::saveFile(const fs::path& path, const json& data,
                           const YamlOutputOptions& options) {
    lastError_.clear();

    std::ofstream file(path);
    if (!file.is_open()) {
        lastError_ = "Cannot open file for writing";
        LOG_ERROR("YamlParser: {}: {}", lastError_, path.string());
        return false;
    }

    file << emit(data, options);
    return true;
}

bool YamlParser::isYamlCppAvailable() {
    return false;
}

#endif

std::string YamlParser::getLastError() {
    return lastError_;
}

}  // namespace lithium::config
