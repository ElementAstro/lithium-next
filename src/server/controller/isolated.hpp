/*
 * IsolatedController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_ISOLATED_HPP
#define LITHIUM_SERVER_CONTROLLER_ISOLATED_HPP

#include "../utils/response.hpp"
#include "controller.hpp"

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "script/isolated/runner.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for isolated Python script execution via HTTP API
 *
 * Provides REST endpoints for:
 * - Executing Python scripts in isolated subprocesses
 * - Managing execution lifecycle (cancel, status, kill)
 * - Monitoring resource usage
 * - Configuration validation
 */
class IsolatedController : public Controller {
private:
    static std::weak_ptr<lithium::isolated::PythonRunner> mRunner;

    static auto handleRunnerAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::isolated::PythonRunner>)>
            func) -> crow::response {
        try {
            auto runner = mRunner.lock();
            if (!runner) {
                spdlog::error(
                    "IsolatedPythonRunner instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "IsolatedPythonRunner instance is null.");
            }
            return func(runner);
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
        GET_OR_CREATE_WEAK_PTR(mRunner, lithium::isolated::PythonRunner,
                               Constants::ISOLATED_PYTHON_RUNNER);

        // Execution endpoints
        CROW_ROUTE(app, "/isolated/execute")
            .methods("POST"_method)(&IsolatedController::execute, this);
        CROW_ROUTE(app, "/isolated/executeFile")
            .methods("POST"_method)(&IsolatedController::executeFile, this);
        CROW_ROUTE(app, "/isolated/executeFunction")
            .methods("POST"_method)(&IsolatedController::executeFunction, this);

        // Control endpoints
        CROW_ROUTE(app, "/isolated/cancel")
            .methods("POST"_method)(&IsolatedController::cancelExecution, this);
        CROW_ROUTE(app, "/isolated/kill")
            .methods("POST"_method)(&IsolatedController::killProcess, this);

        // Status endpoints
        CROW_ROUTE(app, "/isolated/status")
            .methods("GET"_method)(&IsolatedController::getStatus, this);
        CROW_ROUTE(app, "/isolated/memoryUsage")
            .methods("GET"_method)(&IsolatedController::getMemoryUsage, this);
        CROW_ROUTE(app, "/isolated/processId")
            .methods("GET"_method)(&IsolatedController::getProcessId, this);

        // Configuration endpoints
        CROW_ROUTE(app, "/isolated/validateConfig")
            .methods("POST"_method)(&IsolatedController::validateConfig, this);
        CROW_ROUTE(app, "/isolated/pythonVersion")
            .methods("GET"_method)(&IsolatedController::getPythonVersion, this);
        CROW_ROUTE(app, "/isolated/setConfig")
            .methods("POST"_method)(&IsolatedController::setConfig, this);
    }

    // Execute script content
    void execute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRunnerAction(
                req, body, "execute",
                [&](auto runner) -> crow::response {
                    std::string scriptContent = body["script"].get<std::string>();
                    nlohmann::json args = body.value("args", nlohmann::json::object());

                    auto result = runner->execute(scriptContent, args);

                    nlohmann::json data = {
                        {"success", result.success},
                        {"output", result.output},
                        {"errorOutput", result.errorOutput},
                        {"result", result.result},
                        {"executionTimeMs", result.executionTime.count()},
                        {"peakMemoryUsage", result.peakMemoryUsage}
                    };

                    if (!result.success) {
                        data["exception"] = result.exception;
                        data["exceptionType"] = result.exceptionType;
                        data["traceback"] = result.traceback;
                    }

                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Execute script file
    void executeFile(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRunnerAction(
                req, body, "executeFile",
                [&](auto runner) -> crow::response {
                    std::string scriptPath = body["path"].get<std::string>();
                    nlohmann::json args = body.value("args", nlohmann::json::object());

                    auto result = runner->executeFile(scriptPath, args);

                    nlohmann::json data = {
                        {"success", result.success},
                        {"output", result.output},
                        {"errorOutput", result.errorOutput},
                        {"result", result.result},
                        {"executionTimeMs", result.executionTime.count()},
                        {"peakMemoryUsage", result.peakMemoryUsage}
                    };

                    if (!result.success) {
                        data["exception"] = result.exception;
                        data["exceptionType"] = result.exceptionType;
                        data["traceback"] = result.traceback;
                    }

                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Execute function from module
    void executeFunction(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRunnerAction(
                req, body, "executeFunction",
                [&](auto runner) -> crow::response {
                    std::string moduleName = body["module"].get<std::string>();
                    std::string functionName = body["function"].get<std::string>();
                    nlohmann::json args = body.value("args", nlohmann::json::object());

                    auto result = runner->executeFunction(moduleName, functionName, args);

                    nlohmann::json data = {
                        {"success", result.success},
                        {"output", result.output},
                        {"errorOutput", result.errorOutput},
                        {"result", result.result},
                        {"executionTimeMs", result.executionTime.count()},
                        {"peakMemoryUsage", result.peakMemoryUsage}
                    };

                    if (!result.success) {
                        data["exception"] = result.exception;
                        data["exceptionType"] = result.exceptionType;
                        data["traceback"] = result.traceback;
                    }

                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Cancel current execution
    void cancelExecution(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "cancel",
            [&](auto runner) -> crow::response {
                bool cancelled = runner->cancel();
                nlohmann::json data = {{"cancelled", cancelled}};
                return ResponseBuilder::success(data);
            });
    }

    // Kill subprocess
    void killProcess(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "kill",
            [&](auto runner) -> crow::response {
                runner->kill();
                return ResponseBuilder::success(nlohmann::json{{"killed", true}});
            });
    }

    // Get running status
    void getStatus(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "status",
            [&](auto runner) -> crow::response {
                nlohmann::json data = {
                    {"running", runner->isRunning()},
                    {"processId", runner->getProcessId().value_or(-1)}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Get memory usage
    void getMemoryUsage(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "memoryUsage",
            [&](auto runner) -> crow::response {
                auto memUsage = runner->getCurrentMemoryUsage();
                nlohmann::json data = {
                    {"available", memUsage.has_value()},
                    {"bytes", memUsage.value_or(0)},
                    {"megabytes", memUsage.value_or(0) / (1024.0 * 1024.0)}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Get process ID
    void getProcessId(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "processId",
            [&](auto runner) -> crow::response {
                auto pid = runner->getProcessId();
                nlohmann::json data = {
                    {"available", pid.has_value()},
                    {"processId", pid.value_or(-1)}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Validate configuration
    void validateConfig(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "validateConfig",
            [&](auto runner) -> crow::response {
                auto result = runner->validateConfig();
                nlohmann::json data = {
                    {"valid", result.has_value()},
                    {"error", result.has_value() ? "" :
                        std::string(lithium::isolated::runnerErrorToString(result.error()))}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Get Python version
    void getPythonVersion(const crow::request& req, crow::response& res) {
        res = handleRunnerAction(
            req, nlohmann::json{}, "pythonVersion",
            [&](auto runner) -> crow::response {
                auto version = runner->getPythonVersion();
                nlohmann::json data = {
                    {"available", version.has_value()},
                    {"version", version.value_or("unknown")}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Set configuration
    void setConfig(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleRunnerAction(
                req, body, "setConfig",
                [&](auto runner) -> crow::response {
                    lithium::isolated::IsolationConfig config;

                    // Parse configuration from JSON
                    if (body.contains("level")) {
                        config.level = static_cast<lithium::isolated::IsolationLevel>(
                            body["level"].get<int>());
                    }
                    if (body.contains("maxMemoryMB")) {
                        config.maxMemoryMB = body["maxMemoryMB"].get<size_t>();
                    }
                    if (body.contains("maxCpuPercent")) {
                        config.maxCpuPercent = body["maxCpuPercent"].get<int>();
                    }
                    if (body.contains("timeoutSeconds")) {
                        config.timeout = std::chrono::seconds(
                            body["timeoutSeconds"].get<int>());
                    }
                    if (body.contains("allowNetwork")) {
                        config.allowNetwork = body["allowNetwork"].get<bool>();
                    }
                    if (body.contains("allowFilesystem")) {
                        config.allowFilesystem = body["allowFilesystem"].get<bool>();
                    }
                    if (body.contains("pythonExecutable")) {
                        config.pythonExecutable = body["pythonExecutable"].get<std::string>();
                    }
                    if (body.contains("executorScript")) {
                        config.executorScript = body["executorScript"].get<std::string>();
                    }
                    if (body.contains("workingDirectory")) {
                        config.workingDirectory = body["workingDirectory"].get<std::string>();
                    }
                    if (body.contains("captureOutput")) {
                        config.captureOutput = body["captureOutput"].get<bool>();
                    }

                    runner->setConfig(config);
                    return ResponseBuilder::success(nlohmann::json{{"configured", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }
};

inline std::weak_ptr<lithium::isolated::PythonRunner> IsolatedController::mRunner;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_ISOLATED_HPP
