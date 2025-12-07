/*
 * device_plugin.hpp - Device Plugin Management Controller
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_DEVICE_PLUGIN_HPP
#define LITHIUM_SERVER_CONTROLLER_DEVICE_PLUGIN_HPP

#include "../controller.hpp"
#include "../utils/response.hpp"

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "device/events/device_event_bus.hpp"
#include "device/plugin/device_plugin_loader.hpp"
#include "device/service/device_type_registry.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;
using namespace lithium::device;

/**
 * @brief Controller for device plugin management via HTTP API
 *
 * Provides REST endpoints for:
 * - Listing device plugins
 * - Loading/unloading device plugins
 * - Hot-reload support
 * - Device type queries
 * - Device event subscription
 */
class DevicePluginController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // ==================== Plugin Management ====================

        // List all loaded device plugins
        CROW_ROUTE(app, "/api/v1/device-plugins")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("listDevicePlugins", [&]() {
                    return listDevicePlugins();
                });
            });

        // List available device plugins (discovered but not loaded)
        CROW_ROUTE(app, "/api/v1/device-plugins/available")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("listAvailablePlugins", [&]() {
                    return listAvailablePlugins();
                });
            });

        // Get plugin info
        CROW_ROUTE(app, "/api/v1/device-plugins/<string>")
            .methods("GET"_method)(
                [this](const crow::request& req, const std::string& name) {
                    return handleRequest("getPluginInfo", [&]() {
                        return getPluginInfo(name);
                    });
                });

        // Load a device plugin
        CROW_ROUTE(app, "/api/v1/device-plugins/load")
            .methods("POST"_method)([this](const crow::request& req) {
                return handleJsonRequest(req, "loadPlugin",
                                         [&](const nlohmann::json& body) {
                                             return loadPlugin(body);
                                         });
            });

        // Unload a device plugin
        CROW_ROUTE(app, "/api/v1/device-plugins/unload")
            .methods("POST"_method)([this](const crow::request& req) {
                return handleJsonRequest(req, "unloadPlugin",
                                         [&](const nlohmann::json& body) {
                                             return unloadPlugin(body);
                                         });
            });

        // Reload a device plugin (hot-plug)
        CROW_ROUTE(app, "/api/v1/device-plugins/reload")
            .methods("POST"_method)([this](const crow::request& req) {
                return handleJsonRequest(req, "reloadPlugin",
                                         [&](const nlohmann::json& body) {
                                             return reloadPlugin(body);
                                         });
            });

        // Discover and load all plugins
        CROW_ROUTE(app, "/api/v1/device-plugins/discover")
            .methods("POST"_method)([this](const crow::request& req) {
                return handleRequest("discoverPlugins", [&]() {
                    return discoverAndLoadPlugins();
                });
            });

        // Get hot-plug status
        CROW_ROUTE(app, "/api/v1/device-plugins/hotplug-status")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("getHotPlugStatus", [&]() {
                    return getHotPlugStatus();
                });
            });

        // ==================== Device Type Management ====================

        // List all device types
        CROW_ROUTE(app, "/api/v1/device-types")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("listDeviceTypes", [&]() {
                    return listDeviceTypes();
                });
            });

        // Get device type info
        CROW_ROUTE(app, "/api/v1/device-types/<string>")
            .methods("GET"_method)(
                [this](const crow::request& req, const std::string& typeName) {
                    return handleRequest("getDeviceTypeInfo", [&]() {
                        return getDeviceTypeInfo(typeName);
                    });
                });

        // List device types by category
        CROW_ROUTE(app, "/api/v1/device-types/category/<string>")
            .methods("GET"_method)(
                [this](const crow::request& req, const std::string& category) {
                    return handleRequest("getTypesByCategory", [&]() {
                        return getTypesByCategory(category);
                    });
                });

        // List device categories
        CROW_ROUTE(app, "/api/v1/device-categories")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("listCategories", [&]() {
                    return listDeviceCategories();
                });
            });

        // ==================== Event Bus ====================

        // Get recent events
        CROW_ROUTE(app, "/api/v1/device-events")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("getRecentEvents", [&]() {
                    size_t count = 100;
                    if (req.url_params.get("count")) {
                        count = std::stoul(req.url_params.get("count"));
                    }
                    return getRecentEvents(count);
                });
            });

        // Get event bus statistics
        CROW_ROUTE(app, "/api/v1/device-events/statistics")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("getEventStatistics", [&]() {
                    return getEventStatistics();
                });
            });

        // ==================== Statistics ====================

        // Get loader statistics
        CROW_ROUTE(app, "/api/v1/device-plugins/statistics")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("getLoaderStatistics", [&]() {
                    return getLoaderStatistics();
                });
            });

        // Get type registry statistics
        CROW_ROUTE(app, "/api/v1/device-types/statistics")
            .methods("GET"_method)([this](const crow::request& req) {
                return handleRequest("getRegistryStatistics", [&]() {
                    return getRegistryStatistics();
                });
            });
    }

