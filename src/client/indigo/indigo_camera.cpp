/*
 * indigo_camera.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Camera Device Implementation

**************************************************/

#include "indigo_camera.hpp"

#include <cmath>
#include <mutex>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// Frame type conversion
// ============================================================================

auto frameTypeFromString(std::string_view str) -> FrameType {
    if (str == "Light" || str == "LIGHT") return FrameType::Light;
    if (str == "Bias" || str == "BIAS") return FrameType::Bias;
    if (str == "Dark" || str == "DARK") return FrameType::Dark;
    if (str == "Flat" || str == "FLAT") return FrameType::Flat;
    return FrameType::Light;
}

// ============================================================================
// INDIGOCamera Implementation
// ============================================================================

class INDIGOCamera::Impl {
public:
    explicit Impl(INDIGOCamera* parent) : parent_(parent) {}

    void onConnected() {
        // Enable BLOB reception for images
        parent_->enableBlob(true, true);

        // Cache sensor info
        updateSensorInfo();

        LOG_F(INFO, "INDIGO Camera[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        exposing_ = false;
        LOG_F(INFO, "INDIGO Camera[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "CCD_EXPOSURE") {
            handleExposureUpdate(property);
        } else if (name == "CCD_IMAGE") {
            handleImageUpdate(property);
        } else if (name == "CCD_TEMPERATURE") {
            handleTemperatureUpdate(property);
        } else if (name == "CCD_COOLER") {
            handleCoolerUpdate(property);
        } else if (name == "CCD_INFO") {
            updateSensorInfo();
        }
    }

    auto startExposure(double duration, FrameType frameType) -> DeviceResult<bool> {
        // Set frame type first
        auto frameResult = setFrameType(frameType);
        if (!frameResult.has_value()) {
            return frameResult;
        }

        // Start exposure
        auto result = parent_->setNumberProperty("CCD_EXPOSURE",
                                                 "CCD_EXPOSURE_VALUE", duration);
        if (result.has_value()) {
            exposing_ = true;
            exposureStatus_.exposing = true;
            exposureStatus_.duration = duration;
            exposureStatus_.elapsed = 0.0;
            exposureStatus_.remaining = duration;
            exposureStatus_.state = PropertyState::Busy;

            LOG_F(INFO, "INDIGO Camera[{}]: Started {:.2f}s {} exposure",
                  parent_->getINDIGODeviceName(), duration,
                  std::string(frameTypeToString(frameType)));
        }

        return result;
    }

    auto abortExposure() -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty("CCD_ABORT_EXPOSURE",
                                                 "CCD_ABORT_EXPOSURE_ITEM", 1.0);
        if (result.has_value()) {
            exposing_ = false;
            exposureStatus_.exposing = false;
            exposureStatus_.state = PropertyState::Alert;

            LOG_F(INFO, "INDIGO Camera[{}]: Exposure aborted",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    [[nodiscard]] auto isExposing() const -> bool { return exposing_; }

    [[nodiscard]] auto getExposureStatus() const -> ExposureStatus {
        return exposureStatus_;
    }

    void setImageCallback(ImageCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        imageCallback_ = std::move(callback);
    }

    void setExposureProgressCallback(ExposureProgressCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        progressCallback_ = std::move(callback);
    }

    [[nodiscard]] auto hasCooler() const -> bool {
        auto prop = parent_->getProperty("CCD_COOLER");
        return prop.has_value();
    }

    auto setCoolerOn(bool on) -> DeviceResult<bool> {
        return parent_->setSwitchProperty("CCD_COOLER",
                                          on ? "ON" : "OFF", true);
    }

    [[nodiscard]] auto isCoolerOn() const -> bool {
        auto result = parent_->getSwitchValue("CCD_COOLER", "ON");
        return result.value_or(false);
    }

    auto setTargetTemperature(double celsius) -> DeviceResult<bool> {
        return parent_->setNumberProperty("CCD_TEMPERATURE",
                                          "CCD_TEMPERATURE_VALUE", celsius);
    }

    [[nodiscard]] auto getCurrentTemperature() const -> double {
        return tempInfo_.currentTemp;
    }

    [[nodiscard]] auto getTargetTemperature() const -> double {
        return tempInfo_.targetTemp;
    }

    [[nodiscard]] auto getCoolerPower() const -> double {
        return tempInfo_.coolerPower;
    }

    [[nodiscard]] auto getTemperatureInfo() const -> TemperatureInfo {
        return tempInfo_;
    }

    [[nodiscard]] auto getSensorInfo() const -> SensorInfo {
        return sensorInfo_;
    }

    [[nodiscard]] auto getSensorWidth() const -> int {
        return sensorInfo_.width;
    }

    [[nodiscard]] auto getSensorHeight() const -> int {
        return sensorInfo_.height;
    }

    [[nodiscard]] auto getPixelSize() const -> std::pair<double, double> {
        return {sensorInfo_.pixelSizeX, sensorInfo_.pixelSizeY};
    }

    [[nodiscard]] auto getBitDepth() const -> int {
        return sensorInfo_.bitsPerPixel;
    }

    auto setBinning(int horizontal, int vertical) -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty("CCD_BIN", {
            {"CCD_BIN_HORIZONTAL", static_cast<double>(horizontal)},
        });
        if (!result.has_value()) {
            return result;
        }

        return parent_->setNumberProperty("CCD_BIN",
                                          "CCD_BIN_VERTICAL",
                                          static_cast<double>(vertical));
    }

    [[nodiscard]] auto getBinning() const -> BinningMode {
        BinningMode mode;
        mode.horizontal = static_cast<int>(
            parent_->getNumberValue("CCD_BIN", "CCD_BIN_HORIZONTAL").value_or(1.0));
        mode.vertical = static_cast<int>(
            parent_->getNumberValue("CCD_BIN", "CCD_BIN_VERTICAL").value_or(1.0));
        return mode;
    }

    [[nodiscard]] auto getSupportedBinning() const -> std::vector<BinningMode> {
        return sensorInfo_.supportedBinning;
    }

    auto setROI(const CameraROI& roi) -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty("CCD_FRAME", {
            {"CCD_FRAME_X", static_cast<double>(roi.x)},
        });
        if (!result.has_value()) return result;

        result = parent_->setNumberProperty("CCD_FRAME",
                                            "CCD_FRAME_Y",
                                            static_cast<double>(roi.y));
        if (!result.has_value()) return result;

        result = parent_->setNumberProperty("CCD_FRAME",
                                            "CCD_FRAME_WIDTH",
                                            static_cast<double>(roi.width));
        if (!result.has_value()) return result;

        return parent_->setNumberProperty("CCD_FRAME",
                                          "CCD_FRAME_HEIGHT",
                                          static_cast<double>(roi.height));
    }

    [[nodiscard]] auto getROI() const -> CameraROI {
        CameraROI roi;
        roi.x = static_cast<int>(
            parent_->getNumberValue("CCD_FRAME", "CCD_FRAME_X").value_or(0.0));
        roi.y = static_cast<int>(
            parent_->getNumberValue("CCD_FRAME", "CCD_FRAME_Y").value_or(0.0));
        roi.width = static_cast<int>(
            parent_->getNumberValue("CCD_FRAME", "CCD_FRAME_WIDTH")
                .value_or(static_cast<double>(sensorInfo_.width)));
        roi.height = static_cast<int>(
            parent_->getNumberValue("CCD_FRAME", "CCD_FRAME_HEIGHT")
                .value_or(static_cast<double>(sensorInfo_.height)));
        return roi;
    }

    auto resetROI() -> DeviceResult<bool> {
        CameraROI fullFrame{0, 0, sensorInfo_.width, sensorInfo_.height};
        return setROI(fullFrame);
    }

    [[nodiscard]] auto hasGainControl() const -> bool {
        auto prop = parent_->getProperty("CCD_GAIN");
        return prop.has_value();
    }

    auto setGain(double gain) -> DeviceResult<bool> {
        return parent_->setNumberProperty("CCD_GAIN", "GAIN", gain);
    }

    [[nodiscard]] auto getGain() const -> double {
        return parent_->getNumberValue("CCD_GAIN", "GAIN").value_or(0.0);
    }

    [[nodiscard]] auto getGainRange() const -> std::pair<double, double> {
        auto prop = parent_->getProperty("CCD_GAIN");
        if (!prop.has_value() || prop.value().numberElements.empty()) {
            return {0.0, 100.0};
        }
        const auto& elem = prop.value().numberElements[0];
        return {elem.min, elem.max};
    }

    [[nodiscard]] auto hasOffsetControl() const -> bool {
        auto prop = parent_->getProperty("CCD_OFFSET");
        return prop.has_value();
    }

    auto setOffset(double offset) -> DeviceResult<bool> {
        return parent_->setNumberProperty("CCD_OFFSET", "OFFSET", offset);
    }

    [[nodiscard]] auto getOffset() const -> double {
        return parent_->getNumberValue("CCD_OFFSET", "OFFSET").value_or(0.0);
    }

    [[nodiscard]] auto getOffsetRange() const -> std::pair<double, double> {
        auto prop = parent_->getProperty("CCD_OFFSET");
        if (!prop.has_value() || prop.value().numberElements.empty()) {
            return {0.0, 255.0};
        }
        const auto& elem = prop.value().numberElements[0];
        return {elem.min, elem.max};
    }

    auto setFrameType(FrameType type) -> DeviceResult<bool> {
        std::string elementName;
        switch (type) {
            case FrameType::Light: elementName = "LIGHT"; break;
            case FrameType::Bias: elementName = "BIAS"; break;
            case FrameType::Dark: elementName = "DARK"; break;
            case FrameType::Flat: elementName = "FLAT"; break;
        }
        return parent_->setSwitchProperty("CCD_FRAME_TYPE", elementName, true);
    }

    [[nodiscard]] auto getFrameType() const -> FrameType {
        auto result = parent_->getActiveSwitchName("CCD_FRAME_TYPE");
        if (!result.has_value()) {
            return FrameType::Light;
        }
        return frameTypeFromString(result.value());
    }

    auto setImageFormat(const std::string& format) -> DeviceResult<bool> {
        return parent_->setSwitchProperty("CCD_IMAGE_FORMAT", format, true);
    }

    [[nodiscard]] auto getImageFormat() const -> std::string {
        auto result = parent_->getActiveSwitchName("CCD_IMAGE_FORMAT");
        return result.value_or("FITS");
    }

    [[nodiscard]] auto getSupportedFormats() const -> std::vector<std::string> {
        std::vector<std::string> formats;
        auto prop = parent_->getProperty("CCD_IMAGE_FORMAT");
        if (prop.has_value()) {
            for (const auto& elem : prop.value().switchElements) {
                formats.push_back(elem.name);
            }
        }
        if (formats.empty()) {
            formats = {"FITS", "RAW"};
        }
        return formats;
    }

    [[nodiscard]] auto getCapabilities() const -> json {
        json caps;
        caps["hasCooler"] = hasCooler();
        caps["hasGain"] = hasGainControl();
        caps["hasOffset"] = hasOffsetControl();
        caps["sensorWidth"] = sensorInfo_.width;
        caps["sensorHeight"] = sensorInfo_.height;
        caps["pixelSizeX"] = sensorInfo_.pixelSizeX;
        caps["pixelSizeY"] = sensorInfo_.pixelSizeY;
        caps["bitDepth"] = sensorInfo_.bitsPerPixel;
        caps["supportedFormats"] = getSupportedFormats();

        json binningModes = json::array();
        for (const auto& mode : sensorInfo_.supportedBinning) {
            binningModes.push_back({{"h", mode.horizontal}, {"v", mode.vertical}});
        }
        caps["supportedBinning"] = binningModes;

        if (hasGainControl()) {
            auto [minGain, maxGain] = getGainRange();
            caps["gainRange"] = {{"min", minGain}, {"max", maxGain}};
        }

        if (hasOffsetControl()) {
            auto [minOffset, maxOffset] = getOffsetRange();
            caps["offsetRange"] = {{"min", minOffset}, {"max", maxOffset}};
        }

        return caps;
    }

    [[nodiscard]] auto getStatus() const -> json {
        json status;
        status["connected"] = parent_->isConnected();
        status["exposing"] = exposing_;

        if (exposing_) {
            status["exposureProgress"] = {
                {"duration", exposureStatus_.duration},
                {"elapsed", exposureStatus_.elapsed},
                {"remaining", exposureStatus_.remaining}
            };
        }

        status["temperature"] = {
            {"current", tempInfo_.currentTemp},
            {"target", tempInfo_.targetTemp},
            {"coolerOn", tempInfo_.coolerOn},
            {"coolerPower", tempInfo_.coolerPower}
        };

        auto binning = getBinning();
        status["binning"] = {{"h", binning.horizontal}, {"v", binning.vertical}};

        auto roi = getROI();
        status["roi"] = {
            {"x", roi.x}, {"y", roi.y},
            {"width", roi.width}, {"height", roi.height}
        };

        status["gain"] = getGain();
        status["offset"] = getOffset();
        status["frameType"] = std::string(frameTypeToString(getFrameType()));
        status["imageFormat"] = getImageFormat();

        return status;
    }

private:
    void handleExposureUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "CCD_EXPOSURE_VALUE") {
                double remaining = elem.value;
                double elapsed = exposureStatus_.duration - remaining;

                exposureStatus_.elapsed = elapsed;
                exposureStatus_.remaining = remaining;
                exposureStatus_.state = property.state;

                if (property.state == PropertyState::Ok) {
                    exposing_ = false;
                    exposureStatus_.exposing = false;
                    LOG_F(INFO, "INDIGO Camera[{}]: Exposure complete",
                          parent_->getINDIGODeviceName());
                } else if (property.state == PropertyState::Alert) {
                    exposing_ = false;
                    exposureStatus_.exposing = false;
                    LOG_F(WARNING, "INDIGO Camera[{}]: Exposure failed",
                          parent_->getINDIGODeviceName());
                }

                // Call progress callback
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (progressCallback_ && exposing_) {
                    progressCallback_(elapsed, exposureStatus_.duration);
                }
                break;
            }
        }
    }

    void handleImageUpdate(const Property& property) {
        if (property.blobElements.empty()) return;

        const auto& blob = property.blobElements[0];

        ImageData image;
        image.format = blob.format;
        image.url = blob.url;

        // Get image dimensions from CCD_INFO
        image.width = sensorInfo_.width;
        image.height = sensorInfo_.height;
        image.bitDepth = sensorInfo_.bitsPerPixel;

        // If using URL mode, don't copy data
        if (!blob.url.empty()) {
            LOG_F(INFO, "INDIGO Camera[{}]: Image available at URL: {}",
                  parent_->getINDIGODeviceName(), blob.url);
        } else if (!blob.data.empty()) {
            image.data = blob.data;
            LOG_F(INFO, "INDIGO Camera[{}]: Image received, {} bytes",
                  parent_->getINDIGODeviceName(), blob.data.size());
        }

        // Add metadata
        image.metadata["deviceName"] = parent_->getINDIGODeviceName();
        image.metadata["frameType"] = std::string(frameTypeToString(getFrameType()));
        image.metadata["exposure"] = exposureStatus_.duration;
        image.metadata["gain"] = getGain();
        image.metadata["offset"] = getOffset();
        image.metadata["temperature"] = tempInfo_.currentTemp;

        auto binning = getBinning();
        image.metadata["binning"] = {{"h", binning.horizontal}, {"v", binning.vertical}};

        // Call image callback
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (imageCallback_) {
            imageCallback_(image);
        }
    }

    void handleTemperatureUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "CCD_TEMPERATURE_VALUE") {
                tempInfo_.currentTemp = elem.value;
                tempInfo_.targetTemp = elem.target;
            }
        }
    }

    void handleCoolerUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.name == "ON") {
                tempInfo_.coolerOn = elem.value;
            }
        }

        // Also check cooler power
        auto powerResult = parent_->getNumberValue("CCD_COOLER_POWER",
                                                   "CCD_COOLER_POWER_VALUE");
        if (powerResult.has_value()) {
            tempInfo_.coolerPower = powerResult.value();
        }
    }

    void updateSensorInfo() {
        auto prop = parent_->getProperty("CCD_INFO");
        if (!prop.has_value()) return;

        for (const auto& elem : prop.value().numberElements) {
            if (elem.name == "CCD_MAX_X") {
                sensorInfo_.width = static_cast<int>(elem.value);
            } else if (elem.name == "CCD_MAX_Y") {
                sensorInfo_.height = static_cast<int>(elem.value);
            } else if (elem.name == "CCD_PIXEL_SIZE_X") {
                sensorInfo_.pixelSizeX = elem.value;
            } else if (elem.name == "CCD_PIXEL_SIZE_Y") {
                sensorInfo_.pixelSizeY = elem.value;
            } else if (elem.name == "CCD_BITS_PER_PIXEL") {
                sensorInfo_.bitsPerPixel = static_cast<int>(elem.value);
            }
        }

        // Get supported binning modes
        sensorInfo_.supportedBinning.clear();
        auto binProp = parent_->getProperty("CCD_BIN");
        if (binProp.has_value()) {
            // Assume standard binning modes
            sensorInfo_.supportedBinning = {
                {1, 1}, {2, 2}, {3, 3}, {4, 4}
            };
        }

        LOG_F(INFO, "INDIGO Camera[{}]: Sensor {}x{}, pixel {:.2f}x{:.2f}um, {} bit",
              parent_->getINDIGODeviceName(),
              sensorInfo_.width, sensorInfo_.height,
              sensorInfo_.pixelSizeX, sensorInfo_.pixelSizeY,
              sensorInfo_.bitsPerPixel);
    }

    INDIGOCamera* parent_;

    std::atomic<bool> exposing_{false};
    ExposureStatus exposureStatus_;
    TemperatureInfo tempInfo_;
    SensorInfo sensorInfo_;

    std::mutex callbackMutex_;
    ImageCallback imageCallback_;
    ExposureProgressCallback progressCallback_;
};

