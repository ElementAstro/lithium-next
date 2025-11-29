/**
 * @file settings_tasks.cpp
 * @brief Implementation of camera settings tasks
 */

#include "settings_tasks.hpp"
#include <thread>

namespace lithium::task::camera {

// ============================================================================
// CameraSettingsTask Implementation
// ============================================================================

void CameraSettingsTask::setupParameters() {
    addParamDefinition("gain", "integer", false, nullptr, "Camera gain");
    addParamDefinition("offset", "integer", false, nullptr, "Camera offset");
    addParamDefinition("binning", "object", false, nullptr,
                       "Binning configuration");
    addParamDefinition("cooling", "boolean", false, nullptr, "Enable cooling");
    addParamDefinition("target_temp", "number", false, nullptr,
                       "Target temperature");
    addParamDefinition("fan_speed", "integer", false, nullptr,
                       "Fan speed (0-100)");
    addParamDefinition("usb_bandwidth", "integer", false, nullptr,
                       "USB bandwidth limit");
}

void CameraSettingsTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    if (params.contains("gain")) {
        validateGain(params["gain"].get<int>());
    }
    if (params.contains("offset")) {
        validateOffset(params["offset"].get<int>());
    }
    if (params.contains("target_temp")) {
        validateTemperature(params["target_temp"].get<double>());
    }
    if (params.contains("fan_speed")) {
        int fan = params["fan_speed"].get<int>();
        if (fan < 0 || fan > 100) {
            throw ValidationError("Fan speed must be between 0 and 100");
        }
    }
}

void CameraSettingsTask::executeImpl(const json& params) {
    logProgress("Applying camera settings");

    if (params.contains("gain")) {
        int gain = params["gain"].get<int>();
        logProgress("Setting gain: " + std::to_string(gain));
    }

    if (params.contains("offset")) {
        int offset = params["offset"].get<int>();
        logProgress("Setting offset: " + std::to_string(offset));
    }

    if (params.contains("binning")) {
        BinningConfig binning = params["binning"].get<BinningConfig>();
        logProgress("Setting binning: " + std::to_string(binning.x) + "x" +
                    std::to_string(binning.y));
    }

    if (params.contains("cooling") && params["cooling"].get<bool>()) {
        logProgress("Enabling cooling");
        if (params.contains("target_temp")) {
            double temp = params["target_temp"].get<double>();
            logProgress("Target temperature: " + std::to_string(temp) + "°C");
        }
    }

    if (params.contains("fan_speed")) {
        int fan = params["fan_speed"].get<int>();
        logProgress("Setting fan speed: " + std::to_string(fan) + "%");
    }

    logProgress("Camera settings applied");
}

// ============================================================================
// CameraPreviewTask Implementation
// ============================================================================

void CameraPreviewTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 1.0,
                       "Preview exposure time");
    addParamDefinition("binning", "object", false, json{{"x", 2}, {"y", 2}},
                       "Preview binning");
    addParamDefinition("stretch", "boolean", false, true,
                       "Auto-stretch preview");
    addParamDefinition("crosshair", "boolean", false, true, "Show crosshair");
    addParamDefinition("loop", "boolean", false, false,
                       "Continuous preview mode");
    addParamDefinition("loop_count", "integer", false, 0,
                       "Number of loops (0=infinite)");
}

void CameraPreviewTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    double exposure = params.value("exposure", 1.0);
    validateExposure(exposure, 0.001, 60.0);  // Shorter max for preview
}

void CameraPreviewTask::executeImpl(const json& params) {
    double exposure = params.value("exposure", 1.0);
    BinningConfig binning = params.value("binning", BinningConfig{2, 2});
    bool loop = params.value("loop", false);
    int loopCount = params.value("loop_count", 0);

    logProgress("Starting camera preview");
    logProgress("Exposure: " + std::to_string(exposure) + "s, Binning: " +
                std::to_string(binning.x) + "x" + std::to_string(binning.y));

    int iterations = loop ? (loopCount > 0 ? loopCount : 1) : 1;
    bool infinite = loop && loopCount == 0;

    int count = 0;
    while (infinite || count < iterations) {
        logProgress("Preview frame " + std::to_string(count + 1));

        // Simulate quick preview exposure
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(exposure * 100)));

        count++;

        if (!infinite && count >= iterations)
            break;
    }

    logProgress("Preview complete");
}

}  // namespace lithium::task::camera
