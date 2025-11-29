/*
 * ToolRegistryController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_TOOL_REGISTRY_HPP
#define LITHIUM_SERVER_CONTROLLER_TOOL_REGISTRY_HPP

#include "../utils/response.hpp"
#include "controller.hpp"

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "script/tools/tool_registry.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for Python tool registry management via HTTP API
 *
 * Provides REST endpoints for:
 * - Listing registered tools
 * - Getting tool information
 * - Invoking tool functions
 * - Discovering and reloading tools
 */
class ToolRegistryController : public Controller {
private:
    static std::weak_ptr<lithium::tools::PythonToolRegistry> mRegistry;

    static auto handleRegistryAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::tools::PythonToolRegistry>)>
            func) -> crow::response {
        try {
            auto registry = mRegistry.lock();
            if (!registry) {
                spdlog::error(
                    "PythonToolRegistry instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "PythonToolRegistry instance is null.");
            }
            return func(registry);
        } catch (const std::invalid_argument& e) {
            spdlog::error(
                "Invalid argument while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            spdlog::error(
                "Runtime error while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        GET_OR_CREATE_WEAK_PTR(mRegistry, lithium::tools::PythonToolRegistry,
                               Constants::PYTHON_TOOL_REGISTRY);

        // Tool listing and info
        CROW_ROUTE(app, "/tools/list")
            .methods("GET"_method)(&ToolRegistryController::listTools, this);
        CROW_ROUTE(app, "/tools/info")
            .methods("POST"_method)(&ToolRegistryController::getToolInfo, this);
        CROW_ROUTE(app, "/tools/functions")
            .methods("POST"_method)(&ToolRegistryController::getToolFunctions, this);

        // Tool invocation
        CROW_ROUTE(app, "/tools/invoke")
            .methods("POST"_method)(&ToolRegistryController::invokeTool, this);

        // Tool discovery and management
        CROW_ROUTE(app, "/tools/discover")
            .methods("POST"_method)(&ToolRegistryController::discoverTools, this);
        CROW_ROUTE(app, "/tools/reload")
            .methods("POST"_method)(&ToolRegistryController::reloadTool, this);
        CROW_ROUTE(app, "/tools/unregister")
            .methods("POST"_method)(&ToolRegistryController::unregisterTool, this);

        // Registry status
        CROW_ROUTE(app, "/tools/count")
            .methods("GET"_method)(&ToolRegistryController::getToolCount, this);
    }

    // List all registered tools
    void listTools(const crow::request& req, crow::response& res) {
        res = handleRegistryAction(
            req, nlohmann::json{}, "listTools",
            [&](auto registry) -> crow::response {
                auto tools = registry->getRegisteredTools();
                nlohmann::json toolList = nlohmann::json::array();
                for (const auto& tool : tools) {
                    toolList.push_back({
                        {"name", tool.name},
                        {"version", tool.version},
                        {"description", tool.description}
                    });
                }
                nlohmann::json data = {{"tools", toolList}};
                return ResponseBuilder::success(data);
            });
    }

    // Get tool information
    void getToolInfo(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "getToolInfo",
                [&](auto registry) -> crow::response {
                    std::string toolName = body["name"].get<std::string>();
                    auto toolInfo = registry->getToolInfo(toolName);

                    if (!toolInfo) {
                        return ResponseBuilder::notFound("Tool");
                    }

                    nlohmann::json data = {
                        {"name", toolInfo->name},
                        {"version", toolInfo->version},
                        {"description", toolInfo->description},
                        {"author", toolInfo->author},
                        {"functionCount", toolInfo->functions.size()}
                    };
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Get tool functions
    void getToolFunctions(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "getToolFunctions",
                [&](auto registry) -> crow::response {
                    std::string toolName = body["name"].get<std::string>();
                    auto toolInfo = registry->getToolInfo(toolName);

                    if (!toolInfo) {
                        return ResponseBuilder::notFound("Tool");
                    }

                    nlohmann::json functions = nlohmann::json::array();
                    for (const auto& func : toolInfo->functions) {
                        nlohmann::json params = nlohmann::json::array();
                        for (const auto& param : func.parameters) {
                            params.push_back({
                                {"name", param.name},
                                {"type", static_cast<int>(param.type)},
                                {"required", param.required},
                                {"description", param.description}
                            });
                        }
                        functions.push_back({
                            {"name", func.name},
                            {"description", func.description},
                            {"parameters", params}
                        });
                    }

                    nlohmann::json data = {{"functions", functions}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Invoke a tool function
    void invokeTool(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "invokeTool",
                [&](auto registry) -> crow::response {
                    std::string toolName = body["tool"].get<std::string>();
                    std::string functionName = body["function"].get<std::string>();
                    nlohmann::json args = body.value("args", nlohmann::json::object());

                    auto result = registry->invoke(toolName, functionName, args);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to invoke tool function: " +
                            std::string(lithium::tools::toolRegistryErrorToString(result.error())));
                    }

                    nlohmann::json data = {
                        {"success", result->success},
                        {"result", result->result},
                        {"output", result->output},
                        {"executionTimeMs", result->executionTimeMs}
                    };

                    if (!result->success) {
                        data["error"] = result->errorMessage;
                    }

                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Discover tools from directory
    void discoverTools(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "discoverTools",
                [&](auto registry) -> crow::response {
                    std::string directory = body.value("directory", "python/tools");

                    auto result = registry->discoverTools(directory);
                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to discover tools");
                    }

                    nlohmann::json data = {
                        {"discovered", *result}
                    };
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Reload a specific tool
    void reloadTool(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "reloadTool",
                [&](auto registry) -> crow::response {
                    std::string toolName = body["name"].get<std::string>();

                    auto result = registry->reloadTool(toolName);
                    nlohmann::json data = {{"reloaded", result.has_value()}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Unregister a tool
    void unregisterTool(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRegistryAction(
                req, body, "unregisterTool",
                [&](auto registry) -> crow::response {
                    std::string toolName = body["name"].get<std::string>();

                    registry->unregisterTool(toolName);
                    return ResponseBuilder::success(nlohmann::json{{"unregistered", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Get tool count
    void getToolCount(const crow::request& req, crow::response& res) {
        res = handleRegistryAction(
            req, nlohmann::json{}, "getToolCount",
            [&](auto registry) -> crow::response {
                nlohmann::json data = {{"count", registry->getToolCount()}};
                return ResponseBuilder::success(data);
            });
    }
};

inline std::weak_ptr<lithium::tools::PythonToolRegistry> ToolRegistryController::mRegistry;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_TOOL_REGISTRY_HPP
