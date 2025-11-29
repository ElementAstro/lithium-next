/*
 * tool_info.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "tool_info.hpp"

namespace lithium::tools {

// =============================================================================
// ToolParameterInfo JSON Serialization
// =============================================================================

nlohmann::json ToolParameterInfo::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["type"] = static_cast<int>(type);
    j["description"] = description;
    j["required"] = required;
    if (!defaultValue.is_null()) {
        j["default"] = defaultValue;
    }
    if (elementType) {
        j["element_type"] = static_cast<int>(*elementType);
    }
    return j;
}

ToolParameterInfo ToolParameterInfo::fromJson(const nlohmann::json& j) {
    ToolParameterInfo info;
    info.name = j.value("name", "");
    info.type = static_cast<ToolParameterType>(j.value("type", 0));
    info.description = j.value("description", "");
    info.required = j.value("required", true);
    if (j.contains("default")) {
        info.defaultValue = j["default"];
    }
    if (j.contains("element_type")) {
        info.elementType = static_cast<ToolParameterType>(j["element_type"].get<int>());
    }
    return info;
}

// =============================================================================
// ToolFunctionInfo JSON Serialization
// =============================================================================

nlohmann::json ToolFunctionInfo::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["description"] = description;
    j["parameters"] = nlohmann::json::array();
    for (const auto& param : parameters) {
        j["parameters"].push_back(param.toJson());
    }
    j["return_type"] = returnType;
    j["is_async"] = isAsync;
    j["is_static"] = isStatic;
    j["category"] = category;
    j["tags"] = tags;
    return j;
}

ToolFunctionInfo ToolFunctionInfo::fromJson(const nlohmann::json& j) {
    ToolFunctionInfo info;
    info.name = j.value("name", "");
    info.description = j.value("description", "");
    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& param : j["parameters"]) {
            info.parameters.push_back(ToolParameterInfo::fromJson(param));
        }
    }
    info.returnType = j.value("return_type", "dict");
    info.isAsync = j.value("is_async", false);
    info.isStatic = j.value("is_static", true);
    info.category = j.value("category", "");
    if (j.contains("tags") && j["tags"].is_array()) {
        info.tags = j["tags"].get<std::vector<std::string>>();
    }
    return info;
}

// =============================================================================
// ToolInfo JSON Serialization
// =============================================================================

nlohmann::json ToolInfo::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["version"] = version;
    j["description"] = description;
    j["author"] = author;
    j["license"] = license;
    j["supported"] = supported;
    j["platforms"] = platforms;
    j["functions"] = nlohmann::json::array();
    for (const auto& func : functions) {
        j["functions"].push_back(func.toJson());
    }
    j["requirements"] = requirements;
    j["capabilities"] = capabilities;
    j["categories"] = categories;
    j["module_path"] = modulePath;
    return j;
}

ToolInfo ToolInfo::fromJson(const nlohmann::json& j) {
    ToolInfo info;
    info.name = j.value("name", "");
    info.version = j.value("version", "1.0.0");
    info.description = j.value("description", "");
    info.author = j.value("author", "Max Qian");
    info.license = j.value("license", "GPL-3.0-or-later");
    info.supported = j.value("supported", true);
    if (j.contains("platforms") && j["platforms"].is_array()) {
        info.platforms = j["platforms"].get<std::vector<std::string>>();
    }
    if (j.contains("functions") && j["functions"].is_array()) {
        for (const auto& func : j["functions"]) {
            info.functions.push_back(ToolFunctionInfo::fromJson(func));
        }
    }
    if (j.contains("requirements") && j["requirements"].is_array()) {
        info.requirements = j["requirements"].get<std::vector<std::string>>();
    }
    if (j.contains("capabilities") && j["capabilities"].is_array()) {
        info.capabilities = j["capabilities"].get<std::vector<std::string>>();
    }
    if (j.contains("categories") && j["categories"].is_array()) {
        info.categories = j["categories"].get<std::vector<std::string>>();
    }
    info.modulePath = j.value("module_path", "");
    return info;
}

// =============================================================================
// RegisteredTool JSON Serialization
// =============================================================================

nlohmann::json RegisteredTool::toJson() const {
    nlohmann::json j;
    j["name"] = name;
    j["module_path"] = modulePath;
    j["info"] = info.toJson();
    j["is_loaded"] = isLoaded;
    if (loadError) {
        j["error"] = *loadError;
    }
    j["function_names"] = functionNames;
    return j;
}

RegisteredTool RegisteredTool::fromJson(const nlohmann::json& j) {
    RegisteredTool tool;
    tool.name = j.value("name", "");
    tool.modulePath = j.value("module_path", "");
    if (j.contains("info")) {
        tool.info = ToolInfo::fromJson(j["info"]);
    }
    tool.isLoaded = j.value("is_loaded", false);
    if (j.contains("error")) {
        tool.loadError = j["error"].get<std::string>();
    }
    if (j.contains("function_names") && j["function_names"].is_array()) {
        tool.functionNames = j["function_names"].get<std::vector<std::string>>();
    }
    return tool;
}

}  // namespace lithium::tools
