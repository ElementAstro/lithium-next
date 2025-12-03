/*
 * focuser.cpp - Focuser Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "focuser.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "../command.hpp"
#include "device/service/focuser_service.hpp"
#include "../response.hpp"

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;

namespace {

auto& getFocuserService() {
    static lithium::device::FocuserService instance;
    return instance;
}

}  // namespace

void registerFocuser(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Focuser: list
    dispatcher->registerCommand<json>("focuser.list", [](json& payload) {
        LOG_INFO("Executing focuser.list");

        try {
            auto result = getFocuserService().list();

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("focuser.list failed");
                payload = result;
            } else {
                LOG_INFO("focuser.list completed successfully");
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("focuser.list exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("focuser.list", e.what());
        }
    });

    // Focuser: status
    dispatcher->registerCommand<json>("focuser.status", [](json& payload) {
        LOG_INFO("Executing focuser.status");

        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("focuser.status: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getFocuserService().getStatus(deviceId);

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("focuser.status failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("focuser.status completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("focuser.status exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("focuser.status", e.what());
        }
    });

    // Focuser: connect / disconnect
    dispatcher->registerCommand<json>("focuser.connect", [](json& payload) {
        LOG_INFO("Executing focuser.connect");

        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("focuser.connect: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        // Validate required connected parameter
        if (!payload.contains("connected")) {
            LOG_WARN("focuser.connect: missing connected for device {}",
                     deviceId);
            payload = CommandResponse::missingParameter("connected");
            return;
        }
        if (!payload["connected"].is_boolean()) {
            payload = CommandResponse::invalidParameter("connected",
                                                        "must be a boolean");
            return;
        }
        bool connected = payload["connected"].get<bool>();

        try {
            auto result = getFocuserService().connect(deviceId, connected);

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("focuser.connect failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("focuser.connect completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("focuser.connect exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("focuser.connect", e.what());
        }
    });

    // Focuser: move (absolute or relative)
    dispatcher->registerCommand<json>("focuser.move", [](json& payload) {
        LOG_INFO("Executing focuser.move");

        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("focuser.move: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getFocuserService().move(deviceId, payload);

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("focuser.move failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("focuser.move completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("focuser.move exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("focuser.move", e.what());
        }
    });

    // Focuser: update settings
    dispatcher->registerCommand<json>(
        "focuser.update_settings", [](json& payload) {
            LOG_INFO("Executing focuser.update_settings");

            // Validate required deviceId
            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("focuser.update_settings: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFocuserService().updateSettings(deviceId, payload);

                if (result.contains("status") && result["status"] == "error") {
                    LOG_ERROR("focuser.update_settings failed for device {}",
                              deviceId);
                    payload = result;
                } else {
                    LOG_INFO(
                        "focuser.update_settings completed successfully for "
                        "device {}",
                        deviceId);
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("focuser.update_settings exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "focuser.update_settings", e.what());
            }
        });

    // Focuser: halt
    dispatcher->registerCommand<json>("focuser.halt", [](json& payload) {
        LOG_INFO("Executing focuser.halt");

        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("focuser.halt: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getFocuserService().halt(deviceId);

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("focuser.halt failed for device {}", deviceId);
                payload = result;
            } else {
                LOG_INFO("focuser.halt completed successfully for device {}",
                         deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("focuser.halt exception for device {}: {}", deviceId,
                      e.what());
            payload =
                CommandResponse::operationFailed("focuser.halt", e.what());
        }
    });

    // Focuser: capabilities
    dispatcher->registerCommand<json>(
        "focuser.capabilities", [](json& payload) {
            LOG_INFO("Executing focuser.capabilities");

            // Validate required deviceId
            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("focuser.capabilities: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result = getFocuserService().getCapabilities(deviceId);

                if (result.contains("status") && result["status"] == "error") {
                    LOG_ERROR("focuser.capabilities failed for device {}",
                              deviceId);
                    payload = result;
                } else {
                    LOG_INFO(
                        "focuser.capabilities completed successfully for "
                        "device {}",
                        deviceId);
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("focuser.capabilities exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "focuser.capabilities", e.what());
            }
        });

    // Focuser: start autofocus
    dispatcher->registerCommand<json>(
        "focuser.autofocus_start", [](json& payload) {
            LOG_INFO("Executing focuser.autofocus_start");

            // Validate required deviceId
            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("focuser.autofocus_start: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            try {
                auto result =
                    getFocuserService().startAutofocus(deviceId, payload);

                if (result.contains("status") && result["status"] == "error") {
                    LOG_ERROR("focuser.autofocus_start failed for device {}",
                              deviceId);
                    payload = result;
                } else {
                    LOG_INFO(
                        "focuser.autofocus_start completed successfully for "
                        "device {}",
                        deviceId);
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("focuser.autofocus_start exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "focuser.autofocus_start", e.what());
            }
        });

    // Focuser: autofocus status
    dispatcher->registerCommand<json>(
        "focuser.autofocus_status", [](json& payload) {
            LOG_INFO("Executing focuser.autofocus_status");

            // Validate required deviceId
            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("focuser.autofocus_status: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            // autofocusId is optional
            std::string autofocusId =
                payload.value("autofocusId", std::string(""));

            try {
                auto result = getFocuserService().getAutofocusStatus(
                    deviceId, autofocusId);

                if (result.contains("status") && result["status"] == "error") {
                    LOG_ERROR("focuser.autofocus_status failed for device {}",
                              deviceId);
                    payload = result;
                } else {
                    LOG_INFO(
                        "focuser.autofocus_status completed successfully for "
                        "device {}",
                        deviceId);
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR(
                    "focuser.autofocus_status exception for device {}: {}",
                    deviceId, e.what());
                payload = CommandResponse::operationFailed(
                    "focuser.autofocus_status", e.what());
            }
        });
}

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

using json = nlohmann::json;

namespace {
auto& getFocuserServiceInstance() {
    static lithium::device::FocuserService instance;
    return instance;
}
}  // namespace

auto listFocusers() -> json { return getFocuserServiceInstance().list(); }

auto getFocuserStatus(const std::string& deviceId) -> json {
    return getFocuserServiceInstance().getStatus(deviceId);
}

auto connectFocuser(const std::string& deviceId, bool connected) -> json {
    return getFocuserServiceInstance().connect(deviceId, connected);
}

auto moveFocuser(const std::string& deviceId, const json& body) -> json {
    return getFocuserServiceInstance().move(deviceId, body);
}

auto updateFocuserSettings(const std::string& deviceId, const json& body)
    -> json {
    return getFocuserServiceInstance().updateSettings(deviceId, body);
}

auto haltFocuser(const std::string& deviceId) -> json {
    return getFocuserServiceInstance().halt(deviceId);
}

auto getFocuserCapabilities(const std::string& deviceId) -> json {
    return getFocuserServiceInstance().getCapabilities(deviceId);
}

auto startAutofocus(const std::string& deviceId, const json& body) -> json {
    return getFocuserServiceInstance().startAutofocus(deviceId, body);
}

auto getAutofocusStatus(const std::string& deviceId, const std::string& taskId)
    -> json {
    return getFocuserServiceInstance().getAutofocusStatus(deviceId, taskId);
}

}  // namespace lithium::middleware

