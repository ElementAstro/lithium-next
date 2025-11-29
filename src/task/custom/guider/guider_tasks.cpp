/**
 * @file guider_tasks.cpp
 * @brief Implementation of guider tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "guider_tasks.hpp"
#include <cmath>
#include <random>
#include <thread>

namespace lithium::task::guider {

// ============================================================================
// StartGuidingTask Implementation
// ============================================================================

void StartGuidingTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 2.0,
                       "Guide camera exposure");
    addParamDefinition("calibrate", "boolean", false, true,
                       "Calibrate before guiding");
    addParamDefinition("settle_time", "number", false, 10.0,
                       "Settle time in seconds");
    addParamDefinition("settle_threshold", "number", false, 1.5,
                       "Settle threshold in pixels");
    addParamDefinition("guide_star", "object", false, nullptr,
                       "Guide star coordinates");
}

void StartGuidingTask::executeImpl(const json& params) {
    bool calibrate = params.value("calibrate", true);
    double settleTime = params.value("settle_time", 10.0);
    double settleThreshold = params.value("settle_threshold", 1.5);

    logProgress("Starting autoguiding");

    if (calibrate) {
        logProgress("Calibrating guider...");
        if (!calibrateGuider(params)) {
            throw std::runtime_error("Guider calibration failed");
        }
        logProgress("Calibration complete", 0.5);
    }

    logProgress("Starting guiding loop");
    if (!startGuiding(params)) {
        throw std::runtime_error("Failed to start guiding");
    }

    logProgress("Waiting for guiding to settle");
    std::this_thread::sleep_for(
        std::chrono::seconds(static_cast<int>(settleTime)));

    logProgress("Autoguiding started and settled", 1.0);
}

bool StartGuidingTask::calibrateGuider(const json& params) {
    // Simulate calibration
    for (int i = 0; i < 4; ++i) {
        if (!shouldContinue())
            return false;
        logProgress("Calibration step " + std::to_string(i + 1) + "/4");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return true;
}

bool StartGuidingTask::startGuiding(const json& params) {
    // Simulate starting guiding
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return true;
}

// ============================================================================
// StopGuidingTask Implementation
// ============================================================================

void StopGuidingTask::setupParameters() {
    addParamDefinition("wait_settle", "boolean", false, false,
                       "Wait for mount to settle");
}

void StopGuidingTask::executeImpl(const json& params) {
    logProgress("Stopping autoguiding");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (params.value("wait_settle", false)) {
        logProgress("Waiting for mount to settle");
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    logProgress("Autoguiding stopped", 1.0);
}

// ============================================================================
// PauseGuidingTask Implementation
// ============================================================================

void PauseGuidingTask::setupParameters() {
    // No additional parameters needed
}

void PauseGuidingTask::executeImpl(const json& params) {
    logProgress("Pausing autoguiding");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    logProgress("Autoguiding paused", 1.0);
}

// ============================================================================
// ResumeGuidingTask Implementation
// ============================================================================

void ResumeGuidingTask::setupParameters() {
    addParamDefinition("settle_time", "number", false, 5.0,
                       "Settle time after resume");
}

void ResumeGuidingTask::executeImpl(const json& params) {
    double settleTime = params.value("settle_time", 5.0);

    logProgress("Resuming autoguiding");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    logProgress("Waiting for guiding to settle");
    std::this_thread::sleep_for(
        std::chrono::seconds(static_cast<int>(settleTime)));

    logProgress("Autoguiding resumed and settled", 1.0);
}

// ============================================================================
// DitherTask Implementation
// ============================================================================

void DitherTask::setupParameters() {
    addParamDefinition("amount", "number", false, 5.0,
                       "Dither amount in pixels");
    addParamDefinition("ra_only", "boolean", false, false, "Dither in RA only");
    addParamDefinition("settle_time", "number", false, 10.0,
                       "Maximum settle time");
    addParamDefinition("settle_threshold", "number", false, 1.5,
                       "Settle threshold in pixels");
}

void DitherTask::executeImpl(const json& params) {
    double amount = params.value("amount", 5.0);
    double settleTime = params.value("settle_time", 10.0);
    double settleThreshold = params.value("settle_threshold", 1.5);

    logProgress("Dithering by " + std::to_string(amount) + " pixels");

    performDither(amount);

    logProgress("Waiting for settle");
    if (!waitForSettle(settleTime, settleThreshold)) {
        logProgress("Warning: Settle timeout, continuing anyway");
    }

    logProgress("Dither complete", 1.0);
}

void DitherTask::performDither(double amount) {
    // Simulate dither command
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

bool DitherTask::waitForSettle(double timeout, double threshold) {
    // Simulate settle wait with random settle time
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0, timeout);

    double settleDelay = dis(gen);
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(settleDelay * 100)));

    return settleDelay < timeout;
}

// ============================================================================
// GuidedExposureSequenceTask Implementation
// ============================================================================

void GuidedExposureSequenceTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr,
                       "Exposure time per frame");
    addParamDefinition("count", "integer", true, nullptr, "Number of frames");
    addParamDefinition("dither", "boolean", false, true, "Enable dithering");
    addParamDefinition("dither_every", "integer", false, 1,
                       "Dither every N frames");
    addParamDefinition("dither_amount", "number", false, 5.0,
                       "Dither amount in pixels");
    addParamDefinition("settle_time", "number", false, 10.0,
                       "Settle time after dither");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
}

void GuidedExposureSequenceTask::executeImpl(const json& params) {
    auto expResult = ParamValidator::required(params, "exposure");
    auto countResult = ParamValidator::required(params, "count");

    if (!expResult || !countResult) {
        throw std::invalid_argument("Exposure and count are required");
    }

    double exposure = params["exposure"].get<double>();
    int count = params["count"].get<int>();
    bool dither = params.value("dither", true);
    int ditherEvery = params.value("dither_every", 1);
    double ditherAmount = params.value("dither_amount", 5.0);

    logProgress("Starting guided exposure sequence: " + std::to_string(count) +
                " frames");

    // Ensure guiding is running
    if (!waitForGuiding(10.0)) {
        throw std::runtime_error("Guiding is not running");
    }

    for (int i = 0; i < count; ++i) {
        if (!shouldContinue()) {
            logProgress("Sequence cancelled");
            return;
        }

        double progress = static_cast<double>(i) / count;
        logProgress(
            "Frame " + std::to_string(i + 1) + "/" + std::to_string(count),
            progress);

        // Simulate exposure
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(exposure * 100)));

        // Dither after frame if enabled
        if (dither && (i + 1) % ditherEvery == 0 && i < count - 1) {
            DitherTask ditherTask;
            ditherTask.execute(
                {{"amount", ditherAmount},
                 {"settle_time", params.value("settle_time", 10.0)}});
        }
    }

    logProgress("Guided exposure sequence complete: " + std::to_string(count) +
                    " frames",
                1.0);
}

bool GuidedExposureSequenceTask::waitForGuiding(double timeout) {
    // Simulate waiting for guiding to be active
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;  // Assume guiding is running
}

// ============================================================================
// CalibrateGuiderTask Implementation
// ============================================================================

void CalibrateGuiderTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 2.0,
                       "Calibration exposure time");
    addParamDefinition("step_size", "integer", false, 1000,
                       "Calibration step size (ms)");
    addParamDefinition("steps", "integer", false, 12,
                       "Number of calibration steps");
    addParamDefinition("clear_previous", "boolean", false, true,
                       "Clear previous calibration");
}

void CalibrateGuiderTask::executeImpl(const json& params) {
    double exposure = params.value("exposure", 2.0);
    int stepSize = params.value("step_size", 1000);
    int steps = params.value("steps", 12);

    logProgress("Starting guider calibration");

    // Simulate calibration phases
    std::vector<std::string> phases = {"West", "East", "North", "South"};
    int stepsPerPhase = steps / 4;

    for (size_t p = 0; p < phases.size(); ++p) {
        if (!shouldContinue()) {
            logProgress("Calibration cancelled");
            return;
        }

        double phaseProgress = static_cast<double>(p) / phases.size();
        logProgress("Calibrating " + phases[p], phaseProgress);

        for (int i = 0; i < stepsPerPhase; ++i) {
            if (!shouldContinue()) {
                logProgress("Calibration cancelled");
                return;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(exposure * 100 + stepSize / 10)));
        }
    }

    logProgress("Guider calibration complete", 1.0);
}

}  // namespace lithium::task::guider
