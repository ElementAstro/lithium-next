/*
 * script_export.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_EXPORT_HPP
#define LITHIUM_SCRIPT_EXPORT_HPP

#include <optional>
#include <string>
#include <vector>

#include "atom/type/json_fwd.hpp"

namespace lithium {

/**
 * @brief HTTP method enumeration
 */
enum class HttpMethod { GET, POST, PUT, DELETE_, PATCH };

/**
 * @brief Export type enumeration
 */
enum class ExportType { CONTROLLER, COMMAND };

/**
 * @brief Convert HttpMethod to string
 */
std::string httpMethodToString(HttpMethod method);

/**
 * @brief Parse string to HttpMethod
 */
HttpMethod stringToHttpMethod(const std::string& str);

/**
 * @brief Parameter information for exported functions
 */
struct ParamInfo {
    std::string name;
    std::string type;
    bool required{true};
    std::optional<std::string> defaultValue;
    std::string description;

    nlohmann::json toJson() const;
    static ParamInfo fromJson(const nlohmann::json& j);
};

/**
 * @brief Export information for a function
 */
struct ExportInfo {
    std::string name;
    ExportType type;
    std::string description;
    std::vector<ParamInfo> params;
    std::string returnType;

    // Controller fields
    std::string endpoint;
    HttpMethod method{HttpMethod::POST};

    // Command fields
    std::string commandId;
    int priority{0};
    int timeoutMs{5000};

    // Metadata
    std::vector<std::string> tags;
    std::string version{"1.0.0"};
    bool deprecated{false};

    nlohmann::json toJson() const;
    static ExportInfo fromJson(const nlohmann::json& j);
};

/**
 * @brief Script metadata containing exports
 */
struct ScriptExports {
    std::string moduleName;
    std::string moduleFile;
    std::string version;
    std::vector<ExportInfo> controllers;
    std::vector<ExportInfo> commands;

    bool hasExports() const { return !controllers.empty() || !commands.empty(); }
    size_t count() const { return controllers.size() + commands.size(); }

    nlohmann::json toJson() const;
    static ScriptExports fromJson(const nlohmann::json& j);
};

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_EXPORT_HPP
