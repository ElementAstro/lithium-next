/*
 * ascom_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Camera Device Implementation

**************************************************/

#include "ascom_camera.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

// ==================== Constructor/Destructor ====================

ASCOMCamera::ASCOMCamera(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::Camera, deviceNumber) {
    spdlog::debug("ASCOMCamera created: {}", name_);
}

ASCOMCamera::~ASCOMCamera() {
    if (exposing_.load()) {
        abortExposure();
    }
    spdlog::debug("ASCOMCamera destroyed: {}", name_);
}

// ==================== Connection ====================

auto ASCOMCamera::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }

    refreshCapabilities();
    refreshSensorInfo();

    spdlog::info("Camera {} connected", name_);
    return true;
}

auto ASCOMCamera::disconnect() -> bool {
    if (exposing_.load()) {
        abortExposure();
    }
    return ASCOMDeviceBase::disconnect();
}

// ==================== Capabilities ====================

auto ASCOMCamera::getCapabilities() -> CameraCapabilities {
    refreshCapabilities();
    return capabilities_;
}

auto ASCOMCamera::getSensorInfo() -> SensorInfo {
    refreshSensorInfo();
    return sensorInfo_;
}

// ==================== Exposure Control ====================

auto ASCOMCamera::startExposure(double duration, bool light) -> bool {
    if (!isConnected()) {
        setError("Camera not connected");
        return false;
    }

    if (exposing_.load()) {
        setError("Exposure already in progress");
        return false;
    }

    exposureSettings_.duration = duration;
    exposureSettings_.light = light;

    auto response = setProperty("startexposure",
                                {{"Duration", std::to_string(duration)},
                                 {"Light", light ? "true" : "false"}});

    if (!response.isSuccess()) {
        setError("Failed to start exposure: " + response.errorMessage);
        return false;
    }

    exposing_.store(true);
    cameraState_.store(CameraState::Exposing);
    spdlog::info("Camera {} started {}s {} exposure", name_, duration,
                 light ? "light" : "dark");

    return true;
}

auto ASCOMCamera::abortExposure() -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("abortexposure", {});
    exposing_.store(false);
    cameraState_.store(CameraState::Idle);

    exposureCV_.notify_all();
    return response.isSuccess();
}

auto ASCOMCamera::stopExposure() -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("stopexposure", {});
    return response.isSuccess();
}

auto ASCOMCamera::isExposing() const -> bool {
    return exposing_.load();
}

auto ASCOMCamera::isImageReady() -> bool {
    auto result = getBoolProperty("imageready");
    if (result.has_value() && result.value()) {
        exposing_.store(false);
        cameraState_.store(CameraState::Idle);
        return true;
    }
    return false;
}

auto ASCOMCamera::waitForExposure(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (exposing_.load()) {
        if (isImageReady()) {
            return true;
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) {
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return isImageReady();
}

auto ASCOMCamera::getExposureProgress() -> int {
    auto completed = getDoubleProperty("percentcompleted");
    return completed.value_or(0.0);
}

auto ASCOMCamera::getLastExposureDuration() -> double {
    return getDoubleProperty("lastexposureduration").value_or(0.0);
}

auto ASCOMCamera::getLastExposureStartTime() -> std::string {
    return getStringProperty("lastexposurestarttime").value_or("");
}

// ==================== Image Download ====================

auto ASCOMCamera::getImageArray() -> std::vector<int32_t> {
    std::vector<int32_t> data;

    auto response = getProperty("imagearray");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& val : response.value) {
            if (val.is_number_integer()) {
                data.push_back(val.get<int32_t>());
            }
        }
    }

    return data;
}

auto ASCOMCamera::getImageArray2D() -> std::vector<std::vector<int32_t>> {
    std::vector<std::vector<int32_t>> data;

    auto response = getProperty("imagearray");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& row : response.value) {
            if (row.is_array()) {
                std::vector<int32_t> rowData;
                for (const auto& val : row) {
                    if (val.is_number_integer()) {
                        rowData.push_back(val.get<int32_t>());
                    }
                }
                data.push_back(rowData);
            }
        }
    }

    return data;
}

// ==================== Binning ====================

auto ASCOMCamera::setBinning(int binX, int binY) -> bool {
    if (!isConnected()) return false;

    bool success = true;
    success &= setIntProperty("binx", binX);
    success &= setIntProperty("biny", binY);

    if (success) {
        exposureSettings_.binX = binX;
        exposureSettings_.binY = binY;
    }

    return success;
}

