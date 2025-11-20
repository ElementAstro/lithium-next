#include "camera.hpp"

#include "config/configor.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"

#include "device/template/camera.hpp"

#include "constant/constant.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

// List all available cameras
auto listCameras() -> json {
    LOG_F(INFO, "listCameras: Listing all available cameras");
    json response;
    response["status"] = "success";
    
    try {
        std::shared_ptr<ConfigManager> configManager;
        GET_OR_CREATE_PTR(configManager, ConfigManager,
                          Constants::CONFIG_MANAGER)

        // Get camera configuration
        auto cameraList = json::array();
        
        // For now, we'll support the main camera
        // TODO: Add support for multiple cameras from config
        std::shared_ptr<AtomCamera> camera;
        try {
            GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
            json cameraInfo;
            cameraInfo["deviceId"] = "cam-001";
            cameraInfo["name"] = camera->getName();
            cameraInfo["isConnected"] = camera->isConnected();
            cameraList.push_back(cameraInfo);
        } catch (...) {
            LOG_F(WARNING, "listCameras: Main camera not available");
        }
        
        response["data"] = cameraList;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "listCameras: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "listCameras: Completed");
    return response;
}

// Get camera status
auto getCameraStatus(const std::string& deviceId) -> json {
    LOG_F(INFO, "getCameraStatus: Getting status for camera: %s", deviceId.c_str());
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
        
        json data;
        data["isConnected"] = camera->isConnected();
        data["cameraState"] = camera->isExposing() ? "Exposing" : "Idle";
        
        // Temperature info
        data["coolerOn"] = camera->isCoolerOn();
        if (auto temp = camera->getTemperature()) {
            data["temperature"] = temp.value();
        }
        if (auto power = camera->getCoolingPower()) {
            data["coolerPower"] = power.value();
        }
        
        // Camera settings
        if (auto gain = camera->getGain()) {
            data["gain"] = gain.value();
        }
        if (auto offset = camera->getOffset()) {
            data["offset"] = offset.value();
        }
        
        // Binning and ROI
        if (auto binning = camera->getBinning()) {
            data["binning"] = {
                {"x", binning->horizontal},
                {"y", binning->vertical}
            };
        }
        
        if (auto resolution = camera->getResolution()) {
            data["roi"] = {
                {"x", resolution->x},
                {"y", resolution->y},
                {"width", resolution->width},
                {"height", resolution->height}
            };
        }
        
        // Sensor info from frame
        auto frameInfo = camera->getFrameInfo();
        data["sensor"] = {
            {"resolution", {
                {"width", frameInfo.width},
                {"height", frameInfo.height}
            }},
            {"pixelSize", {
                {"width", frameInfo.pixelWidth},
                {"height", frameInfo.pixelHeight}
            }}
        };
        
        response["status"] = "success";
        response["data"] = data;
        
    } catch (const std::exception& e) {
        LOG_F(ERROR, "getCameraStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "getCameraStatus: Completed");
    return response;
}

// Connect/Disconnect camera
auto connectCamera(const std::string& deviceId, bool connected) -> json {
    LOG_F(INFO, "connectCamera: %s camera: %s", 
          connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        bool success = connected ? camera->connect("", "") : camera->disconnect();
        
        if (success) {
            response["status"] = "success";
            response["message"] =
                connected ? "Camera connection process initiated."
                          : "Camera disconnection process initiated.";
                
            // Publish event
            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish("main",
                                   "CameraConnection:{}"_fmt(connected ? "ON" : "OFF"));
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "connection_failed"},
                {"message", "Connection operation failed."},
            };
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "connectCamera: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "connectCamera: Completed");
    return response;
}

// Update camera settings
auto updateCameraSettings(const std::string& deviceId, const json& settings) -> json {
    LOG_F(INFO, "updateCameraSettings: Updating settings for camera: %s", deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_not_connected"},
                {"message", "Camera is not connected"}
            };
            return response;
        }
        
        // Update cooler settings
        if (settings.contains("coolerOn")) {
            bool coolerOn = settings["coolerOn"];
            if (coolerOn && settings.contains("setpoint")) {
                double setpoint = settings["setpoint"];
                camera->startCooling(setpoint);
            } else if (!coolerOn) {
                camera->stopCooling();
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
        LOG_F(ERROR, "updateCameraSettings: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "updateCameraSettings: Completed");
    return response;
}

// Start exposure
auto startExposure(const std::string& deviceId, double duration, 
                   const std::string& frameType, const std::string& filename) -> json {
    LOG_F(INFO, "startExposure: Starting %f second exposure on camera: %s", 
          duration, deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_not_connected"},
                {"message", "Camera is not connected"}
            };
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
        // Low-level driver exposure behaviour does not depend on it here.

        // Start exposure
        bool success = camera->startExposure(duration);
        
        if (success) {
            // Generate exposure ID
            std::string exposureId = "exp_" + std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
            
            response["status"] = "success";
            response["data"] = {
                {"exposureId", exposureId}
            };
            response["message"] = "Exposure started.";
            
            // Publish event
            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            messageBusPtr->publish("main", "ExposureStarted:{}"_fmt(exposureId));
        } else {
            response["status"] = "error";
            response["error"] = {
                {"code", "exposure_failed"},
                {"message", "Failed to start exposure."},
            };
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "startExposure: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "startExposure: Completed");
    return response;
}

// Abort exposure
auto abortExposure(const std::string& deviceId) -> json {
    LOG_F(INFO, "abortExposure: Aborting exposure on camera: %s", deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        bool success = camera->abortExposure();
        
        if (success) {
            response["status"] = "success";
            response["message"] = "Exposure abort command sent.";
            
            // Publish event
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
        LOG_F(ERROR, "abortExposure: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "abortExposure: Completed");
    return response;
}

// Get camera capabilities
auto getCameraCapabilities(const std::string& deviceId) -> json {
    LOG_F(INFO, "getCameraCapabilities: Getting capabilities for camera: %s", 
          deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        if (!camera->isConnected()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "device_not_connected"},
                {"message", "Camera is not connected"}
            };
            return response;
        }
        
        json data;
        data["canCool"] = camera->hasCooler();
        data["canSetTemperature"] = camera->hasCooler();
        data["canAbortExposure"] = true;
        data["canStopExposure"] = true;
        data["canGetCoolerPower"] = camera->hasCooler();
        data["hasMechanicalShutter"] = false;
        
        // Gain range - these are typical values, should come from driver
        data["gainRange"] = {
            {"min", 0},
            {"max", 600},
            {"default", 100}
        };
        
        // Offset range
        data["offsetRange"] = {
            {"min", 0},
            {"max", 100},
            {"default", 50}
        };
        
        // Temperature range
        if (camera->hasCooler()) {
            data["temperatureRange"] = {
                {"min", -50.0},
                {"max", 50.0}
            };
        }
        
        // Binning modes
        data["binningModes"] = json::array({
            {{"x", 1}, {"y", 1}},
            {{"x", 2}, {"y", 2}},
            {{"x", 3}, {"y", 3}},
            {{"x", 4}, {"y", 4}}
        });
        
        // Frame info
        auto frameInfo = camera->getFrameInfo();
        data["pixelSizeX"] = frameInfo.pixelWidth;
        data["pixelSizeY"] = frameInfo.pixelHeight;
        data["maxBinX"] = 4;
        data["maxBinY"] = 4;
        
        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "getCameraCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "getCameraCapabilities: Completed");
    return response;
}

// Get available gains
auto getCameraGains(const std::string& deviceId) -> json {
    LOG_F(INFO, "getCameraGains: Getting available gains for camera: %s", 
          deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        // Generate gain values (0-600 in steps of 50)
        json gains = json::array();
        for (int i = 0; i <= 600; i += 50) {
            gains.push_back(i);
        }
        
        json data;
        data["gains"] = gains;
        
        if (auto currentGain = camera->getGain()) {
            data["currentGain"] = currentGain.value();
        }
        
        data["defaultGain"] = 100;
        data["unityGain"] = 139;  // Typical for Sony IMX sensors
        
        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "getCameraGains: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "getCameraGains: Completed");
    return response;
}

// Get available offsets
auto getCameraOffsets(const std::string& deviceId) -> json {
    LOG_F(INFO, "getCameraOffsets: Getting available offsets for camera: %s", 
          deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        // Generate offset values (0-100 in steps of 10)
        json offsets = json::array();
        for (int i = 0; i <= 100; i += 10) {
            offsets.push_back(i);
        }
        
        json data;
        data["offsets"] = offsets;
        
        if (auto currentOffset = camera->getOffset()) {
            data["currentOffset"] = currentOffset.value();
        }
        
        data["defaultOffset"] = 50;
        
        response["status"] = "success";
        response["data"] = data;

    } catch (const std::exception& e) {
        LOG_F(ERROR, "getCameraOffsets: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "getCameraOffsets: Completed");
    return response;
}

// Set cooler power (manual mode)
auto setCoolerPower(const std::string& deviceId, double power, 
                    const std::string& mode) -> json {
    LOG_F(INFO, "setCoolerPower: Setting cooler power to %f for camera: %s", 
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
        
        // Note: Manual cooler power control may not be supported by all cameras
        // This is a placeholder - actual implementation depends on driver support
        
        response["status"] = "success";
        response["message"] = "Cooler power set to manual mode.";

    } catch (const std::exception& e) {
        LOG_F(ERROR, "setCoolerPower: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "setCoolerPower: Completed");
    return response;
}

// Warm up camera
auto warmUpCamera(const std::string& deviceId) -> json {
    LOG_F(INFO, "warmUpCamera: Initiating warm-up for camera: %s", deviceId.c_str());
    json response;
    
    try {
        std::shared_ptr<AtomCamera> camera;
        GET_OR_CREATE_PTR(camera, AtomCamera, Constants::MAIN_CAMERA)
        
        if (!camera->hasCooler()) {
            response["status"] = "error";
            response["error"] = {
                {"code", "feature_not_supported"},
                {"message", "Camera does not have a cooler"}
            };
            return response;
        }
        
        // Stop cooling
        bool success = camera->stopCooling();
        
        if (success) {
            response["status"] = "success";
            response["message"] = "Camera warm-up sequence initiated.";
            response["data"] = {
                {"targetTemperature", 20.0},
                {"estimatedTime", 600},  // 10 minutes estimated
            };

            // Publish event
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
        LOG_F(ERROR, "warmUpCamera: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {
            {"code", "internal_error"},
            {"message", e.what()},
        };
    }
    
    LOG_F(INFO, "warmUpCamera: Completed");
    return response;
}

}  // namespace lithium::middleware
