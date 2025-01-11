/*
 * ScriptController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP
#define LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "script/check.hpp"
#include "script/sheller.hpp"

class ScriptController : public Controller {
private:
    static std::weak_ptr<lithium::ScriptManager> mScriptManager;

    // Utility function to handle all script actions
    static auto handleScriptAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ScriptManager>)> func) {
        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto scriptManager = mScriptManager.lock();
            if (!scriptManager) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ScriptManager instance is null.";
                LOG_F(ERROR,
                      "ScriptManager instance is null. Unable to proceed with "
                      "command: {}",
                      command);
                return crow::response(500, res);
            } else {
                bool success = func(scriptManager);
                if (success) {
                    res["status"] = "success";
                    res["code"] = 200;
                } else {
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] = "Not Found: The specified operation failed.";
                }
            }
        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] =
                std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_F(ERROR,
                  "Invalid argument while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Runtime error - ") +
                e.what();
            LOG_F(ERROR,
                  "Runtime error while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
            LOG_F(
                ERROR,
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
        }

        return crow::response(200, res);
    }

    static std::weak_ptr<lithium::ScriptAnalyzer> mScriptAnalyzer;

    // Utility function to handle all script analyzer actions
    static auto handleAnalyzerAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ScriptAnalyzer>)> func) {
        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto scriptAnalyzer = mScriptAnalyzer.lock();
            if (!scriptAnalyzer) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ScriptAnalyzer instance is null.";
                LOG_F(ERROR,
                      "ScriptAnalyzer instance is null. Unable to proceed with "
                      "command: {}",
                      command);
                return crow::response(500, res);
            } else {
                bool success = func(scriptAnalyzer);
                if (success) {
                    res["status"] = "success";
                    res["code"] = 200;
                } else {
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] = "Not Found: The specified operation failed.";
                }
            }
        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] =
                std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_F(ERROR,
                  "Invalid argument while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Runtime error - ") +
                e.what();
            LOG_F(ERROR,
                  "Runtime error while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
            LOG_F(
                ERROR,
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
        }

        return crow::response(200, res);
    }

public:
    void registerRoutes(crow::SimpleApp& app) override {
        // Create a weak pointer to the ScriptManager
        GET_OR_CREATE_WEAK_PTR(mScriptManager, lithium::ScriptManager,
                               Constants::SCRIPT_MANAGER);
        // Define the routes
        CROW_ROUTE(app, "/script/register")
            .methods("POST"_method)(&ScriptController::registerScript);
        CROW_ROUTE(app, "/script/delete")
            .methods("POST"_method)(&ScriptController::deleteScript);
        CROW_ROUTE(app, "/script/update")
            .methods("POST"_method)(&ScriptController::updateScript);
        CROW_ROUTE(app, "/script/run")
            .methods("POST"_method)(&ScriptController::runScript);
        CROW_ROUTE(app, "/script/runAsync")
            .methods("POST"_method)(&ScriptController::runScriptAsync);
        CROW_ROUTE(app, "/script/output")
            .methods("POST"_method)(&ScriptController::getScriptOutput);
        CROW_ROUTE(app, "/script/status")
            .methods("POST"_method)(&ScriptController::getScriptStatus);
        CROW_ROUTE(app, "/script/logs")
            .methods("POST"_method)(&ScriptController::getScriptLogs);
        CROW_ROUTE(app, "/script/list")
            .methods("GET"_method)(&ScriptController::listScripts);
        CROW_ROUTE(app, "/script/info")
            .methods("POST"_method)(&ScriptController::getScriptInfo);

        // Create a weak pointer to the ScriptAnalyzer
        GET_OR_CREATE_WEAK_PTR(mScriptAnalyzer, lithium::ScriptAnalyzer,
                               Constants::SCRIPT_ANALYZER);
        // Define the routes
        CROW_ROUTE(app, "/analyzer/analyze")
            .methods("POST"_method)(&ScriptController::analyzeScript);
        CROW_ROUTE(app, "/analyzer/analyzeWithOptions")
            .methods("POST"_method)(
                &ScriptController::analyzeScriptWithOptions);
        CROW_ROUTE(app, "/analyzer/updateConfig")
            .methods("POST"_method)(&ScriptController::updateConfig);
        CROW_ROUTE(app, "/analyzer/addCustomPattern")
            .methods("POST"_method)(&ScriptController::addCustomPattern);
        CROW_ROUTE(app, "/analyzer/validateScript")
            .methods("POST"_method)(&ScriptController::validateScript);
        CROW_ROUTE(app, "/analyzer/getSafeVersion")
            .methods("POST"_method)(&ScriptController::getSafeVersion);
        CROW_ROUTE(app, "/analyzer/getTotalAnalyzed")
            .methods("GET"_method)(&ScriptController::getTotalAnalyzed);
        CROW_ROUTE(app, "/analyzer/getAverageAnalysisTime")
            .methods("GET"_method)(&ScriptController::getAverageAnalysisTime);
    }

    // Endpoint to register a script
    void registerScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "registerScript", [&](auto scriptManager) {
                scriptManager->registerScript(std::string(body["name"].s()),
                                              std::string(body["script"].s()));
                return true;
            });
    }

    // Endpoint to delete a script
    void deleteScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "deleteScript", [&](auto scriptManager) {
                scriptManager->deleteScript(std::string(body["name"].s()));
                return true;
            });
    }

    // Endpoint to update a script
    void updateScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "updateScript", [&](auto scriptManager) {
                scriptManager->updateScript(std::string(body["name"].s()),
                                            std::string(body["script"].s()));
                return true;
            });
    }

    // Endpoint to run a script
    void runScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        std::unordered_map<std::string, std::string> args;
        for (const auto& p : body["args"]) {
            args[p.key()] = p.s();
        }
        res =
            handleScriptAction(req, body, "runScript", [&](auto scriptManager) {
                auto result = scriptManager->runScript(
                    std::string(body["name"].s()), args);
                if (result) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["output"] = result->first;
                    response["exitStatus"] = result->second;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to run a script asynchronously
    void runScriptAsync(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        std::unordered_map<std::string, std::string> args;
        for (const auto& p : body["args"]) {
            args[p.key()] = p.s();
        }
        res = handleScriptAction(
            req, body, "runScriptAsync", [&](auto scriptManager) {
                auto future = scriptManager->runScriptAsync(
                    std::string(body["name"].s()), args);
                auto result = future.get();
                if (result) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["output"] = result->first;
                    response["exitStatus"] = result->second;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to get the output of a script
    void getScriptOutput(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "getScriptOutput", [&](auto scriptManager) {
                auto output = scriptManager->getScriptOutput(
                    std::string(body["name"].s()));
                if (output) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["output"] = output.value();
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to get the status of a script
    void getScriptStatus(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "getScriptStatus", [&](auto scriptManager) {
                auto status = scriptManager->getScriptStatus(
                    std::string(body["name"].s()));
                if (status) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["status"] = status.value();
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to get the logs of a script
    void getScriptLogs(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "getScriptLogs", [&](auto scriptManager) {
                auto logs =
                    scriptManager->getScriptLogs(std::string(body["name"].s()));
                if (!logs.empty()) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["logs"] = logs;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to list all scripts
    void listScripts(const crow::request& req, crow::response& res) {
        res = handleScriptAction(
            req, crow::json::rvalue{}, "listScripts", [&](auto scriptManager) {
                auto scripts = scriptManager->getAllScripts();
                std::vector<std::string> scriptNames;
                for (const auto& script : scripts) {
                    scriptNames.push_back(script.first);
                }
                if (!scripts.empty()) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["scripts"] = scriptNames;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to get script information
    void getScriptInfo(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleScriptAction(
            req, body, "getScriptInfo", [&](auto scriptManager) {
                auto info =
                    scriptManager->getScriptInfo(std::string(body["name"].s()));
                if (!info.empty()) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["info"] = info;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to analyze a script
    void analyzeScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(
            req, body, "analyzeScript", [&](auto scriptAnalyzer) {
                scriptAnalyzer->analyze(
                    std::string(body["script"].s()), body["output_json"].b(),
                    static_cast<lithium::ReportFormat>(body["format"].i()));
                return true;
            });
    }

    // Endpoint to analyze a script with options
    void analyzeScriptWithOptions(const crow::request& req,
                                  crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(
            req, body, "analyzeScriptWithOptions", [&](auto scriptAnalyzer) {
                lithium::AnalyzerOptions options;
                options.async_mode = body["options"]["async_mode"].b();
                options.deep_analysis = body["options"]["deep_analysis"].b();
                options.thread_count = body["options"]["thread_count"].i();
                options.timeout_seconds =
                    body["options"]["timeout_seconds"].i();
                for (const auto& pattern : body["options"]["ignore_patterns"]) {
                    options.ignore_patterns.push_back(pattern.s());
                }
                auto result = scriptAnalyzer->analyzeWithOptions(
                    std::string(body["script"].s()), options);
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["complexity"] = result.complexity;
                response["execution_time"] = result.execution_time;
                response["timeout_occurred"] = result.timeout_occurred;
                response["dangers"] = crow::json::wvalue::list();
                std::vector<lithium::DangerItem> dangers;
                for (const auto& danger : result.dangers) {
                    crow::json::wvalue danger_item;
                    danger_item["category"] = danger.category;
                    danger_item["command"] = danger.command;
                    danger_item["reason"] = danger.reason;
                    danger_item["line"] = danger.line;
                    danger_item["context"] = danger.context.value_or("");
                    dangers.push_back(danger);
                }
                response["dangers"] = dangers;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to update the configuration
    void updateConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(
            req, body, "updateConfig", [&](auto scriptAnalyzer) {
                scriptAnalyzer->updateConfig(
                    std::string(body["config_file"].s()));
                return true;
            });
    }

    // Endpoint to add a custom pattern
    void addCustomPattern(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(req, body, "addCustomPattern",
                                   [&](auto scriptAnalyzer) {
                                       scriptAnalyzer->addCustomPattern(
                                           std::string(body["pattern"].s()),
                                           std::string(body["category"].s()));
                                       return true;
                                   });
    }

    // Endpoint to validate a script
    void validateScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(
            req, body, "validateScript", [&](auto scriptAnalyzer) {
                bool isValid = scriptAnalyzer->validateScript(
                    std::string(body["script"].s()));
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["is_valid"] = isValid;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to get the safe version of a script
    void getSafeVersion(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleAnalyzerAction(
            req, body, "getSafeVersion", [&](auto scriptAnalyzer) {
                std::string safeScript = scriptAnalyzer->getSafeVersion(
                    std::string(body["script"].s()));
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["safe_script"] = safeScript;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to get the total number of analyzed scripts
    void getTotalAnalyzed(const crow::request& req, crow::response& res) {
        res = handleAnalyzerAction(
            req, crow::json::rvalue{}, "getTotalAnalyzed",
            [&](auto scriptAnalyzer) {
                size_t totalAnalyzed = scriptAnalyzer->getTotalAnalyzed();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["total_analyzed"] = totalAnalyzed;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to get the average analysis time
    void getAverageAnalysisTime(const crow::request& req, crow::response& res) {
        res = handleAnalyzerAction(
            req, crow::json::rvalue{}, "getAverageAnalysisTime",
            [&](auto scriptAnalyzer) {
                double avgTime = scriptAnalyzer->getAverageAnalysisTime();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["average_analysis_time"] = avgTime;
                res.write(response.dump());
                return true;
            });
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_SCRIPT_HPP