auto ASCOMCamera::getBinning() -> std::pair<int, int> {
    int binX = getIntProperty("binx").value_or(1);
    int binY = getIntProperty("biny").value_or(1);
    return {binX, binY};
}

// ==================== Subframe ====================

auto ASCOMCamera::setSubframe(int startX, int startY, int numX, int numY) -> bool {
    if (!isConnected()) return false;

    bool success = true;
    success &= setIntProperty("startx", startX);
    success &= setIntProperty("starty", startY);
    success &= setIntProperty("numx", numX);
    success &= setIntProperty("numy", numY);

    if (success) {
        exposureSettings_.startX = startX;
        exposureSettings_.startY = startY;
        exposureSettings_.numX = numX;
        exposureSettings_.numY = numY;
    }

    return success;
}

auto ASCOMCamera::getSubframe() -> std::tuple<int, int, int, int> {
    int startX = getIntProperty("startx").value_or(0);
    int startY = getIntProperty("starty").value_or(0);
    int numX = getIntProperty("numx").value_or(0);
    int numY = getIntProperty("numy").value_or(0);
    return {startX, startY, numX, numY};
}

auto ASCOMCamera::resetSubframe() -> bool {
    return setSubframe(0, 0, sensorInfo_.cameraXSize, sensorInfo_.cameraYSize);
}

// ==================== Temperature Control ====================

auto ASCOMCamera::getTemperatureInfo() -> TemperatureInfo {
    TemperatureInfo info;
    info.ccdTemperature = getDoubleProperty("ccdtemperature").value_or(0.0);
    info.setPoint = getDoubleProperty("setccdtemperature").value_or(0.0);
    info.coolerOn = getBoolProperty("cooleron").value_or(false);

    if (capabilities_.canGetCoolerPower) {
        info.coolerPower = getDoubleProperty("coolerpower").value_or(0.0);
    }

    info.heatSinkTemperature = getDoubleProperty("heatsinktemperature").value_or(0.0);
    return info;
}

auto ASCOMCamera::getCCDTemperature() -> std::optional<double> {
    return getDoubleProperty("ccdtemperature");
}

auto ASCOMCamera::setTargetTemperature(double temperature) -> bool {
    if (!capabilities_.canSetCCDTemperature) {
        setError("Camera does not support temperature control");
        return false;
    }
    return setDoubleProperty("setccdtemperature", temperature);
}

auto ASCOMCamera::getTargetTemperature() -> std::optional<double> {
    return getDoubleProperty("setccdtemperature");
}

auto ASCOMCamera::setCoolerOn(bool enable) -> bool {
    return setBoolProperty("cooleron", enable);
}

auto ASCOMCamera::isCoolerOn() -> bool {
    return getBoolProperty("cooleron").value_or(false);
}

auto ASCOMCamera::getCoolerPower() -> std::optional<double> {
    if (!capabilities_.canGetCoolerPower) {
        return std::nullopt;
    }
    return getDoubleProperty("coolerpower");
}

// ==================== Gain/Offset ====================

auto ASCOMCamera::getGainSettings() -> GainSettings {
    GainSettings settings;
    settings.gain = getIntProperty("gain").value_or(0);
    settings.gainMin = getIntProperty("gainmin").value_or(0);
    settings.gainMax = getIntProperty("gainmax").value_or(100);

    auto gainsResponse = getProperty("gains");
    if (gainsResponse.isSuccess() && gainsResponse.value.is_array()) {
        for (const auto& g : gainsResponse.value) {
            if (g.is_string()) {
                settings.gains.push_back(g.get<std::string>());
            }
        }
    }

    settings.offset = getIntProperty("offset").value_or(0);
    settings.offsetMin = getIntProperty("offsetmin").value_or(0);
    settings.offsetMax = getIntProperty("offsetmax").value_or(100);

    auto offsetsResponse = getProperty("offsets");
    if (offsetsResponse.isSuccess() && offsetsResponse.value.is_array()) {
        for (const auto& o : offsetsResponse.value) {
            if (o.is_string()) {
                settings.offsets.push_back(o.get<std::string>());
            }
        }
    }

    return settings;
}

auto ASCOMCamera::setGain(int gain) -> bool {
    return setIntProperty("gain", gain);
}

auto ASCOMCamera::getGain() -> std::optional<int> {
    return getIntProperty("gain");
}

auto ASCOMCamera::setOffset(int offset) -> bool {
    return setIntProperty("offset", offset);
}

auto ASCOMCamera::getOffset() -> std::optional<int> {
    return getIntProperty("offset");
}

