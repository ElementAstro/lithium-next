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
#include "../command/config_ws.hpp"
#include "../utils/response.hpp"
#include "atom/function/global_ptr.hpp"
#include "config/config.hpp"
#include "constant/constant.hpp"
#include "controller.hpp"
#include "utils/format.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class ConfigController : public Controller {
private:
    static std::weak_ptr<lithium::ConfigManager> mConfigManager;
    static std::unique_ptr<lithium::server::config::ConfigWebSocketService>
        mConfigWsService;

    static crow::response handleConfigAction(
        const nlohmann::json& body, const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ConfigManager>)> func) {
        spdlog::info("Handling config action: {}", command);

        if (command != "reloadConfig" &&
            (!body["path"] || body["path"].get<std::string>().empty())) {
            spdlog::warn(
                "The 'path' parameter is missing or empty for command: {}",
                command);
            return ResponseBuilder::badRequest(
                "The 'path' parameter is required and cannot be empty.");
        }

        try {
            auto configManager = mConfigManager.lock();
            if (!configManager) {
                spdlog::error("ConfigManager instance is null. Command: {}",
                              command);
                return ResponseBuilder::internalError(
                    "ConfigManager instance is null.");
            }

            spdlog::info("Executing function for command: {}", command);
            bool success = func(configManager);
            if (success) {
                spdlog::info("Command {} executed successfully.", command);
                nlohmann::json responseData;
                if (command != "reloadConfig") {
                    responseData["path"] = body["path"].get<std::string>();
                }
                spdlog::info("Config action {} completed.", command);
                return ResponseBuilder::success(responseData);
            } else {
                spdlog::warn("Command {} failed to execute.", command);
                spdlog::info("Config action {} completed.", command);
                return ResponseBuilder::notFound(
                    "The specified path could not be found or the operation "
                    "failed.");
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception occurred while executing command {}: {}",
                          command, e.what());
            return ResponseBuilder::internalError(
                std::string("Exception occurred - ") + e.what());
        }
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
            mConfigWsService = std::make_unique<
                lithium::server::config::ConfigWebSocketService>(app, wsConfig);
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("getConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            spdlog::info("Retrieving config for path: {}", path);
            if (auto tmp = configManager->get(path)) {
                spdlog::info("Config retrieved successfully for path: {}",
                             path);
                nlohmann::json responseData = {{"path", path},
                                               {"value", tmp.value().dump()},
                                               {"type", "string"}};
                res = ResponseBuilder::success(responseData);
            } else {
                spdlog::warn("Config not found for path: {}", path);
                res = ResponseBuilder::notFound("Config at path " + path);
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in getConfig: {}", e.what());
            res = ResponseBuilder::internalError(e.what());
        }
        spdlog::info("getConfig completed.");
    }

    void setConfig(const crow::request& req, crow::response& res) {
        spdlog::info("setConfig called.");
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("setConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
        std::string value = body["value"].get<std::string>();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in setConfig.");
            res = ResponseBuilder::missingField("value");
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("deleteConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("loadConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
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
            nlohmann::json{}, "reloadConfig", [&](auto configManager) {
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("saveConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("appendConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
        std::string value = body["value"].get<std::string>();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in appendConfig.");
            res = ResponseBuilder::missingField("value");
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
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("hasConfig request body: {}", req.body);
        std::string path = body["path"].get<std::string>();
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
        try {
            auto configManager = mConfigManager.lock();
            if (!configManager) {
                res = ResponseBuilder::internalError(
                    "ConfigManager not available");
                return;
            }
            spdlog::info("Listing all config keys.");
            auto keys = configManager->getKeys();
            spdlog::info("Retrieved {} config keys.", keys.size());
            nlohmann::json keysData = nlohmann::json::array();
            for (const auto& key : keys) {
                keysData.push_back(key);
            }
            nlohmann::json responseData = {{"keys", keysData}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
        spdlog::info("listConfigKeys completed.");
    }

    void listConfigPaths([[maybe_unused]] const crow::request& req,
                         crow::response& res) {
        spdlog::info("listConfigPaths called.");
        try {
            auto configManager = mConfigManager.lock();
            if (!configManager) {
                res = ResponseBuilder::internalError(
                    "ConfigManager not available");
                return;
            }
            spdlog::info("Listing all config paths.");
            auto paths = configManager->listPaths();
            spdlog::info("Retrieved {} config paths.", paths.size());
            nlohmann::json pathsData = nlohmann::json::array();
            for (const auto& path : paths) {
                pathsData.push_back(path);
            }
            nlohmann::json responseData = {{"paths", pathsData}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
        spdlog::info("listConfigPaths completed.");
    }

    void tidyConfig([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        spdlog::info("tidyConfig called.");
        res = handleConfigAction(
            nlohmann::json{}, "tidyConfig", [&](auto configManager) {
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
            nlohmann::json{}, "clearConfig", [&](auto configManager) {
                spdlog::info("Clearing all config.");
                configManager->clear();
                spdlog::info("All config cleared successfully.");
                return true;
            });
        spdlog::info("clearConfig completed.");
    }

    void mergeConfig(const crow::request& req, crow::response& res) {
        spdlog::info("mergeConfig called.");
        auto body = nlohmann::json::parse(req.body);
        spdlog::info("mergeConfig request body: {}", req.body);
        std::string value = body["value"].get<std::string>();
        if (value.empty()) {
            spdlog::warn("Missing 'value' parameter in mergeConfig.");
            res = ResponseBuilder::missingField("value");
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
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto result = configManager->validate(path);
            nlohmann::json errors = nlohmann::json::array();
            for (const auto& err : result.errors) {
                errors.push_back(err);
            }
            nlohmann::json warnings = nlohmann::json::array();
            for (const auto& warn : result.warnings) {
                warnings.push_back(warn);
            }

            nlohmann::json responseData = {{"valid", result.isValid},
                                           {"path", path},
                                           {"errors", errors},
                                           {"warnings", warnings}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void validateAllConfig([[maybe_unused]] const crow::request& req,
                           crow::response& res) {
        spdlog::info("validateAllConfig called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto result = configManager->validateAll();
            nlohmann::json errors = nlohmann::json::array();
            for (const auto& err : result.errors) {
                errors.push_back(err);
            }
            nlohmann::json warnings = nlohmann::json::array();
            for (const auto& warn : result.warnings) {
                warnings.push_back(warn);
            }

            nlohmann::json responseData = {{"valid", result.isValid},
                                           {"errors", errors},
                                           {"warnings", warnings}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void setSchema(const crow::request& req, crow::response& res) {
        spdlog::info("setSchema called.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();
        std::string schema = body["schema"].get<std::string>();

        if (path.empty() || schema.empty()) {
            res =
                ResponseBuilder::badRequest("Missing path or schema parameter");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success =
                configManager->setSchema(path, nlohmann::json::parse(schema));
            nlohmann::json responseData = {{"path", path}};
            res = success ? ResponseBuilder::success(responseData)
                          : ResponseBuilder::badRequest("Failed to set schema");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void loadSchema(const crow::request& req, crow::response& res) {
        spdlog::info("loadSchema called.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();
        std::string filePath = body["file_path"].get<std::string>();

        if (path.empty() || filePath.empty()) {
            res = ResponseBuilder::badRequest(
                "Missing path or file_path parameter");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success = configManager->loadSchema(path, filePath);
            nlohmann::json responseData = {{"path", path},
                                           {"file_path", filePath}};
            res = success
                      ? ResponseBuilder::success(responseData)
                      : ResponseBuilder::badRequest("Failed to load schema");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    // ========================================================================
    // File Watching Methods
    // ========================================================================

    void enableAutoReload(const crow::request& req, crow::response& res) {
        spdlog::info("enableAutoReload called.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();

        if (path.empty()) {
            res = ResponseBuilder::missingField("path");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success = configManager->enableAutoReload(path);
            nlohmann::json responseData = {{"path", path},
                                           {"watching", success}};
            res = success ? ResponseBuilder::success(responseData)
                          : ResponseBuilder::badRequest(
                                "Failed to enable auto reload");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void disableAutoReload(const crow::request& req, crow::response& res) {
        spdlog::info("disableAutoReload called.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();

        if (path.empty()) {
            res = ResponseBuilder::missingField("path");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success = configManager->disableAutoReload(path);
            nlohmann::json responseData = {{"path", path}, {"watching", false}};
            res = success ? ResponseBuilder::success(responseData)
                          : ResponseBuilder::badRequest(
                                "Failed to disable auto reload");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void getWatchStatus(const crow::request& req, crow::response& res) {
        spdlog::info("getWatchStatus called.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();

        if (path.empty()) {
            res = ResponseBuilder::missingField("path");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool watching = configManager->isAutoReloadEnabled(path);
            nlohmann::json responseData = {{"path", path},
                                           {"watching", watching}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
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
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto metrics = configManager->getMetrics();
            nlohmann::json metricsData = {
                {"total_operations", metrics.total_operations},
                {"cache_hits", metrics.cache_hits},
                {"cache_misses", metrics.cache_misses},
                {"validation_successes", metrics.validation_successes},
                {"validation_failures", metrics.validation_failures},
                {"files_loaded", metrics.files_loaded},
                {"files_saved", metrics.files_saved},
                {"auto_reloads", metrics.auto_reloads},
                {"average_access_time_ms", metrics.average_access_time_ms},
                {"average_save_time_ms", metrics.average_save_time_ms}};
            res = ResponseBuilder::success(metricsData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void resetMetrics([[maybe_unused]] const crow::request& req,
                      crow::response& res) {
        spdlog::info("resetMetrics called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            configManager->resetMetrics();
            nlohmann::json responseData = {
                {"message", "Metrics reset successfully"}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void getCacheStats([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("getCacheStats called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto stats = configManager->getCache().getStatistics();
            nlohmann::json cacheData = {
                {"hits", stats.hits.load()},
                {"misses", stats.misses.load()},
                {"evictions", stats.evictions.load()},
                {"expirations", stats.expirations.load()},
                {"current_size", stats.currentSize.load()},
                {"hit_ratio", stats.getHitRatio()}};
            res = ResponseBuilder::success(cacheData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
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
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            std::string snapshotId = configManager->createSnapshot();
            nlohmann::json responseData = {{"snapshot_id", snapshotId}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void restoreSnapshot(const crow::request& req, crow::response& res) {
        spdlog::info("restoreSnapshot called.");
        auto body = nlohmann::json::parse(req.body);
        std::string snapshotId = body["snapshot_id"].get<std::string>();

        if (snapshotId.empty()) {
            res = ResponseBuilder::missingField("snapshot_id");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success = configManager->restoreSnapshot(snapshotId);
            nlohmann::json responseData = {{"snapshot_id", snapshotId}};
            res = success ? ResponseBuilder::success(responseData)
                          : ResponseBuilder::notFound("Snapshot " + snapshotId);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void listSnapshots([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("listSnapshots called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto snapshots = configManager->listSnapshots();
            nlohmann::json snapshotList = nlohmann::json::array();
            for (const auto& id : snapshots) {
                snapshotList.push_back(id);
            }
            nlohmann::json responseData = {{"snapshots", snapshotList},
                                           {"count", snapshots.size()}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void deleteSnapshot(const crow::request& req, crow::response& res) {
        spdlog::info("deleteSnapshot called.");
        auto body = nlohmann::json::parse(req.body);
        std::string snapshotId = body["snapshot_id"].get<std::string>();

        if (snapshotId.empty()) {
            res = ResponseBuilder::missingField("snapshot_id");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success = configManager->deleteSnapshot(snapshotId);
            nlohmann::json responseData = {{"snapshot_id", snapshotId}};
            res = success ? ResponseBuilder::success(responseData)
                          : ResponseBuilder::notFound("Snapshot " + snapshotId);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    // ========================================================================
    // Import/Export Methods
    // ========================================================================

    void exportConfig(const crow::request& req, crow::response& res) {
        spdlog::info("exportConfig called.");
        auto body = nlohmann::json::parse(req.body);
        std::string formatStr =
            body["format"] ? body["format"].get<std::string>() : "json";

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
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
            nlohmann::json responseData = {{"format", formatStr},
                                           {"data", exported}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void importConfig(const crow::request& req, crow::response& res) {
        spdlog::info("importConfig called.");
        auto body = nlohmann::json::parse(req.body);
        std::string data = body["data"].get<std::string>();
        std::string formatStr =
            body["format"] ? body["format"].get<std::string>() : "json";

        if (data.empty()) {
            res = ResponseBuilder::missingField("data");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            lithium::config::SerializationFormat format =
                lithium::config::SerializationFormat::JSON;
            if (formatStr == "json5") {
                format = lithium::config::SerializationFormat::JSON5;
            }

            bool success = configManager->importFrom(data, format);
            nlohmann::json responseData = {{"format", formatStr}};
            res = success
                      ? ResponseBuilder::success(responseData)
                      : ResponseBuilder::badRequest("Failed to import config");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void diffConfig(const crow::request& req, crow::response& res) {
        spdlog::info("diffConfig called.");
        auto body = nlohmann::json::parse(req.body);
        std::string otherConfig = body["config"].get<std::string>();

        if (otherConfig.empty()) {
            res = ResponseBuilder::missingField("config");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto diffResult =
                configManager->diff(nlohmann::json::parse(otherConfig));
            nlohmann::json responseData = {{"diff", diffResult}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void applyPatch(const crow::request& req, crow::response& res) {
        spdlog::info("applyPatch called.");
        auto body = nlohmann::json::parse(req.body);
        std::string patch = body["patch"].get<std::string>();

        if (patch.empty()) {
            res = ResponseBuilder::missingField("patch");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            bool success =
                configManager->applyPatch(nlohmann::json::parse(patch));
            nlohmann::json responseData = {};
            res = success
                      ? ResponseBuilder::success(responseData)
                      : ResponseBuilder::badRequest("Failed to apply patch");
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void flattenConfig([[maybe_unused]] const crow::request& req,
                       crow::response& res) {
        spdlog::info("flattenConfig called.");

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            auto flattened = configManager->flatten();
            nlohmann::json flatData;
            for (const auto& [key, value] : flattened) {
                flatData[key] = value;
            }
            nlohmann::json responseData = {{"data", flatData},
                                           {"count", flattened.size()}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    // ========================================================================
    // Batch Operations Methods
    // ========================================================================

    void batchGetConfig(const crow::request& req, crow::response& res) {
        spdlog::info("batchGetConfig called.");
        auto body = nlohmann::json::parse(req.body);

        if (!body["paths"]) {
            res = ResponseBuilder::missingField("paths");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            nlohmann::json results;
            for (size_t i = 0; i < body["paths"].size(); ++i) {
                std::string path = body["paths"][i].get<std::string>();
                auto value = configManager->get(path);
                if (value) {
                    results[path] = *value;
                } else {
                    results[path] = nullptr;
                }
            }
            nlohmann::json responseData = {{"results", results}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void batchSetConfig(const crow::request& req, crow::response& res) {
        spdlog::info("batchSetConfig called.");
        auto body = nlohmann::json::parse(req.body);

        if (!body["items"]) {
            res = ResponseBuilder::missingField("items");
            return;
        }

        auto configManager = mConfigManager.lock();
        if (!configManager) {
            res = ResponseBuilder::internalError("ConfigManager not available");
            return;
        }

        try {
            size_t successCount = 0;
            size_t failCount = 0;
            nlohmann::json results;

            for (size_t i = 0; i < body["items"].size(); ++i) {
                std::string path = body["items"][i]["path"].get<std::string>();
                std::string value =
                    body["items"][i]["value"].get<std::string>();

                bool success =
                    configManager->set(path, nlohmann::json::parse(value));
                results[path] = success;
                if (success) {
                    ++successCount;
                } else {
                    ++failCount;
                }
            }

            nlohmann::json responseData = {{"results", results},
                                           {"success_count", successCount},
                                           {"fail_count", failCount}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    // ========================================================================
    // WebSocket Service Methods
    // ========================================================================

    void getWsStats([[maybe_unused]] const crow::request& req,
                    crow::response& res) {
        spdlog::info("getWsStats called.");

        if (!mConfigWsService) {
            res = ResponseBuilder::serviceUnavailable(
                "WebSocket service not available");
            return;
        }

        try {
            auto stats = mConfigWsService->getStatistics();
            nlohmann::json responseData = {{"websocket", stats}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }

    void broadcastConfigNotification(const crow::request& req,
                                     crow::response& res) {
        spdlog::info("broadcastConfigNotification called.");
        auto body = nlohmann::json::parse(req.body);

        if (!mConfigWsService) {
            res = ResponseBuilder::serviceUnavailable(
                "WebSocket service not available");
            return;
        }

        std::string type =
            body["type"] ? body["type"].get<std::string>() : "value_changed";
        std::string path = body["path"] ? body["path"].get<std::string>() : "";
        std::string message =
            body["message"] ? body["message"].get<std::string>() : "";

        try {
            lithium::server::config::ConfigWebSocketService::NotificationType
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::VALUE_CHANGED;

            // Map string type to enum
            if (type == "value_removed") {
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::VALUE_REMOVED;
            } else if (type == "file_loaded") {
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::FILE_LOADED;
            } else if (type == "file_saved") {
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::FILE_SAVED;
            } else if (type == "config_cleared") {
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::CONFIG_CLEARED;
            } else if (type == "config_merged") {
                notifType = lithium::server::config::ConfigWebSocketService::
                    NotificationType::CONFIG_MERGED;
            }

            nlohmann::json data;
            if (!message.empty()) {
                data["message"] = message;
            }

            mConfigWsService->broadcastNotification(notifType, path, data);

            nlohmann::json responseData = {
                {"message", "Notification broadcast sent"},
                {"type", type},
                {"path", path},
                {"clients", mConfigWsService->getClientCount()}};
            res = ResponseBuilder::success(responseData);
        } catch (const std::exception& e) {
            res = ResponseBuilder::internalError(e.what());
        }
    }
};

inline std::weak_ptr<lithium::ConfigManager> ConfigController::mConfigManager;
inline std::unique_ptr<lithium::server::config::ConfigWebSocketService>
    ConfigController::mConfigWsService;

}  // namespace lithium::server::controller
