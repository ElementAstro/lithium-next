/*
 * AsyncConfigController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_CONFIG_HPP
#define LITHIUM_SERVER_CONTROLLER_CONFIG_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"

class ConfigController : public Controller {
private:
    static std::weak_ptr<lithium::ConfigManager> mConfigManager;

    // Utility function to handle all configuration actions
    static auto handleConfigAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ConfigManager>)> func) {
        // Ensure the 'path' parameter is not empty (only for non-reloadConfig)
        if (command != "reloadConfig" &&
            (!body["path"] || body["path"].s().size() == 0)) {
            return crow::response(
                400, "The 'path' parameter is required and cannot be empty.");
        }

        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto configManager = mConfigManager.lock();
            if (!configManager) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ConfigManager instance is null.";
                LOG_F(ERROR,
                      "ConfigManager instance is null. Unable to proceed with "
                      "command: {}",
                      command);
                return crow::response(500, res);
            } else {
                bool success = func(configManager);
                if (success) {
                    res["status"] = "success";
                    res["code"] = 200;
                    if (command != "reloadConfig") {
                        res["path"] = body["path"].s();
                    }
                } else {
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] =
                        "Not Found: The specified path could not be found or "
                        "the operation failed.";
                }
            }
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
        // Create a weak pointer to the ConfigManager
        GET_OR_CREATE_WEAK_PTR(mConfigManager, lithium::ConfigManager,
                               Constants::CONFIG_MANAGER);
        // Define the routes
        CROW_ROUTE(app, "/config/get")
            .methods("POST"_method)(&ConfigController::getConfig);
        CROW_ROUTE(app, "/config/set")
            .methods("POST"_method)(&ConfigController::setConfig);
        CROW_ROUTE(app, "/config/delete")
            .methods("POST"_method)(&ConfigController::deleteConfig);
        CROW_ROUTE(app, "/config/load")
            .methods("POST"_method)(&ConfigController::loadConfig);
        CROW_ROUTE(app, "/config/reload")
            .methods("POST"_method)(&ConfigController::reloadConfig);
        CROW_ROUTE(app, "/config/save")
            .methods("POST"_method)(&ConfigController::saveConfig);
        CROW_ROUTE(app, "/config/append")
            .methods("POST"_method)(&ConfigController::appendConfig);
        CROW_ROUTE(app, "/config/has")
            .methods("POST"_method)(&ConfigController::hasConfig);
        CROW_ROUTE(app, "/config/keys")
            .methods("GET"_method)(&ConfigController::listConfigKeys);
        CROW_ROUTE(app, "/config/paths")
            .methods("GET"_method)(&ConfigController::listConfigPaths);
        CROW_ROUTE(app, "/config/tidy")
            .methods("POST"_method)(&ConfigController::tidyConfig);
        CROW_ROUTE(app, "/config/clear")
            .methods("POST"_method)(&ConfigController::clearConfig);
    }

    // Endpoint to get config from ConfigManager
    void getConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res =
            handleConfigAction(req, body, "getConfig", [&](auto configManager) {
                if (auto tmp = configManager->get(body["path"].s())) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["value"] = tmp.value().dump();
                    response["type"] = "string";
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to set config to ConfigManager
    void setConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        if (body["value"].s().size() == 0) {
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res =
            handleConfigAction(req, body, "setConfig", [&](auto configManager) {
                return configManager->set(body["path"].s(), body["value"].s());
            });
    }

    // Endpoint to delete config from ConfigManager
    void deleteConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleConfigAction(
            req, body, "deleteConfig", [&](auto configManager) {
                return configManager->remove(body["path"].s());
            });
    }

    // Endpoint to load config from file
    void loadConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleConfigAction(req, body, "loadConfig",
                                 [&](auto configManager) {
                                     return configManager->loadFromFile(
                                         std::string(body["path"].s()));
                                 });
    }

    // Endpoint to reload config from file
    void reloadConfig(const crow::request& req, crow::response& res) {
        res = handleConfigAction(
            req, crow::json::rvalue{}, "reloadConfig", [&](auto configManager) {
                return configManager->loadFromFile("config/config.json");
            });
    }

    // Endpoint to save config to file
    void saveConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleConfigAction(
            req, body, "saveConfig", [&](auto configManager) {
                return configManager->save(std::string(body["path"].s()));
            });
    }

    // Endpoint to append config to an array in ConfigManager
    void appendConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        if (body["value"].s().size() == 0) {
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(req, body, "appendConfig",
                                 [&](auto configManager) {
                                     return configManager->append(
                                         body["path"].s(), body["value"].s());
                                 });
    }

    // Endpoint to check if a config exists in ConfigManager
    void hasConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res =
            handleConfigAction(req, body, "hasConfig", [&](auto configManager) {
                return configManager->has(body["path"].s());
            });
    }

    // Endpoint to list all config keys in ConfigManager
    void listConfigKeys(const crow::request& req, crow::response& res) {
        res = handleConfigAction(req, crow::json::rvalue{}, "listConfigKeys",
                                 [&](auto configManager) {
                                     auto keys = configManager->getKeys();
                                     crow::json::wvalue response;
                                     response["status"] = "success";
                                     response["code"] = 200;
                                     response["keys"] = keys;
                                     res.write(response.dump());
                                     return true;
                                 });
    }

    // Endpoint to list all config paths in ConfigManager
    void listConfigPaths(const crow::request& req, crow::response& res) {
        res = handleConfigAction(req, crow::json::rvalue{}, "listConfigPaths",
                                 [&](auto configManager) {
                                     auto paths = configManager->listPaths();
                                     crow::json::wvalue response;
                                     response["status"] = "success";
                                     response["code"] = 200;
                                     response["paths"] = paths;
                                     res.write(response.dump());
                                     return true;
                                 });
    }

    // Endpoint to tidy config in ConfigManager
    void tidyConfig(const crow::request& req, crow::response& res) {
        res = handleConfigAction(req, crow::json::rvalue{}, "tidyConfig",
                                 [&](auto configManager) {
                                     configManager->tidy();
                                     return true;
                                 });
    }

    // Endpoint to clear config in ConfigManager
    void clearConfig(const crow::request& req, crow::response& res) {
        res = handleConfigAction(req, crow::json::rvalue{}, "clearConfig",
                                 [&](auto configManager) {
                                     configManager->clear();
                                     return true;
                                 });
    }

    // Endpoint to merge config in ConfigManager
    void mergeConfig(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        if (body["value"].s().size() == 0) {
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(req, body, "mergeConfig",
                                 [&](auto configManager) {
                                     configManager->merge(body["value"].s());
                                     return true;
                                 });
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_CONFIG_HPP