// ============================================================================
// INDIGOCamera public interface
// ============================================================================

INDIGOCamera::INDIGOCamera(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Camera"),
      cameraImpl_(std::make_unique<Impl>(this)) {}

INDIGOCamera::~INDIGOCamera() = default;

auto INDIGOCamera::startExposure(double duration, FrameType frameType)
    -> DeviceResult<bool> {
    return cameraImpl_->startExposure(duration, frameType);
}

auto INDIGOCamera::abortExposure() -> DeviceResult<bool> {
    return cameraImpl_->abortExposure();
}

auto INDIGOCamera::isExposing() const -> bool {
    return cameraImpl_->isExposing();
}

auto INDIGOCamera::getExposureStatus() const -> ExposureStatus {
    return cameraImpl_->getExposureStatus();
}

void INDIGOCamera::setImageCallback(ImageCallback callback) {
    cameraImpl_->setImageCallback(std::move(callback));
}

void INDIGOCamera::setExposureProgressCallback(ExposureProgressCallback callback) {
    cameraImpl_->setExposureProgressCallback(std::move(callback));
}

auto INDIGOCamera::hasCooler() const -> bool {
    return cameraImpl_->hasCooler();
}

auto INDIGOCamera::setCoolerOn(bool on) -> DeviceResult<bool> {
    return cameraImpl_->setCoolerOn(on);
}

auto INDIGOCamera::isCoolerOn() const -> bool {
    return cameraImpl_->isCoolerOn();
}

auto INDIGOCamera::setTargetTemperature(double celsius) -> DeviceResult<bool> {
    return cameraImpl_->setTargetTemperature(celsius);
}

auto INDIGOCamera::getCurrentTemperature() const -> double {
    return cameraImpl_->getCurrentTemperature();
}

auto INDIGOCamera::getTargetTemperature() const -> double {
    return cameraImpl_->getTargetTemperature();
}

auto INDIGOCamera::getCoolerPower() const -> double {
    return cameraImpl_->getCoolerPower();
}

auto INDIGOCamera::getTemperatureInfo() const -> TemperatureInfo {
    return cameraImpl_->getTemperatureInfo();
}

auto INDIGOCamera::getSensorInfo() const -> SensorInfo {
    return cameraImpl_->getSensorInfo();
}

auto INDIGOCamera::getSensorWidth() const -> int {
    return cameraImpl_->getSensorWidth();
}

auto INDIGOCamera::getSensorHeight() const -> int {
    return cameraImpl_->getSensorHeight();
}

auto INDIGOCamera::getPixelSize() const -> std::pair<double, double> {
    return cameraImpl_->getPixelSize();
}

auto INDIGOCamera::getBitDepth() const -> int {
    return cameraImpl_->getBitDepth();
}

auto INDIGOCamera::setBinning(int horizontal, int vertical) -> DeviceResult<bool> {
    return cameraImpl_->setBinning(horizontal, vertical);
}

auto INDIGOCamera::getBinning() const -> BinningMode {
    return cameraImpl_->getBinning();
}

auto INDIGOCamera::getSupportedBinning() const -> std::vector<BinningMode> {
    return cameraImpl_->getSupportedBinning();
}

auto INDIGOCamera::setROI(const CameraROI& roi) -> DeviceResult<bool> {
    return cameraImpl_->setROI(roi);
}

auto INDIGOCamera::getROI() const -> CameraROI {
    return cameraImpl_->getROI();
}

auto INDIGOCamera::resetROI() -> DeviceResult<bool> {
    return cameraImpl_->resetROI();
}

auto INDIGOCamera::hasGainControl() const -> bool {
    return cameraImpl_->hasGainControl();
}

auto INDIGOCamera::setGain(double gain) -> DeviceResult<bool> {
    return cameraImpl_->setGain(gain);
}

auto INDIGOCamera::getGain() const -> double {
    return cameraImpl_->getGain();
}

auto INDIGOCamera::getGainRange() const -> std::pair<double, double> {
    return cameraImpl_->getGainRange();
}

auto INDIGOCamera::hasOffsetControl() const -> bool {
    return cameraImpl_->hasOffsetControl();
}

auto INDIGOCamera::setOffset(double offset) -> DeviceResult<bool> {
    return cameraImpl_->setOffset(offset);
}

auto INDIGOCamera::getOffset() const -> double {
    return cameraImpl_->getOffset();
}

auto INDIGOCamera::getOffsetRange() const -> std::pair<double, double> {
    return cameraImpl_->getOffsetRange();
}

auto INDIGOCamera::setFrameType(FrameType type) -> DeviceResult<bool> {
    return cameraImpl_->setFrameType(type);
}

auto INDIGOCamera::getFrameType() const -> FrameType {
    return cameraImpl_->getFrameType();
}

auto INDIGOCamera::setImageFormat(const std::string& format) -> DeviceResult<bool> {
    return cameraImpl_->setImageFormat(format);
}

auto INDIGOCamera::getImageFormat() const -> std::string {
    return cameraImpl_->getImageFormat();
}

auto INDIGOCamera::getSupportedFormats() const -> std::vector<std::string> {
    return cameraImpl_->getSupportedFormats();
}

auto INDIGOCamera::getCapabilities() const -> json {
    return cameraImpl_->getCapabilities();
}

auto INDIGOCamera::getStatus() const -> json {
    return cameraImpl_->getStatus();
}

void INDIGOCamera::onConnected() {
    INDIGODeviceBase::onConnected();
    cameraImpl_->onConnected();
}

void INDIGOCamera::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    cameraImpl_->onDisconnected();
}

void INDIGOCamera::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    cameraImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
