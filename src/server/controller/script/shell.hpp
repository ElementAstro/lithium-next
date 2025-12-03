/*
 * ScriptController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP
#define LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP

#include <crow/json.h>
#include "../../utils/response.hpp"
#include "../controller.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "script/check.hpp"
#include "script/shell/script_manager.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class ScriptController : public Controller {
private:
    static std::weak_ptr<lithium::ScriptManager> mScriptManager;

    // Utility function to handle all script actions
    static auto handleScriptAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::ScriptManager>)>
            func) -> crow::response {
        try {
            auto scriptManager = mScriptManager.lock();
            if (!scriptManager) {
                LOG_ERROR(
                    "ScriptManager instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "ScriptManager instance is null.");
            }
            return func(scriptManager);
        } catch (const std::invalid_argument& e) {
            LOG_ERROR(
                "Invalid argument while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            LOG_ERROR(
                "Runtime error while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    static std::weak_ptr<lithium::ScriptAnalyzer> mScriptAnalyzer;

    // Utility function to handle all script analyzer actions
    static auto handleAnalyzerAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::ScriptAnalyzer>)>
            func) -> crow::response {
        try {
            auto scriptAnalyzer = mScriptAnalyzer.lock();
            if (!scriptAnalyzer) {
                LOG_ERROR(
                    "ScriptAnalyzer instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "ScriptAnalyzer instance is null.");
            }
            return func(scriptAnalyzer);
        } catch (const std::invalid_argument& e) {
            LOG_ERROR(
                "Invalid argument while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            LOG_ERROR(
                "Runtime error while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Create a weak pointer to the ScriptManager
        GET_OR_CREATE_WEAK_PTR(mScriptManager, lithium::ScriptManager,
                               Constants::SCRIPT_MANAGER);
        // Define the routes
        CROW_ROUTE(app, "/script/register")
            .methods("POST"_method)(&ScriptController::registerScript, this);
        CROW_ROUTE(app, "/script/delete")
            .methods("POST"_method)(&ScriptController::deleteScript, this);
        CROW_ROUTE(app, "/script/update")
            .methods("POST"_method)(&ScriptController::updateScript, this);
        CROW_ROUTE(app, "/script/run")
            .methods("POST"_method)(&ScriptController::runScript, this);
        CROW_ROUTE(app, "/script/runAsync")
            .methods("POST"_method)(&ScriptController::runScriptAsync, this);
        CROW_ROUTE(app, "/script/output")
            .methods("POST"_method)(&ScriptController::getScriptOutput, this);
        CROW_ROUTE(app, "/script/status")
            .methods("POST"_method)(&ScriptController::getScriptStatus, this);
        CROW_ROUTE(app, "/script/logs")
            .methods("POST"_method)(&ScriptController::getScriptLogs, this);
        CROW_ROUTE(app, "/script/list")
            .methods("GET"_method)(&ScriptController::listScripts, this);
        CROW_ROUTE(app, "/script/info")
            .methods("POST"_method)(&ScriptController::getScriptInfo, this);

        // Get a weak pointer to the ScriptAnalyzer
        mScriptAnalyzer =
            GetWeakPtr<lithium::ScriptAnalyzer>(Constants::SCRIPT_ANALYZER);

        // Define the routes
        CROW_ROUTE(app, "/analyzer/analyze")
            .methods("POST"_method)(&ScriptController::analyzeScript, this);
        CROW_ROUTE(app, "/analyzer/analyzeWithOptions")
            .methods("POST"_method)(&ScriptController::analyzeScriptWithOptions,
                                    this);
        CROW_ROUTE(app, "/analyzer/updateConfig")
            .methods("POST"_method)(&ScriptController::updateConfig, this);
        CROW_ROUTE(app, "/analyzer/addCustomPattern")
            .methods("POST"_method)(&ScriptController::addCustomPattern, this);
        CROW_ROUTE(app, "/analyzer/validateScript")
            .methods("POST"_method)(&ScriptController::validateScript, this);
        CROW_ROUTE(app, "/analyzer/getSafeVersion")
            .methods("POST"_method)(&ScriptController::getSafeVersion, this);
        CROW_ROUTE(app, "/analyzer/getTotalAnalyzed")
            .methods("GET"_method)(&ScriptController::getTotalAnalyzed, this);
        CROW_ROUTE(app, "/analyzer/getAverageAnalysisTime")
            .methods("GET"_method)(&ScriptController::getAverageAnalysisTime,
                                   this);

        // Enhanced Script Management Routes
        CROW_ROUTE(app, "/script/discover")
            .methods("POST"_method)(&ScriptController::discoverScripts, this);
        CROW_ROUTE(app, "/script/statistics")
            .methods("POST"_method)(&ScriptController::getScriptStatistics,
                                    this);
        CROW_ROUTE(app, "/script/globalStatistics")
            .methods("GET"_method)(&ScriptController::getGlobalStatistics,
                                   this);
        CROW_ROUTE(app, "/script/resourceLimits")
            .methods("POST"_method)(&ScriptController::setResourceLimits, this);
        CROW_ROUTE(app, "/script/resourceUsage")
            .methods("GET"_method)(&ScriptController::getResourceUsage, this);
        CROW_ROUTE(app, "/script/executeWithConfig")
            .methods("POST"_method)(&ScriptController::executeWithConfig, this);
        CROW_ROUTE(app, "/script/executePipeline")
            .methods("POST"_method)(&ScriptController::executePipeline, this);
        CROW_ROUTE(app, "/script/metadata")
            .methods("POST"_method)(&ScriptController::getScriptMetadata, this);
        CROW_ROUTE(app, "/script/pythonAvailable")
            .methods("GET"_method)(&ScriptController::isPythonAvailable, this);
    }

    // Endpoint to register a script
    void registerScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "registerScript",
                [&](auto scriptManager) -> crow::response {
                    scriptManager->registerScript(
                        body["name"].get<std::string>(),
                        body["script"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to delete a script
    void deleteScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "deleteScript",
                [&](auto scriptManager) -> crow::response {
                    scriptManager->deleteScript(
                        body["name"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to update a script
    void updateScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "updateScript",
                [&](auto scriptManager) -> crow::response {
                    scriptManager->updateScript(
                        body["name"].get<std::string>(),
                        body["script"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to run a script
    void runScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::unordered_map<std::string, std::string> args;
            if (body.contains("args") && body["args"].is_object()) {
                for (auto& [key, value] : body["args"].items()) {
                    args[key] = value.get<std::string>();
                }
            }
            res = handleScriptAction(
                req, body, "runScript",
                [&](auto scriptManager) -> crow::response {
                    auto result = scriptManager->runScript(
                        body["name"].get<std::string>(), args);
                    if (result) {
                        nlohmann::json data = {{"output", result->first},
                                               {"exitStatus", result->second}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to run a script asynchronously
    void runScriptAsync(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            std::unordered_map<std::string, std::string> args;
            if (body.contains("args") && body["args"].is_object()) {
                for (auto& [key, value] : body["args"].items()) {
                    args[key] = value.get<std::string>();
                }
            }
            res = handleScriptAction(
                req, body, "runScriptAsync",
                [&](auto scriptManager) -> crow::response {
                    auto future = scriptManager->runScriptAsync(
                        body["name"].get<std::string>(), args);
                    auto result = future.get();
                    if (result) {
                        nlohmann::json data = {{"output", result->first},
                                               {"exitStatus", result->second}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get the output of a script
    void getScriptOutput(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptOutput",
                [&](auto scriptManager) -> crow::response {
                    auto output = scriptManager->getScriptOutput(
                        body["name"].get<std::string>());
                    if (output) {
                        nlohmann::json data = {{"output", output.value()}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get the status of a script
    void getScriptStatus(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptStatus",
                [&](auto scriptManager) -> crow::response {
                    auto status = scriptManager->getScriptStatus(
                        body["name"].get<std::string>());
                    if (status) {
                        nlohmann::json data = {{"status", status.value()}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get the logs of a script
    void getScriptLogs(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptLogs",
                [&](auto scriptManager) -> crow::response {
                    auto logs = scriptManager->getScriptLogs(
                        body["name"].get<std::string>());
                    if (!logs.empty()) {
                        nlohmann::json data = {{"logs", logs}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to list all scripts
    void listScripts(const crow::request& req, crow::response& res) {
        res = handleScriptAction(
            req, nlohmann::json{}, "listScripts",
            [&](auto scriptManager) -> crow::response {
                auto scripts = scriptManager->getAllScripts();
                nlohmann::json scriptNames = nlohmann::json::array();
                for (const auto& script : scripts) {
                    scriptNames.push_back(script.first);
                }
                if (!scripts.empty()) {
                    nlohmann::json data = {{"scripts", scriptNames}};
                    return ResponseBuilder::success(data);
                }
                return ResponseBuilder::success(
                    nlohmann::json{{"scripts", nlohmann::json::array()}});
            });
    }

    // Endpoint to get script information
    void getScriptInfo(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptInfo",
                [&](auto scriptManager) -> crow::response {
                    auto info = scriptManager->getScriptInfo(
                        body["name"].get<std::string>());
                    if (!info.empty()) {
                        nlohmann::json data = {{"info", info}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to analyze a script
    void analyzeScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "analyzeScript",
                [&](auto scriptAnalyzer) -> crow::response {
                    scriptAnalyzer->analyze(body["script"].get<std::string>(),
                                            body.value("output_json", false),
                                            static_cast<lithium::ReportFormat>(
                                                body.value("format", 0)));
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    nlohmann::json to_json(const lithium::DangerItem& item) {
        return nlohmann::json{{"category", item.category},
                              {"command", item.command},
                              {"reason", item.reason},
                              {"line", item.line},
                              {"context", item.context.value_or("")}};
    }

    nlohmann::json to_json(const std::vector<lithium::DangerItem>& items) {
        nlohmann::json json = nlohmann::json::array();
        for (const auto& item : items) {
            json.push_back(to_json(item));
        }
        return json;
    }

    // Endpoint to analyze a script with options
    void analyzeScriptWithOptions(const crow::request& req,
                                  crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "analyzeScriptWithOptions",
                [&](auto scriptAnalyzer) -> crow::response {
                    lithium::AnalyzerOptions options;
                    auto opts = body["options"];
                    options.async_mode = opts.value("async_mode", false);
                    options.deep_analysis = opts.value("deep_analysis", false);
                    options.thread_count = opts.value("thread_count", 1);
                    options.timeout_seconds = opts.value("timeout_seconds", 0);
                    if (opts.contains("ignore_patterns") &&
                        opts["ignore_patterns"].is_array()) {
                        for (const auto& pattern : opts["ignore_patterns"]) {
                            options.ignore_patterns.push_back(
                                pattern.get<std::string>());
                        }
                    }
                    auto result = scriptAnalyzer->analyzeWithOptions(
                        body["script"].get<std::string>(), options);

                    nlohmann::json data = {
                        {"complexity", result.complexity},
                        {"execution_time", result.execution_time},
                        {"timeout_occurred", result.timeout_occurred},
                        {"dangers", to_json(result.dangers)}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to update the configuration
    void updateConfig(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "updateConfig",
                [&](auto scriptAnalyzer) -> crow::response {
                    scriptAnalyzer->updateConfig(
                        body["config_file"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to add a custom pattern
    void addCustomPattern(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "addCustomPattern",
                [&](auto scriptAnalyzer) -> crow::response {
                    scriptAnalyzer->addCustomPattern(
                        body["pattern"].get<std::string>(),
                        body["category"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to validate a script
    void validateScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "validateScript",
                [&](auto scriptAnalyzer) -> crow::response {
                    bool isValid = scriptAnalyzer->validateScript(
                        body["script"].get<std::string>());
                    nlohmann::json data = {{"is_valid", isValid}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get the safe version of a script
    void getSafeVersion(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleAnalyzerAction(
                req, body, "getSafeVersion",
                [&](auto scriptAnalyzer) -> crow::response {
                    std::string safeScript = scriptAnalyzer->getSafeVersion(
                        body["script"].get<std::string>());
                    nlohmann::json data = {{"safe_script", safeScript}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get the total number of analyzed scripts
    void getTotalAnalyzed(const crow::request& req, crow::response& res) {
        res = handleAnalyzerAction(
            req, nlohmann::json{}, "getTotalAnalyzed",
            [&](auto scriptAnalyzer) -> crow::response {
                size_t totalAnalyzed = scriptAnalyzer->getTotalAnalyzed();
                nlohmann::json data = {{"total_analyzed", totalAnalyzed}};
                return ResponseBuilder::success(data);
            });
    }

    // Endpoint to get the average analysis time
    void getAverageAnalysisTime(const crow::request& req, crow::response& res) {
        res = handleAnalyzerAction(
            req, nlohmann::json{}, "getAverageAnalysisTime",
            [&](auto scriptAnalyzer) -> crow::response {
                double avgTime = scriptAnalyzer->getAverageAnalysisTime();
                nlohmann::json data = {{"average_analysis_time", avgTime}};
                return ResponseBuilder::success(data);
            });
    }

    // =========================================================================
    // Enhanced Script Management Endpoints
    // =========================================================================

    // Endpoint to discover scripts from a directory
    void discoverScripts(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "discoverScripts",
                [&](auto scriptManager) -> crow::response {
                    std::string directory =
                        body["directory"].get<std::string>();
                    bool recursive = body.value("recursive", true);

                    std::vector<std::string> extensions = {".py", ".sh"};
                    if (body.contains("extensions") &&
                        body["extensions"].is_array()) {
                        extensions.clear();
                        for (const auto& ext : body["extensions"]) {
                            extensions.push_back(ext.get<std::string>());
                        }
                    }

                    size_t count = scriptManager->discoverScripts(
                        std::filesystem::path(directory), extensions,
                        recursive);

                    nlohmann::json data = {{"scripts_discovered", count}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get script statistics
    void getScriptStatistics(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptStatistics",
                [&](auto scriptManager) -> crow::response {
                    auto stats = scriptManager->getScriptStatistics(
                        body["name"].get<std::string>());

                    nlohmann::json statistics = nlohmann::json::object();
                    for (const auto& [key, value] : stats) {
                        statistics[key] = value;
                    }
                    nlohmann::json data = {{"statistics", statistics}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get global statistics
    void getGlobalStatistics(const crow::request& req, crow::response& res) {
        res = handleScriptAction(
            req, nlohmann::json{}, "getGlobalStatistics",
            [&](auto scriptManager) -> crow::response {
                auto stats = scriptManager->getGlobalStatistics();

                nlohmann::json statistics = nlohmann::json::object();
                for (const auto& [key, value] : stats) {
                    statistics[key] = value;
                }
                nlohmann::json data = {{"statistics", statistics}};
                return ResponseBuilder::success(data);
            });
    }

    // Endpoint to set resource limits
    void setResourceLimits(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "setResourceLimits",
                [&](auto scriptManager) -> crow::response {
                    lithium::ScriptResourceLimits limits;
                    limits.maxMemoryMB = body.value("maxMemoryMB", 1024);
                    limits.maxCpuPercent = body.value("maxCpuPercent", 100);
                    limits.maxExecutionTime = std::chrono::seconds(
                        body.value("maxExecutionTimeSeconds", 3600));
                    limits.maxConcurrentScripts =
                        body.value("maxConcurrentScripts", 4);

                    scriptManager->setResourceLimits(limits);

                    nlohmann::json data = {
                        {"message", "Resource limits updated"}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get resource usage
    void getResourceUsage(const crow::request& req, crow::response& res) {
        res = handleScriptAction(
            req, nlohmann::json{}, "getResourceUsage",
            [&](auto scriptManager) -> crow::response {
                auto usage = scriptManager->getResourceUsage();

                nlohmann::json usageJson = nlohmann::json::object();
                for (const auto& [key, value] : usage) {
                    usageJson[key] = value;
                }
                nlohmann::json data = {{"usage", usageJson}};
                return ResponseBuilder::success(data);
            });
    }

    // Endpoint to execute script with configuration
    void executeWithConfig(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "executeWithConfig",
                [&](auto scriptManager) -> crow::response {
                    std::string name = body["name"].get<std::string>();
                    std::unordered_map<std::string, std::string> args;
                    if (body.contains("args") && body["args"].is_object()) {
                        for (auto& [key, value] : body["args"].items()) {
                            args[key] = value.get<std::string>();
                        }
                    }

                    lithium::RetryConfig config;
                    if (body.contains("retryConfig")) {
                        auto retryConfig = body["retryConfig"];
                        config.maxRetries = retryConfig.value("maxRetries", 0);
                        config.strategy = static_cast<lithium::RetryStrategy>(
                            retryConfig.value("strategy", 0));
                    }

                    auto result =
                        scriptManager->executeWithConfig(name, args, config);

                    nlohmann::json resultData = {
                        {"success", result.success},
                        {"exitCode", result.exitCode},
                        {"output", result.output},
                        {"errorOutput", result.errorOutput},
                        {"executionTimeMs", result.executionTime.count()}};
                    nlohmann::json data = {{"result", resultData}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to execute pipeline
    void executePipeline(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "executePipeline",
                [&](auto scriptManager) -> crow::response {
                    std::vector<std::string> scripts;
                    if (body.contains("scripts") &&
                        body["scripts"].is_array()) {
                        for (const auto& s : body["scripts"]) {
                            scripts.push_back(s.get<std::string>());
                        }
                    }

                    std::unordered_map<std::string, std::string> context;
                    if (body.contains("context") &&
                        body["context"].is_object()) {
                        for (auto& [key, value] : body["context"].items()) {
                            context[key] = value.get<std::string>();
                        }
                    }

                    bool stopOnError = body.value("stopOnError", true);

                    auto results = scriptManager->executePipeline(
                        scripts, context, stopOnError);

                    nlohmann::json resultsArray = nlohmann::json::array();
                    for (size_t i = 0; i < results.size(); i++) {
                        nlohmann::json resultItem = {
                            {"script", scripts[i]},
                            {"success", results[i].success},
                            {"exitCode", results[i].exitCode},
                            {"output", results[i].output},
                            {"executionTimeMs",
                             results[i].executionTime.count()}};
                        resultsArray.push_back(resultItem);
                    }

                    nlohmann::json data = {{"results", resultsArray}};
                    return ResponseBuilder::success(data);
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get script metadata
    void getScriptMetadata(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleScriptAction(
                req, body, "getScriptMetadata",
                [&](auto scriptManager) -> crow::response {
                    auto metadata = scriptManager->getScriptMetadata(
                        body["name"].get<std::string>());

                    if (metadata) {
                        nlohmann::json tagsArray = nlohmann::json::array();
                        for (const auto& tag : metadata->tags) {
                            tagsArray.push_back(tag);
                        }

                        nlohmann::json metadataData = {
                            {"description", metadata->description},
                            {"version", metadata->version},
                            {"author", metadata->author},
                            {"isPython", metadata->isPython},
                            {"language", static_cast<int>(metadata->language)},
                            {"tags", tagsArray}};
                        nlohmann::json data = {{"metadata", metadataData}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::notFound("Script");
                });
        } catch (const std::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to check Python availability
    void isPythonAvailable(const crow::request& req, crow::response& res) {
        res = handleScriptAction(
            req, nlohmann::json{}, "isPythonAvailable",
            [&](auto scriptManager) -> crow::response {
                bool available = scriptManager->isPythonAvailable();
                nlohmann::json data = {{"python_available", available}};
                return ResponseBuilder::success(data);
            });
    }
};

inline std::weak_ptr<lithium::ScriptManager> ScriptController::mScriptManager;
inline std::weak_ptr<lithium::ScriptAnalyzer> ScriptController::mScriptAnalyzer;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP
