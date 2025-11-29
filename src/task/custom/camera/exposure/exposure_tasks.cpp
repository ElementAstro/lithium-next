/**
 * @file exposure_tasks.cpp
 * @brief Implementation of camera exposure tasks
 */

#include "exposure_tasks.hpp"
#include <algorithm>
#include <cmath>
#include <thread>

namespace lithium::task::camera {

// ============================================================================
// TakeExposureTask Implementation
// ============================================================================

void TakeExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr,
                       "Exposure time in seconds");
    addParamDefinition("type", "string", false, "light",
                       "Frame type (light/dark/bias/flat/snapshot)");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("offset", "integer", false, 10,
                       "Camera offset/black level");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning configuration");
    addParamDefinition("filter", "string", false, "L", "Filter name");
    addParamDefinition("output_path", "string", false, "", "Output file path");
}

void TakeExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateType(params, "exposure", "number");

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);

    if (params.contains("gain")) {
        validateGain(params["gain"].get<int>());
    }
    if (params.contains("offset")) {
        validateOffset(params["offset"].get<int>());
    }
    if (params.contains("binning")) {
        BinningConfig binning = params["binning"].get<BinningConfig>();
        validateBinning(binning);
    }
}

void TakeExposureTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    ExposureType type = params.value("type", ExposureType::Light);
    int gain = params.value("gain", 100);
    int offset = params.value("offset", 10);
    BinningConfig binning = params.value("binning", BinningConfig{1, 1});
    std::string filter = params.value("filter", "L");

    logProgress("Configuring camera settings");
    logProgress("Gain: " + std::to_string(gain) +
                ", Offset: " + std::to_string(offset));
    logProgress("Binning: " + std::to_string(binning.x) + "x" +
                std::to_string(binning.y));

    logProgress("Starting exposure: " + std::to_string(exposure) + "s");

    // Simulate exposure (replace with actual camera API)
    int simulatedMs = static_cast<int>(std::min(exposure * 100, 5000.0));
    std::this_thread::sleep_for(std::chrono::milliseconds(simulatedMs));

    logProgress("Exposure complete, reading out sensor");

    // Simulate readout
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    logProgress("Frame captured successfully");
}

// ============================================================================
// TakeManyExposureTask Implementation
// ============================================================================

void TakeManyExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr,
                       "Exposure time in seconds");
    addParamDefinition("count", "integer", true, nullptr,
                       "Number of exposures");
    addParamDefinition("delay", "number", false, 0.0,
                       "Delay between exposures in seconds");
    addParamDefinition("type", "string", false, "light", "Frame type");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("offset", "integer", false, 10, "Camera offset");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
    addParamDefinition("filter", "string", false, "L", "Filter name");
    addParamDefinition("output_pattern", "string", false, "",
                       "Output filename pattern");
}

void TakeManyExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateRequired(params, "count");

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);

    int count = params["count"].get<int>();
    validateCount(count);

    if (params.contains("delay")) {
        double delay = params["delay"].get<double>();
        if (delay < 0 || delay > 3600) {
            throw ValidationError("Delay must be between 0 and 3600 seconds");
        }
    }
}

void TakeManyExposureTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    int count = params["count"].get<int>();
    double delay = params.value("delay", 0.0);

    logProgress("Starting sequence of " + std::to_string(count) + " exposures");

    for (int i = 0; i < count; ++i) {
        double progress = static_cast<double>(i) / count;
        logProgress("Taking exposure " + std::to_string(i + 1) + "/" +
                        std::to_string(count),
                    progress);

        // Create single exposure params
        json singleParams = params;
        singleParams.erase("count");
        singleParams.erase("delay");

        TakeExposureTask singleExposure;
        singleExposure.execute(singleParams);

        // Delay between exposures
        if (delay > 0 && i < count - 1) {
            logProgress("Waiting " + std::to_string(delay) +
                        "s before next exposure");
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(delay * 1000)));
        }
    }

    logProgress(
        "Sequence complete: " + std::to_string(count) + " frames captured",
        1.0);
}

// ============================================================================
// SubframeExposureTask Implementation
// ============================================================================

void SubframeExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("subframe", "object", true, nullptr,
                       "Subframe coordinates {x, y, width, height}");
    addParamDefinition("type", "string", false, "light", "Frame type");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("offset", "integer", false, 10, "Camera offset");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
}

void SubframeExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateRequired(params, "subframe");

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);

    SubframeConfig subframe = params["subframe"].get<SubframeConfig>();
    // Use default sensor size for validation (can be configured)
    validateSubframe(subframe, 9576, 6388);  // Example: full-frame sensor
}

void SubframeExposureTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    SubframeConfig subframe = params["subframe"].get<SubframeConfig>();

    logProgress("Setting subframe ROI: " + std::to_string(subframe.width) +
                "x" + std::to_string(subframe.height) + " at (" +
                std::to_string(subframe.x) + ", " + std::to_string(subframe.y) +
                ")");

    // Take exposure with subframe
    json exposureParams = params;
    exposureParams.erase("subframe");

    TakeExposureTask exposure_task;
    exposure_task.execute(exposureParams);

    logProgress("Subframe exposure complete");
}

// ============================================================================
// SmartExposureTask Implementation
// ============================================================================

void SmartExposureTask::setupParameters() {
    addParamDefinition("target_snr", "number", false, 50.0,
                       "Target signal-to-noise ratio");
    addParamDefinition("min_exposure", "number", false, 1.0,
                       "Minimum exposure time");
    addParamDefinition("max_exposure", "number", false, 300.0,
                       "Maximum exposure time");
    addParamDefinition("max_attempts", "integer", false, 5,
                       "Maximum optimization attempts");
    addParamDefinition("tolerance", "number", false, 0.1,
                       "SNR tolerance (fraction)");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("offset", "integer", false, 10, "Camera offset");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
}

void SmartExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    double targetSNR = params.value("target_snr", 50.0);
    if (targetSNR < 1.0 || targetSNR > 1000.0) {
        throw ValidationError("Target SNR must be between 1 and 1000");
    }

    double minExp = params.value("min_exposure", 1.0);
    double maxExp = params.value("max_exposure", 300.0);
    if (minExp >= maxExp) {
        throw ValidationError("min_exposure must be less than max_exposure");
    }
}

void SmartExposureTask::executeImpl(const json& params) {
    double targetSNR = params.value("target_snr", 50.0);
    double minExposure = params.value("min_exposure", 1.0);
    double maxExposure = params.value("max_exposure", 300.0);
    int maxAttempts = params.value("max_attempts", 5);
    double tolerance = params.value("tolerance", 0.1);

    logProgress("Starting smart exposure targeting SNR " +
                std::to_string(targetSNR));

    double currentExposure = (minExposure + maxExposure) / 2.0;
    double achievedSNR = 0.0;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        double progress = static_cast<double>(attempt - 1) / maxAttempts;
        logProgress("Attempt " + std::to_string(attempt) + "/" +
                        std::to_string(maxAttempts) + " with " +
                        std::to_string(currentExposure) + "s exposure",
                    progress);

        // Take test exposure
        json exposureParams = {
            {"exposure", currentExposure},
            {"type", "light"},
            {"gain", params.value("gain", 100)},
            {"offset", params.value("offset", 10)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

        TakeExposureTask testExposure;
        testExposure.execute(exposureParams);

        // Simulate SNR measurement (replace with actual image analysis)
        achievedSNR =
            std::min(targetSNR * 1.2, std::sqrt(currentExposure) * 15.0);

        logProgress("Achieved SNR: " + std::to_string(achievedSNR) +
                    ", Target: " + std::to_string(targetSNR));

        // Check if within tolerance
        if (std::abs(achievedSNR - targetSNR) <= targetSNR * tolerance) {
            logProgress("Target SNR achieved within tolerance");
            break;
        }

        // Calculate next exposure
        if (attempt < maxAttempts) {
            currentExposure = calculateOptimalExposure(currentExposure,
                                                       achievedSNR, targetSNR);
            currentExposure =
                std::clamp(currentExposure, minExposure, maxExposure);
        }
    }

    // Take final exposure
    logProgress("Taking final exposure: " + std::to_string(currentExposure) +
                "s");
    json finalParams = params;
    finalParams["exposure"] = currentExposure;
    finalParams.erase("target_snr");
    finalParams.erase("min_exposure");
    finalParams.erase("max_exposure");
    finalParams.erase("max_attempts");
    finalParams.erase("tolerance");

    TakeExposureTask finalExposure;
    finalExposure.execute(finalParams);

    logProgress(
        "Smart exposure complete with final SNR " + std::to_string(achievedSNR),
        1.0);
}

double SmartExposureTask::calculateOptimalExposure(double currentExposure,
                                                   double achievedSNR,
                                                   double targetSNR) {
    // SNR scales with sqrt(exposure) for shot-noise limited images
    double ratio = targetSNR / achievedSNR;
    return currentExposure * ratio * ratio;
}

}  // namespace lithium::task::camera
