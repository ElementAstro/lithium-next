/*
 * PythonController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP
#define LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP

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
#include "script/python_caller.hpp"

class PythonController : public Controller {
private:
    static std::weak_ptr<lithium::PythonWrapper> mPythonWrapper;

    // Utility function to handle all Python actions
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
                res["error"] =
                    "Internal Server Error: PythonWrapper instance is null.";
                LOG_F(ERROR,
                      "PythonWrapper instance is null. Unable to proceed with "
                      "command: {}",
                      command);
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
        // Create a weak pointer to the PythonWrapper
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                               Constants::PYTHON_WRAPPER);
        // Define the routes
        CROW_ROUTE(app, "/python/load")
            .methods("POST"_method)(&PythonController::loadScript);
        CROW_ROUTE(app, "/python/unload")
            .methods("POST"_method)(&PythonController::unloadScript);
        CROW_ROUTE(app, "/python/reload")
            .methods("POST"_method)(&PythonController::reloadScript);
        CROW_ROUTE(app, "/python/call")
            .methods("POST"_method)(&PythonController::callFunction);
        CROW_ROUTE(app, "/python/getVariable")
            .methods("POST"_method)(&PythonController::getVariable);
        CROW_ROUTE(app, "/python/setVariable")
            .methods("POST"_method)(&PythonController::setVariable);
        CROW_ROUTE(app, "/python/functions")
            .methods("POST"_method)(&PythonController::getFunctionList);
        CROW_ROUTE(app, "/python/eval")
            .methods("POST"_method)(&PythonController::evalExpression);
        CROW_ROUTE(app, "/python/list")
            .methods("GET"_method)(&PythonController::listScripts);
    }

    // Endpoint to load a Python script
    void loadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "loadScript", [&](auto pythonWrapper) {
                pythonWrapper->load_script(std::string(body["script_name"].s()),
                                           std::string(body["alias"].s()));
                return true;
            });
    }

    // Endpoint to unload a Python script
    void unloadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "unloadScript", [&](auto pythonWrapper) {
                pythonWrapper->unload_script(std::string(body["alias"].s()));
                return true;
            });
    }

    // Endpoint to reload a Python script
    void reloadScript(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "reloadScript", [&](auto pythonWrapper) {
                pythonWrapper->reload_script(std::string(body["alias"].s()));
                return true;
            });
    }

    // Endpoint to call a function in a Python script
    void callFunction(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "callFunction", [&](auto pythonWrapper) {
                auto result = pythonWrapper->template call_function<py::object>(
                    std::string(body["alias"].s()),
                    std::string(body["function_name"].s()), body["args"]);
                if (!result.is_none()) {
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

    // Endpoint to get a variable from a Python script
    void getVariable(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "getVariable", [&](auto pythonWrapper) {
                auto result = pythonWrapper->template get_variable<py::object>(
                    std::string(body["alias"].s()),
                    std::string(body["variable_name"].s()));
                if (!result.is_none()) {
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

    // Endpoint to set a variable in a Python script
    void setVariable(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(req, body, "setVariable",
                                 [&](auto pythonWrapper) {
                                     pythonWrapper->set_variable(
                                         std::string(body["alias"].s()),
                                         std::string(body["variable_name"].s()),
                                         py::str(body["value"].s()));
                                     return true;
                                 });
    }

    // Endpoint to get the list of functions in a Python script
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

    // Endpoint to evaluate an expression in a Python script
    void evalExpression(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handlePythonAction(
            req, body, "evalExpression", [&](auto pythonWrapper) {
                auto result = pythonWrapper->eval_expression(
                    std::string(body["alias"].s()),
                    std::string(body["expression"].s()));
                if (!result.is_none()) {
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

    // Endpoint to list all loaded Python scripts
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
};

#endif  // LITHIUM_ASYNC_PYTHON_CONTROLLER_HPP