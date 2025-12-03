/*
 * python.hpp - Unified Python Controller
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This controller provides a unified interface for Python script operations:
 * - High-level execution via ScriptService (/api/python/*)
 * - Low-level PythonWrapper access (/python/*)
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SCRIPT_PYTHON_HPP
#define LITHIUM_SERVER_CONTROLLER_SCRIPT_PYTHON_HPP

#include "../../utils/response.hpp"
#include "../controller.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "script/python_caller.hpp"
#include "script/script_service.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Unified Python Controller
 *
 * Provides both high-level ScriptService API and low-level PythonWrapper access.
 *
 * High-Level API (/api/python/*):
 * - execute, executeFile, executeFunction, executeAsync
 * - numpy operations
 * - validation and analysis
 * - statistics
 *
 * Low-Level API (/python/*):
 * - Script management (load, unload, reload)
 * - Direct function calls and variable access
 * - Performance and memory management
 * - Export discovery
 */
class PythonController : public Controller {
private:
    static std::weak_ptr<lithium::ScriptService> mService;
    static std::weak_ptr<lithium::PythonWrapper> mPythonWrapper;

    // =========================================================================
    // Helper Functions
    // =========================================================================

    static auto handleServiceAction(
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::ScriptService>)> func)
        -> crow::response {
        try {
            auto service = mService.lock();
            if (!service || !service->isInitialized()) {
                spdlog::error("ScriptService unavailable for command: {}", command);
                return ResponseBuilder::internalError("ScriptService unavailable");
            }
            return func(service);
        } catch (const std::exception& e) {
            spdlog::error("Error in {}: {}", command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    static auto handleWrapperAction(
        const nlohmann::json& body, const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::PythonWrapper>)> func)
        -> crow::response {
        try {
            auto wrapper = mPythonWrapper.lock();
            if (!wrapper) {
                spdlog::error("PythonWrapper unavailable for command: {}", command);
                return ResponseBuilder::internalError("PythonWrapper unavailable");
            }
            return func(wrapper);
        } catch (const std::invalid_argument& e) {
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::exception& e) {
            spdlog::error("Error in {}: {}", command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Initialize weak pointers
        GET_OR_CREATE_WEAK_PTR(mService, lithium::ScriptService,
                               Constants::SCRIPT_SERVICE);
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                               Constants::PYTHON_WRAPPER);

        // =====================================================================
        // High-Level API via ScriptService (/api/python/*)
        // =====================================================================

        CROW_ROUTE(app, "/api/python/execute")
            .methods("POST"_method)(&PythonController::apiExecute, this);
        CROW_ROUTE(app, "/api/python/executeFile")
            .methods("POST"_method)(&PythonController::apiExecuteFile, this);
        CROW_ROUTE(app, "/api/python/executeFunction")
            .methods("POST"_method)(&PythonController::apiExecuteFunction, this);
        CROW_ROUTE(app, "/api/python/executeAsync")
            .methods("POST"_method)(&PythonController::apiExecuteAsync, this);
        CROW_ROUTE(app, "/api/python/numpy")
            .methods("POST"_method)(&PythonController::apiNumpyOp, this);
        CROW_ROUTE(app, "/api/python/validate")
            .methods("POST"_method)(&PythonController::apiValidate, this);
        CROW_ROUTE(app, "/api/python/analyze")
            .methods("POST"_method)(&PythonController::apiAnalyze, this);
        CROW_ROUTE(app, "/api/python/statistics")
            .methods("GET"_method)(&PythonController::apiGetStatistics, this);
        CROW_ROUTE(app, "/api/python/statistics/reset")
            .methods("POST"_method)(&PythonController::apiResetStatistics, this);

        // =====================================================================
        // Low-Level API via PythonWrapper (/python/*)
        // =====================================================================

        // Script Management
        CROW_ROUTE(app, "/python/load")
            .methods("POST"_method)(&PythonController::loadScript, this);
        CROW_ROUTE(app, "/python/unload")
            .methods("POST"_method)(&PythonController::unloadScript, this);
        CROW_ROUTE(app, "/python/reload")
            .methods("POST"_method)(&PythonController::reloadScript, this);
        CROW_ROUTE(app, "/python/list")
            .methods("GET"_method)(&PythonController::listScripts, this);

        // Function and Variable Management
        CROW_ROUTE(app, "/python/call")
            .methods("POST"_method)(&PythonController::callFunction, this);
        CROW_ROUTE(app, "/python/callAsync")
            .methods("POST"_method)(&PythonController::callFunctionAsync, this);
        CROW_ROUTE(app, "/python/batchExecute")
            .methods("POST"_method)(&PythonController::batchExecute, this);
        CROW_ROUTE(app, "/python/getVariable")
            .methods("POST"_method)(&PythonController::getVariable, this);
        CROW_ROUTE(app, "/python/setVariable")
            .methods("POST"_method)(&PythonController::setVariable, this);
        CROW_ROUTE(app, "/python/functions")
            .methods("POST"_method)(&PythonController::getFunctionList, this);

        // Expression and Code Execution
        CROW_ROUTE(app, "/python/eval")
            .methods("POST"_method)(&PythonController::evalExpression, this);
        CROW_ROUTE(app, "/python/inject")
            .methods("POST"_method)(&PythonController::injectCode, this);

        // Object-Oriented Programming
        CROW_ROUTE(app, "/python/callMethod")
            .methods("POST"_method)(&PythonController::callMethod, this);
        CROW_ROUTE(app, "/python/getObjectAttribute")
            .methods("POST"_method)(&PythonController::getObjectAttribute, this);
        CROW_ROUTE(app, "/python/setObjectAttribute")
            .methods("POST"_method)(&PythonController::setObjectAttribute, this);

        // System and Environment
        CROW_ROUTE(app, "/python/addSysPath")
            .methods("POST"_method)(&PythonController::addSysPath, this);
        CROW_ROUTE(app, "/python/getMemoryUsage")
            .methods("GET"_method)(&PythonController::getMemoryUsage, this);
        CROW_ROUTE(app, "/python/optimizeMemory")
            .methods("POST"_method)(&PythonController::optimizeMemory, this);

        // Export Discovery
        CROW_ROUTE(app, "/python/exports/discover")
            .methods("POST"_method)(&PythonController::discoverExports, this);
        CROW_ROUTE(app, "/python/exports/all")
            .methods("GET"_method)(&PythonController::getAllExports, this);
        CROW_ROUTE(app, "/python/exports/invoke")
            .methods("POST"_method)(&PythonController::invokeExport, this);

        spdlog::info("PythonController routes registered");
    }

    // =========================================================================
    // High-Level API Handlers (ScriptService)
    // =========================================================================

    crow::response apiExecute(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("code")) {
            return ResponseBuilder::badRequest("Missing 'code' field");
        }

        return handleServiceAction("execute", [&body](auto service) {
            std::string code = body["code"].get<std::string>();
            nlohmann::json args = body.value("args", nlohmann::json::object());

            lithium::ScriptExecutionConfig config;
            if (body.contains("mode")) {
                std::string mode = body["mode"].get<std::string>();
                if (mode == "inprocess") config.mode = lithium::ExecutionMode::InProcess;
                else if (mode == "pooled") config.mode = lithium::ExecutionMode::Pooled;
                else if (mode == "isolated") config.mode = lithium::ExecutionMode::Isolated;
            }
            if (body.contains("timeout")) {
                config.timeout = std::chrono::milliseconds(body["timeout"].get<int>());
            }

            auto result = service->executePython(code, args, config);

            nlohmann::json response;
            response["success"] = result.success;
            response["result"] = result.result;
            response["stdout"] = result.stdoutOutput;
            response["stderr"] = result.stderrOutput;
            response["error"] = result.errorMessage;
            response["executionTime"] = result.executionTime.count();
            response["mode"] = static_cast<int>(result.actualMode);

            return result.success
                ? ResponseBuilder::success(response)
                : ResponseBuilder::internalError(response.dump());
        });
    }

    crow::response apiExecuteFile(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("path")) {
            return ResponseBuilder::badRequest("Missing 'path' field");
        }

        return handleServiceAction("executeFile", [&body](auto service) {
            std::string path = body["path"].get<std::string>();
            nlohmann::json args = body.value("args", nlohmann::json::object());
            auto result = service->executePythonFile(path, args);

            nlohmann::json response;
            response["success"] = result.success;
            response["result"] = result.result;
            response["error"] = result.errorMessage;
            response["executionTime"] = result.executionTime.count();

            return result.success
                ? ResponseBuilder::success(response)
                : ResponseBuilder::internalError(response.dump());
        });
    }

    crow::response apiExecuteFunction(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("module") || !body.contains("function")) {
            return ResponseBuilder::badRequest("Missing 'module' or 'function'");
        }

        return handleServiceAction("executeFunction", [&body](auto service) {
            std::string moduleName = body["module"].get<std::string>();
            std::string functionName = body["function"].get<std::string>();
            nlohmann::json args = body.value("args", nlohmann::json::object());
            auto result = service->executePythonFunction(moduleName, functionName, args);

            nlohmann::json response;
            response["success"] = result.success;
            response["result"] = result.result;
            response["error"] = result.errorMessage;
            response["executionTime"] = result.executionTime.count();

            return result.success
                ? ResponseBuilder::success(response)
                : ResponseBuilder::internalError(response.dump());
        });
    }

    crow::response apiExecuteAsync(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("code")) {
            return ResponseBuilder::badRequest("Missing 'code' field");
        }

        return handleServiceAction("executeAsync", [&body](auto service) {
            std::string code = body["code"].get<std::string>();
            nlohmann::json args = body.value("args", nlohmann::json::object());

            lithium::ScriptExecutionConfig config;
            if (body.contains("timeout")) {
                config.timeout = std::chrono::milliseconds(body["timeout"].get<int>());
            }

            auto future = service->executePythonAsync(code, args, config);
            static std::atomic<uint64_t> taskCounter{0};
            uint64_t taskId = ++taskCounter;

            auto result = future.get();

            nlohmann::json response;
            response["taskId"] = taskId;
            response["success"] = result.success;
            response["result"] = result.result;
            response["stdout"] = result.stdoutOutput;
            response["stderr"] = result.stderrOutput;
            response["error"] = result.errorMessage;
            response["executionTime"] = result.executionTime.count();

            return result.success
                ? ResponseBuilder::success(response)
                : ResponseBuilder::internalError(response.dump());
        });
    }

    crow::response apiNumpyOp(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("operation") || !body.contains("arrays")) {
            return ResponseBuilder::badRequest("Missing 'operation' or 'arrays'");
        }

        return handleServiceAction("numpyOp", [&body](auto service) {
            std::string operation = body["operation"].get<std::string>();
            nlohmann::json arrays = body["arrays"];
            nlohmann::json params = body.value("params", nlohmann::json::object());

            auto result = service->executeNumpyOp(operation, arrays, params);
            if (!result) {
                return ResponseBuilder::internalError("NumPy operation failed");
            }
            return ResponseBuilder::success(*result);
        });
    }

    crow::response apiValidate(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("code")) {
            return ResponseBuilder::badRequest("Missing 'code' field");
        }

        return handleServiceAction("validate", [&body](auto service) {
            std::string code = body["code"].get<std::string>();
            bool valid = service->validateScript(code);

            nlohmann::json response;
            response["valid"] = valid;
            if (!valid) {
                response["safeVersion"] = service->getSafeScript(code);
            }
            return ResponseBuilder::success(response);
        });
    }

    crow::response apiAnalyze(const crow::request& req) {
        auto body = nlohmann::json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("code")) {
            return ResponseBuilder::badRequest("Missing 'code' field");
        }

        return handleServiceAction("analyze", [&body](auto service) {
            std::string code = body["code"].get<std::string>();
            auto analysis = service->analyzeScript(code);
            return ResponseBuilder::success(analysis);
        });
    }

    crow::response apiGetStatistics(const crow::request& /*req*/) {
        return handleServiceAction("getStatistics", [](auto service) {
            auto stats = service->getStatistics();
            return ResponseBuilder::success(stats);
        });
    }

    crow::response apiResetStatistics(const crow::request& /*req*/) {
        return handleServiceAction("resetStatistics", [](auto service) {
            service->resetStatistics();
            return ResponseBuilder::success(nlohmann::json{{"reset", true}});
        });
    }

    // =========================================================================
    // Low-Level API Handlers (PythonWrapper)
    // =========================================================================

    void loadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "loadScript", [&body](auto wrapper) {
                wrapper->load_script(
                    body["script_name"].get<std::string>(),
                    body["alias"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void unloadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "unloadScript", [&body](auto wrapper) {
                wrapper->unload_script(body["alias"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void reloadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "reloadScript", [&body](auto wrapper) {
                wrapper->reload_script(body["alias"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void listScripts(const crow::request& /*req*/, crow::response& res) {
        res = handleWrapperAction({}, "listScripts", [](auto wrapper) {
            auto scripts = wrapper->list_scripts();
            return ResponseBuilder::success(nlohmann::json{{"scripts", scripts}});
        });
    }

    void callFunction(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "callFunction", [&body](auto wrapper) {
                auto result = wrapper->template call_function<py::object>(
                    body["alias"].get<std::string>(),
                    body["function_name"].get<std::string>());
                if (!result.is(pybind11::none())) {
                    return ResponseBuilder::success(
                        nlohmann::json{{"result", std::string(py::str(result))}});
                }
                return ResponseBuilder::internalError("Function returned None");
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void callFunctionAsync(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "callFunctionAsync", [&body](auto wrapper) {
                auto future = wrapper->template async_call_function<py::object>(
                    body["alias"].get<std::string>(),
                    body["function_name"].get<std::string>());
                auto result = future.get();
                if (!result.is(pybind11::none())) {
                    return ResponseBuilder::success(
                        nlohmann::json{{"result", std::string(py::str(result))}});
                }
                return ResponseBuilder::internalError("Async function returned None");
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void batchExecute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "batchExecute", [&body](auto wrapper) {
                std::vector<std::string> functionNames =
                    body["function_names"].get<std::vector<std::string>>();
                auto results = wrapper->template batch_execute<py::object>(
                    body["alias"].get<std::string>(), functionNames);

                nlohmann::json resultArray = nlohmann::json::array();
                for (const auto& r : results) {
                    resultArray.push_back(std::string(py::str(r)));
                }
                return ResponseBuilder::success(nlohmann::json{{"results", resultArray}});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getVariable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "getVariable", [&body](auto wrapper) {
                auto result = wrapper->template get_variable<py::object>(
                    body["alias"].get<std::string>(),
                    body["variable_name"].get<std::string>());
                if (!result.is(pybind11::none())) {
                    return ResponseBuilder::success(
                        nlohmann::json{{"value", std::string(py::str(result))}});
                }
                return ResponseBuilder::internalError("Variable not found");
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void setVariable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "setVariable", [&body](auto wrapper) {
                wrapper->set_variable(
                    body["alias"].get<std::string>(),
                    body["variable_name"].get<std::string>(),
                    py::str(body["value"].get<std::string>()));
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getFunctionList(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "getFunctionList", [&body](auto wrapper) {
                auto functions = wrapper->get_function_list(body["alias"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{{"functions", functions}});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void evalExpression(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "evalExpression", [&body](auto wrapper) {
                auto result = wrapper->eval_expression(
                    body["alias"].get<std::string>(),
                    body["expression"].get<std::string>());
                if (!result.is(pybind11::none())) {
                    return ResponseBuilder::success(
                        nlohmann::json{{"result", std::string(py::str(result))}});
                }
                return ResponseBuilder::internalError("Expression returned None");
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void injectCode(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "injectCode", [&body](auto wrapper) {
                wrapper->inject_code(body["code_snippet"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void callMethod(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "callMethod", [&body](auto wrapper) {
                py::args args;
                auto result = wrapper->call_method(
                    body["alias"].get<std::string>(),
                    body["class_name"].get<std::string>(),
                    body["method_name"].get<std::string>(), args);
                return ResponseBuilder::success(
                    nlohmann::json{{"result", std::string(py::str(result))}});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getObjectAttribute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "getObjectAttribute", [&body](auto wrapper) {
                auto result = wrapper->template get_object_attribute<py::object>(
                    body["alias"].get<std::string>(),
                    body["class_name"].get<std::string>(),
                    body["attr_name"].get<std::string>());
                return ResponseBuilder::success(
                    nlohmann::json{{"value", std::string(py::str(result))}});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void setObjectAttribute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "setObjectAttribute", [&body](auto wrapper) {
                wrapper->set_object_attribute(
                    body["alias"].get<std::string>(),
                    body["class_name"].get<std::string>(),
                    body["attr_name"].get<std::string>(),
                    py::str(body["value"].get<std::string>()));
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void addSysPath(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "addSysPath", [&body](auto wrapper) {
                wrapper->add_sys_path(body["path"].get<std::string>());
                return ResponseBuilder::success(nlohmann::json{});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getMemoryUsage(const crow::request& /*req*/, crow::response& res) {
        res = handleWrapperAction({}, "getMemoryUsage", [](auto wrapper) {
            auto memInfo = wrapper->get_memory_usage();
            return ResponseBuilder::success(
                nlohmann::json{{"memory_info", std::string(py::str(memInfo))}});
        });
    }

    void optimizeMemory(const crow::request& /*req*/, crow::response& res) {
        res = handleWrapperAction({}, "optimizeMemory", [](auto wrapper) {
            wrapper->optimize_memory_usage();
            return ResponseBuilder::success(nlohmann::json{});
        });
    }

    void discoverExports(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "discoverExports", [&body](auto wrapper) {
                auto exports = wrapper->discover_exports(body["alias"].get<std::string>());
                if (exports) {
                    return ResponseBuilder::success(exports->toJson());
                }
                return ResponseBuilder::notFound("Script");
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getAllExports(const crow::request& /*req*/, crow::response& res) {
        res = handleWrapperAction({}, "getAllExports", [](auto wrapper) {
            auto allExports = wrapper->get_all_exports();
            nlohmann::json data = nlohmann::json::object();
            for (const auto& [alias, exports] : allExports) {
                data[alias] = exports.toJson();
            }
            return ResponseBuilder::success(data);
        });
    }

    void invokeExport(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleWrapperAction(body, "invokeExport", [&body](auto wrapper) {
                py::dict kwargs;
                if (body.contains("kwargs") && body["kwargs"].is_object()) {
                    for (auto& [key, val] : body["kwargs"].items()) {
                        if (val.is_string()) {
                            kwargs[key.c_str()] = py::str(val.get<std::string>());
                        } else if (val.is_number_integer()) {
                            kwargs[key.c_str()] = py::int_(val.get<int64_t>());
                        } else if (val.is_number_float()) {
                            kwargs[key.c_str()] = py::float_(val.get<double>());
                        } else if (val.is_boolean()) {
                            kwargs[key.c_str()] = py::bool_(val.get<bool>());
                        }
                    }
                }

                auto result = wrapper->invoke_export(
                    body["alias"].get<std::string>(),
                    body["function_name"].get<std::string>(),
                    kwargs);
                return ResponseBuilder::success(
                    nlohmann::json{{"result", std::string(py::str(result))}});
            });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }
};

inline std::weak_ptr<lithium::ScriptService> PythonController::mService;
inline std::weak_ptr<lithium::PythonWrapper> PythonController::mPythonWrapper;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SCRIPT_PYTHON_HPP
