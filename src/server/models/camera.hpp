#ifndef LITHIUM_SERVER_MODELS_CAMERA_HPP
#define LITHIUM_SERVER_MODELS_CAMERA_HPP

#include <optional>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "device/template/camera.hpp"

namespace lithium::models::camera {

using json = nlohmann::json;

inline auto makeCameraSummary(const std::string &deviceId,
                              const std::string &name,
                              bool isConnected) -> json {
    return json{{"deviceId", deviceId},
                {"name", name},
                {"isConnected", isConnected}};
}

inline auto makeCameraStatusData(AtomCamera &camera,
                                 const std::optional<double> &setpoint) -> json {
    json data;

    data["isConnected"] = camera.isConnected();
    data["cameraState"] = camera.isExposing() ? "Exposing" : "Idle";

    data["coolerOn"] = camera.isCoolerOn();
    if (auto temp = camera.getTemperature()) {
        data["temperature"] = *temp;
    }
    if (auto power = camera.getCoolingPower()) {
        data["coolerPower"] = *power;
    }
    if (setpoint) {
        data["setpoint"] = *setpoint;
    }

    if (auto gain = camera.getGain()) {
        data["gain"] = *gain;
    }
    if (auto offset = camera.getOffset()) {
        data["offset"] = *offset;
    }

    if (auto binning = camera.getBinning()) {
        data["binning"] = json{{"x", binning->horizontal},
                                {"y", binning->vertical}};
    }

    if (auto resolution = camera.getResolution()) {
        data["roi"] = json{{"x", resolution->x},
                            {"y", resolution->y},
                            {"width", resolution->width},
                            {"height", resolution->height}};
    }

    auto frameInfo = camera.getFrameInfo();
    json sensor;
    sensor["resolution"] = json{{"width", frameInfo.width},
                                 {"height", frameInfo.height}};
    sensor["pixelSize"] = json{{"width", frameInfo.pixelWidth},
                                {"height", frameInfo.pixelHeight}};
    data["sensor"] = sensor;

    return data;
}

inline auto makeCameraCapabilitiesData(AtomCamera &camera) -> json {
    json data;

    data["canCool"] = camera.hasCooler();
    data["canSetTemperature"] = camera.hasCooler();
    data["canAbortExposure"] = true;
    data["canStopExposure"] = true;
    data["canGetCoolerPower"] = camera.hasCooler();
    data["hasMechanicalShutter"] = false;

    data["gainRange"] = json{{"min", 0}, {"max", 600}, {"default", 100}};
    data["offsetRange"] = json{{"min", 0}, {"max", 100}, {"default", 50}};

    if (camera.hasCooler()) {
        data["temperatureRange"] = json{{"min", -50.0}, {"max", 50.0}};
    }

    data["binningModes"] = json::array({json{{"x", 1}, {"y", 1}},
                                         json{{"x", 2}, {"y", 2}},
                                         json{{"x", 3}, {"y", 3}},
                                         json{{"x", 4}, {"y", 4}}});

    auto frameInfo = camera.getFrameInfo();
    data["pixelSizeX"] = frameInfo.pixelWidth;
    data["pixelSizeY"] = frameInfo.pixelHeight;
    data["maxBinX"] = 4;
    data["maxBinY"] = 4;

    data["bayerPattern"] = "RGGB";
    data["electronsPerADU"] = 1.0;
    data["fullWellCapacity"] = 50000;
    data["readNoise"] = 2.3;
    data["readoutModes"] = json::array({json{{"name", "High Quality"}, {"id", 0}},
                                         json{{"name", "Fast"}, {"id", 1}}});

    return data;
}

inline auto makeGainsData(AtomCamera &camera,
                          const std::vector<int> &gains) -> json {
    json data;
    data["gains"] = gains;
    if (auto currentGain = camera.getGain()) {
        data["currentGain"] = *currentGain;
    }
    data["defaultGain"] = 100;
    data["unityGain"] = 139;
    return data;
}

inline auto makeOffsetsData(AtomCamera &camera,
                            const std::vector<int> &offsets) -> json {
    json data;
    data["offsets"] = offsets;
    if (auto currentOffset = camera.getOffset()) {
        data["currentOffset"] = *currentOffset;
    }
    data["defaultOffset"] = 50;
    return data;
}

}  // namespace lithium::models::camera

#endif  // LITHIUM_SERVER_MODELS_CAMERA_HPP
