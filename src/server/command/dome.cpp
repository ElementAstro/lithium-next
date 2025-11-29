/*
 * dome.cpp - Dome Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "dome.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/dome_service.hpp"
#include "response.hpp"

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;

namespace {

auto& getDomeService() {
    static lithium::device::DomeService instance;
    return instance;
}

}  // namespace

void registerDome(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Dome: list
    dispatcher->registerCommand<json>("dome.list", [](json& payload) {
        LOG_INFO("Executing dome.list");

        try {
            auto result = getDomeService().list();
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.list failed");
                payload = result;
            } else {
                LOG_INFO("dome.list completed successfully");
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("dome.list", e.what());
        }
    });

    // Dome: status
    dispatcher->registerCommand<json>("dome.status", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.status: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.status for device: {}", deviceId);

        try {
            auto result = getDomeService().getStatus(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.status failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.status completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.status exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.status", e.what());
        }
    });

    // Dome: connect
    dispatcher->registerCommand<json>("dome.connect", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.connect: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        // Validate required connected parameter
        if (!payload.contains("connected")) {
            LOG_WARN("dome.connect: missing connected for device {}", deviceId);
            payload = CommandResponse::missingParameter("connected");
            return;
        }
        if (!payload["connected"].is_boolean()) {
            payload = CommandResponse::invalidParameter("connected",
                                                        "must be a boolean");
            return;
        }
        bool connected = payload["connected"].get<bool>();

        LOG_INFO("Executing dome.connect for device: {} (connected: {})",
                 deviceId, connected);

        try {
            auto result = getDomeService().connect(deviceId, connected);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.connect failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.connect completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.connect exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("dome.connect", e.what());
        }
    });

    // Dome: slew
    dispatcher->registerCommand<json>("dome.slew", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.slew: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        // Validate required azimuth parameter
        if (!payload.contains("azimuth")) {
            LOG_WARN("dome.slew: missing azimuth for device {}", deviceId);
            payload = CommandResponse::missingParameter("azimuth");
            return;
        }
        if (!payload["azimuth"].is_number()) {
            payload = CommandResponse::invalidParameter("azimuth",
                                                        "must be a number");
            return;
        }
        double az = payload["azimuth"].get<double>();

        LOG_INFO("Executing dome.slew for device: {} (azimuth: {})", deviceId,
                 az);

        try {
            auto result = getDomeService().slew(deviceId, az);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.slew failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.slew completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.slew exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.slew", e.what());
        }
    });

    // Dome: shutter
    dispatcher->registerCommand<json>("dome.shutter", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.shutter: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        // Validate required open parameter
        if (!payload.contains("open")) {
            LOG_WARN("dome.shutter: missing open for device {}", deviceId);
            payload = CommandResponse::missingParameter("open");
            return;
        }
        if (!payload["open"].is_boolean()) {
            payload =
                CommandResponse::invalidParameter("open", "must be a boolean");
            return;
        }
        bool open = payload["open"].get<bool>();

        LOG_INFO("Executing dome.shutter for device: {} (open: {})", deviceId,
                 open);

        try {
            auto result = getDomeService().shutterControl(deviceId, open);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.shutter failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.shutter completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.shutter exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("dome.shutter", e.what());
        }
    });

    // Dome: park
    dispatcher->registerCommand<json>("dome.park", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.park: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.park for device: {}", deviceId);

        try {
            auto result = getDomeService().park(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.park failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.park completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.park exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.park", e.what());
        }
    });

    // Dome: unpark
    dispatcher->registerCommand<json>("dome.unpark", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.unpark: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.unpark for device: {}", deviceId);

        try {
            auto result = getDomeService().unpark(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.unpark failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.unpark completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.unpark exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.unpark", e.what());
        }
    });

    // Dome: home
    dispatcher->registerCommand<json>("dome.home", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.home: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.home for device: {}", deviceId);

        try {
            auto result = getDomeService().home(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.home failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.home completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.home exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.home", e.what());
        }
    });

    // Dome: stop
    dispatcher->registerCommand<json>("dome.stop", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.stop: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.stop for device: {}", deviceId);

        try {
            auto result = getDomeService().stop(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.stop failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("dome.stop completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.stop exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("dome.stop", e.what());
        }
    });

    // Dome: capabilities
    dispatcher->registerCommand<json>("dome.capabilities", [](json& payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("dome.capabilities: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing dome.capabilities for device: {}", deviceId);

        try {
            auto result = getDomeService().getCapabilities(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("dome.capabilities failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO(
                    "dome.capabilities completed successfully for device "
                    "{}",
                    deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("dome.capabilities exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("dome.capabilities", e.what());
        }
    });
}

}  // namespace lithium::app
