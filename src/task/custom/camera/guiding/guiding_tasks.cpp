/**
 * @file guiding_tasks.cpp
 * @brief Implementation of guiding tasks
 */

#include "guiding_tasks.hpp"
#include <random>
#include <thread>
#include "../exposure/exposure_tasks.hpp"

namespace lithium::task::camera {

// ============================================================================
// AutoGuidingTask Implementation
// ============================================================================

void AutoGuidingTask::setupParameters() {
    addParamDefinition("action", "string", false, "start",
                       "Action (start/stop/calibrate)");
    addParamDefinition("exposure", "number", false, 2.0,
                       "Guide camera exposure");
    addParamDefinition("settle_timeout", "number", false, 60.0,
                       "Settling timeout");
    addParamDefinition("settle_threshold", "number", false, 0.5,
                       "Settle threshold (arcsec)");
    addParamDefinition("calibration_step", "integer", false, 1000,
                       "Calibration step size");
    addParamDefinition("guide_rate", "number", false, 0.5,
                       "Guide rate (x sidereal)");
}

void AutoGuidingTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    std::string action = params.value("action", "start");
    if (action != "start" && action != "stop" && action != "calibrate") {
        throw ValidationError(
            "Invalid action. Must be start, stop, or calibrate");
    }
}

void AutoGuidingTask::executeImpl(const json& params) {
    std::string action = params.value("action", "start");

    if (action == "calibrate") {
        logProgress("Starting guider calibration");
        if (!calibrateGuider(params)) {
            throw std::runtime_error("Guider calibration failed");
        }
        logProgress("Calibration complete", 1.0);
    } else if (action == "start") {
        logProgress("Starting autoguiding");
        if (!startGuiding(params)) {
            throw std::runtime_error("Failed to start guiding");
        }
        logProgress("Guiding active", 1.0);
    } else if (action == "stop") {
        logProgress("Stopping autoguiding");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        logProgress("Guiding stopped", 1.0);
    }
}

bool AutoGuidingTask::calibrateGuider(const json& params) {
    int calibStep = params.value("calibration_step", 1000);

    logProgress("Calibrating RA+ direction");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    logProgress("Calibrating RA- direction");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    logProgress("Calibrating Dec+ direction");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    logProgress("Calibrating Dec- direction");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return true;
}

bool AutoGuidingTask::startGuiding(const json& params) {
    double settleTimeout = params.value("settle_timeout", 60.0);
    double settleThreshold = params.value("settle_threshold", 0.5);

    logProgress("Acquiring guide star");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    logProgress("Waiting for guiding to settle");
    std::this_thread::sleep_for(std::chrono::seconds(3));

    logProgress("Guiding settled within " + std::to_string(settleThreshold) +
                " arcsec");
    return true;
}

// ============================================================================
// GuidedExposureTask Implementation
// ============================================================================

void GuidedExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("count", "integer", false, 1, "Number of exposures");
    addParamDefinition("wait_for_guide", "boolean", false, true,
                       "Wait for guiding to settle");
    addParamDefinition("guide_timeout", "number", false, 60.0,
                       "Guide settling timeout");
    addParamDefinition("abort_on_guide_loss", "boolean", false, true,
                       "Abort if guiding lost");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("filter", "string", false, "L", "Filter name");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
}

void GuidedExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);
}

void GuidedExposureTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    int count = params.value("count", 1);
    bool waitGuide = params.value("wait_for_guide", true);
    double guideTimeout = params.value("guide_timeout", 60.0);

    logProgress("Starting guided exposure sequence");

    for (int i = 0; i < count; ++i) {
        double progress = static_cast<double>(i) / count;

        if (waitGuide) {
            logProgress("Waiting for guiding to settle", progress);
            if (!waitForGuiding(guideTimeout)) {
                throw std::runtime_error(
                    "Guiding did not settle within timeout");
            }
        }

        logProgress("Taking guided exposure " + std::to_string(i + 1) + "/" +
                        std::to_string(count),
                    progress);

        json exposureParams = {
            {"exposure", exposure},
            {"type", "light"},
            {"filter", params.value("filter", "L")},
            {"gain", params.value("gain", 100)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

        TakeExposureTask guidedExposure;
        guidedExposure.execute(exposureParams);
    }

    logProgress("Guided exposure sequence complete", 1.0);
}

bool GuidedExposureTask::waitForGuiding(double timeout) {
    // Simulate waiting for guiding to settle
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

// ============================================================================
// DitherSequenceTask Implementation
// ============================================================================

void DitherSequenceTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("count", "integer", true, nullptr,
                       "Number of exposures");
    addParamDefinition("dither_amount", "number", false, 5.0,
                       "Dither amount (pixels)");
    addParamDefinition("dither_every", "integer", false, 1,
                       "Dither every N frames");
    addParamDefinition("settle_timeout", "number", false, 30.0,
                       "Settle timeout after dither");
    addParamDefinition("settle_threshold", "number", false, 0.5,
                       "Settle threshold (arcsec)");
    addParamDefinition("random_dither", "boolean", false, true,
                       "Random dither pattern");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("filter", "string", false, "L", "Filter name");
}

void DitherSequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateRequired(params, "count");

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);

    int count = params["count"].get<int>();
    validateCount(count);

    double ditherAmount = params.value("dither_amount", 5.0);
    if (ditherAmount < 0.5 || ditherAmount > 50.0) {
        throw ValidationError(
            "Dither amount must be between 0.5 and 50 pixels");
    }
}

void DitherSequenceTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    int count = params["count"].get<int>();
    double ditherAmount = params.value("dither_amount", 5.0);
    int ditherEvery = params.value("dither_every", 1);
    double settleTimeout = params.value("settle_timeout", 30.0);
    double settleThreshold = params.value("settle_threshold", 0.5);

    logProgress("Starting dither sequence with " + std::to_string(count) +
                " exposures");

    for (int i = 0; i < count; ++i) {
        double progress = static_cast<double>(i) / count;

        // Dither if needed
        if (i > 0 && i % ditherEvery == 0) {
            logProgress("Dithering...", progress);
            performDither(ditherAmount);

            logProgress("Waiting for settle", progress);
            if (!waitForSettle(settleTimeout, settleThreshold)) {
                logProgress("Warning: settle timeout exceeded");
            }
        }

        logProgress(
            "Exposure " + std::to_string(i + 1) + "/" + std::to_string(count),
            progress);

        json exposureParams = {{"exposure", exposure},
                               {"type", "light"},
                               {"filter", params.value("filter", "L")},
                               {"gain", params.value("gain", 100)}};

        TakeExposureTask frameExposure;
        frameExposure.execute(exposureParams);
    }

    logProgress(
        "Dither sequence complete: " + std::to_string(count) + " frames", 1.0);
}

void DitherSequenceTask::performDither(double amount) {
    // Simulate random dither
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-amount, amount);

    double dx = dis(gen);
    double dy = dis(gen);

    logProgress("Dithering by (" + std::to_string(dx) + ", " +
                std::to_string(dy) + ") pixels");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

bool DitherSequenceTask::waitForSettle(double timeout, double threshold) {
    // Simulate settling
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    logProgress("Settled to " + std::to_string(threshold * 0.8) + " arcsec");
    return true;
}

}  // namespace lithium::task::camera
