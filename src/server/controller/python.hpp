/*
 * PythonController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP
#define LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP

#include <functional>
#include <memory>
#include <string>
#include "../utils/response.hpp"
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "controller.hpp"
#include "script/python_caller.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for managing Python script operations via HTTP API
 *
 * This controller provides comprehensive Python script management including:
 * - Script loading/unloading/reloading
 * - Function calling and variable management
 * - Object attribute manipulation
 * - Asynchronous execution
 * - Performance optimization
 * - Package management
 * - Virtual environment control
 */
class PythonController : public Controller {
private:
    static std::weak_ptr<lithium::PythonWrapper> mPythonWrapper;

    /**
     * @brief Convert Crow JSON to Python object
     */
    static py::object jsonToPy(const crow::json::rvalue& val) {
        switch (val.t()) {
            case crow::json::type::String:
                return py::str(std::string(val.s()));
            case crow::json::type::Number:
                // Check if it's an integer or double
                if (std::floor(val.d()) == val.d()) {
                    return py::int_(static_cast<int64_t>(val.d()));
                }
                return py::float_(val.d());
            case crow::json::type::True:
                return py::bool_(true);
            case crow::json::type::False:
                return py::bool_(false);
            case crow::json::type::List: {
                py::list l;
                for (const auto& item : val) {
                    l.append(jsonToPy(item));
                }
                return l;
            }
            case crow::json::type::Object: {
                py::dict d;
                for (const auto& key : val.keys()) {
                    d[key.c_str()] = jsonToPy(val[key]);
                }
                return d;
            }
            default:
                return py::none();
        }
    }

