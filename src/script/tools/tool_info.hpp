/*
 * tool_info.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_TOOLS_TOOL_INFO_HPP
#define LITHIUM_SCRIPT_TOOLS_TOOL_INFO_HPP

#include "types.hpp"
#include <optional>
#include <vector>

namespace lithium::tools {

/**
 * @brief Describes a function parameter
 */
struct ToolParameterInfo {
    std::string name;
    ToolParameterType type{ToolParameterType::Any};
    std::string description;
    bool required{true};
    nlohmann::json defaultValue;
    std::optional<ToolParameterType> elementType;

    [[nodiscard]] nlohmann::json toJson() const;
    static ToolParameterInfo fromJson(const nlohmann::json& j);
};

/**
 * @brief Describes an exported function
 */
struct ToolFunctionInfo {
    std::string name;
    std::string description;
    std::vector<ToolParameterInfo> parameters;
    std::string returnType{"dict"};
    bool isAsync{false};
    bool isStatic{true};
    std::string category;
    std::vector<std::string> tags;

    [[nodiscard]] nlohmann::json toJson() const;
    static ToolFunctionInfo fromJson(const nlohmann::json& j);
};

/**
 * @brief Comprehensive metadata about a Python tool
 */
struct ToolInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string author{"Max Qian"};
    std::string license{"GPL-3.0-or-later"};
    bool supported{true};
    std::vector<std::string> platforms;
    std::vector<ToolFunctionInfo> functions;
    std::vector<std::string> requirements;
    std::vector<std::string> capabilities;
    std::vector<std::string> categories;
    std::string modulePath;

    [[nodiscard]] nlohmann::json toJson() const;
    static ToolInfo fromJson(const nlohmann::json& j);
};

/**
 * @brief Registered tool entry
 */
struct RegisteredTool {
    std::string name;
    std::string modulePath;
    ToolInfo info;
    bool isLoaded{false};
    std::optional<std::string> loadError;
    std::vector<std::string> functionNames;

    [[nodiscard]] nlohmann::json toJson() const;
    static RegisteredTool fromJson(const nlohmann::json& j);
};

}  // namespace lithium::tools

#endif