// ==================== Readout Mode ====================

auto ASCOMCamera::getReadoutModes() -> std::vector<std::string> {
    std::vector<std::string> modes;
    auto response = getProperty("readoutmodes");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& m : response.value) {
            if (m.is_string()) {
                modes.push_back(m.get<std::string>());
            }
        }
    }
    return modes;
}

auto ASCOMCamera::setReadoutMode(int mode) -> bool {
    return setIntProperty("readoutmode", mode);
}

auto ASCOMCamera::getReadoutMode() -> std::optional<int> {
    return getIntProperty("readoutmode");
}

auto ASCOMCamera::setFastReadout(bool fast) -> bool {
    if (!capabilities_.canFastReadout) {
        return false;
    }
    return setBoolProperty("fastreadout", fast);
}

auto ASCOMCamera::isFastReadout() -> bool {
    return getBoolProperty("fastreadout").value_or(false);
}

// ==================== Pulse Guiding ====================

auto ASCOMCamera::pulseGuide(int direction, int duration) -> bool {
    if (!capabilities_.canPulseGuide) {
        setError("Camera does not support pulse guiding");
        return false;
    }

    auto response = setProperty("pulseguide",
                                {{"Direction", std::to_string(direction)},
                                 {"Duration", std::to_string(duration)}});
    return response.isSuccess();
}

auto ASCOMCamera::isPulseGuiding() -> bool {
    return getBoolProperty("ispulseguiding").value_or(false);
}

// ==================== Status ====================

auto ASCOMCamera::getCameraState() -> CameraState {
    auto state = getIntProperty("camerastate");
    if (state.has_value()) {
        return static_cast<CameraState>(state.value());
    }
    return cameraState_.load();
}

auto ASCOMCamera::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();

    status["cameraState"] = static_cast<int>(cameraState_.load());
    status["exposing"] = exposing_.load();
    status["capabilities"] = capabilities_.toJson();
    status["sensorInfo"] = sensorInfo_.toJson();
    status["exposureSettings"] = exposureSettings_.toJson();

    return status;
}

// ==================== Internal Methods ====================

void ASCOMCamera::refreshCapabilities() {
    capabilities_.canAbortExposure = getBoolProperty("canabortexposure").value_or(false);
    capabilities_.canAsymmetricBin = getBoolProperty("canasymmetricbin").value_or(false);
    capabilities_.canFastReadout = getBoolProperty("canfastreadout").value_or(false);
    capabilities_.canGetCoolerPower = getBoolProperty("cangetcoolerpower").value_or(false);
    capabilities_.canPulseGuide = getBoolProperty("canpulseguide").value_or(false);
    capabilities_.canSetCCDTemperature = getBoolProperty("cansetccdtemperature").value_or(false);
    capabilities_.canStopExposure = getBoolProperty("canstopexposure").value_or(false);
    capabilities_.hasShutter = getBoolProperty("hasshutter").value_or(false);
}

void ASCOMCamera::refreshSensorInfo() {
    sensorInfo_.cameraXSize = getIntProperty("cameraxsize").value_or(0);
    sensorInfo_.cameraYSize = getIntProperty("cameraysize").value_or(0);
    sensorInfo_.pixelSizeX = getDoubleProperty("pixelsizex").value_or(0.0);
    sensorInfo_.pixelSizeY = getDoubleProperty("pixelsizey").value_or(0.0);
    sensorInfo_.maxBinX = getIntProperty("maxbinx").value_or(1);
    sensorInfo_.maxBinY = getIntProperty("maxbiny").value_or(1);
    sensorInfo_.maxADU = getIntProperty("maxadu").value_or(65535);
    sensorInfo_.electronsPerADU = getDoubleProperty("electronsperadu").value_or(1.0);
    sensorInfo_.fullWellCapacity = getDoubleProperty("fullwellcapacity").value_or(0.0);

    auto sensorType = getIntProperty("sensortype");
    if (sensorType.has_value()) {
        sensorInfo_.sensorType = static_cast<SensorType>(sensorType.value());
    }

    sensorInfo_.sensorName = getStringProperty("sensorname").value_or("");
    sensorInfo_.bayerOffsetX = getIntProperty("bayeroffsetx").value_or(0);
    sensorInfo_.bayerOffsetY = getIntProperty("bayeroffsety").value_or(0);

    // Update exposure settings with full frame
    exposureSettings_.numX = sensorInfo_.cameraXSize;
    exposureSettings_.numY = sensorInfo_.cameraYSize;
}

}  // namespace lithium::client::ascom
