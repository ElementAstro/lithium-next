/*
 * AsyncConfigController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#pragma once

#include <spdlog/spdlog.h>
#include <functional>
#include <memory>
#include <string>
#include "atom/function/global_ptr.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "controller.hpp"
#include "utils/format.hpp"


class ConfigController : public Controller {
private:
    static std::weak_ptr<lithium::ConfigManager> mConfigManager;

    static auto handleConfigAction(
        const crow::json::rvalue& body, const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ConfigManager>)> func) {
        spdlog::info("Handling config action: {}", command);

        if (command != "reloadConfig" &&
            (!body["path"] || std::string(body["path"].s()).empty())) {
            spdlog::warn(
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
                spdlog::error("ConfigManager instance is null. Command: {}",
                              command);
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ConfigManager instance is null.";
                return crow::response(500, res);
            } else {
                spdlog::info("Executing function for command: {}", command);
                bool success = func(configManager);
                if (success) {
                    spdlog::info("Command {} executed successfully.", command);
                    res["status"] = "success";
                    res["code"] = 200;
                    if (command != "reloadConfig") {
                        res["path"] = std::string(body["path"].s());
                    }
                } else {
                    spdlog::warn("Command {} failed to execute.", command);
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] =
                        "Not Found: The specified path could not be found or "
                        "the operation failed.";
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception occurred while executing command {}: {}",
                          command, e.what());
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
        }

        spdlog::info("Config action {} completed.", command);
        return crow::response(200, res);
    }

public:
    void registerRoutes(crow::SimpleApp& app) override {
        spdlog::info("Registering config routes.");
        GET_OR_CREATE_WEAK_PTR(mConfigManager, lithium::ConfigManager,
                               Constants::CONFIG_MANAGER);

        CROW_ROUTE(app, "/config/get")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getConfig(req, res);
                });
        CROW_ROUTE(app, "/config/set")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setConfig(req, res);
                });
        CROW_ROUTE(app, "/config/delete")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->deleteConfig(req, res);
                });
        CROW_ROUTE(app, "/config/load")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->loadConfig(req, res);
                });
        CROW_ROUTE(app, "/config/reload")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->reloadConfig(req, res);
                });
        CROW_ROUTE(app, "/config/save")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->saveConfig(req, res);
                });
        CROW_ROUTE(app, "/config/append")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->appendConfig(req, res);
                });
        CROW_ROUTE(app, "/config/has")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->hasConfig(req, res);
                });
        CROW_ROUTE(app, "/config/keys")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->listConfigKeys(req, res);
                });
        CROW_ROUTE(app, "/config/paths")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->listConfigPaths(req, res);
                });
        CROW_ROUTE(app, "/config/tidy")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->tidyConfig(req, res);
                });
        CROW_ROUTE(app, "/config/clear")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->clearConfig(req, res);
                });
        spdlog::info("Config routes registered successfully.");
    }

    void getConfig(const crow::request& req, crow::response& res) {
        spdlog::info("getConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("getConfig request body: {}", req.body);
        std::string path = body["path"].s();
        res = handleConfigAction(body, "getConfig", [&](auto configManager) {
            spdlog::info("Retrieving config for path: {}", path);
            if (auto tmp = configManager->get(path)) {
                spdlog::info("Config retrieved successfully for path: {}",
                             path);
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["value"] = tmp.value().dump();
                response["type"] = "string";
                res.write(response.dump());
                return true;
            }
            spdlog::warn("Config not found for path: {}", path);
            return false;
        });
        spdlog::info("getConfig completed.");
    }

    void setConfig(const crow::request& req, crow::response& res) {
        spdlog::info("setConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("setConfig request body: {}", req.body);
        std::string path = body["path"].s();
        std::string value = body["value"].s();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in setConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(body, "setConfig", [&](auto configManager) {
            spdlog::info("Setting config for path: {} with value: {}", path,
                         value);
            bool result = configManager->set(path, value);
            if (result) {
                spdlog::info("Config set successfully for path: {}", path);
            } else {
                spdlog::warn("Failed to set config for path: {}", path);
            }
            return result;
        });
        spdlog::info("setConfig completed.");
    }

    void deleteConfig(const crow::request& req, crow::response& res) {
        spdlog::info("deleteConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("deleteConfig request body: {}", req.body);
        std::string path = body["path"].s();
        res = handleConfigAction(body, "deleteConfig", [&](auto configManager) {
            spdlog::info("Deleting config for path: {}", path);
            bool result = configManager->remove(path);
            if (result) {
                spdlog::info("Config deleted successfully for path: {}", path);
            } else {
                spdlog::warn("Failed to delete config for path: {}", path);
            }
            return result;
        });
        spdlog::info("deleteConfig completed.");
    }

    void loadConfig(const crow::request& req, crow::response& res) {
        spdlog::info("loadConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("loadConfig request body: {}", req.body);
        std::string path = body["path"].s();
        res = handleConfigAction(body, "loadConfig", [&](auto configManager) {
            spdlog::info("Loading config from file: {}", path);
            bool result = configManager->loadFromFile(path);
            if (result) {
                spdlog::info("Config loaded successfully from file: {}", path);
            } else {
                spdlog::warn("Failed to load config from file: {}", path);
            }
            return result;
        });
        spdlog::info("loadConfig completed.");
    }

    void reloadConfig([[maybe_unused]] const crow::request& req,
                      crow::response& res) {
        spdlog::info("reloadConfig called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "reloadConfig", [&](auto configManager) {
                spdlog::info("Reloading config from default file.");
                bool result = configManager->loadFromFile("config/config.json");
                if (result) {
                    spdlog::info(
                        "Config reloaded successfully from default file.");
                } else {
                    spdlog::warn("Failed to reload config from default file.");
                }
                return result;
            });
        spdlog::info("reloadConfig completed.");
    }

    void saveConfig(const crow::request& req, crow::response& res) {
        spdlog::info("saveConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("saveConfig request body: {}", req.body);
        std::string path = body["path"].s();
        res = handleConfigAction(body, "saveConfig", [&](auto configManager) {
            spdlog::info("Saving config to file: {}", path);
            bool result = configManager->save(path);
            if (result) {
                spdlog::info("Config saved successfully to file: {}", path);
            } else {
                spdlog::warn("Failed to save config to file: {}", path);
            }
            return result;
        });
        spdlog::info("saveConfig completed.");
    }

    void appendConfig(const crow::request& req, crow::response& res) {
        spdlog::info("appendConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("appendConfig request body: {}", req.body);
        std::string path = body["path"].s();
        std::string value = body["value"].s();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in appendConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        res = handleConfigAction(body, "appendConfig", [&](auto configManager) {
            spdlog::info("Appending config to path: {} with value: {}", path,
                         value);
            bool result = configManager->append(path, value);
            if (result) {
                spdlog::info("Config appended successfully to path: {}", path);
            } else {
                spdlog::warn("Failed to append config to path: {}", path);
            }
            return result;
        });
        spdlog::info("appendConfig completed.");
    }

    void hasConfig(const crow::request& req, crow::response& res) {
        spdlog::info("hasConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("hasConfig request body: {}", req.body);
        std::string path = body["path"].s();
        res = handleConfigAction(body, "hasConfig", [&](auto configManager) {
            spdlog::info("Checking existence of config at path: {}", path);
            bool exists = configManager->has(path);
            spdlog::info("Config at path {} exists: {}", path,
                         exists ? "true" : "false");
            return exists;
        });
        spdlog::info("hasConfig completed.");
    }

    void listConfigKeys([[maybe_unused]] const crow::request& req,
                        crow::response& res) {
        spdlog::info("listConfigKeys called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "listConfigKeys", [&](auto configManager) {
                spdlog::info("Listing all config keys.");
                auto keys = configManager->getKeys();
                spdlog::info("Retrieved {} config keys.", keys.size());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["keys"] = keys;
                res.write(response.dump());
                return true;
            });
        spdlog::info("listConfigKeys completed.");
    }

    void listConfigPaths([[maybe_unused]] const crow::request& req,
                         crow::response& res) {
        spdlog::info("listConfigPaths called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "listConfigPaths", [&](auto configManager) {
                spdlog::info("Listing all config paths.");
                auto paths = configManager->listPaths();
                spdlog::info("Retrieved {} config paths.", paths.size());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["paths"] = paths;
                res.write(response.dump());
                return true;
            });
        spdlog::info("listConfigPaths completed.");
    }

    void tidyConfig([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        spdlog::info("tidyConfig called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "tidyConfig", [&](auto configManager) {
                spdlog::info("Tidying config.");
                configManager->tidy();
                spdlog::info("Config tidied successfully.");
                return true;
            });
        spdlog::info("tidyConfig completed.");
    }

    void clearConfig([[maybe_unused]] const crow::request& req,
                     crow::response& res) {
        spdlog::info("clearConfig called.");
        res = handleConfigAction(
            crow::json::rvalue{}, "clearConfig", [&](auto configManager) {
                spdlog::info("Clearing all config.");
                configManager->clear();
                spdlog::info("All config cleared successfully.");
                return true;
            });
        spdlog::info("clearConfig completed.");
    }

    void mergeConfig(const crow::request& req, crow::response& res) {
        spdlog::info("mergeConfig called.");
        auto body = crow::json::load(req.body);
        spdlog::info("mergeConfig request body: {}", req.body);
        std::string value = body["value"].s();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in mergeConfig.");
            res.code = 400;
            res.write("Missing Parameters");
            return;
        }

        spdlog::info("Merging config with value: {}", value);
        res = handleConfigAction(body, "mergeConfig", [&](auto configManager) {
            configManager->merge(value);
            spdlog::info("Config merged successfully.");
            return true;
        });
        spdlog::info("mergeConfig completed.");
    }
};

inline std::weak_ptr<lithium::ConfigManager> ConfigController::mConfigManager;
