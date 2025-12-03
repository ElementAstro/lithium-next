/*
 * camera.cpp - Camera Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "camera.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "../command.hpp"
#include "device/service/camera_service.hpp"
#include "../response.hpp"

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;

namespace {

auto& getCameraService() {
    static lithium::device::CameraService instance;
    return instance;
}

auto normalizeFrameType(const std::string& frameType)
    -> std::optional<std::string> {
    std::string lower = frameType;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "light") {
        return "Light";
    }
    if (lower == "dark") {
        return "Dark";
    }
    if (lower == "flat") {
        return "Flat";
    }
    if (lower == "bias") {
        return "Bias";
    }
    return std::nullopt;
}

}  // namespace

void registerCamera(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Camera: start exposure
    dispatcher->registerCommand<json>("camera.start_exposure", [](json&
                                                                      payload) {
        // Validate required deviceId
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("camera.start_exposure: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_INFO("Executing camera.start_exposure for device: {}", deviceId);

        // Validate required duration
        if (!payload.contains("duration")) {
            LOG_WARN("camera.start_exposure: missing duration for device {}",
                     deviceId);
            payload = CommandResponse::missingParameter("duration");
            return;
        }
        if (!payload["duration"].is_number()) {
            payload = CommandResponse::invalidParameter("duration",
                                                        "must be a number");
            return;
        }
        double duration = payload["duration"].get<double>();
        if (duration <= 0) {
            payload = CommandResponse::invalidParameter("duration",
                                                        "must be positive");
            return;
        }

        // Validate required frameType
        if (!payload.contains("frameType")) {
            LOG_WARN("camera.start_exposure: missing frameType for device {}",
                     deviceId);
            payload = CommandResponse::missingParameter("frameType");
            return;
        }
        if (!payload["frameType"].is_string()) {
            payload = CommandResponse::invalidParameter("frameType",
                                                        "must be a string");
            return;
        }

        auto normalizedType =
            normalizeFrameType(payload["frameType"].get<std::string>());
        if (!normalizedType) {
            payload = CommandResponse::invalidParameter(
                "frameType", "must be one of: Light, Dark, Flat, Bias");
            return;
        }
        std::string frameType = *normalizedType;

        std::string filename = payload.value("filename", std::string(""));

        // Apply optional settings (binning/gain/offset)
        json settings = json::object();
        if (payload.contains("binning")) {
            settings["binning"] = payload["binning"];
        }
        if (payload.contains("gain")) {
            settings["gain"] = payload["gain"];
        }
        if (payload.contains("offset")) {
            settings["offset"] = payload["offset"];
        }

        try {
            if (!settings.empty()) {
                auto settingsResult =
                    getCameraService().updateSettings(deviceId, settings);
                if (settingsResult.contains("status") &&
                    settingsResult["status"] == "error") {
                    LOG_ERROR(
                        "camera.start_exposure: failed to update settings "
                        "for device {}",
                        deviceId);
                    payload = settingsResult;
                    return;
                }
            }

            auto result = getCameraService().startExposure(deviceId, duration,
                                                           frameType, filename);

            if (result.contains("status") && result["status"] == "error") {
                LOG_ERROR("camera.start_exposure failed for device {}",
                          deviceId);
                payload = result;
            } else {
                LOG_INFO(
                    "camera.start_exposure completed successfully for "
                    "device {}",
                    deviceId);
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("camera.start_exposure exception for device {}: {}",
                      deviceId, e.what());
            payload =
                CommandResponse::operationFailed("start_exposure", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'camera.start_exposure'");

    // Camera: abort exposure
    dispatcher->registerCommand<json>(
        "camera.abort_exposure", [](json& payload) {
            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                LOG_WARN("camera.abort_exposure: missing deviceId");
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            LOG_INFO("Executing camera.abort_exposure for device: {}",
                     deviceId);

            try {
                auto result = getCameraService().abortExposure(deviceId);
                if (result.contains("status") && result["status"] == "error") {
                    LOG_ERROR("camera.abort_exposure failed for device {}",
                              deviceId);
                    payload = result;
                } else {
                    LOG_INFO(
                        "camera.abort_exposure completed successfully for "
                        "device {}",
                        deviceId);
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("camera.abort_exposure exception for device {}: {}",
                          deviceId, e.what());
                payload = CommandResponse::operationFailed("abort_exposure",
                                                           e.what());
            }
        });
    LOG_INFO("Registered command handler for 'camera.abort_exposure'");

    // Camera: get status
    dispatcher->registerCommand<json>("camera.status", [](json& payload) {
        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            LOG_WARN("camera.status: missing deviceId");
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        LOG_DEBUG("Executing camera.status for device: {}", deviceId);

        try {
            auto result = getCameraService().getStatus(deviceId);
            if (result.contains("status") && result["status"] == "error") {
                LOG_WARN("camera.status failed for device {}", deviceId);
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("camera.status exception for device {}: {}", deviceId,
                      e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'camera.status'");
}

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

using json = nlohmann::json;

namespace {
auto& getCameraServiceInstance() {
    static lithium::device::CameraService instance;
    return instance;
}
}  // namespace

auto listCameras() -> json { return getCameraServiceInstance().list(); }

auto getCameraStatus(const std::string& deviceId) -> json {
    return getCameraServiceInstance().getStatus(deviceId);
}

auto connectCamera(const std::string& deviceId, bool connected) -> json {
    return getCameraServiceInstance().connect(deviceId, connected);
}

auto updateCameraSettings(const std::string& deviceId, const json& settings)
    -> json {
    return getCameraServiceInstance().updateSettings(deviceId, settings);
}

auto startExposure(const std::string& deviceId, const json& params) -> json {
    return getCameraServiceInstance().startExposure(deviceId, params);
}

auto abortExposure(const std::string& deviceId) -> json {
    return getCameraServiceInstance().abortExposure(deviceId);
}

auto getCameraCapabilities(const std::string& deviceId) -> json {
    return getCameraServiceInstance().getCapabilities(deviceId);
}

auto getCameraGains(const std::string& deviceId) -> json {
    return getCameraServiceInstance().getGains(deviceId);
}

auto getCameraOffsets(const std::string& deviceId) -> json {
    return getCameraServiceInstance().getOffsets(deviceId);
}

auto setCoolerPower(const std::string& deviceId, const json& body) -> json {
    return getCameraServiceInstance().setCoolerPower(deviceId, body);
}

auto warmupCamera(const std::string& deviceId) -> json {
    return getCameraServiceInstance().warmup(deviceId);
}

}  // namespace lithium::middleware

