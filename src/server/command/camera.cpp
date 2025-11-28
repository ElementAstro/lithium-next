#include "camera.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/camera_service.hpp"

namespace lithium::app {

using nlohmann::json;

// Global service instance
static lithium::device::CameraService& getCameraService() {
    static lithium::device::CameraService instance;
    return instance;
}

void registerCamera(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Camera: start exposure
    dispatcher->registerCommand<json>(
        "camera.start_exposure",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("cam-001"));

            if (!p.contains("duration")) {
                throw std::runtime_error(
                    "camera.start_exposure: missing 'duration'");
            }
            double duration = p["duration"].get<double>();

            if (!p.contains("frameType")) {
                throw std::runtime_error(
                    "camera.start_exposure: missing 'frameType'");
            }
            if (!p["frameType"].is_string()) {
                throw std::runtime_error(
                    "camera.start_exposure: 'frameType' must be a string");
            }

            std::string frameType = p["frameType"].get<std::string>();

            if (frameType == "light" || frameType == "LIGHT") {
                frameType = "Light";
            } else if (frameType == "dark" || frameType == "DARK") {
                frameType = "Dark";
            } else if (frameType == "flat" || frameType == "FLAT") {
                frameType = "Flat";
            } else if (frameType == "bias" || frameType == "BIAS") {
                frameType = "Bias";
            }

            if (frameType != "Light" && frameType != "Dark" &&
                frameType != "Flat" && frameType != "Bias") {
                throw std::runtime_error(
                    "camera.start_exposure: 'frameType' must be one of: Light, Dark, Flat, Bias");
            }

            std::string filename = p.value("filename", std::string(""));

            // Apply optional settings (binning/gain/offset) via shared middleware
            // implementation to keep HTTP and WebSocket behaviour consistent.
            json settings = json::object();
            if (p.contains("binning")) {
                settings["binning"] = p["binning"];
            }
            if (p.contains("gain")) {
                settings["gain"] = p["gain"];
            }
            if (p.contains("offset")) {
                settings["offset"] = p["offset"];
            }

            if (!settings.empty()) {
                auto settingsResult =
                    getCameraService().updateSettings(deviceId, settings);
                if (settingsResult.contains("status") &&
                    settingsResult["status"] == "error") {
                    p = settingsResult;
                    return;
                }
            }

            auto result = getCameraService().startExposure(
                deviceId, duration, frameType, filename);
            p = result;
        });
    spdlog::info("Registered command handler for 'camera.start_exposure'");

    // Camera: abort exposure
    dispatcher->registerCommand<json>(
        "camera.abort_exposure",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("cam-001"));
            auto result = getCameraService().abortExposure(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'camera.abort_exposure'");

    // Camera: get status
    dispatcher->registerCommand<json>(
        "camera.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("cam-001"));
            auto result = getCameraService().getStatus(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'camera.status'");
}

}  // namespace lithium::app