private:
    // ==================== Request Handlers ====================

    template <typename Func>
    crow::response handleRequest(const std::string& operation, Func func) {
        try {
            return func();
        } catch (const std::exception& e) {
            LOG_ERROR("Error in {}: {}", operation, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    template <typename Func>
    crow::response handleJsonRequest(const crow::request& req,
                                     const std::string& operation, Func func) {
        try {
            auto body = nlohmann::json::parse(req.body);
            return func(body);
        } catch (const nlohmann::json::exception& e) {
            return ResponseBuilder::invalidJson(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Error in {}: {}", operation, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

    // ==================== Plugin Management ====================

    crow::response listDevicePlugins() {
        auto& loader = DevicePluginLoader::getInstance();
        auto plugins = loader.getLoadedPlugins();

        nlohmann::json pluginList = nlohmann::json::array();
        for (const auto& [name, info] : plugins) {
            nlohmann::json pluginJson;
            pluginJson["name"] = info.name;
            pluginJson["path"] = info.path.string();
            pluginJson["loadedAt"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    info.loadedAt.time_since_epoch())
                    .count();
            pluginJson["reloadCount"] = info.reloadCount;
            pluginJson["isBuiltIn"] = info.isBuiltIn;

            if (info.plugin) {
                auto metadata = info.plugin->getDeviceMetadata();
                pluginJson["version"] = metadata.version;
                pluginJson["description"] = metadata.description;
                pluginJson["backendName"] = metadata.backendName;
                pluginJson["state"] =
                    devicePluginStateToString(info.plugin->getDevicePluginState());
                pluginJson["supportsHotPlug"] = info.plugin->supportsHotPlug();
                pluginJson["healthy"] = info.plugin->isHealthy();
            }

            pluginList.push_back(pluginJson);
        }

        return ResponseBuilder::success(
            {{"plugins", pluginList}, {"count", plugins.size()}});
    }

    crow::response listAvailablePlugins() {
        auto& loader = DevicePluginLoader::getInstance();
        auto discovered = loader.discoverPlugins();

        nlohmann::json availableList = nlohmann::json::array();
        for (const auto& result : discovered) {
            nlohmann::json pluginJson;
            pluginJson["path"] = result.path.string();
            pluginJson["name"] = result.name;
            pluginJson["version"] = result.version;
            pluginJson["isDevicePlugin"] = result.isDevicePlugin;
            pluginJson["loaded"] = loader.isPluginLoaded(result.name);

            if (!result.error.empty()) {
                pluginJson["error"] = result.error;
            }
            if (!result.metadata.is_null()) {
                pluginJson["metadata"] = result.metadata;
            }

            availableList.push_back(pluginJson);
        }

        return ResponseBuilder::success(
            {{"available", availableList}, {"count", discovered.size()}});
    }

    crow::response getPluginInfo(const std::string& name) {
        auto& loader = DevicePluginLoader::getInstance();
        auto plugin = loader.getPlugin(name);

        if (!plugin) {
            return ResponseBuilder::notFound("Device plugin");
        }

        auto metadata = plugin->getDeviceMetadata();
        nlohmann::json data;
        data["name"] = metadata.name;
        data["version"] = metadata.version;
        data["description"] = metadata.description;
        data["author"] = metadata.author;
        data["license"] = metadata.license;
        data["backendName"] = metadata.backendName;
        data["backendVersion"] = metadata.backendVersion;
        data["supportsHotPlug"] = metadata.supportsHotPlug;
        data["supportsAutoDiscovery"] = metadata.supportsAutoDiscovery;
        data["requiresServer"] = metadata.requiresServer;
        data["supportedDeviceCategories"] = metadata.supportedDeviceCategories;
        data["tags"] = metadata.tags;
        data["capabilities"] = metadata.capabilities;
        data["state"] =
            devicePluginStateToString(plugin->getDevicePluginState());
        data["healthy"] = plugin->isHealthy();
        data["hasBackend"] = plugin->hasBackend();
        data["backendRunning"] = plugin->isBackendRunning();

        auto lastError = plugin->getLastError();
        if (!lastError.empty()) {
            data["lastError"] = lastError;
        }

        // Get device types
        auto types = plugin->getDeviceTypes();
        nlohmann::json typeList = nlohmann::json::array();
        for (const auto& type : types) {
            typeList.push_back({{"typeName", type.typeName},
                                {"category", type.category},
                                {"displayName", type.displayName}});
        }
        data["deviceTypes"] = typeList;

        // Get discovered devices
        auto devices = plugin->getDiscoveredDevices();
        nlohmann::json deviceList = nlohmann::json::array();
        for (const auto& device : devices) {
            deviceList.push_back({{"deviceId", device.deviceId},
                                  {"displayName", device.displayName},
                                  {"deviceType", device.deviceType}});
        }
        data["discoveredDevices"] = deviceList;

        return ResponseBuilder::success(data);
    }

    crow::response loadPlugin(const nlohmann::json& body) {
        std::string name = body.value("name", "");
        std::string path = body.value("path", "");
        nlohmann::json config = body.value("config", nlohmann::json::object());

        auto& loader = DevicePluginLoader::getInstance();
        DeviceResult<bool> result;

        if (!path.empty()) {
            result = loader.loadPlugin(path, config);
        } else if (!name.empty()) {
            result = loader.loadPluginByName(name, config);
        } else {
            return ResponseBuilder::missingParameter("name or path");
        }

        if (!result) {
            return ResponseBuilder::internalError("Failed to load plugin: " +
                                                  result.error().message);
        }

        return ResponseBuilder::success({{"name", name.empty() ? path : name},
                                         {"loaded", true}});
    }

    crow::response unloadPlugin(const nlohmann::json& body) {
        std::string name = body.value("name", "");
        if (name.empty()) {
            return ResponseBuilder::missingParameter("name");
        }

        auto& loader = DevicePluginLoader::getInstance();
        auto result = loader.unloadPlugin(name);

        if (!result) {
            return ResponseBuilder::internalError("Failed to unload plugin: " +
                                                  result.error().message);
        }

        return ResponseBuilder::success({{"name", name}, {"unloaded", true}});
    }

    crow::response reloadPlugin(const nlohmann::json& body) {
        std::string name = body.value("name", "");
        if (name.empty()) {
            return ResponseBuilder::missingParameter("name");
        }

        nlohmann::json config = body.value("config", nlohmann::json::object());

        auto& loader = DevicePluginLoader::getInstance();
        auto result = loader.reloadPlugin(name, config);

        if (!result) {
            return ResponseBuilder::internalError("Failed to reload plugin: " +
                                                  result.error().message);
        }

        return ResponseBuilder::success({{"name", name}, {"reloaded", true}});
    }

    crow::response discoverAndLoadPlugins() {
        auto& loader = DevicePluginLoader::getInstance();
        size_t loaded = loader.loadAllPlugins();

        return ResponseBuilder::success({{"loaded", loaded}});
    }

    crow::response getHotPlugStatus() {
        auto& loader = DevicePluginLoader::getInstance();
        return ResponseBuilder::success(loader.getHotPlugStatus());
    }

    // ==================== Device Type Management ====================

    crow::response listDeviceTypes() {
        auto& registry = DeviceTypeRegistry::getInstance();
        auto types = registry.getAllTypes();

        nlohmann::json typeList = nlohmann::json::array();
        for (const auto& type : types) {
            typeList.push_back(type.toJson());
        }

        return ResponseBuilder::success(
            {{"types", typeList}, {"count", types.size()}});
    }

    crow::response getDeviceTypeInfo(const std::string& typeName) {
        auto& registry = DeviceTypeRegistry::getInstance();
        auto typeInfo = registry.getTypeInfo(typeName);

        if (!typeInfo) {
            return ResponseBuilder::notFound("Device type");
        }

        return ResponseBuilder::success(typeInfo->toJson());
    }

    crow::response getTypesByCategory(const std::string& category) {
        auto& registry = DeviceTypeRegistry::getInstance();
        auto types = registry.getTypesByCategory(category);

        nlohmann::json typeList = nlohmann::json::array();
        for (const auto& type : types) {
            typeList.push_back(type.toJson());
        }

        return ResponseBuilder::success(
            {{"category", category},
             {"types", typeList},
             {"count", types.size()}});
    }

    crow::response listDeviceCategories() {
        auto& registry = DeviceTypeRegistry::getInstance();
        auto categories = registry.getAllCategories();

        nlohmann::json categoryList = nlohmann::json::array();
        for (const auto& category : categories) {
            categoryList.push_back(category.toJson());
        }

        return ResponseBuilder::success(
            {{"categories", categoryList}, {"count", categories.size()}});
    }

    // ==================== Event Bus ====================

    crow::response getRecentEvents(size_t count) {
        auto& eventBus = DeviceEventBus::getInstance();
        auto events = eventBus.getRecentEvents(count);

        nlohmann::json eventList = nlohmann::json::array();
        for (const auto& event : events) {
            eventList.push_back(event.toJson());
        }

        return ResponseBuilder::success(
            {{"events", eventList}, {"count", events.size()}});
    }

    crow::response getEventStatistics() {
        auto& eventBus = DeviceEventBus::getInstance();
        return ResponseBuilder::success(eventBus.getStatistics());
    }

    // ==================== Statistics ====================

    crow::response getLoaderStatistics() {
        auto& loader = DevicePluginLoader::getInstance();
        return ResponseBuilder::success(loader.getStatistics());
    }

    crow::response getRegistryStatistics() {
        auto& registry = DeviceTypeRegistry::getInstance();
        return ResponseBuilder::success(registry.getStatistics());
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_DEVICE_PLUGIN_HPP