    /**
     * @brief Generic handler for Python operations
     *
     * @param req HTTP request object
     * @param body Parsed JSON request body
     * @param command Command name for logging
     * @param func Function to execute with PythonWrapper that returns a
     * crow::response
     * @return HTTP response with operation result
     */
    static crow::response handlePythonAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::PythonWrapper>)>
            func) {
        try {
            auto pythonWrapper = mPythonWrapper.lock();
            if (!pythonWrapper) {
                spdlog::error(
                    "PythonWrapper instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "PythonWrapper instance is null.");
            }
            return func(pythonWrapper);
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
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                               Constants::PYTHON_WRAPPER);

        // Basic Script Management
        CROW_ROUTE(app, "/python/load")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->loadScript(req, res);
                });
        CROW_ROUTE(app, "/python/unload")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->unloadScript(req, res);
                });
        CROW_ROUTE(app, "/python/reload")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->reloadScript(req, res);
                });
        CROW_ROUTE(app, "/python/list")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->listScripts(req, res);
                });

        // Function and Variable Management
        CROW_ROUTE(app, "/python/call")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->callFunction(req, res);
                });
        CROW_ROUTE(app, "/python/callAsync")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->callFunctionAsync(req, res);
                });
        CROW_ROUTE(app, "/python/batchExecute")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->batchExecute(req, res);
                });
        CROW_ROUTE(app, "/python/getVariable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getVariable(req, res);
                });
        CROW_ROUTE(app, "/python/setVariable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setVariable(req, res);
                });
        CROW_ROUTE(app, "/python/functions")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getFunctionList(req, res);
                });

        // Expression and Code Execution
        CROW_ROUTE(app, "/python/eval")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->evalExpression(req, res);
                });
        CROW_ROUTE(app, "/python/inject")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->injectCode(req, res);
                });
        CROW_ROUTE(app, "/python/executeWithLogging")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->executeWithLogging(req, res);
                });
        CROW_ROUTE(app, "/python/executeWithProfiling")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->executeWithProfiling(req, res);
                });

        // Object-Oriented Programming Support
        CROW_ROUTE(app, "/python/callMethod")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->callMethod(req, res);
                });
        CROW_ROUTE(app, "/python/getObjectAttribute")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getObjectAttribute(req, res);
                });
        CROW_ROUTE(app, "/python/setObjectAttribute")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setObjectAttribute(req, res);
                });
        CROW_ROUTE(app, "/python/manageObjectLifecycle")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->manageObjectLifecycle(req, res);
                });

        // System and Environment Management
        CROW_ROUTE(app, "/python/addSysPath")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->addSysPath(req, res);
                });
        CROW_ROUTE(app, "/python/syncVariableToGlobal")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->syncVariableToGlobal(req, res);
                });
        CROW_ROUTE(app, "/python/syncVariableFromGlobal")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->syncVariableFromGlobal(req, res);
                });

        // Performance and Memory Management
        CROW_ROUTE(app, "/python/getMemoryUsage")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getMemoryUsage(req, res);
                });
        CROW_ROUTE(app, "/python/optimizeMemory")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->optimizeMemory(req, res);
                });
        CROW_ROUTE(app, "/python/clearUnusedResources")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->clearUnusedResources(req, res);
                });
        CROW_ROUTE(app, "/python/configurePerformance")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->configurePerformance(req, res);
                });

        // Package Management
        CROW_ROUTE(app, "/python/installPackage")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->installPackage(req, res);
                });
        CROW_ROUTE(app, "/python/uninstallPackage")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->uninstallPackage(req, res);
                });

        // Virtual Environment Management
        CROW_ROUTE(app, "/python/createVenv")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->createVirtualEnvironment(req, res);
                });
        CROW_ROUTE(app, "/python/activateVenv")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->activateVirtualEnvironment(req, res);
                });

        // Debugging Support
        CROW_ROUTE(app, "/python/enableDebug")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->enableDebugMode(req, res);
                });
        CROW_ROUTE(app, "/python/setBreakpoint")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setBreakpoint(req, res);
                });

        // Advanced Features
        CROW_ROUTE(app, "/python/registerFunction")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->registerFunction(req, res);
                });
        CROW_ROUTE(app, "/python/setErrorHandlingStrategy")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setErrorHandlingStrategy(req, res);
                });
    }

    // Basic Script Management Endpoints

    /**
     * @brief Load a Python script with an alias
     * @param req Request containing script_name and alias
     */
    void loadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "loadScript",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->load_script(
                        body["script_name"].get<std::string>(),
                        body["alias"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Unload a Python script by alias
     * @param req Request containing alias
     */
    void unloadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "unloadScript",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->unload_script(
                        body["alias"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Reload a Python script by alias
     * @param req Request containing alias
     */
    void reloadScript(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "reloadScript",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->reload_script(
                        body["alias"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief List all loaded scripts
     */
    void listScripts(const crow::request& req, crow::response& res) {
        res =
            handlePythonAction(req, nlohmann::json{}, "listScripts",
                               [&](auto pythonWrapper) -> crow::response {
                                   auto scripts = pythonWrapper->list_scripts();
                                   nlohmann::json data = {{"scripts", scripts}};
                                   return ResponseBuilder::success(data);
                               });
    }

    // Function and Variable Management Endpoints

    /**
     * @brief Call a Python function synchronously
     * @param req Request containing alias, function_name, and args
     */
    void callFunction(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "callFunction",
                [&](auto pythonWrapper) -> crow::response {
                    auto result =
                        pythonWrapper->template call_function<py::object>(
                            body["alias"].get<std::string>(),
                            body["function_name"].get<std::string>());
                    if (!result.is(pybind11::none())) {
                        nlohmann::json data = {
                            {"result", std::string(py::str(result))}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::internalError(
                        "Function returned None");
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Call a Python function asynchronously
     * @param req Request containing alias and function_name
     */
    void callFunctionAsync(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "callFunctionAsync",
                [&](auto pythonWrapper) -> crow::response {
                    auto future =
                        pythonWrapper->template async_call_function<py::object>(
                            body["alias"].get<std::string>(),
                            body["function_name"].get<std::string>());
                    auto result = future.get();
                    if (!result.is(pybind11::none())) {
                        nlohmann::json data = {
                            {"result", std::string(py::str(result))}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::internalError(
                        "Async function returned None");
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Execute multiple functions in batch
     * @param req Request containing alias and function_names array
     */
    void batchExecute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "batchExecute",
                [&](auto pythonWrapper) -> crow::response {
                    std::vector<std::string> function_names =
                        body["function_names"].get<std::vector<std::string>>();
                    auto results =
                        pythonWrapper->template batch_execute<py::object>(
                            body["alias"].get<std::string>(), function_names);

                    nlohmann::json result_array = nlohmann::json::array();
                    for (size_t i = 0; i < results.size(); ++i) {
                        result_array.push_back(
                            std::string(py::str(results[i])));
                    }

                    nlohmann::json data = {{"results", result_array}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Get a variable value from Python script
     * @param req Request containing alias and variable_name
     */
    void getVariable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "getVariable",
                [&](auto pythonWrapper) -> crow::response {
                    auto result =
                        pythonWrapper->template get_variable<py::object>(
                            body["alias"].get<std::string>(),
                            body["variable_name"].get<std::string>());
                    if (!result.is(pybind11::none())) {
                        nlohmann::json data = {
                            {"value", std::string(py::str(result))}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::internalError(
                        "Variable not found or is None");
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Set a variable value in Python script
     * @param req Request containing alias, variable_name, and value
     */
    void setVariable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "setVariable",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->set_variable(
                        body["alias"].get<std::string>(),
                        body["variable_name"].get<std::string>(),
                        py::str(body["value"].get<std::string>()));
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Get list of available functions in a script
     * @param req Request containing alias
     */
    void getFunctionList(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "getFunctionList",
                [&](auto pythonWrapper) -> crow::response {
                    auto functions = pythonWrapper->get_function_list(
                        body["alias"].get<std::string>());
                    nlohmann::json data = {{"functions", functions}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Expression and Code Execution Endpoints

    /**
     * @brief Evaluate a Python expression
     * @param req Request containing alias and expression
     */
    void evalExpression(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "evalExpression",
                [&](auto pythonWrapper) -> crow::response {
                    auto result = pythonWrapper->eval_expression(
                        body["alias"].get<std::string>(),
                        body["expression"].get<std::string>());
                    if (!result.is(pybind11::none())) {
                        nlohmann::json data = {
                            {"result", std::string(py::str(result))}};
                        return ResponseBuilder::success(data);
                    }
                    return ResponseBuilder::internalError(
                        "Expression evaluation returned None");
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Inject code into Python runtime
     * @param req Request containing code_snippet
     */
    void injectCode(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "injectCode",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->inject_code(
                        body["code_snippet"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Execute script with logging to file
     * @param req Request containing script_content and log_file
     */
    void executeWithLogging(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "executeWithLogging",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->execute_script_with_logging(
                        body["script_content"].get<std::string>(),
                        body["log_file"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Execute script with performance profiling
     * @param req Request containing script_content
     */
    void executeWithProfiling(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "executeWithProfiling",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->execute_with_profiling(
                        body["script_content"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Object-Oriented Programming Support

    /**
     * @brief Call a method on a Python object
     * @param req Request containing alias, class_name, method_name, and args
     */
    void callMethod(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "callMethod",
                [&](auto pythonWrapper) -> crow::response {
                    py::args args;
                    if (body.contains("args") && body["args"].is_array()) {
                        for (const auto& arg : body["args"]) {
                            // Convert JSON argument to Python object using the
                            // existing helper
                            args = py::args(py::tuple(
                                py::tuple(*args) + py::tuple(py::cast(arg))));
                        }
                    }
                    auto result = pythonWrapper->call_method(
                        body["alias"].get<std::string>(),
                        body["class_name"].get<std::string>(),
                        body["method_name"].get<std::string>(), args);

                    nlohmann::json data = {
                        {"result", std::string(py::str(result))}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Get an object attribute
     * @param req Request containing alias, class_name, and attr_name
     */
    void getObjectAttribute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "getObjectAttribute",
                [&](auto pythonWrapper) -> crow::response {
                    auto result =
                        pythonWrapper
                            ->template get_object_attribute<py::object>(
                                body["alias"].get<std::string>(),
                                body["class_name"].get<std::string>(),
                                body["attr_name"].get<std::string>());

                    nlohmann::json data = {
                        {"value", std::string(py::str(result))}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Set an object attribute
     * @param req Request containing alias, class_name, attr_name, and value
     */
    void setObjectAttribute(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "setObjectAttribute",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->set_object_attribute(
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

    /**
     * @brief Manage object lifecycle
     * @param req Request containing alias, object_name, and auto_cleanup
     */
    void manageObjectLifecycle(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "manageObjectLifecycle",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->manage_object_lifecycle(
                        body["alias"].get<std::string>(),
                        body["object_name"].get<std::string>(),
                        body["auto_cleanup"].get<bool>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // System and Environment Management

    /**
     * @brief Add path to Python sys.path
     * @param req Request containing path
     */
    void addSysPath(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "addSysPath",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->add_sys_path(
                        body["path"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Sync variable to Python global namespace
     * @param req Request containing name and value
     */
    void syncVariableToGlobal(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "syncVariableToGlobal",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->sync_variable_to_python(
                        body["name"].get<std::string>(),
                        py::str(body["value"].get<std::string>()));
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Sync variable from Python global namespace
     * @param req Request containing name
     */
    void syncVariableFromGlobal(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "syncVariableFromGlobal",
                [&](auto pythonWrapper) -> crow::response {
                    auto result = pythonWrapper->sync_variable_from_python(
                        body["name"].get<std::string>());

                    nlohmann::json data = {
                        {"value", std::string(py::str(result))}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Performance and Memory Management

    /**
     * @brief Get Python memory usage information
     */
    void getMemoryUsage(const crow::request& req, crow::response& res) {
        res = handlePythonAction(
            req, nlohmann::json{}, "getMemoryUsage",
            [&](auto pythonWrapper) -> crow::response {
                auto memory_info = pythonWrapper->get_memory_usage();

                nlohmann::json data = {
                    {"memory_info", std::string(py::str(memory_info))}};
                return ResponseBuilder::success(data);
            });
    }

    /**
     * @brief Optimize memory usage
     */
    void optimizeMemory(const crow::request& req, crow::response& res) {
        res = handlePythonAction(
            req, nlohmann::json{}, "optimizeMemory",
            [&](auto pythonWrapper) -> crow::response {
                pythonWrapper->optimize_memory_usage();
                return ResponseBuilder::success(nlohmann::json{});
            });
    }

    /**
     * @brief Clear unused resources
     */
    void clearUnusedResources(const crow::request& req, crow::response& res) {
        res = handlePythonAction(
            req, nlohmann::json{}, "clearUnusedResources",
            [&](auto pythonWrapper) -> crow::response {
                pythonWrapper->clear_unused_resources();
                return ResponseBuilder::success(nlohmann::json{});
            });
    }

    /**
     * @brief Configure performance settings
     * @param req Request containing performance configuration
     */
    void configurePerformance(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "configurePerformance",
                [&](auto pythonWrapper) -> crow::response {
                    lithium::PythonWrapper::PerformanceConfig config;
                    config.enable_threading =
                        body["enable_threading"].get<bool>();
                    config.enable_gil_optimization =
                        body["enable_gil_optimization"].get<bool>();
                    config.thread_pool_size =
                        body["thread_pool_size"].get<int>();
                    config.enable_caching = body["enable_caching"].get<bool>();

                    pythonWrapper->configure_performance(config);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Package Management

    /**
     * @brief Install a Python package
     * @param req Request containing package_name
     */
    void installPackage(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "installPackage",
                [&](auto pythonWrapper) -> crow::response {
                    bool success = pythonWrapper->install_package(
                        body["package_name"].get<std::string>());

                    nlohmann::json data = {{"installed", success}};
                    if (success) {
                        return ResponseBuilder::success(data);
                    } else {
                        return ResponseBuilder::internalError(
                            "Failed to install package");
                    }
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Uninstall a Python package
     * @param req Request containing package_name
     */
    void uninstallPackage(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "uninstallPackage",
                [&](auto pythonWrapper) -> crow::response {
                    bool success = pythonWrapper->uninstall_package(
                        body["package_name"].get<std::string>());

                    nlohmann::json data = {{"uninstalled", success}};
                    if (success) {
                        return ResponseBuilder::success(data);
                    } else {
                        return ResponseBuilder::internalError(
                            "Failed to uninstall package");
                    }
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Virtual Environment Management

    /**
     * @brief Create a virtual environment
     * @param req Request containing env_name
     */
    void createVirtualEnvironment(const crow::request& req,
                                  crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "createVirtualEnvironment",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->create_virtual_environment(
                        body["env_name"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Activate a virtual environment
     * @param req Request containing env_name
     */
    void activateVirtualEnvironment(const crow::request& req,
                                    crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "activateVirtualEnvironment",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->activate_virtual_environment(
                        body["env_name"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Debugging Support

    /**
     * @brief Enable or disable debug mode
     * @param req Request containing enable flag
     */
    void enableDebugMode(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "enableDebugMode",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->enable_debug_mode(
                        body["enable"].get<bool>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Set a breakpoint in a script
     * @param req Request containing alias and line_number
     */
    void setBreakpoint(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "setBreakpoint",
                [&](auto pythonWrapper) -> crow::response {
                    pythonWrapper->set_breakpoint(
                        body["alias"].get<std::string>(),
                        body["line_number"].get<int>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Advanced Features

    /**
     * @brief Register a C++ function to be callable from Python
     * @param req Request containing function name and implementation details
     */
    void registerFunction(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "registerFunction",
                [&](auto pythonWrapper) -> crow::response {
                    // Note: This is a simplified implementation
                    // In practice, you'd need to handle function registration
                    // more carefully
                    std::function<void()> dummy_func = []() {
                        spdlog::info("Registered function called");
                    };
                    pythonWrapper->register_function(
                        body["name"].get<std::string>(), dummy_func);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    /**
     * @brief Set error handling strategy
     * @param req Request containing strategy type
     */
    void setErrorHandlingStrategy(const crow::request& req,
                                  crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePythonAction(
                req, body, "setErrorHandlingStrategy",
                [&](auto pythonWrapper) -> crow::response {
                    int strategy_int = body["strategy"].get<int>();
                    auto strategy = static_cast<
                        lithium::PythonWrapper::ErrorHandlingStrategy>(
                        strategy_int);
                    pythonWrapper->set_error_handling_strategy(strategy);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }
};

inline std::weak_ptr<lithium::PythonWrapper> PythonController::mPythonWrapper;

}  // namespace lithium::server::controller

#endif  // LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP
