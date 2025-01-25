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
#include "utils/format.hpp"

class ConfigController : public Controller {
private:
    static std::weak_ptr<lithium::ConfigManager> mConfigManager;

    // Utility function to handle all configuration actions
    static auto handleConfigAction(
        const crow::json::rvalue& body, const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ConfigManager>)> func) {
        LOG_F(INFO, "Handling config action: {}", command);

        // Ensure the 'path' parameter is not empty (only for non-reloadConfig)
        if (command != "reloadConfig" &&
            (!body["path"] || body["path"].s().size() == 0)) {
            LOG_F(WARNING,
                  "The 'path' parameter is missing or empty for command: {}",
                  command);
            return crow::response(
                400, "The 'path' parameter is required and cannot be empty.");
        }

        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto configManager = mConfigManager.lock();
            if (!configManager) {
                LOG_F(ERROR, "ConfigManager instance is null. Command: {}",
                      command);
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ConfigManager instance is null.";
                return crow::response(500, res);
            } else {
                LOG_F(INFO, "Executing function for command: {}", command);
                bool success = func(configManager);
                if (success) {
                    LOG_F(INFO, "Command {} executed successfully.", command);
                    res["status"] = "success";
                    res["code"] = 200;
                    if (command != "reloadConfig") {
                        res["path"] = body["path"].s();
                    }
                } else {
                    LOG_F(WARNING, "Command {} failed to execute.", command);
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] =
                        "Not Found: The specified path could not be found or "
                        "the operation failed.";
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception occurred while executing command {}: {}",
                  command, e.what());
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
        }

        LOG_F(INFO, "Config action {} completed.", command);
        return crow::response(200, res);
    }

