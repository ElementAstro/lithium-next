/*
 * plugin.hpp - Plugin Management Controller
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_PLUGIN_HPP
#define LITHIUM_SERVER_CONTROLLER_PLUGIN_HPP

#include "../controller.hpp"
#include "../utils/response.hpp"

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "server/plugin/plugin_manager.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;
using namespace lithium::server::plugin;

/**
 * @brief Controller for plugin management via HTTP API
 *
 * Provides REST endpoints for:
 * - Listing loaded plugins
 * - Loading/unloading plugins
 * - Enabling/disabling plugins
 * - Plugin configuration
 * - Plugin health monitoring
 * - Hot reload support
 */
class PluginController : public Controller {
private:
    static std::weak_ptr<PluginManager> mPluginManager;

    static auto handlePluginAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<PluginManager>)> func)
        -> crow::response {
        try {
            auto manager = mPluginManager.lock();
            if (!manager) {
                spdlog::error(
                    "PluginManager instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "PluginManager instance is null.");
            }
            return func(manager);
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
        GET_OR_CREATE_WEAK_PTR(mPluginManager, PluginManager,
                               Constants::PLUGIN_MANAGER);

        // Plugin listing
        CROW_ROUTE(app, "/api/v1/plugins")
            .methods("GET"_method)(&PluginController::listPlugins, this);

        CROW_ROUTE(app, "/api/v1/plugins/available")
            .methods("GET"_method)(&PluginController::listAvailablePlugins,
                                   this);

        // Plugin info
        CROW_ROUTE(app, "/api/v1/plugins/<string>")
            .methods("GET"_method)(&PluginController::getPluginInfo, this);

        // Plugin loading/unloading
        CROW_ROUTE(app, "/api/v1/plugins/load")
            .methods("POST"_method)(&PluginController::loadPlugin, this);

        CROW_ROUTE(app, "/api/v1/plugins/unload")
            .methods("POST"_method)(&PluginController::unloadPlugin, this);

        CROW_ROUTE(app, "/api/v1/plugins/reload")
            .methods("POST"_method)(&PluginController::reloadPlugin, this);

        // Plugin enable/disable
        CROW_ROUTE(app, "/api/v1/plugins/enable")
            .methods("POST"_method)(&PluginController::enablePlugin, this);

        CROW_ROUTE(app, "/api/v1/plugins/disable")
            .methods("POST"_method)(&PluginController::disablePlugin, this);

        // Plugin configuration
        CROW_ROUTE(app, "/api/v1/plugins/<string>/config")
            .methods("GET"_method)(&PluginController::getPluginConfig, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/config")
            .methods("PUT"_method)(&PluginController::updatePluginConfig, this);

        // Plugin health
        CROW_ROUTE(app, "/api/v1/plugins/<string>/health")
            .methods("GET"_method)(&PluginController::getPluginHealth, this);

        // System status
        CROW_ROUTE(app, "/api/v1/plugins/status")
            .methods("GET"_method)(&PluginController::getSystemStatus, this);

        // Discover and load all
        CROW_ROUTE(app, "/api/v1/plugins/discover")
            .methods("POST"_method)(&PluginController::discoverAndLoad, this);

        // Save configuration
        CROW_ROUTE(app, "/api/v1/plugins/config/save")
            .methods("POST"_method)(&PluginController::saveConfiguration, this);

        // Extended API - Batch operations
        CROW_ROUTE(app, "/api/v1/plugins/batch/load")
            .methods("POST"_method)(&PluginController::batchLoad, this);

        CROW_ROUTE(app, "/api/v1/plugins/batch/unload")
            .methods("POST"_method)(&PluginController::batchUnload, this);

        CROW_ROUTE(app, "/api/v1/plugins/batch/enable")
            .methods("POST"_method)(&PluginController::batchEnable, this);

        CROW_ROUTE(app, "/api/v1/plugins/batch/disable")
            .methods("POST"_method)(&PluginController::batchDisable, this);

        // Extended API - Group management
        CROW_ROUTE(app, "/api/v1/plugins/groups")
            .methods("GET"_method)(&PluginController::listGroups, this);

        CROW_ROUTE(app, "/api/v1/plugins/groups")
            .methods("POST"_method)(&PluginController::createGroup, this);

        CROW_ROUTE(app, "/api/v1/plugins/groups/<string>")
            .methods("DELETE"_method)(&PluginController::deleteGroup, this);

        CROW_ROUTE(app, "/api/v1/plugins/groups/<string>/enable")
            .methods("POST"_method)(&PluginController::enableGroup, this);

        CROW_ROUTE(app, "/api/v1/plugins/groups/<string>/disable")
            .methods("POST"_method)(&PluginController::disableGroup, this);

        // Extended API - Plugin execution
        CROW_ROUTE(app, "/api/v1/plugins/<string>/execute")
            .methods("POST"_method)(&PluginController::executeAction, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/command")
            .methods("POST"_method)(&PluginController::executeCommand, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/actions")
            .methods("GET"_method)(&PluginController::getPluginActions, this);

        // Extended API - Plugin queries
        CROW_ROUTE(app, "/api/v1/plugins/search")
            .methods("GET"_method)(&PluginController::searchPlugins, this);

        CROW_ROUTE(app, "/api/v1/plugins/by-capability/<string>")
            .methods("GET"_method)(&PluginController::getPluginsByCapability,
                                   this);

        CROW_ROUTE(app, "/api/v1/plugins/by-tag/<string>")
            .methods("GET"_method)(&PluginController::getPluginsByTag, this);

        // Extended API - Plugin state control
        CROW_ROUTE(app, "/api/v1/plugins/<string>/pause")
            .methods("POST"_method)(&PluginController::pausePlugin, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/resume")
            .methods("POST"_method)(&PluginController::resumePlugin, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/restart")
            .methods("POST"_method)(&PluginController::restartPlugin, this);

        // Extended API - Schema and documentation
        CROW_ROUTE(app, "/api/v1/plugins/<string>/commands")
            .methods("GET"_method)(&PluginController::getCommandSchemas, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/routes")
            .methods("GET"_method)(&PluginController::getRouteInfo, this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/openapi")
            .methods("GET"_method)(&PluginController::getOpenApiSpec, this);

        CROW_ROUTE(app, "/api/v1/plugins/openapi")
            .methods("GET"_method)(&PluginController::getCombinedOpenApiSpec,
                                   this);

        // Extended API - Statistics
        CROW_ROUTE(app, "/api/v1/plugins/<string>/statistics")
            .methods("GET"_method)(&PluginController::getPluginStatistics,
                                   this);

        CROW_ROUTE(app, "/api/v1/plugins/statistics")
            .methods("GET"_method)(&PluginController::getAllStatistics, this);

        // Extended API - Dependencies
        CROW_ROUTE(app, "/api/v1/plugins/<string>/dependencies")
            .methods("GET"_method)(&PluginController::getPluginDependencies,
                                   this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/dependents")
            .methods("GET"_method)(&PluginController::getDependentPlugins,
                                   this);

        CROW_ROUTE(app, "/api/v1/plugins/<string>/conflicts")
            .methods("GET"_method)(&PluginController::getPluginConflicts, this);
    }

    // List all loaded plugins
    void listPlugins(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "listPlugins",
            [&](auto manager) -> crow::response {
                auto plugins = manager->getAllPlugins();
                nlohmann::json pluginList = nlohmann::json::array();

                for (const auto& plugin : plugins) {
                    auto metadata = plugin.instance->getMetadata();
                    pluginList.push_back(
                        {{"name", plugin.name},
                         {"version", metadata.version},
                         {"description", metadata.description},
                         {"author", metadata.author},
                         {"type", static_cast<int>(plugin.type)},
                         {"state", pluginStateToString(plugin.state)},
                         {"enabled", manager->isPluginEnabled(plugin.name)},
                         {"healthy", plugin.instance->isHealthy()}});
                }

                nlohmann::json data = {{"plugins", pluginList},
                                       {"count", plugins.size()}};
                return ResponseBuilder::success(data);
            });
    }

    // List available (discovered but not loaded) plugins
    void listAvailablePlugins(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "listAvailablePlugins",
            [&](auto manager) -> crow::response {
                auto available = manager->getAvailablePlugins();
                nlohmann::json pathList = nlohmann::json::array();

                for (const auto& path : available) {
                    pathList.push_back({{"path", path.string()},
                                        {"name", path.stem().string()},
                                        {"loaded", manager->isPluginLoaded(
                                                       path.stem().string())}});
                }

                nlohmann::json data = {{"available", pathList},
                                       {"count", available.size()}};
                return ResponseBuilder::success(data);
            });
    }

    // Get plugin info by name
    void getPluginInfo(const crow::request& req, crow::response& res,
                       const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginInfo",
            [&](auto manager) -> crow::response {
                auto pluginInfo = manager->getPluginInfo(name);
                if (!pluginInfo) {
                    return ResponseBuilder::notFound("Plugin");
                }

                auto metadata = pluginInfo->instance->getMetadata();
                nlohmann::json data = {
                    {"name", pluginInfo->name},
                    {"path", pluginInfo->path},
                    {"version", metadata.version},
                    {"description", metadata.description},
                    {"author", metadata.author},
                    {"license", metadata.license},
                    {"dependencies", metadata.dependencies},
                    {"tags", metadata.tags},
                    {"type", static_cast<int>(pluginInfo->type)},
                    {"state", pluginStateToString(pluginInfo->state)},
                    {"enabled", manager->isPluginEnabled(name)},
                    {"healthy", pluginInfo->instance->isHealthy()}};

                // Add command IDs if command plugin
                if (auto cmdPlugin = pluginInfo->asCommandPlugin()) {
                    data["commands"] = cmdPlugin->getCommandIds();
                }

                // Add route paths if controller plugin
                if (auto ctrlPlugin = pluginInfo->asControllerPlugin()) {
                    data["routes"] = ctrlPlugin->getRoutePaths();
                    data["routePrefix"] = ctrlPlugin->getRoutePrefix();
                }

                auto lastError = pluginInfo->instance->getLastError();
                if (!lastError.empty()) {
                    data["lastError"] = lastError;
                }

                return ResponseBuilder::success(data);
            });
    }

    // Load a plugin
    void loadPlugin(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "loadPlugin", [&](auto manager) -> crow::response {
                    std::string name = body.value("name", "");
                    std::string path = body.value("path", "");
                    nlohmann::json config =
                        body.value("config", nlohmann::json::object());

                    PluginResult<LoadedPluginInfo> result;

                    if (!path.empty()) {
                        result = manager->loadPluginFromPath(path, config);
                    } else if (!name.empty()) {
                        result = manager->loadPlugin(name, config);
                    } else {
                        return ResponseBuilder::missingParameter(
                            "name or path");
                    }

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to load plugin: " +
                            pluginLoadErrorToString(result.error()));
                    }

                    auto metadata = result->instance->getMetadata();
                    nlohmann::json data = {{"name", result->name},
                                           {"version", metadata.version},
                                           {"loaded", true}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Unload a plugin
    void unloadPlugin(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "unloadPlugin", [&](auto manager) -> crow::response {
                    std::string name = body.value("name", "");
                    if (name.empty()) {
                        return ResponseBuilder::missingParameter("name");
                    }

                    auto result = manager->unloadPlugin(name);
                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to unload plugin: " +
                            pluginLoadErrorToString(result.error()));
                    }

                    nlohmann::json data = {{"name", name}, {"unloaded", true}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Reload a plugin
    void reloadPlugin(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "reloadPlugin", [&](auto manager) -> crow::response {
                    std::string name = body.value("name", "");
                    if (name.empty()) {
                        return ResponseBuilder::missingParameter("name");
                    }

                    auto result = manager->reloadPlugin(name);
                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to reload plugin: " +
                            pluginLoadErrorToString(result.error()));
                    }

                    auto metadata = result->instance->getMetadata();
                    nlohmann::json data = {{"name", result->name},
                                           {"version", metadata.version},
                                           {"reloaded", true}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Enable a plugin
    void enablePlugin(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "enablePlugin", [&](auto manager) -> crow::response {
                    std::string name = body.value("name", "");
                    if (name.empty()) {
                        return ResponseBuilder::missingParameter("name");
                    }

                    bool success = manager->enablePlugin(name);
                    nlohmann::json data = {{"name", name},
                                           {"enabled", success}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Disable a plugin
    void disablePlugin(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "disablePlugin",
                [&](auto manager) -> crow::response {
                    std::string name = body.value("name", "");
                    if (name.empty()) {
                        return ResponseBuilder::missingParameter("name");
                    }

                    bool success = manager->disablePlugin(name);
                    nlohmann::json data = {{"name", name},
                                           {"disabled", success}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Get plugin configuration
    void getPluginConfig(const crow::request& req, crow::response& res,
                         const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginConfig",
            [&](auto manager) -> crow::response {
                auto config = manager->getPluginConfig(name);
                if (!config) {
                    return ResponseBuilder::notFound("Plugin configuration");
                }

                nlohmann::json data = {{"name", name}, {"config", *config}};
                return ResponseBuilder::success(data);
            });
    }

    // Update plugin configuration
    void updatePluginConfig(const crow::request& req, crow::response& res,
                            const std::string& name) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "updatePluginConfig",
                [&](auto manager) -> crow::response {
                    nlohmann::json config =
                        body.value("config", nlohmann::json::object());

                    manager->updatePluginConfig(name, config);

                    nlohmann::json data = {{"name", name}, {"updated", true}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Get plugin health
    void getPluginHealth(const crow::request& req, crow::response& res,
                         const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginHealth",
            [&](auto manager) -> crow::response {
                auto health = manager->getPluginHealth(name);
                if (health.contains("error")) {
                    return ResponseBuilder::notFound("Plugin");
                }
                return ResponseBuilder::success(health);
            });
    }

    // Get system status
    void getSystemStatus(const crow::request& req, crow::response& res) {
        res = handlePluginAction(req, nlohmann::json{}, "getSystemStatus",
                                 [&](auto manager) -> crow::response {
                                     auto status = manager->getSystemStatus();
                                     return ResponseBuilder::success(status);
                                 });
    }

    // Discover and load all plugins
    void discoverAndLoad(const crow::request& req, crow::response& res) {
        res = handlePluginAction(req, nlohmann::json{}, "discoverAndLoad",
                                 [&](auto manager) -> crow::response {
                                     size_t loaded =
                                         manager->discoverAndLoadAll();
                                     nlohmann::json data = {{"loaded", loaded}};
                                     return ResponseBuilder::success(data);
                                 });
    }

    // Save configuration
    void saveConfiguration(const crow::request& req, crow::response& res) {
        res = handlePluginAction(req, nlohmann::json{}, "saveConfiguration",
                                 [&](auto manager) -> crow::response {
                                     bool success =
                                         manager->saveConfiguration();
                                     nlohmann::json data = {{"saved", success}};
                                     return ResponseBuilder::success(data);
                                 });
    }

    // ========================================================================
    // Extended API - Batch Operations
    // ========================================================================

    void batchLoad(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "batchLoad", [&](auto manager) -> crow::response {
                    auto names =
                        body.value("names", std::vector<std::string>{});
                    size_t loaded = manager->batchLoad(names);
                    return ResponseBuilder::success({{"loaded", loaded}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void batchUnload(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "batchUnload", [&](auto manager) -> crow::response {
                    auto names =
                        body.value("names", std::vector<std::string>{});
                    size_t unloaded = manager->batchUnload(names);
                    return ResponseBuilder::success({{"unloaded", unloaded}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void batchEnable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "batchEnable", [&](auto manager) -> crow::response {
                    auto names =
                        body.value("names", std::vector<std::string>{});
                    size_t enabled = manager->batchEnable(names);
                    return ResponseBuilder::success({{"enabled", enabled}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void batchDisable(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "batchDisable", [&](auto manager) -> crow::response {
                    auto names =
                        body.value("names", std::vector<std::string>{});
                    size_t disabled = manager->batchDisable(names);
                    return ResponseBuilder::success({{"disabled", disabled}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // ========================================================================
    // Extended API - Group Management
    // ========================================================================

    void listGroups(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "listGroups",
            [&](auto manager) -> crow::response {
                auto groups = manager->getAllGroups();
                nlohmann::json groupList = nlohmann::json::array();
                for (const auto& group : groups) {
                    groupList.push_back({{"name", group.name},
                                         {"description", group.description},
                                         {"plugins", group.plugins},
                                         {"enabled", group.enabled}});
                }
                return ResponseBuilder::success({{"groups", groupList}});
            });
    }

    void createGroup(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "createGroup", [&](auto manager) -> crow::response {
                    PluginGroup group;
                    group.name = body.value("name", "");
                    group.description = body.value("description", "");
                    group.plugins =
                        body.value("plugins", std::vector<std::string>{});
                    group.enabled = body.value("enabled", true);

                    if (group.name.empty()) {
                        return ResponseBuilder::missingParameter("name");
                    }

                    manager->createGroup(group);
                    return ResponseBuilder::success({{"created", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void deleteGroup(const crow::request& req, crow::response& res,
                     const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "deleteGroup",
            [&](auto manager) -> crow::response {
                manager->deleteGroup(name);
                return ResponseBuilder::success({{"deleted", true}});
            });
    }

    void enableGroup(const crow::request& req, crow::response& res,
                     const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "enableGroup",
            [&](auto manager) -> crow::response {
                size_t enabled = manager->enableGroup(name);
                return ResponseBuilder::success({{"enabled", enabled}});
            });
    }

    void disableGroup(const crow::request& req, crow::response& res,
                      const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "disableGroup",
            [&](auto manager) -> crow::response {
                size_t disabled = manager->disableGroup(name);
                return ResponseBuilder::success({{"disabled", disabled}});
            });
    }

    // ========================================================================
    // Extended API - Plugin Execution
    // ========================================================================

    void executeAction(const crow::request& req, crow::response& res,
                       const std::string& name) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "executeAction",
                [&](auto manager) -> crow::response {
                    std::string action = body.value("action", "");
                    auto params =
                        body.value("params", nlohmann::json::object());

                    if (action.empty()) {
                        return ResponseBuilder::missingParameter("action");
                    }

                    auto result = manager->executeAction(name, action, params);
                    return ResponseBuilder::success(result);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void executeCommand(const crow::request& req, crow::response& res,
                        const std::string& name) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handlePluginAction(
                req, body, "executeCommand",
                [&](auto manager) -> crow::response {
                    std::string commandId = body.value("commandId", "");
                    auto params =
                        body.value("params", nlohmann::json::object());

                    if (commandId.empty()) {
                        return ResponseBuilder::missingParameter("commandId");
                    }

                    auto result =
                        manager->executeCommand(name, commandId, params);
                    return ResponseBuilder::success(result);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    void getPluginActions(const crow::request& req, crow::response& res,
                          const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginActions",
            [&](auto manager) -> crow::response {
                auto actions = manager->getPluginActions(name);
                return ResponseBuilder::success({{"actions", actions}});
            });
    }

    // ========================================================================
    // Extended API - Plugin Queries
    // ========================================================================

    void searchPlugins(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "searchPlugins",
            [&](auto manager) -> crow::response {
                std::string pattern = req.url_params.get("pattern")
                                          ? req.url_params.get("pattern")
                                          : "*";
                auto plugins = manager->searchPlugins(pattern);
                nlohmann::json pluginList = nlohmann::json::array();
                for (const auto& plugin : plugins) {
                    pluginList.push_back(plugin.toJson());
                }
                return ResponseBuilder::success({{"plugins", pluginList}});
            });
    }

    void getPluginsByCapability(const crow::request& req, crow::response& res,
                                const std::string& capability) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginsByCapability",
            [&](auto manager) -> crow::response {
                auto plugins = manager->getPluginsByCapability(capability);
                nlohmann::json pluginList = nlohmann::json::array();
                for (const auto& plugin : plugins) {
                    pluginList.push_back(plugin.toJson());
                }
                return ResponseBuilder::success({{"plugins", pluginList}});
            });
    }

    void getPluginsByTag(const crow::request& req, crow::response& res,
                         const std::string& tag) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginsByTag",
            [&](auto manager) -> crow::response {
                auto plugins = manager->getPluginsByTag(tag);
                nlohmann::json pluginList = nlohmann::json::array();
                for (const auto& plugin : plugins) {
                    pluginList.push_back(plugin.toJson());
                }
                return ResponseBuilder::success({{"plugins", pluginList}});
            });
    }

    // ========================================================================
    // Extended API - Plugin State Control
    // ========================================================================

    void pausePlugin(const crow::request& req, crow::response& res,
                     const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "pausePlugin",
            [&](auto manager) -> crow::response {
                bool success = manager->pausePlugin(name);
                return ResponseBuilder::success({{"paused", success}});
            });
    }

    void resumePlugin(const crow::request& req, crow::response& res,
                      const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "resumePlugin",
            [&](auto manager) -> crow::response {
                bool success = manager->resumePlugin(name);
                return ResponseBuilder::success({{"resumed", success}});
            });
    }

    void restartPlugin(const crow::request& req, crow::response& res,
                       const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "restartPlugin",
            [&](auto manager) -> crow::response {
                bool success = manager->restartPlugin(name);
                return ResponseBuilder::success({{"restarted", success}});
            });
    }

    // ========================================================================
    // Extended API - Schema and Documentation
    // ========================================================================

    void getCommandSchemas(const crow::request& req, crow::response& res,
                           const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getCommandSchemas",
            [&](auto manager) -> crow::response {
                auto schemas = manager->getAllCommandSchemas(name);
                return ResponseBuilder::success({{"commands", schemas}});
            });
    }

    void getRouteInfo(const crow::request& req, crow::response& res,
                      const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getRouteInfo",
            [&](auto manager) -> crow::response {
                auto routes = manager->getRouteInfo(name);
                nlohmann::json routeList = nlohmann::json::array();
                for (const auto& route : routes) {
                    routeList.push_back({{"path", route.path},
                                         {"method", route.method},
                                         {"description", route.description},
                                         {"requiresAuth", route.requiresAuth}});
                }
                return ResponseBuilder::success({{"routes", routeList}});
            });
    }

    void getOpenApiSpec(const crow::request& req, crow::response& res,
                        const std::string& name) {
        res = handlePluginAction(req, nlohmann::json{}, "getOpenApiSpec",
                                 [&](auto manager) -> crow::response {
                                     auto spec = manager->getOpenApiSpec(name);
                                     return ResponseBuilder::success(spec);
                                 });
    }

    void getCombinedOpenApiSpec(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getCombinedOpenApiSpec",
            [&](auto manager) -> crow::response {
                auto spec = manager->getCombinedOpenApiSpec();
                return ResponseBuilder::success(spec);
            });
    }

    // ========================================================================
    // Extended API - Statistics
    // ========================================================================

    void getPluginStatistics(const crow::request& req, crow::response& res,
                             const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginStatistics",
            [&](auto manager) -> crow::response {
                auto stats = manager->getPluginStatistics(name);
                if (!stats) {
                    return ResponseBuilder::notFound("Plugin");
                }
                return ResponseBuilder::success(
                    {{"callCount", stats->callCount},
                     {"errorCount", stats->errorCount},
                     {"avgResponseTimeMs", stats->avgResponseTimeMs},
                     {"memoryUsageBytes", stats->memoryUsageBytes}});
            });
    }

    void getAllStatistics(const crow::request& req, crow::response& res) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getAllStatistics",
            [&](auto manager) -> crow::response {
                auto stats = manager->getAllStatistics();
                return ResponseBuilder::success({{"statistics", stats}});
            });
    }

    // ========================================================================
    // Extended API - Dependencies
    // ========================================================================

    void getPluginDependencies(const crow::request& req, crow::response& res,
                               const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginDependencies",
            [&](auto manager) -> crow::response {
                auto deps = manager->getPluginDependencies(name);
                return ResponseBuilder::success({{"dependencies", deps}});
            });
    }

    void getDependentPlugins(const crow::request& req, crow::response& res,
                             const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getDependentPlugins",
            [&](auto manager) -> crow::response {
                auto dependents = manager->getDependentPlugins(name);
                return ResponseBuilder::success({{"dependents", dependents}});
            });
    }

    void getPluginConflicts(const crow::request& req, crow::response& res,
                            const std::string& name) {
        res = handlePluginAction(
            req, nlohmann::json{}, "getPluginConflicts",
            [&](auto manager) -> crow::response {
                auto conflicts = manager->getConflictingPlugins(name);
                bool hasConflicts = manager->hasConflicts(name);
                return ResponseBuilder::success(
                    {{"hasConflicts", hasConflicts}, {"conflicts", conflicts}});
            });
    }
};

inline std::weak_ptr<PluginManager> PluginController::mPluginManager;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_PLUGIN_HPP
