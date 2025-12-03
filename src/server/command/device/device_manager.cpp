/*
 * device_manager.cpp - Device Manager Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "device_manager.hpp"

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "../command.hpp"
#include "constant/constant.hpp"
#include "device/manager.hpp"
#include "../response.hpp"
#include "server/models/device.hpp"

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;
using namespace lithium::models::device;

namespace {

auto& getDeviceManager() {
    static std::shared_ptr<lithium::DeviceManager> manager;
    if (!manager) {
        GET_OR_CREATE_PTR(manager, lithium::DeviceManager,
                          Constants::DEVICE_MANAGER)
    }
    return *manager;
}

}  // namespace

void registerDeviceManager(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Device: list all devices
    dispatcher->registerCommand<json>("device.list", [](json& payload) {
        LOG_DEBUG("Executing device.list");

        try {
            auto devices = getDeviceManager().getDevices();
            std::vector<DeviceSummary> summaries;

            for (const auto& [type, deviceList] : devices) {
                for (const auto& device : deviceList) {
                    DeviceSummary summary;
                    summary.deviceId = device->getUUID();
                    summary.name = device->getName();
                    summary.type = stringToDeviceType(type);
                    summary.status = device->isConnected()
                                         ? ConnectionStatus::Connected
                                         : ConnectionStatus::Disconnected;
                    summary.driver = type;

                    // Add metadata as description if available
                    auto meta =
                        getDeviceManager().getDeviceMetadata(device->getName());
                    if (meta) {
                        summary.description = meta->toJson().dump();
                    }

                    summaries.push_back(summary);
                }
            }

            payload = CommandResponse::success(makeDeviceListResponse(summaries));
        } catch (const std::exception& e) {
            LOG_ERROR("device.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.list'");

    // Device: get status
    dispatcher->registerCommand<json>("device.status", [](json& payload) {
        LOG_DEBUG("Executing device.status");

        try {
            auto status = getDeviceManager().getStatus();
            payload = CommandResponse::success(status);
        } catch (const std::exception& e) {
            LOG_ERROR("device.status exception: {}", e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.status'");

    // Device: connect by name
    dispatcher->registerCommand<json>("device.connect", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string() ||
            payload["name"].get<std::string>().empty()) {
            LOG_WARN("device.connect: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }
        std::string name = payload["name"].get<std::string>();
        int timeout = payload.value("timeout", 5000);

        LOG_INFO("Executing device.connect for: {}", name);

        try {
            if (payload.value("async", false)) {
                auto future =
                    getDeviceManager().connectDeviceAsync(name, timeout);
                // For async, return immediately with pending status
                payload = CommandResponse::success(
                    makeConnectionResult(name, true, "Connection initiated asynchronously"));
            } else {
                getDeviceManager().connectDeviceByName(name);
                payload = CommandResponse::success(
                    makeConnectionResult(name, true, "Connected successfully"));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("device.connect exception for {}: {}", name, e.what());
            payload = CommandResponse::operationFailed("connect", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.connect'");

    // Device: disconnect by name
    dispatcher->registerCommand<json>("device.disconnect", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string() ||
            payload["name"].get<std::string>().empty()) {
            LOG_WARN("device.disconnect: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }
        std::string name = payload["name"].get<std::string>();

        LOG_INFO("Executing device.disconnect for: {}", name);

        try {
            if (payload.value("async", false)) {
                auto future = getDeviceManager().disconnectDeviceAsync(name);
                payload = CommandResponse::success(makeConnectionResult(
                    name, true, "Disconnection initiated asynchronously"));
            } else {
                getDeviceManager().disconnectDeviceByName(name);
                payload = CommandResponse::success(
                    makeConnectionResult(name, true, "Disconnected successfully"));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("device.disconnect exception for {}: {}", name, e.what());
            payload = CommandResponse::operationFailed("disconnect", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.disconnect'");

    // Device: connect batch
    dispatcher->registerCommand<json>("device.connect_batch", [](json& payload) {
        if (!payload.contains("names") || !payload["names"].is_array()) {
            LOG_WARN("device.connect_batch: missing names array");
            payload = CommandResponse::missingParameter("names");
            return;
        }

        std::vector<std::string> names;
        for (const auto& name : payload["names"]) {
            if (name.is_string()) {
                names.push_back(name.get<std::string>());
            }
        }

        if (names.empty()) {
            payload = CommandResponse::invalidParameter("names", "must contain at least one device name");
            return;
        }

        int timeout = payload.value("timeout", 5000);

        LOG_INFO("Executing device.connect_batch for {} devices", names.size());

        try {
            auto results = getDeviceManager().connectDevicesBatch(names, timeout);
            json resultJson = json::array();
            int successCount = 0;
            for (const auto& [name, success] : results) {
                json item;
                item["name"] = name;
                item["success"] = success;
                resultJson.push_back(item);
                if (success) successCount++;
            }

            json response;
            response["results"] = resultJson;
            response["totalDevices"] = names.size();
            response["successCount"] = successCount;
            response["failureCount"] = names.size() - successCount;
            payload = CommandResponse::success(response);
        } catch (const std::exception& e) {
            LOG_ERROR("device.connect_batch exception: {}", e.what());
            payload = CommandResponse::operationFailed("connect_batch", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.connect_batch'");

    // Device: disconnect batch
    dispatcher->registerCommand<json>("device.disconnect_batch", [](json& payload) {
        if (!payload.contains("names") || !payload["names"].is_array()) {
            LOG_WARN("device.disconnect_batch: missing names array");
            payload = CommandResponse::missingParameter("names");
            return;
        }

        std::vector<std::string> names;
        for (const auto& name : payload["names"]) {
            if (name.is_string()) {
                names.push_back(name.get<std::string>());
            }
        }

        LOG_INFO("Executing device.disconnect_batch for {} devices", names.size());

        try {
            auto results = getDeviceManager().disconnectDevicesBatch(names);
            json resultJson = json::array();
            int successCount = 0;
            for (const auto& [name, success] : results) {
                json item;
                item["name"] = name;
                item["success"] = success;
                resultJson.push_back(item);
                if (success) successCount++;
            }

            json response;
            response["results"] = resultJson;
            response["totalDevices"] = names.size();
            response["successCount"] = successCount;
            response["failureCount"] = names.size() - successCount;
            payload = CommandResponse::success(response);
        } catch (const std::exception& e) {
            LOG_ERROR("device.disconnect_batch exception: {}", e.what());
            payload = CommandResponse::operationFailed("disconnect_batch", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.disconnect_batch'");

    // Device: get health
    dispatcher->registerCommand<json>("device.health", [](json& payload) {
        LOG_DEBUG("Executing device.health");

        try {
            if (payload.contains("name") && payload["name"].is_string()) {
                // Get health for specific device
                std::string name = payload["name"].get<std::string>();
                float health = getDeviceManager().getDeviceHealth(name);
                auto state = getDeviceManager().getDeviceState(name);

                json result;
                result["device"] = name;
                result["healthScore"] = health;
                if (state) {
                    result["state"] = state->toJson();
                }
                payload = CommandResponse::success(result);
            } else {
                // Get health for all devices
                auto healthReport = getDeviceManager().checkAllDevicesHealth();
                payload = CommandResponse::success(healthReport);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("device.health exception: {}", e.what());
            payload = CommandResponse::operationFailed("health", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.health'");

    // Device: get unhealthy devices
    dispatcher->registerCommand<json>("device.unhealthy", [](json& payload) {
        LOG_DEBUG("Executing device.unhealthy");

        try {
            float threshold = payload.value("threshold", 0.5f);
            auto unhealthy = getDeviceManager().getUnhealthyDevices(threshold);

            json result;
            result["threshold"] = threshold;
            result["devices"] = unhealthy;
            result["count"] = unhealthy.size();
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.unhealthy exception: {}", e.what());
            payload = CommandResponse::operationFailed("unhealthy", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.unhealthy'");

    // Device: get statistics
    dispatcher->registerCommand<json>("device.statistics", [](json& payload) {
        LOG_DEBUG("Executing device.statistics");

        try {
            auto stats = getDeviceManager().getStatistics();
            payload = CommandResponse::success(stats);
        } catch (const std::exception& e) {
            LOG_ERROR("device.statistics exception: {}", e.what());
            payload = CommandResponse::operationFailed("statistics", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.statistics'");

    // Device: reset statistics
    dispatcher->registerCommand<json>("device.reset_statistics", [](json& payload) {
        LOG_INFO("Executing device.reset_statistics");

        try {
            getDeviceManager().resetStatistics();
            payload = CommandResponse::success("Statistics reset successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("device.reset_statistics exception: {}", e.what());
            payload = CommandResponse::operationFailed("reset_statistics", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.reset_statistics'");

    // Device: set retry config
    dispatcher->registerCommand<json>("device.set_retry_config", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string()) {
            LOG_WARN("device.set_retry_config: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }
        std::string name = payload["name"].get<std::string>();

        LOG_INFO("Executing device.set_retry_config for: {}", name);

        try {
            lithium::DeviceRetryConfig config;

            if (payload.contains("strategy")) {
                int strategyInt = payload["strategy"].get<int>();
                config.strategy = static_cast<lithium::DeviceRetryConfig::Strategy>(strategyInt);
            }
            if (payload.contains("maxRetries")) {
                config.maxRetries = payload["maxRetries"].get<int>();
            }
            if (payload.contains("initialDelayMs")) {
                config.initialDelay = std::chrono::milliseconds(
                    payload["initialDelayMs"].get<int>());
            }
            if (payload.contains("maxDelayMs")) {
                config.maxDelay = std::chrono::milliseconds(
                    payload["maxDelayMs"].get<int>());
            }
            if (payload.contains("multiplier")) {
                config.multiplier = payload["multiplier"].get<float>();
            }

            getDeviceManager().setDeviceRetryConfig(name, config);

            json result;
            result["device"] = name;
            result["config"] = config.toJson();
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.set_retry_config exception for {}: {}", name, e.what());
            payload = CommandResponse::operationFailed("set_retry_config", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.set_retry_config'");

    // Device: get retry config
    dispatcher->registerCommand<json>("device.get_retry_config", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string()) {
            LOG_WARN("device.get_retry_config: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }
        std::string name = payload["name"].get<std::string>();

        LOG_DEBUG("Executing device.get_retry_config for: {}", name);

        try {
            auto config = getDeviceManager().getDeviceRetryConfig(name);
            json result;
            result["device"] = name;
            result["config"] = config.toJson();
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.get_retry_config exception for {}: {}", name, e.what());
            payload = CommandResponse::operationFailed("get_retry_config", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.get_retry_config'");

    // Device: reset device
    dispatcher->registerCommand<json>("device.reset", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string()) {
            LOG_WARN("device.reset: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }
        std::string name = payload["name"].get<std::string>();

        LOG_INFO("Executing device.reset for: {}", name);

        try {
            getDeviceManager().resetDevice(name);
            json result;
            result["device"] = name;
            result["message"] = "Device reset successfully";
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.reset exception for {}: {}", name, e.what());
            payload = CommandResponse::operationFailed("reset", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.reset'");

    // Device: start health monitor
    dispatcher->registerCommand<json>("device.start_health_monitor", [](json& payload) {
        int intervalSeconds = payload.value("interval", 30);

        LOG_INFO("Executing device.start_health_monitor with interval {}s", intervalSeconds);

        try {
            getDeviceManager().startHealthMonitor(std::chrono::seconds(intervalSeconds));
            json result;
            result["message"] = "Health monitor started";
            result["interval"] = intervalSeconds;
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.start_health_monitor exception: {}", e.what());
            payload = CommandResponse::operationFailed("start_health_monitor", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.start_health_monitor'");

    // Device: stop health monitor
    dispatcher->registerCommand<json>("device.stop_health_monitor", [](json& payload) {
        LOG_INFO("Executing device.stop_health_monitor");

        try {
            getDeviceManager().stopHealthMonitor();
            payload = CommandResponse::success("Health monitor stopped");
        } catch (const std::exception& e) {
            LOG_ERROR("device.stop_health_monitor exception: {}", e.what());
            payload = CommandResponse::operationFailed("stop_health_monitor", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.stop_health_monitor'");

    // Device: get pending events
    dispatcher->registerCommand<json>("device.get_events", [](json& payload) {
        size_t maxEvents = payload.value("maxEvents", 100);

        LOG_DEBUG("Executing device.get_events (max: {})", maxEvents);

        try {
            auto events = getDeviceManager().getPendingEvents(maxEvents);
            json result = json::array();
            for (const auto& event : events) {
                result.push_back(event.toJson());
            }
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("device.get_events exception: {}", e.what());
            payload = CommandResponse::operationFailed("get_events", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.get_events'");

    // Device: clear pending events
    dispatcher->registerCommand<json>("device.clear_events", [](json& payload) {
        LOG_INFO("Executing device.clear_events");

        try {
            getDeviceManager().clearPendingEvents();
            payload = CommandResponse::success("Events cleared");
        } catch (const std::exception& e) {
            LOG_ERROR("device.clear_events exception: {}", e.what());
            payload = CommandResponse::operationFailed("clear_events", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.clear_events'");

    // Device: export configuration
    dispatcher->registerCommand<json>("device.export_config", [](json& payload) {
        LOG_DEBUG("Executing device.export_config");

        try {
            auto config = getDeviceManager().exportConfiguration();
            payload = CommandResponse::success(config);
        } catch (const std::exception& e) {
            LOG_ERROR("device.export_config exception: {}", e.what());
            payload = CommandResponse::operationFailed("export_config", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.export_config'");

    // Device: import configuration
    dispatcher->registerCommand<json>("device.import_config", [](json& payload) {
        if (!payload.contains("config") || !payload["config"].is_object()) {
            LOG_WARN("device.import_config: missing config object");
            payload = CommandResponse::missingParameter("config");
            return;
        }

        LOG_INFO("Executing device.import_config");

        try {
            getDeviceManager().importConfiguration(payload["config"]);
            payload = CommandResponse::success("Configuration imported successfully");
        } catch (const std::exception& e) {
            LOG_ERROR("device.import_config exception: {}", e.what());
            payload = CommandResponse::operationFailed("import_config", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.import_config'");

    // Device: refresh devices
    dispatcher->registerCommand<json>("device.refresh", [](json& payload) {
        LOG_INFO("Executing device.refresh");

        try {
            getDeviceManager().refreshDevices();
            auto status = getDeviceManager().getStatus();
            payload = CommandResponse::success(status);
        } catch (const std::exception& e) {
            LOG_ERROR("device.refresh exception: {}", e.what());
            payload = CommandResponse::operationFailed("refresh", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'device.refresh'");
}

}  // namespace lithium::app