public:
    void registerRoutes(crow::SimpleApp& app) override {
        LOG_F(INFO, "Registering config routes.");
        // Create a weak pointer to the ConfigManager
        GET_OR_CREATE_WEAK_PTR(mConfigManager, lithium::ConfigManager,
                               Constants::CONFIG_MANAGER);
        // Define the routes
        CROW_ROUTE(app, "/config/get")
            .methods("POST"_method)(&ConfigController::getConfig, this);
        CROW_ROUTE(app, "/config/set")
            .methods("POST"_method)(&ConfigController::setConfig, this);
        CROW_ROUTE(app, "/config/delete")
            .methods("POST"_method)(&ConfigController::deleteConfig, this);
        CROW_ROUTE(app, "/config/load")
            .methods("POST"_method)(&ConfigController::loadConfig, this);
        CROW_ROUTE(app, "/config/reload")
            .methods("POST"_method)(&ConfigController::reloadConfig, this);
        CROW_ROUTE(app, "/config/save")
            .methods("POST"_method)(&ConfigController::saveConfig, this);
        CROW_ROUTE(app, "/config/append")
            .methods("POST"_method)(&ConfigController::appendConfig, this);
        CROW_ROUTE(app, "/config/has")
            .methods("POST"_method)(&ConfigController::hasConfig, this);
        CROW_ROUTE(app, "/config/keys")
            .methods("GET"_method)(&ConfigController::listConfigKeys, this);
        CROW_ROUTE(app, "/config/paths")
            .methods("GET"_method)(&ConfigController::listConfigPaths, this);
        CROW_ROUTE(app, "/config/tidy")
            .methods("POST"_method)(&ConfigController::tidyConfig, this);
        CROW_ROUTE(app, "/config/clear")
            .methods("POST"_method)(&ConfigController::clearConfig, this);
        LOG_F(INFO, "Config routes registered successfully.");
    }

    // Endpoint to get config from ConfigManager
    void getConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "getConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "getConfig request body: {}", req.body);
        res = handleConfigAction(body, "getConfig", [&](auto configManager) {
            LOG_F(INFO, "Retrieving config for path: {}", body["path"].s());
            if (auto tmp = configManager->get(body["path"].s())) {
                LOG_F(INFO, "Config retrieved successfully for path: {}",
                      body["path"].s());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["value"] = tmp.value().dump();
                response["type"] = "string";
                res.write(response.dump());
                return true;
            }
            LOG_F(WARNING, "Config not found for path: {}", body["path"].s());
            return false;
        });
        LOG_F(INFO, "getConfig completed.");
    }

    // Endpoint to set config to ConfigManager
    void setConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "setConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "setConfig request body: {}", req.body);
        if (body["value"].s().size() == 0) {
            LOG_F(WARNING, "Missing 'value' parameter in setConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(body, "setConfig", [&](auto configManager) {
            LOG_F(INFO, "Setting config for path: {} with value: {}",
                  body["path"].s(), body["value"].s());
            bool result =
                configManager->set(body["path"].s(), body["value"].s());
            if (result) {
                LOG_F(INFO, "Config set successfully for path: {}",
                      body["path"].s());
            } else {
                LOG_F(WARNING, "Failed to set config for path: {}",
                      body["path"].s());
            }
            return result;
        });
        LOG_F(INFO, "setConfig completed.");
    }

    // Endpoint to delete config from ConfigManager
    void deleteConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "deleteConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "deleteConfig request body: {}", req.body);
        res = handleConfigAction(body, "deleteConfig", [&](auto configManager) {
            LOG_F(INFO, "Deleting config for path: {}", body["path"].s());
            bool result = configManager->remove(body["path"].s());
            if (result) {
                LOG_F(INFO, "Config deleted successfully for path: {}",
                      body["path"].s());
            } else {
                LOG_F(WARNING, "Failed to delete config for path: {}",
                      body["path"].s());
            }
            return result;
        });
        LOG_F(INFO, "deleteConfig completed.");
    }

    // Endpoint to load config from file
    void loadConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "loadConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "loadConfig request body: {}", req.body);
        res = handleConfigAction(body, "loadConfig", [&](auto configManager) {
            LOG_F(INFO, "Loading config from file: {}", body["path"].s());
            bool result =
                configManager->loadFromFile(std::string(body["path"].s()));
            if (result) {
                LOG_F(INFO, "Config loaded successfully from file: {}",
                      body["path"].s());
            } else {
                LOG_F(WARNING, "Failed to load config from file: {}",
                      body["path"].s());
            }
            return result;
        });
        LOG_F(INFO, "loadConfig completed.");
    }

    // Endpoint to reload config from file
    void reloadConfig([[maybe_unused]] const crow::request& req,
                      crow::response& res) {
        LOG_F(INFO, "reloadConfig called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "reloadConfig", [&](auto configManager) {
                LOG_F(INFO, "Reloading config from default file.");
                bool result = configManager->loadFromFile("config/config.json");
                if (result) {
                    LOG_F(INFO,
                          "Config reloaded successfully from default file.");
                } else {
                    LOG_F(WARNING,
                          "Failed to reload config from default file.");
                }
                return result;
            });
        LOG_F(INFO, "reloadConfig completed.");
    }

    // Endpoint to save config to file
    void saveConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "saveConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "saveConfig request body: {}", req.body);
        res = handleConfigAction(body, "saveConfig", [&](auto configManager) {
            LOG_F(INFO, "Saving config to file: {}", body["path"].s());
            bool result = configManager->save(std::string(body["path"].s()));
            if (result) {
                LOG_F(INFO, "Config saved successfully to file: {}",
                      body["path"].s());
            } else {
                LOG_F(WARNING, "Failed to save config to file: {}",
                      body["path"].s());
            }
            return result;
        });
        LOG_F(INFO, "saveConfig completed.");
    }

    // Endpoint to append config to an array in ConfigManager
    void appendConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "appendConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "appendConfig request body: {}", req.body);
        if (body["value"].s().size() == 0) {
            LOG_F(WARNING, "Missing 'value' parameter in appendConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(body, "appendConfig", [&](auto configManager) {
            LOG_F(INFO, "Appending config to path: {} with value: {}",
                  body["path"].s(), body["value"].s());
            bool result =
                configManager->append(body["path"].s(), body["value"].s());
            if (result) {
                LOG_F(INFO, "Config appended successfully to path: {}",
                      body["path"].s());
            } else {
                LOG_F(WARNING, "Failed to append config to path: {}",
                      body["path"].s());
            }
            return result;
        });
        LOG_F(INFO, "appendConfig completed.");
    }

    // Endpoint to check if a config exists in ConfigManager
    void hasConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "hasConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "hasConfig request body: {}", req.body);
        res = handleConfigAction(body, "hasConfig", [&](auto configManager) {
            LOG_F(INFO, "Checking existence of config at path: {}",
                  body["path"].s());
            bool exists = configManager->has(body["path"].s());
            LOG_F(INFO, "Config at path {} exists: {}", body["path"].s(),
                  exists ? "true" : "false");
            return exists;
        });
        LOG_F(INFO, "hasConfig completed.");
    }

    // Endpoint to list all config keys in ConfigManager
    void listConfigKeys([[maybe_unused]] const crow::request& req,
                        crow::response& res) {
        LOG_F(INFO, "listConfigKeys called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "listConfigKeys", [&](auto configManager) {
                LOG_F(INFO, "Listing all config keys.");
                auto keys = configManager->getKeys();
                LOG_F(INFO, "Retrieved %zu config keys.", keys.size());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["keys"] = keys;
                res.write(response.dump());
                return true;
            });
        LOG_F(INFO, "listConfigKeys completed.");
    }

    // Endpoint to list all config paths in ConfigManager
    void listConfigPaths([[maybe_unused]] const crow::request& req,
                         crow::response& res) {
        LOG_F(INFO, "listConfigPaths called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "listConfigPaths", [&](auto configManager) {
                LOG_F(INFO, "Listing all config paths.");
                auto paths = configManager->listPaths();
                LOG_F(INFO, "Retrieved %zu config paths.", paths.size());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["paths"] = paths;
                res.write(response.dump());
                return true;
            });
        LOG_F(INFO, "listConfigPaths completed.");
    }

    // Endpoint to tidy config in ConfigManager
    void tidyConfig([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        LOG_F(INFO, "tidyConfig called.");
        res = handleConfigAction(crow::json::rvalue{}, "tidyConfig",
                                 [&](auto configManager) {
                                     LOG_F(INFO, "Tidying config.");
                                     configManager->tidy();
                                     LOG_F(INFO, "Config tidied successfully.");
                                     return true;
                                 });
        LOG_F(INFO, "tidyConfig completed.");
    }

    // Endpoint to clear config in ConfigManager
    void clearConfig([[maybe_unused]] const crow::request& req,
                     crow::response& res) {
        LOG_F(INFO, "clearConfig called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "clearConfig", [&](auto configManager) {
                LOG_F(INFO, "Clearing all config.");
                configManager->clear();
                LOG_F(INFO, "All config cleared successfully.");
                return true;
            });
        LOG_F(INFO, "clearConfig completed.");
    }

    // Endpoint to merge config in ConfigManager
    void mergeConfig(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "mergeConfig called.");
        auto body = crow::json::load(req.body);
        LOG_F(INFO, "mergeConfig request body: {}", req.body);
        if (body["value"].s().size() == 0) {
            LOG_F(WARNING, "Missing 'value' parameter in mergeConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        LOG_F(INFO, "Merging config with value: {}", body["value"].s());
        res = handleConfigAction(body, "mergeConfig", [&](auto configManager) {
            configManager->merge(body["value"].s());
            LOG_F(INFO, "Config merged successfully.");
            return true;
        });
        LOG_F(INFO, "mergeConfig completed.");
    }
};

inline std::weak_ptr<lithium::ConfigManager> ConfigController::mConfigManager;

#endif  // LITHIUM_SERVER_CONTROLLER_CONFIG_HPP
