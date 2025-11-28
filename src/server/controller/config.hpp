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
#include "config/config.hpp"
#include "constant/constant.hpp"
#include "controller.hpp"
#include "utils/format.hpp"
#include "../command/config_ws.hpp"


class ConfigController : public Controller {
private:
    static std::weak_ptr<lithium::ConfigManager> mConfigManager;
    static std::unique_ptr<lithium::server::config::ConfigWebSocketService> mConfigWsService;

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
    void registerRoutes(lithium::server::ServerApp& app) override {
        spdlog::info("Registering config routes.");
        GET_OR_CREATE_WEAK_PTR(mConfigManager, lithium::ConfigManager,
                               Constants::CONFIG_MANAGER);

        // Initialize WebSocket service for real-time config notifications
        if (!mConfigWsService) {
            lithium::server::config::ConfigWebSocketService::Config wsConfig;
            wsConfig.enableBroadcast = true;
            wsConfig.enableFiltering = true;
            wsConfig.maxClients = 100;
            wsConfig.includeTimestamp = true;
            mConfigWsService = std::make_unique<lithium::server::config::ConfigWebSocketService>(
                app, wsConfig);
            mConfigWsService->start();
            spdlog::info("Config WebSocket service started");
        }

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
        CROW_ROUTE(app, "/config/merge")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->mergeConfig(req, res);
                });

        // ====================================================================
        // Validation Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/validate")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->validateConfig(req, res);
                });
        CROW_ROUTE(app, "/config/validate/all")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->validateAllConfig(req, res);
                });
        CROW_ROUTE(app, "/config/schema/set")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->setSchema(req, res);
                });
        CROW_ROUTE(app, "/config/schema/load")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->loadSchema(req, res);
                });

        // ====================================================================
        // File Watching Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/watch/enable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->enableAutoReload(req, res);
                });
        CROW_ROUTE(app, "/config/watch/disable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->disableAutoReload(req, res);
                });
        CROW_ROUTE(app, "/config/watch/status")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getWatchStatus(req, res);
                });

        // ====================================================================
        // Metrics and Statistics Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/metrics")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getMetrics(req, res);
                });
        CROW_ROUTE(app, "/config/metrics/reset")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->resetMetrics(req, res);
                });
        CROW_ROUTE(app, "/config/cache/stats")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getCacheStats(req, res);
                });

        // ====================================================================
        // Snapshot Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/snapshot/create")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->createSnapshot(req, res);
                });
        CROW_ROUTE(app, "/config/snapshot/restore")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->restoreSnapshot(req, res);
                });
        CROW_ROUTE(app, "/config/snapshot/list")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->listSnapshots(req, res);
                });
        CROW_ROUTE(app, "/config/snapshot/delete")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->deleteSnapshot(req, res);
                });

        // ====================================================================
        // Import/Export Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/export")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->exportConfig(req, res);
                });
        CROW_ROUTE(app, "/config/import")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->importConfig(req, res);
                });
        CROW_ROUTE(app, "/config/diff")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->diffConfig(req, res);
                });
        CROW_ROUTE(app, "/config/patch")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->applyPatch(req, res);
                });
        CROW_ROUTE(app, "/config/flatten")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->flattenConfig(req, res);
                });

        // ====================================================================
        // Batch Operations Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/batch/get")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->batchGetConfig(req, res);
                });
        CROW_ROUTE(app, "/config/batch/set")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->batchSetConfig(req, res);
                });

        // ====================================================================
        // WebSocket Service Routes
        // ====================================================================
        CROW_ROUTE(app, "/config/ws/stats")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getWsStats(req, res);
                });
        CROW_ROUTE(app, "/config/ws/broadcast")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->broadcastConfigNotification(req, res);
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
            configManager->merge(nlohmann::json::parse(value));
            spdlog::info("Config merged successfully.");
            return true;
        });
        spdlog::info("mergeConfig completed.");
    }

    // ========================================================================
    // Validation Methods
    // ========================================================================

    void validateConfig(const crow::request& req, crow::response& res) {
        spdlog::info("validateConfig called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto result = configManager->validate(path);
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["valid"] = result.isValid;
            response["path"] = path;

            crow::json::wvalue::list errorList;
            for (const auto& err : result.errors) {
                errorList.push_back(err);
            }
            response["errors"] = std::move(errorList);

            crow::json::wvalue::list warningList;
            for (const auto& warn : result.warnings) {
                warningList.push_back(warn);
            }
            response["warnings"] = std::move(warningList);

            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void validateAllConfig([[maybe_unused]] const crow::request& req,
                           crow::response& res) {
        spdlog::info("validateAllConfig called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto result = configManager->validateAll();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["valid"] = result.isValid;

            crow::json::wvalue::list errorList;
            for (const auto& err : result.errors) {
                errorList.push_back(err);
            }
            response["errors"] = std::move(errorList);

            crow::json::wvalue::list warningList;
            for (const auto& warn : result.warnings) {
                warningList.push_back(warn);
            }
            response["warnings"] = std::move(warningList);

            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void setSchema(const crow::request& req, crow::response& res) {
        spdlog::info("setSchema called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();
        std::string schema = body["schema"].s();

        if (path.empty() || schema.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing path or schema parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->setSchema(path, nlohmann::json::parse(schema));
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            response["path"] = path;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void loadSchema(const crow::request& req, crow::response& res) {
        spdlog::info("loadSchema called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();
        std::string filePath = body["file_path"].s();

        if (path.empty() || filePath.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing path or file_path parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->loadSchema(path, filePath);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            response["path"] = path;
            response["file_path"] = filePath;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // File Watching Methods
    // ========================================================================

    void enableAutoReload(const crow::request& req, crow::response& res) {
        spdlog::info("enableAutoReload called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();

        if (path.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing path parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->enableAutoReload(path);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            response["path"] = path;
            response["watching"] = success;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void disableAutoReload(const crow::request& req, crow::response& res) {
        spdlog::info("disableAutoReload called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();

        if (path.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing path parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->disableAutoReload(path);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            response["path"] = path;
            response["watching"] = false;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void getWatchStatus(const crow::request& req, crow::response& res) {
        spdlog::info("getWatchStatus called.");
        auto body = crow::json::load(req.body);
        std::string path = body["path"].s();

        if (path.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing path parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool watching = configManager->isAutoReloadEnabled(path);
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["path"] = path;
            response["watching"] = watching;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // Metrics Methods
    // ========================================================================

    void getMetrics([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        spdlog::info("getMetrics called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto metrics = configManager->getMetrics();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["metrics"]["total_operations"] = metrics.total_operations;
            response["metrics"]["cache_hits"] = metrics.cache_hits;
            response["metrics"]["cache_misses"] = metrics.cache_misses;
            response["metrics"]["validation_successes"] = metrics.validation_successes;
            response["metrics"]["validation_failures"] = metrics.validation_failures;
            response["metrics"]["files_loaded"] = metrics.files_loaded;
            response["metrics"]["files_saved"] = metrics.files_saved;
            response["metrics"]["auto_reloads"] = metrics.auto_reloads;
            response["metrics"]["average_access_time_ms"] = metrics.average_access_time_ms;
            response["metrics"]["average_save_time_ms"] = metrics.average_save_time_ms;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void resetMetrics([[maybe_unused]] const crow::request& req,
                      crow::response& res) {
        spdlog::info("resetMetrics called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            configManager->resetMetrics();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["message"] = "Metrics reset successfully";
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void getCacheStats([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("getCacheStats called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto stats = configManager->getCache().getStatistics();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["cache"]["hits"] = stats.hits.load();
            response["cache"]["misses"] = stats.misses.load();
            response["cache"]["evictions"] = stats.evictions.load();
            response["cache"]["expirations"] = stats.expirations.load();
            response["cache"]["current_size"] = stats.currentSize.load();
            response["cache"]["hit_ratio"] = stats.getHitRatio();
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // Snapshot Methods
    // ========================================================================

    void createSnapshot([[maybe_unused]] const crow::request& req,
                        crow::response& res) {
        spdlog::info("createSnapshot called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            std::string snapshotId = configManager->createSnapshot();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["snapshot_id"] = snapshotId;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void restoreSnapshot(const crow::request& req, crow::response& res) {
        spdlog::info("restoreSnapshot called.");
        auto body = crow::json::load(req.body);
        std::string snapshotId = body["snapshot_id"].s();

        if (snapshotId.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing snapshot_id parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->restoreSnapshot(snapshotId);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 404;
            response["snapshot_id"] = snapshotId;
            if (!success) {
                response["error"] = "Snapshot not found";
            }
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void listSnapshots([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("listSnapshots called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto snapshots = configManager->listSnapshots();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;

            crow::json::wvalue::list snapshotList;
            for (const auto& id : snapshots) {
                snapshotList.push_back(id);
            }
            response["snapshots"] = std::move(snapshotList);
            response["count"] = snapshots.size();
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void deleteSnapshot(const crow::request& req, crow::response& res) {
        spdlog::info("deleteSnapshot called.");
        auto body = crow::json::load(req.body);
        std::string snapshotId = body["snapshot_id"].s();

        if (snapshotId.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing snapshot_id parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->deleteSnapshot(snapshotId);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 404;
            response["snapshot_id"] = snapshotId;
            if (!success) {
                response["error"] = "Snapshot not found";
            }
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // Import/Export Methods
    // ========================================================================

    void exportConfig(const crow::request& req, crow::response& res) {
        spdlog::info("exportConfig called.");
        auto body = crow::json::load(req.body);
        std::string formatStr = body["format"] ? body["format"].s() : "json";

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            lithium::config::SerializationFormat format =
                lithium::config::SerializationFormat::PRETTY_JSON;
            if (formatStr == "compact") {
                format = lithium::config::SerializationFormat::COMPACT_JSON;
            } else if (formatStr == "json5") {
                format = lithium::config::SerializationFormat::JSON5;
            }

            std::string exported = configManager->exportAs(format);
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["format"] = formatStr;
            response["data"] = exported;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void importConfig(const crow::request& req, crow::response& res) {
        spdlog::info("importConfig called.");
        auto body = crow::json::load(req.body);
        std::string data = body["data"].s();
        std::string formatStr = body["format"] ? body["format"].s() : "json";

        if (data.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing data parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            lithium::config::SerializationFormat format =
                lithium::config::SerializationFormat::JSON;
            if (formatStr == "json5") {
                format = lithium::config::SerializationFormat::JSON5;
            }

            bool success = configManager->importFrom(data, format);
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            response["format"] = formatStr;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void diffConfig(const crow::request& req, crow::response& res) {
        spdlog::info("diffConfig called.");
        auto body = crow::json::load(req.body);
        std::string otherConfig = body["config"].s();

        if (otherConfig.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing config parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto diffResult = configManager->diff(nlohmann::json::parse(otherConfig));
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["diff"] = diffResult.dump();
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void applyPatch(const crow::request& req, crow::response& res) {
        spdlog::info("applyPatch called.");
        auto body = crow::json::load(req.body);
        std::string patch = body["patch"].s();

        if (patch.empty()) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing patch parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            bool success = configManager->applyPatch(nlohmann::json::parse(patch));
            crow::json::wvalue response;
            response["status"] = success ? "success" : "error";
            response["code"] = success ? 200 : 400;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void flattenConfig([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("flattenConfig called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            auto flattened = configManager->flatten();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;

            crow::json::wvalue flatData;
            for (const auto& [key, value] : flattened) {
                flatData[key] = value.dump();
            }
            response["data"] = std::move(flatData);
            response["count"] = flattened.size();
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // Batch Operations Methods
    // ========================================================================

    void batchGetConfig(const crow::request& req, crow::response& res) {
        spdlog::info("batchGetConfig called.");
        auto body = crow::json::load(req.body);

        if (!body["paths"]) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing paths parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;

            crow::json::wvalue results;
            for (size_t i = 0; i < body["paths"].size(); ++i) {
                std::string path = body["paths"][i].s();
                auto value = configManager->get(path);
                if (value) {
                    results[path] = value->dump();
                } else {
                    results[path] = nullptr;
                }
            }
            response["results"] = std::move(results);
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void batchSetConfig(const crow::request& req, crow::response& res) {
        spdlog::info("batchSetConfig called.");
        auto body = crow::json::load(req.body);

        if (!body["items"]) {
            res.code = 400;
            res.write(R"({"status":"error","error":"Missing items parameter"})");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res.code = 500;
            res.write(R"({"status":"error","error":"ConfigManager not available"})");
            return;
        }

        try {
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;

            size_t successCount = 0;
            size_t failCount = 0;
            crow::json::wvalue results;

            for (size_t i = 0; i < body["items"].size(); ++i) {
                std::string path = body["items"][i]["path"].s();
                std::string value = body["items"][i]["value"].s();

                bool success = configManager->set(path, nlohmann::json::parse(value));
                results[path] = success;
                if (success) {
                    ++successCount;
                } else {
                    ++failCount;
                }
            }

            response["results"] = std::move(results);
            response["success_count"] = successCount;
            response["fail_count"] = failCount;
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    // ========================================================================
    // WebSocket Service Methods
    // ========================================================================

    void getWsStats([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        spdlog::info("getWsStats called.");

        if (!mConfigWsService) {
            res.code = 503;
            res.write(R"({"status":"error","error":"WebSocket service not available"})");
            return;
        }

        try {
            auto stats = mConfigWsService->getStatistics();
            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["websocket"] = crow::json::load(stats.dump());
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }

    void broadcastConfigNotification(const crow::request& req,
                                     crow::response& res) {
        spdlog::info("broadcastConfigNotification called.");
        auto body = crow::json::load(req.body);

        if (!mConfigWsService) {
            res.code = 503;
            res.write(R"({"status":"error","error":"WebSocket service not available"})");
            return;
        }

        std::string type = body["type"] ? body["type"].s() : "value_changed";
        std::string path = body["path"] ? body["path"].s() : "";
        std::string message = body["message"] ? body["message"].s() : "";

        try {
            lithium::server::config::ConfigWebSocketService::NotificationType notifType =
                lithium::server::config::ConfigWebSocketService::NotificationType::VALUE_CHANGED;

            // Map string type to enum
            if (type == "value_removed") {
                notifType = lithium::server::config::ConfigWebSocketService::NotificationType::VALUE_REMOVED;
            } else if (type == "file_loaded") {
                notifType = lithium::server::config::ConfigWebSocketService::NotificationType::FILE_LOADED;
            } else if (type == "file_saved") {
                notifType = lithium::server::config::ConfigWebSocketService::NotificationType::FILE_SAVED;
            } else if (type == "config_cleared") {
                notifType = lithium::server::config::ConfigWebSocketService::NotificationType::CONFIG_CLEARED;
            } else if (type == "config_merged") {
                notifType = lithium::server::config::ConfigWebSocketService::NotificationType::CONFIG_MERGED;
            }

            nlohmann::json data;
            if (!message.empty()) {
                data["message"] = message;
            }

            mConfigWsService->broadcastNotification(notifType, path, data);

            crow::json::wvalue response;
            response["status"] = "success";
            response["code"] = 200;
            response["message"] = "Notification broadcast sent";
            response["type"] = type;
            response["path"] = path;
            response["clients"] = mConfigWsService->getClientCount();
            res.write(response.dump());
        } catch (const std::exception& e) {
            res.code = 500;
            res.write(std::string(R"({"status":"error","error":")") + e.what() + "\"}");
        }
    }
};

inline std::weak_ptr<lithium::ConfigManager> ConfigController::mConfigManager;
inline std::unique_ptr<lithium::server::config::ConfigWebSocketService> ConfigController::mConfigWsService;
