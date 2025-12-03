/*
 * script_export.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "script_export.hpp"

#include "atom/type/json.hpp"

namespace lithium {

std::string httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::DELETE_:
            return "DELETE";
        case HttpMethod::PATCH:
            return "PATCH";
        default:
            return "POST";
    }
}

HttpMethod stringToHttpMethod(const std::string& str) {
    if (str == "GET") return HttpMethod::GET;
    if (str == "PUT") return HttpMethod::PUT;
    if (str == "DELETE") return HttpMethod::DELETE_;
    if (str == "PATCH") return HttpMethod::PATCH;
    return HttpMethod::POST;
}

nlohmann::json ParamInfo::toJson() const {
    nlohmann::json j = {
        {"name", name}, {"type", type}, {"required", required}, {"description", description}};
    if (defaultValue) {
        j["default"] = *defaultValue;
    }
    return j;
}

ParamInfo ParamInfo::fromJson(const nlohmann::json& j) {
    ParamInfo p;
    p.name = j.value("name", "");
    p.type = j.value("type", "Any");
    p.required = j.value("required", true);
    p.description = j.value("description", "");
    if (j.contains("default")) {
        p.defaultValue = j["default"].is_string() ? j["default"].get<std::string>()
                                                  : j["default"].dump();
    }
    return p;
}

nlohmann::json ExportInfo::toJson() const {
    nlohmann::json j = {
        {"name", name},
        {"export_type", type == ExportType::CONTROLLER ? "controller" : "command"},
        {"description", description},
        {"return_type", returnType},
        {"tags", tags},
        {"version", version},
        {"deprecated", deprecated}};

    nlohmann::json paramsJson = nlohmann::json::array();
    for (const auto& p : params) {
        paramsJson.push_back(p.toJson());
    }
    j["parameters"] = paramsJson;

    if (type == ExportType::CONTROLLER) {
        j["endpoint"] = endpoint;
        j["method"] = httpMethodToString(method);
    } else {
        j["command_id"] = commandId;
        j["priority"] = priority;
        j["timeout_ms"] = timeoutMs;
    }

    return j;
}

ExportInfo ExportInfo::fromJson(const nlohmann::json& j) {
    ExportInfo info;
    info.name = j.value("name", "");
    info.description = j.value("description", "");
    info.returnType = j.value("return_type", "Any");
    info.version = j.value("version", "1.0.0");
    info.deprecated = j.value("deprecated", false);

    std::string typeStr = j.value("export_type", "controller");
    info.type = (typeStr == "command") ? ExportType::COMMAND : ExportType::CONTROLLER;

    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& tag : j["tags"]) {
            info.tags.push_back(tag.get<std::string>());
        }
    }

    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& p : j["parameters"]) {
            info.params.push_back(ParamInfo::fromJson(p));
        }
    }

    if (info.type == ExportType::CONTROLLER) {
        info.endpoint = j.value("endpoint", "");
        info.method = stringToHttpMethod(j.value("method", "POST"));
    } else {
        info.commandId = j.value("command_id", "");
        info.priority = j.value("priority", 0);
        info.timeoutMs = j.value("timeout_ms", 5000);
    }

    return info;
}

nlohmann::json ScriptExports::toJson() const {
    nlohmann::json j = {
        {"module_name", moduleName}, {"module_file", moduleFile}, {"version", version}};

    nlohmann::json exports;
    exports["controllers"] = nlohmann::json::array();
    exports["commands"] = nlohmann::json::array();

    for (const auto& c : controllers) {
        exports["controllers"].push_back(c.toJson());
    }
    for (const auto& c : commands) {
        exports["commands"].push_back(c.toJson());
    }

    j["exports"] = exports;
    return j;
}

ScriptExports ScriptExports::fromJson(const nlohmann::json& j) {
    ScriptExports exports;
    exports.moduleName = j.value("module_name", "");
    exports.moduleFile = j.value("module_file", "");
    exports.version = j.value("version", "1.0.0");

    if (j.contains("exports")) {
        const auto& e = j["exports"];
        if (e.contains("controllers") && e["controllers"].is_array()) {
            for (const auto& c : e["controllers"]) {
                exports.controllers.push_back(ExportInfo::fromJson(c));
            }
        }
        if (e.contains("commands") && e["commands"].is_array()) {
            for (const auto& c : e["commands"]) {
                exports.commands.push_back(ExportInfo::fromJson(c));
            }
        }
    }

    return exports;
}

}  // namespace lithium
