/*
 * filterwheel.cpp - FilterWheel Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "filterwheel.hpp"

#include "atom/log/spdlog_logger.hpp"

#include "../command.hpp"
#include "../response.hpp"
#include "atom/type/json.hpp"
#include "device/service/filterwheel_service.hpp"

namespace lithium::app {

using command::CommandResponse;
using nlohmann::json;

// Global service instance
static lithium::device::FilterWheelService& getFilterWheelService() {
    static lithium::device::FilterWheelService instance;
    return instance;
}

void registerFilterWheel(std::shared_ptr<CommandDispatcher> dispatcher) {
    // FilterWheel: list
    dispatcher->registerCommand<json>("filterwheel.list", [](json& payload) {
        LOG_INFO("Executing command: filterwheel.list");

        try {
            auto result = getFilterWheelService().list();
            payload = result;
        } catch (const std::exception& e) {
            LOG_ERROR("filterwheel.list exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("filterwheel.list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'filterwheel.list'");

    // FilterWheel: status
    dispatcher->registerCommand<json>("filterwheel.status", [](json& payload) {
        LOG_INFO("Executing command: filterwheel.status");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("filterwheel.status: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getFilterWheelService().getStatus(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            LOG_ERROR("filterwheel.status exception for device {}: {}",
                      deviceId, e.what());
            payload = CommandResponse::operationFailed("filterwheel.status",
                                                       e.what());
        }
    });
    LOG_INFO("Registered command handler for 'filterwheel.status'");

    // FilterWheel: connect / disconnect
    dispatcher->registerCommand<json>("filterwheel.connect", [](json& payload) {
        LOG_INFO("Executing command: filterwheel.connect");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("filterwheel.connect: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("connected")) {
            LOG_WARN("filterwheel.connect: missing 'connected' for device {}",
                     deviceId);
            payload = CommandResponse::missingParameter("connected");
            return;
        }
        bool connected = payload["connected"].get<bool>();

        try {
            auto result = getFilterWheelService().connect(deviceId, connected);
            payload = result;
        } catch (const std::exception& e) {
            LOG_ERROR("filterwheel.connect exception for device {}: {}",
                      deviceId, e.what());
            payload = CommandResponse::operationFailed("filterwheel.connect",
                                                       e.what());
        }
    });
    LOG_INFO("Registered command handler for 'filterwheel.connect'");

    // FilterWheel: set position by slot
    dispatcher->registerCommand<json>(
        "filterwheel.set_position", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.set_position");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.set_position: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFilterWheelService().setPosition(deviceId, payload);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR(
                    "filterwheel.set_position exception for device {}: {}",
                    deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.set_position", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.set_position'");

    // FilterWheel: set position by filter name
    dispatcher->registerCommand<json>(
        "filterwheel.set_by_name", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.set_by_name");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.set_by_name: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFilterWheelService().setByName(deviceId, payload);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR("filterwheel.set_by_name exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.set_by_name", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.set_by_name'");

    // FilterWheel: capabilities
    dispatcher->registerCommand<json>(
        "filterwheel.capabilities", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.capabilities");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.capabilities: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result = getFilterWheelService().getCapabilities(deviceId);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR(
                    "filterwheel.capabilities exception for device {}: {}",
                    deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.capabilities", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.capabilities'");

    // FilterWheel: configure filter names
    dispatcher->registerCommand<json>(
        "filterwheel.configure_names", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.configure_names");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.configure_names: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFilterWheelService().configureNames(deviceId, payload);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR(
                    "filterwheel.configure_names exception for device {}: {}",
                    deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.configure_names", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.configure_names'");

    // FilterWheel: get offsets
    dispatcher->registerCommand<json>(
        "filterwheel.get_offsets", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.get_offsets");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.get_offsets: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result = getFilterWheelService().getOffsets(deviceId);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR("filterwheel.get_offsets exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.get_offsets", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.get_offsets'");

    // FilterWheel: set offsets
    dispatcher->registerCommand<json>(
        "filterwheel.set_offsets", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.set_offsets");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.set_offsets: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFilterWheelService().setOffsets(deviceId, payload);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR("filterwheel.set_offsets exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.set_offsets", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.set_offsets'");

    // FilterWheel: halt (currently feature_not_supported)
    dispatcher->registerCommand<json>("filterwheel.halt", [](json& payload) {
        LOG_INFO("Executing command: filterwheel.halt");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("filterwheel.halt: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getFilterWheelService().halt(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            LOG_ERROR("filterwheel.halt exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("filterwheel.halt", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'filterwheel.halt'");

    // FilterWheel: calibrate (currently feature_not_supported)
    dispatcher->registerCommand<json>(
        "filterwheel.calibrate", [](json& payload) {
            LOG_INFO("Executing command: filterwheel.calibrate");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("filterwheel.calibrate: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result = getFilterWheelService().calibrate(deviceId);
                payload = result;
            } catch (const std::exception& e) {
                LOG_ERROR("filterwheel.calibrate exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "filterwheel.calibrate", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'filterwheel.calibrate'");
}

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

using json = nlohmann::json;

namespace {
auto& getFilterWheelServiceInstance() {
    static lithium::device::FilterWheelService instance;
    return instance;
}
}  // namespace

auto listFilterWheels() -> json {
    return getFilterWheelServiceInstance().list();
}

auto getFilterWheelStatus(const std::string& deviceId) -> json {
    return getFilterWheelServiceInstance().getStatus(deviceId);
}

auto connectFilterWheel(const std::string& deviceId, bool connected) -> json {
    return getFilterWheelServiceInstance().connect(deviceId, connected);
}

auto setFilterPosition(const std::string& deviceId, const json& body) -> json {
    int position = body.value("position", 0);
    return getFilterWheelServiceInstance().setPosition(deviceId, position);
}

auto setFilterByName(const std::string& deviceId, const json& body) -> json {
    std::string name = body.value("name", "");
    return getFilterWheelServiceInstance().setByName(deviceId, name);
}

auto getFilterWheelCapabilities(const std::string& deviceId) -> json {
    return getFilterWheelServiceInstance().getCapabilities(deviceId);
}

auto configureFilterNames(const std::string& deviceId, const json& body)
    -> json {
    return getFilterWheelServiceInstance().configureNames(deviceId, body);
}

auto getFilterOffsets(const std::string& deviceId) -> json {
    return getFilterWheelServiceInstance().getOffsets(deviceId);
}

auto setFilterOffsets(const std::string& deviceId, const json& body) -> json {
    return getFilterWheelServiceInstance().setOffsets(deviceId, body);
}

auto haltFilterWheel(const std::string& deviceId) -> json {
    return getFilterWheelServiceInstance().halt(deviceId);
}

auto calibrateFilterWheel(const std::string& deviceId) -> json {
    return getFilterWheelServiceInstance().calibrate(deviceId);
}

}  // namespace lithium::middleware
