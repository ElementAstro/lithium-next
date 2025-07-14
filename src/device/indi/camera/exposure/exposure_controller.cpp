#include "exposure_controller.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>
#include <fstream>

namespace lithium::device::indi::camera {

ExposureController::ExposureController(std::shared_ptr<INDICameraCore> core)
    : ComponentBase(core) {
    spdlog::debug("Creating exposure controller");
}

auto ExposureController::initialize() -> bool {
    spdlog::debug("Initializing exposure controller");

    // Reset exposure state
    isExposing_.store(false);
    currentExposureDuration_.store(0.0);
    lastExposureDuration_.store(0.0);
    exposureCount_.store(0);

    return true;
}

auto ExposureController::destroy() -> bool {
    spdlog::debug("Destroying exposure controller");

    // Abort any ongoing exposure
    if (isExposing()) {
        abortExposure();
    }

    return true;
}

auto ExposureController::getComponentName() const -> std::string {
    return "ExposureController";
}

auto ExposureController::handleProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        return false;
    }

    std::string propertyName = property.getName();

    if (propertyName == "CCD_EXPOSURE") {
        handleExposureProperty(property);
        return true;
    } else if (propertyName == "CCD1") {
        handleBlobProperty(property);
        return true;
    }

    return false;
}

auto ExposureController::startExposure(double duration) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    if (isExposing()) {
        spdlog::warn("Exposure already in progress");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber exposureProperty = device.getProperty("CCD_EXPOSURE");
        if (!exposureProperty.isValid()) {
            spdlog::error("CCD_EXPOSURE property not found");
            return false;
        }

        spdlog::info("Starting exposure of {} seconds...", duration);
        currentExposureDuration_.store(duration);
        exposureStartTime_ = std::chrono::system_clock::now();
        isExposing_.store(true);

        exposureProperty[0].setValue(duration);
        getCore()->sendNewProperty(exposureProperty);
        getCore()->updateCameraState(CameraState::EXPOSING);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start exposure: {}", e.what());
        return false;
    }
}

auto ExposureController::abortExposure() -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }

    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch ccdAbort = device.getProperty("CCD_ABORT_EXPOSURE");
        if (!ccdAbort.isValid()) {
            spdlog::error("CCD_ABORT_EXPOSURE property not found");
            return false;
        }

        spdlog::info("Aborting exposure...");
        ccdAbort[0].setState(ISS_ON);
        getCore()->sendNewProperty(ccdAbort);
        getCore()->updateCameraState(CameraState::ABORTED);
        isExposing_.store(false);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to abort exposure: {}", e.what());
        return false;
    }
}

auto ExposureController::isExposing() const -> bool {
    return isExposing_.load();
}

auto ExposureController::getExposureProgress() const -> double {
    if (!isExposing()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - exposureStartTime_).count() / 1000.0;

    double duration = currentExposureDuration_.load();
    if (duration <= 0) {
        return 0.0;
    }

    return std::min(1.0, elapsed / duration);
}

auto ExposureController::getExposureRemaining() const -> double {
    if (!isExposing()) {
        return 0.0;
    }

    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - exposureStartTime_).count() / 1000.0;

    double duration = currentExposureDuration_.load();
    return std::max(0.0, duration - elapsed);
}

auto ExposureController::getExposureResult() -> std::shared_ptr<AtomCameraFrame> {
    return getCore()->getCurrentFrame();
}

auto ExposureController::getLastExposureDuration() const -> double {
    return lastExposureDuration_.load();
}

auto ExposureController::getExposureCount() const -> uint32_t {
    return exposureCount_.load();
}

auto ExposureController::resetExposureCount() -> bool {
    exposureCount_.store(0);
    return true;
}

auto ExposureController::saveImage(const std::string& path) -> bool {
    auto frame = getCore()->getCurrentFrame();
    if (!frame || !frame->data) {
        spdlog::error("No image data available");
        return false;
    }

    try {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            spdlog::error("Failed to open file for writing: {}", path);
            return false;
        }

        file.write(static_cast<const char*>(frame->data), frame->size);
        file.close();

        spdlog::info("Image saved to: {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save image: {}", e.what());
        return false;
    }
}

// Private methods
void ExposureController::handleExposureProperty(INDI::Property property) {
    if (property.getType() != INDI_NUMBER) {
        return;
    }

    INDI::PropertyNumber exposureProperty = property;
    if (!exposureProperty.isValid()) {
        return;
    }

    if (exposureProperty.getState() == IPS_BUSY) {
        if (!isExposing()) {
            // Exposure started
            isExposing_.store(true);
            exposureStartTime_ = std::chrono::system_clock::now();
            currentExposureDuration_.store(exposureProperty[0].getValue());
            getCore()->updateCameraState(CameraState::EXPOSING);
            spdlog::debug("Exposure started");
        }
    } else if (exposureProperty.getState() == IPS_OK) {
        if (isExposing()) {
            // Exposure completed
            isExposing_.store(false);
            lastExposureDuration_.store(currentExposureDuration_.load());
            exposureCount_.fetch_add(1);
            getCore()->updateCameraState(CameraState::DOWNLOADING);
            spdlog::debug("Exposure completed");
        }
    } else if (exposureProperty.getState() == IPS_ALERT) {
        // Exposure error
        isExposing_.store(false);
        getCore()->updateCameraState(CameraState::ERROR);
        spdlog::error("Exposure error");
    }
}

void ExposureController::handleBlobProperty(INDI::Property property) {
    if (property.getType() != INDI_BLOB) {
        return;
    }

    INDI::PropertyBlob blobProperty = property;
    if (!blobProperty.isValid() || blobProperty[0].getBlobLen() == 0) {
        return;
    }

    processReceivedImage(blobProperty);
}

void ExposureController::processReceivedImage(const INDI::PropertyBlob& property) {
    if (!property.isValid() || property[0].getBlobLen() == 0) {
        spdlog::warn("Invalid image data received");
        return;
    }

    size_t imageSize = property[0].getBlobLen();
    const void* imageData = property[0].getBlob();
    const char* format = property[0].getFormat();

    spdlog::info("Processing exposure image: size={}, format={}", imageSize, format ? format : "unknown");

    // Validate image data
    if (!validateImageData(imageData, imageSize)) {
        spdlog::error("Invalid image data received");
        return;
    }

    // Create frame structure
    auto frame = std::make_shared<AtomCameraFrame>();
    frame->data = const_cast<void*>(imageData);
    frame->size = imageSize;

    // Store the frame
    getCore()->setCurrentFrame(frame);
    getCore()->updateCameraState(CameraState::IDLE);

    spdlog::info("Image received: {} bytes", frame->size);
}

auto ExposureController::validateImageData(const void* data, size_t size) -> bool {
    if (!data || size == 0) {
        return false;
    }

    // Basic validation - check if data looks like a valid image
    // This is a simple check, more sophisticated validation could be added
    const auto* bytes = static_cast<const uint8_t*>(data);

    // Check for common image format headers
    if (size >= 4) {
        // FITS format check
        if (std::memcmp(bytes, "SIMP", 4) == 0) {
            return true;
        }

        // JPEG format check
        if (bytes[0] == 0xFF && bytes[1] == 0xD8) {
            return true;
        }

        // PNG format check
        if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
            return true;
        }
    }

    // If no specific format detected, assume it's valid raw data
    return true;
}

} // namespace lithium::device::indi::camera
