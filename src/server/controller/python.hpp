/*
 * PythonController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP
#define LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP

#include "atom/error/exception.hpp"
#include "controller.hpp"
#include "../utils/response.hpp"
#include <functional>
#include <memory>
#include <string>
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
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
     * @param func Function to execute with PythonWrapper
     * @return HTTP response with operation result
     */
    static auto handlePythonAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::PythonWrapper>)> func) {
        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto pythonWrapper = mPythonWrapper.lock();
            if (!pythonWrapper) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] = "Internal Server Error: PythonWrapper instance is null.";
                spdlog::error("PythonWrapper instance is null. Unable to proceed with command: {}", command);
                return crow::response(500, res);
            } else {
                bool success = func(pythonWrapper);
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
            res["error"] = std::string("Bad Request: Invalid argument - ") + e.what();
            spdlog::error("Invalid argument while executing command: {}. Exception: {}", command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Runtime error - ") + e.what();
            spdlog::error("Runtime error while executing command: {}. Exception: {}", command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Exception occurred - ") + e.what();
            spdlog::error("Exception occurred while executing command: {}. Exception: {}", command, e.what());
        }

        return crow::response(200, res);
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper, Constants::PYTHON_WRAPPER);

        // Basic Script Management
        CROW_ROUTE(app, "/python/load")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->loadScript(req, res); 
            });
        CROW_ROUTE(app, "/python/unload")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->unloadScript(req, res); 
            });
        CROW_ROUTE(app, "/python/reload")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->reloadScript(req, res); 
            });
        CROW_ROUTE(app, "/python/list")
            .methods("GET"_method)([this](const crow::request& req, crow::response& res) { 
                this->listScripts(req, res); 
            });

        // Function and Variable Management
        CROW_ROUTE(app, "/python/call")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->callFunction(req, res); 
            });
        CROW_ROUTE(app, "/python/callAsync")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->callFunctionAsync(req, res); 
            });
        CROW_ROUTE(app, "/python/batchExecute")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->batchExecute(req, res); 
            });
        CROW_ROUTE(app, "/python/getVariable")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->getVariable(req, res); 
            });
        CROW_ROUTE(app, "/python/setVariable")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->setVariable(req, res); 
            });
        CROW_ROUTE(app, "/python/functions")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->getFunctionList(req, res); 
            });

        // Expression and Code Execution
        CROW_ROUTE(app, "/python/eval")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->evalExpression(req, res); 
            });
        CROW_ROUTE(app, "/python/inject")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->injectCode(req, res); 
            });
        CROW_ROUTE(app, "/python/executeWithLogging")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->executeWithLogging(req, res); 
            });
        CROW_ROUTE(app, "/python/executeWithProfiling")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->executeWithProfiling(req, res); 
            });

        // Object-Oriented Programming Support
        CROW_ROUTE(app, "/python/callMethod")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->callMethod(req, res); 
            });
        CROW_ROUTE(app, "/python/getObjectAttribute")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->getObjectAttribute(req, res); 
            });
        CROW_ROUTE(app, "/python/setObjectAttribute")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->setObjectAttribute(req, res); 
            });
        CROW_ROUTE(app, "/python/manageObjectLifecycle")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->manageObjectLifecycle(req, res); 
            });

        // System and Environment Management
        CROW_ROUTE(app, "/python/addSysPath")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->addSysPath(req, res); 
            });
        CROW_ROUTE(app, "/python/syncVariableToGlobal")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->syncVariableToGlobal(req, res); 
            });
        CROW_ROUTE(app, "/python/syncVariableFromGlobal")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->syncVariableFromGlobal(req, res); 
            });

        // Performance and Memory Management
        CROW_ROUTE(app, "/python/getMemoryUsage")
            .methods("GET"_method)([this](const crow::request& req, crow::response& res) { 
                this->getMemoryUsage(req, res); 
            });
        CROW_ROUTE(app, "/python/optimizeMemory")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->optimizeMemory(req, res); 
            });
        CROW_ROUTE(app, "/python/clearUnusedResources")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->clearUnusedResources(req, res); 
            });
        CROW_ROUTE(app, "/python/configurePerformance")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->configurePerformance(req, res); 
            });

        // Package Management
        CROW_ROUTE(app, "/python/installPackage")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->installPackage(req, res); 
            });
        CROW_ROUTE(app, "/python/uninstallPackage")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->uninstallPackage(req, res); 
            });

        // Virtual Environment Management
        CROW_ROUTE(app, "/python/createVenv")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->createVirtualEnvironment(req, res); 
            });
        CROW_ROUTE(app, "/python/activateVenv")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->activateVirtualEnvironment(req, res); 
            });

        // Debugging Support
        CROW_ROUTE(app, "/python/enableDebug")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->enableDebugMode(req, res); 
            });
        CROW_ROUTE(app, "/python/setBreakpoint")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->setBreakpoint(req, res); 
            });

        // Advanced Features
        CROW_ROUTE(app, "/python/registerFunction")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->registerFunction(req, res); 
            });
        CROW_ROUTE(app, "/python/setErrorHandlingStrategy")
            .methods("POST"_method)([this](const crow::request& req, crow::response& res) { 
                this->setErrorHandlingStrategy(req, res); 
            });
    }

    // Basic Script Management Endpoints

    /**
     * @brief Load a Python script with an alias
     * @param req Request containing script_name and alias
     */
    void loadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "loadScript", [&](auto pythonWrapper) {
                pythonWrapper->load_script(std::string(body["script_name"].s()),
                                           std::string(body["alias"].s()));
                return true;
            });
    }

    /**
     * @brief Unload a Python script by alias
     * @param req Request containing alias
     */
    void unloadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "unloadScript", [&](auto pythonWrapper) {
                pythonWrapper->unload_script(std::string(body["alias"].s()));
                return true;
            });
    }

    /**
     * @brief Reload a Python script by alias
     * @param req Request containing alias
     */
    void reloadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "reloadScript", [&](auto pythonWrapper) {
                pythonWrapper->reload_script(std::string(body["alias"].s()));
                return true;
            });
    }

    /**
     * @brief List all loaded scripts
     */
    void listScripts(const crow::request& req, crow::response& res) {
        res = handlePythonAction(
            req, crow::json::rvalue{}, "listScripts", [&](auto pythonWrapper) {
                auto scripts = pythonWrapper->list_scripts();
                if (!scripts.empty()) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["scripts"] = scripts;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Function and Variable Management Endpoints

    /**
     * @brief Call a Python function synchronously
     * @param req Request containing alias, function_name, and args
     */
    void callFunction(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "callFunction", [&](auto pythonWrapper) {
                auto result = pythonWrapper->template call_function<py::object>(
                    std::string(body["alias"].s()),
                    std::string(body["function_name"].s()));
                if (!result.is(pybind11::none())) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["result"] = std::string(py::str(result));
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    /**
     * @brief Call a Python function asynchronously
     * @param req Request containing alias and function_name
     */
    void callFunctionAsync(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "callFunctionAsync", [&](auto pythonWrapper) {
                auto future = pythonWrapper->template async_call_function<py::object>(
                    std::string(body["alias"].s()),
                    std::string(body["function_name"].s()));
                auto result = future.get();
                if (!result.is(pybind11::none())) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["result"] = std::string(py::str(result));
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    /**
     * @brief Execute multiple functions in batch
     * @param req Request containing alias and function_names array
     */
    void batchExecute(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "batchExecute", [&](auto pythonWrapper) {
                std::vector<std::string> function_names;
                for (const auto& func : body["function_names"]) {
                    function_names.push_back(std::string(func.s()));
                }
                auto results = pythonWrapper->template batch_execute<py::object>(
                    std::string(body["alias"].s()), function_names);
                
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = crow::json::wvalue::list();
                
                for (size_t i = 0; i < results.size(); ++i) {
                    response["results"][i] = std::string(py::str(results[i]));
                }
                res.write(response.dump());
                return true;
            });
    }

    /**
     * @brief Get a variable value from Python script
     * @param req Request containing alias and variable_name
     */
    void getVariable(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "getVariable", [&](auto pythonWrapper) {
                auto result = pythonWrapper->template get_variable<py::object>(
                    std::string(body["alias"].s()),
                    std::string(body["variable_name"].s()));
                if (!result.is(pybind11::none())) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["value"] = std::string(py::str(result));
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    /**
     * @brief Set a variable value in Python script
     * @param req Request containing alias, variable_name, and value
     */
    void setVariable(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "setVariable",
                                 [&](auto pythonWrapper) {
                                     pythonWrapper->set_variable(
                                         std::string(body["alias"].s()),
                                         std::string(body["variable_name"].s()),
                                         py::str(std::string(body["value"].s())));
                                     return true;
                                 });
    }

    /**
     * @brief Get list of available functions in a script
     * @param req Request containing alias
     */
    void getFunctionList(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "getFunctionList", [&](auto pythonWrapper) {
                auto functions = pythonWrapper->get_function_list(
                    std::string(body["alias"].s()));
                if (!functions.empty()) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["functions"] = functions;
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Expression and Code Execution Endpoints

    /**
     * @brief Evaluate a Python expression
     * @param req Request containing alias and expression
     */
    void evalExpression(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "evalExpression", [&](auto pythonWrapper) {
                auto result = pythonWrapper->eval_expression(
                    std::string(body["alias"].s()),
                    std::string(body["expression"].s()));
                if (!result.is(pybind11::none())) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["result"] = std::string(py::str(result));
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    /**
     * @brief Inject code into Python runtime
     * @param req Request containing code_snippet
     */
    void injectCode(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "injectCode", [&](auto pythonWrapper) {
            pythonWrapper->inject_code(std::string(body["code_snippet"].s()));
            return true;
        });
    }

    /**
     * @brief Execute script with logging to file
     * @param req Request containing script_content and log_file
     */
    void executeWithLogging(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "executeWithLogging", [&](auto pythonWrapper) {
            pythonWrapper->execute_script_with_logging(
                std::string(body["script_content"].s()),
                std::string(body["log_file"].s()));
            return true;
        });
    }

    /**
     * @brief Execute script with performance profiling
     * @param req Request containing script_content
     */
    void executeWithProfiling(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "executeWithProfiling", [&](auto pythonWrapper) {
            pythonWrapper->execute_with_profiling(std::string(body["script_content"].s()));
            return true;
        });
    }

    // Object-Oriented Programming Support

    /**
     * @brief Call a method on a Python object
     * @param req Request containing alias, class_name, method_name, and args
     */
    void callMethod(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "callMethod", [&](auto pythonWrapper) {
            py::args args;  // TODO: Convert from JSON args
            auto result = pythonWrapper->call_method(
                std::string(body["alias"].s()),
                std::string(body["class_name"].s()),
                std::string(body["method_name"].s()),
                args);
            
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["result"] = std::string(py::str(result));
            res.write(response.dump());
            return true;
        });
    }

    /**
     * @brief Get an object attribute
     * @param req Request containing alias, class_name, and attr_name
     */
    void getObjectAttribute(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "getObjectAttribute", [&](auto pythonWrapper) {
            auto result = pythonWrapper->template get_object_attribute<py::object>(
                std::string(body["alias"].s()),
                std::string(body["class_name"].s()),
                std::string(body["attr_name"].s()));
            
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["value"] = std::string(py::str(result));
            res.write(response.dump());
            return true;
        });
    }

    /**
     * @brief Set an object attribute
     * @param req Request containing alias, class_name, attr_name, and value
     */
    void setObjectAttribute(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "setObjectAttribute", [&](auto pythonWrapper) {
            pythonWrapper->set_object_attribute(
                std::string(body["alias"].s()),
                std::string(body["class_name"].s()),
                std::string(body["attr_name"].s()),
                py::str(std::string(body["value"].s())));
            return true;
        });
    }

    /**
     * @brief Manage object lifecycle
     * @param req Request containing alias, object_name, and auto_cleanup
     */
    void manageObjectLifecycle(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "manageObjectLifecycle", [&](auto pythonWrapper) {
            pythonWrapper->manage_object_lifecycle(
                std::string(body["alias"].s()),
                std::string(body["object_name"].s()),
                body["auto_cleanup"].b());
            return true;
        });
    }

    // System and Environment Management

    /**
     * @brief Add path to Python sys.path
     * @param req Request containing path
     */
    void addSysPath(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "addSysPath", [&](auto pythonWrapper) {
            pythonWrapper->add_sys_path(std::string(body["path"].s()));
            return true;
        });
    }

    /**
     * @brief Sync variable to Python global namespace
     * @param req Request containing name and value
     */
    void syncVariableToGlobal(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "syncVariableToGlobal", [&](auto pythonWrapper) {
            pythonWrapper->sync_variable_to_python(
                std::string(body["name"].s()),
                py::str(std::string(body["value"].s())));
            return true;
        });
    }

    /**
     * @brief Sync variable from Python global namespace
     * @param req Request containing name
     */
    void syncVariableFromGlobal(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "syncVariableFromGlobal", [&](auto pythonWrapper) {
            auto result = pythonWrapper->sync_variable_from_python(std::string(body["name"].s()));
            
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["value"] = std::string(py::str(result));
            res.write(response.dump());
            return true;
        });
    }

    // Performance and Memory Management

    /**
     * @brief Get Python memory usage information
     */
    void getMemoryUsage(const crow::request& req, crow::response& res) {
        res = handlePythonAction(req, crow::json::rvalue{}, "getMemoryUsage", [&](auto pythonWrapper) {
            auto memory_info = pythonWrapper->get_memory_usage();
            
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["memory_info"] = std::string(py::str(memory_info));
            res.write(response.dump());
            return true;
        });
    }

    /**
     * @brief Optimize memory usage
     */
    void optimizeMemory(const crow::request& req, crow::response& res) {
        res = handlePythonAction(req, crow::json::rvalue{}, "optimizeMemory", [&](auto pythonWrapper) {
            pythonWrapper->optimize_memory_usage();
            return true;
        });
    }

    /**
     * @brief Clear unused resources
     */
    void clearUnusedResources(const crow::request& req, crow::response& res) {
        res = handlePythonAction(req, crow::json::rvalue{}, "clearUnusedResources", [&](auto pythonWrapper) {
            pythonWrapper->clear_unused_resources();
            return true;
        });
    }

    /**
     * @brief Configure performance settings
     * @param req Request containing performance configuration
     */
    void configurePerformance(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "configurePerformance", [&](auto pythonWrapper) {
            lithium::PythonWrapper::PerformanceConfig config;
            config.enable_threading = body["enable_threading"].b();
            config.enable_gil_optimization = body["enable_gil_optimization"].b();
            config.thread_pool_size = body["thread_pool_size"].i();
            config.enable_caching = body["enable_caching"].b();
            
            pythonWrapper->configure_performance(config);
            return true;
        });
    }

    // Package Management

    /**
     * @brief Install a Python package
     * @param req Request containing package_name
     */
    void installPackage(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "installPackage", [&](auto pythonWrapper) {
            bool success = pythonWrapper->install_package(std::string(body["package_name"].s()));
            
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 500;
            response["installed"] = success;
            res.write(response.dump());
            return success;
        });
    }

    /**
     * @brief Uninstall a Python package
     * @param req Request containing package_name
     */
    void uninstallPackage(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "uninstallPackage", [&](auto pythonWrapper) {
            bool success = pythonWrapper->uninstall_package(std::string(body["package_name"].s()));
            
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 500;
            response["uninstalled"] = success;
            res.write(response.dump());
            return success;
        });
    }

    // Virtual Environment Management

    /**
     * @brief Create a virtual environment
     * @param req Request containing env_name
     */
    void createVirtualEnvironment(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "createVirtualEnvironment", [&](auto pythonWrapper) {
            pythonWrapper->create_virtual_environment(std::string(body["env_name"].s()));
            return true;
        });
    }

    /**
     * @brief Activate a virtual environment
     * @param req Request containing env_name
     */
    void activateVirtualEnvironment(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "activateVirtualEnvironment", [&](auto pythonWrapper) {
            pythonWrapper->activate_virtual_environment(std::string(body["env_name"].s()));
            return true;
        });
    }

    // Debugging Support

    /**
     * @brief Enable or disable debug mode
     * @param req Request containing enable flag
     */
    void enableDebugMode(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "enableDebugMode", [&](auto pythonWrapper) {
            pythonWrapper->enable_debug_mode(body["enable"].b());
            return true;
        });
    }

    /**
     * @brief Set a breakpoint in a script
     * @param req Request containing alias and line_number
     */
    void setBreakpoint(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "setBreakpoint", [&](auto pythonWrapper) {
            pythonWrapper->set_breakpoint(
                std::string(body["alias"].s()),
                body["line_number"].i());
            return true;
        });
    }

    // Advanced Features

    /**
     * @brief Register a C++ function to be callable from Python
     * @param req Request containing function name and implementation details
     */
    void registerFunction(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "registerFunction", [&](auto pythonWrapper) {
            // Note: This is a simplified implementation
            // In practice, you'd need to handle function registration more carefully
            std::function<void()> dummy_func = []() { 
                spdlog::info("Registered function called"); 
            };
            pythonWrapper->register_function(std::string(body["name"].s()), dummy_func);
            return true;
        });
    }

    /**
     * @brief Set error handling strategy
     * @param req Request containing strategy type
     */
    void setErrorHandlingStrategy(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "setErrorHandlingStrategy", [&](auto pythonWrapper) {
            int strategy_int = body["strategy"].i();
            auto strategy = static_cast<lithium::PythonWrapper::ErrorHandlingStrategy>(strategy_int);
            pythonWrapper->set_error_handling_strategy(strategy);
            return true;
        });
    }
};

inline std::weak_ptr<lithium::PythonWrapper> PythonController::mPythonWrapper;

}  // namespace lithium::server::controller

#endif  // LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP
