/*
 * camera_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "camera_service.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"

#include "config/config.hpp"
#include "constant/constant.hpp"
#include "device/template/camera.hpp"
#include "server/models/camera.hpp"

namespace lithium::device {

class CameraService::Impl {
public:
    // Tracks the last requested cooling setpoint
    std::optional<double> lastCoolingSetpoint;

    // Frame type names
    std::vector<std::string> frameTypes = {"Light", "Dark", "Flat", "Bias"};

    // Readout modes (populated from device)
    std::vector<std::pair<int, std::string>> readoutModes;
};

CameraService::CameraService()
    : TypedDeviceService("CameraService", "Camera", Constants::MAIN_CAMERA),
      impl_(std::make_unique<Impl>()) {}

CameraService::~CameraService() = default;

auto CameraService::list() -> json {
    LOG_INFO("CameraService::list: Listing all available cameras");
    json response;
    response["status"] = "success";

    try {
        std::shared_ptr<ConfigManager> configManager;
        GET_OR_CREATE_PTR(configManager, ConfigManager,
                          Constants::CONFIG_MANAGER)

        auto cameraList = json::array();

        std::shared_ptr<AtomCamera> camera;
        try {
            GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
            cameraList.push_back(lithium::models::camera::makeCameraSummary(
                "cam-001", camera->getName(), camera->isConnected()));
        } catch (...) {
            LOG_WARN("CameraService::list: Main camera not available");
        }

        response["data"] = cameraList;
    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::list: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::list: Completed");
    return response;
}

auto CameraService::getStatus(const std::string& deviceId) -> json {
    LOG_INFO("CameraService::getStatus: Getting status for camera: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_not_connected"},
                {"message", "Camera is not connected"},
            };
            return response;
        }

        json data = lithium::models::camera::makeCameraStatusData(
            *camera, impl_->lastCoolingSetpoint);

        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::getStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::getStatus: Completed");
    return response;
}

auto CameraService::connect(const std::string& deviceId, bool connected)
    -> json {
    LOG_INFO("CameraService::connect: %s camera: %s",
             connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        bool success =
            connected ? camera->connect("", "") : camera->disconnect();

        if (success) {
            response["status"] = "success";
            response["message"] =
                connected ? "Camera connection process initiated."
                          : "Camera disconnection process initiated.";

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish(
                "main", "CameraConnection:{}"_fmt(connected ? "ON" : "OFF"));
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "connection_failed"},
                {"message", "Connection operation failed."},
            };
        }

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::connect: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::connect: Completed");
    return response;
}

auto CameraService::updateSettings(const std::string& deviceId,
                                   const json& settings) -> json {
    LOG_INFO("CameraService::updateSettings: Updating settings for camera: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Camera is not connected"}};
            return response;
        }

        // Update cooler settings
        if (settings.contains("coolerOn")) {
            bool coolerOn = settings["coolerOn"];
            if (coolerOn && settings.contains("setpoint")) {
                double setpoint = settings["setpoint"];
                camera->startCooling(setpoint);
                impl_->lastCoolingSetpoint = setpoint;
            } else if (!coolerOn) {
                camera->stopCooling();
                impl_->lastCoolingSetpoint.reset();
            }
        }

        // Update gain
        if (settings.contains("gain")) {
            int gain = settings["gain"];
            camera->setGain(gain);
        }

        // Update offset
        if (settings.contains("offset")) {
            int offset = settings["offset"];
            camera->setOffset(offset);
        }

        // Update binning
        if (settings.contains("binning")) {
            int binX = settings["binning"]["x"];
            int binY = settings["binning"]["y"];
            camera->setBinning(binX, binY);
        }

        // Update ROI
        if (settings.contains("roi")) {
            int x = settings["roi"]["x"];
            int y = settings["roi"]["y"];
            int width = settings["roi"]["width"];
            int height = settings["roi"]["height"];
            camera->setResolution(x, y, width, height);
        }

        response["status"] = "success";
        response["message"] = "Camera settings update initiated.";

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::updateSettings: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::updateSettings: Completed");
    return response;
}

auto CameraService::startExposure(const std::string& deviceId, double duration,
                                  const std::string& frameType,
                                  const std::string& filename) -> json {
    LOG_INFO(
        "CameraService::startExposure: Starting %f second exposure on camera: "
        "%s",
        duration, deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Camera is not connected"}};
            return response;
        }

        if (camera->isExposing()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_busy"},
                {"message", "Camera is already exposing"},
            };
            return response;
        }

        // NOTE: frameType (Light/Dark/Flat/Bias) is currently advisory only.
        (void)frameType;
        (void)filename;

        bool success = camera->startExposure(duration);

        if (success) {
            std::string exposureId =
                "exp_" + std::to_string(std::chrono::system_clock::now()
                                            .time_since_epoch()
                                            .count());

            response["status"] = "success";
            response["data"] = {{"exposureId", exposureId}};
            response["message"] = "Exposure started.";

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish("main",
                                   "ExposureStarted:{}"_fmt(exposureId));
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "exposure_failed"},
                {"message", "Failed to start exposure."},
            };
        }

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::startExposure: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::startExposure: Completed");
    return response;
}

auto CameraService::abortExposure(const std::string& deviceId) -> json {
    LOG_INFO("CameraService::abortExposure: Aborting exposure on camera: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        bool success = camera->abortExposure();

        if (success) {
            response["status"] = "success";
            response["message"] = "Exposure abort command sent.";

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish("main", "ExposureAborted");
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "exposure_abort_failed"},
                {"message", "Failed to abort exposure."},
            };
        }

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::abortExposure: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::abortExposure: Completed");
    return response;
}

auto CameraService::getCapabilities(const std::string& deviceId) -> json {
    LOG_INFO(
        "CameraService::getCapabilities: Getting capabilities for camera: %s",
        deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                 {"message", "Camera is not connected"}};
            return response;
        }

        json data =
            lithium::models::camera::makeCameraCapabilitiesData(*camera);

        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::getCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::getCapabilities: Completed");
    return response;
}

auto CameraService::getGains(const std::string& deviceId) -> json {
    LOG_INFO("CameraService::getGains: Getting available gains for camera: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        std::vector<int> gains;
        gains.reserve(13);
        for (int i = 0; i <= 600; i += 50) {
            gains.push_back(i);
        }

        json data = lithium::models::camera::makeGainsData(*camera, gains);

        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::getGains: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::getGains: Completed");
    return response;
}

auto CameraService::getOffsets(const std::string& deviceId) -> json {
    LOG_INFO(
        "CameraService::getOffsets: Getting available offsets for camera: %s",
        deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        std::vector<int> offsets;
        offsets.reserve(11);
        for (int i = 0; i <= 100; i += 10) {
            offsets.push_back(i);
        }

        json data = lithium::models::camera::makeOffsetsData(*camera, offsets);

        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::getOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::getOffsets: Completed");
    return response;
}

auto CameraService::setCoolerPower(const std::string& deviceId, double power,
                                   const std::string& mode) -> json {
    LOG_INFO(
        "CameraService::setCoolerPower: Setting cooler power to %f for camera: "
        "%s",
        power, deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->hasCooler()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "feature_not_supported"},
                {"message", "Camera does not have a cooler"},
            };
            return response;
        }

        (void)power;
        (void)mode;

        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message",
             "Manual cooler power control is not supported. Use setpoint "
             "cooling "
             "instead."},
        };

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::setCoolerPower: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::setCoolerPower: Completed");
    return response;
}

auto CameraService::warmUp(const std::string& deviceId) -> json {
    LOG_INFO("CameraService::warmUp: Initiating warm-up for camera: %s",
             deviceId.c_str());
    json response;

    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)

        if (!camera->hasCooler()) {
            response["status"] = "error";
            response["error"] = {{"code", "feature_not_supported"},
                                 {"message", "Camera does not have a cooler"}};
            return response;
        }

        bool success = camera->stopCooling();

        if (success) {
            impl_->lastCoolingSetpoint.reset();
            response["status"] = "success";
            response["message"] = "Camera warm-up sequence initiated.";
            response["data"] = {
                {"targetTemperature", 20.0},
                {"estimatedTime", 600},
            };

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish("main", "CameraWarmupStarted");
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "warmup_failed"},
                {"message", "Failed to initiate warm-up."},
            };
        }

    } catch (const std::exception& e) {
        LOG_ERROR("CameraService::warmUp: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }

    LOG_INFO("CameraService::warmUp: Completed");
    return response;
}

// ========== INDI-specific operations ==========

auto CameraService::getINDIProperties(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getINDIProperties", [this](auto camera) -> json {
            json data;
            data["driverName"] = "INDI Camera";
            data["driverVersion"] = "1.0";

            // Get basic properties
            json properties = json::object();

            if (auto gain = camera->getGain()) {
                properties["CCD_GAIN"] = {{"value", *gain}, {"type", "number"}};
            }

            if (auto offset = camera->getOffset()) {
                properties["CCD_OFFSET"] = {{"value", *offset},
                                            {"type", "number"}};
            }

            if (auto temp = camera->getTemperature()) {
                properties["CCD_TEMPERATURE"] = {{"value", *temp},
                                                 {"type", "number"}};
            }

            data["properties"] = properties;
            return makeSuccessResponse(data);
        });
}

auto CameraService::setINDIProperty(const std::string& deviceId,
                                    const std::string& propertyName,
                                    const json& value) -> json {
    return withConnectedDevice(
        deviceId, "setINDIProperty", [&](auto camera) -> json {
            bool success = false;

            if (propertyName == "CCD_GAIN" && value.is_number()) {
                success = camera->setGain(value.get<int>());
            } else if (propertyName == "CCD_OFFSET" && value.is_number()) {
                success = camera->setOffset(value.get<int>());
            } else if (propertyName == "CCD_TEMPERATURE" && value.is_number()) {
                success = camera->startCooling(value.get<double>());
                if (success) {
                    impl_->lastCoolingSetpoint = value.get<double>();
                }
            } else {
                return makeErrorResponse(
                    ErrorCode::INVALID_FIELD_VALUE,
                    "Unknown or invalid property: " + propertyName);
            }

            if (success) {
                return makeSuccessResponse("Property " + propertyName +
                                           " updated");
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to set property " + propertyName);
        });
}

auto CameraService::getFrameTypes(const std::string& deviceId) -> json {
    return withConnectedDevice(deviceId, "getFrameTypes",
                               [this](auto camera) -> json {
                                   (void)camera;
                                   json data;
                                   data["frameTypes"] = impl_->frameTypes;
                                   data["currentType"] = "Light";  // Default
                                   return makeSuccessResponse(data);
                               });
}

auto CameraService::setFrameType(const std::string& deviceId,
                                 const std::string& frameType) -> json {
    return withConnectedDevice(
        deviceId, "setFrameType", [&](auto camera) -> json {
            // Validate frame type
            auto it = std::find(impl_->frameTypes.begin(),
                                impl_->frameTypes.end(), frameType);
            if (it == impl_->frameTypes.end()) {
                return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                         "Invalid frame type: " + frameType);
            }

            FrameType type = FrameType::LIGHT;
            if (frameType == "Dark") {
                type = FrameType::DARK;
            } else if (frameType == "Flat") {
                type = FrameType::FLAT;
            } else if (frameType == "Bias") {
                type = FrameType::BIAS;
            }

            if (camera->setFrameType(type)) {
                return makeSuccessResponse("Frame type set to " + frameType);
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to set frame type");
        });
}

auto CameraService::getReadoutModes(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getReadoutModes", [this](auto camera) -> json {
            (void)camera;
            json data;
            json modes = json::array();

            // Default readout modes if not populated from device
            if (impl_->readoutModes.empty()) {
                modes.push_back({{"id", 0}, {"name", "High Quality"}});
                modes.push_back({{"id", 1}, {"name", "Fast"}});
            } else {
                for (const auto& [id, name] : impl_->readoutModes) {
                    modes.push_back({{"id", id}, {"name", name}});
                }
            }

            data["modes"] = modes;
            data["currentMode"] = 0;
            return makeSuccessResponse(data);
        });
}

auto CameraService::setReadoutMode(const std::string& deviceId, int modeIndex)
    -> json {
    return withConnectedDevice(
        deviceId, "setReadoutMode", [&](auto camera) -> json {
            (void)camera;
            // Validate mode index
            int maxModes = impl_->readoutModes.empty()
                               ? 2
                               : static_cast<int>(impl_->readoutModes.size());
            if (modeIndex < 0 || modeIndex >= maxModes) {
                return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                         "Invalid readout mode index");
            }

            // TODO: Actually set the readout mode on the camera
            return makeSuccessResponse("Readout mode set to " +
                                       std::to_string(modeIndex));
        });
}

}  // namespace lithium::device
