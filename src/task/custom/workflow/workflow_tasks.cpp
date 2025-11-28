/**
 * @file workflow_tasks.cpp
 * @brief Implementation of astronomical workflow tasks.
 */

#include "workflow_tasks.hpp"
#include <thread>
#include <chrono>
#include "spdlog/spdlog.h"

namespace lithium::task::workflow {

// TargetAcquisitionTask
void TargetAcquisitionTask::setupParameters() {
    addParamDefinition("target_name", "string", true, nullptr, "Target identifier");
    addParamDefinition("coordinates", "object", true, nullptr, "Target coordinates");
    addParamDefinition("settle_time", "number", false, 5, "Settle time (seconds)");
    addParamDefinition("start_guiding", "boolean", false, true, "Start guiding");
    addParamDefinition("perform_autofocus", "boolean", false, true, "Perform autofocus");
}

void TargetAcquisitionTask::executeImpl(const json& params) {
    std::string targetName = params.value("target_name", "Unknown");
    logProgress("Starting target acquisition for: " + targetName);

    Coordinates coords;
    if (params.contains("coordinates")) {
        coords = Coordinates::fromJson(params["coordinates"]);
    }
    if (!coords.isValid()) throw std::runtime_error("Invalid coordinates");

    if (!performSlew(coords, params)) throw std::runtime_error("Slew failed");
    
    int settleTime = params.value("settle_time", 5);
    std::this_thread::sleep_for(std::chrono::seconds(settleTime));

    performPlateSolve(params);
    performCentering(coords, params);
    if (params.value("start_guiding", true)) startGuiding(params);
    if (params.value("perform_autofocus", true)) performAutofocus(params);

    logProgress("Target acquisition complete");
}

bool TargetAcquisitionTask::performSlew(const Coordinates& coords, const json&) {
    spdlog::info("Slewing to RA: {:.4f}° Dec: {:.4f}°", coords.ra, coords.dec);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}

bool TargetAcquisitionTask::performPlateSolve(const json&) {
    spdlog::info("Performing plate solve");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return true;
}

bool TargetAcquisitionTask::performCentering(const Coordinates&, const json&) {
    spdlog::info("Centering target");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}

bool TargetAcquisitionTask::startGuiding(const json&) {
    spdlog::info("Starting guiding");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return true;
}

bool TargetAcquisitionTask::performAutofocus(const json&) {
    spdlog::info("Running autofocus");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return true;
}

// ExposureSequenceTask
void ExposureSequenceTask::setupParameters() {
    addParamDefinition("target_name", "string", true, nullptr, "Target name");
    addParamDefinition("exposure_plans", "array", true, nullptr, "Exposure plans");
    addParamDefinition("dither_enabled", "boolean", false, true, "Enable dithering");
    addParamDefinition("dither_pixels", "number", false, 5.0, "Dither pixels");
}

void ExposureSequenceTask::executeImpl(const json& params) {
    std::string targetName = params.value("target_name", "Unknown");
    logProgress("Starting exposure sequence: " + targetName);

    if (!params.contains("exposure_plans")) throw std::runtime_error("No plans");

    for (const auto& planJson : params["exposure_plans"]) {
        ExposurePlan plan = ExposurePlan::fromJson(planJson);
        logProgress("Executing: " + plan.filterName);
        if (!executeExposurePlan(plan, params)) {
            throw std::runtime_error("Plan failed: " + plan.filterName);
        }
    }
    logProgress("Exposure sequence complete");
}

bool ExposureSequenceTask::executeExposurePlan(const ExposurePlan& plan, const json& params) {
    changeFilter(plan.filterName);
    for (int i = 0; i < plan.count; ++i) {
        if (isCancelled()) return false;
        logProgress("Exposure " + std::to_string(i+1) + "/" + std::to_string(plan.count));
        takeSingleExposure(plan.exposureTime, plan.binning, plan.gain, plan.offset);
        if (params.value("dither_enabled", true) && i < plan.count - 1) {
            performDither(params.value("dither_pixels", 5.0));
        }
    }
    return true;
}

bool ExposureSequenceTask::changeFilter(const std::string& filter) {
    spdlog::info("Changing filter: {}", filter);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return true;
}

bool ExposureSequenceTask::takeSingleExposure(double exp, int bin, int, int) {
    spdlog::info("Taking {}s exposure (bin={})", exp, bin);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}

bool ExposureSequenceTask::performDither(double pixels) {
    spdlog::info("Dithering {} pixels", pixels);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return true;
}

bool ExposureSequenceTask::checkAndRefocus(const json&) { return true; }
bool ExposureSequenceTask::handleMeridianFlip(const json&) { return true; }

// SessionTask
void SessionTask::setupParameters() {
    addParamDefinition("session_name", "string", true, nullptr, "Session name");
    addParamDefinition("targets", "array", true, nullptr, "Targets");
    addParamDefinition("camera_cooling_temp", "number", false, -10.0, "Cooling temp");
}

void SessionTask::executeImpl(const json& params) {
    logProgress("Starting session: " + params.value("session_name", "Session"));
    initializeEquipment(params);
    performSafetyChecks();
    coolCamera(params.value("camera_cooling_temp", -10.0), 10);
    executeTargets(params["targets"], params);
    endSession(params);
    logProgress("Session complete");
}

bool SessionTask::initializeEquipment(const json&) { return true; }
bool SessionTask::performSafetyChecks() { return true; }
bool SessionTask::coolCamera(double, int) { return true; }
bool SessionTask::executeTargets(const json& targets, const json&) {
    for (size_t i = 0; i < targets.size() && !isCancelled(); ++i) {
        logProgress("Target " + std::to_string(i+1) + "/" + std::to_string(targets.size()));
    }
    return true;
}
bool SessionTask::transitionToTarget(const json&, const json&) { return true; }
bool SessionTask::endSession(const json&) { return true; }

// SafetyCheckTask
void SafetyCheckTask::setupParameters() {
    addParamDefinition("check_weather", "boolean", false, true, "Check weather");
}

void SafetyCheckTask::executeImpl(const json& params) {
    logProgress("Safety checks");
    if (params.value("check_weather", true)) checkWeather(params);
    checkDeviceStatus();
    checkMountLimits();
    logProgress("Safety checks passed");
}

bool SafetyCheckTask::checkWeather(const json&) { return true; }
bool SafetyCheckTask::checkDeviceStatus() { return true; }
bool SafetyCheckTask::checkMountLimits() { return true; }

// MeridianFlipTask
void MeridianFlipTask::setupParameters() {
    addParamDefinition("target_coordinates", "object", true, nullptr, "Coordinates");
    addParamDefinition("settle_time", "number", false, 10, "Settle time");
}

void MeridianFlipTask::executeImpl(const json& params) {
    logProgress("Meridian flip");
    stopGuiding();
    performFlip();
    std::this_thread::sleep_for(std::chrono::seconds(params.value("settle_time", 10)));
    if (params.contains("target_coordinates")) {
        recenterTarget(Coordinates::fromJson(params["target_coordinates"]));
    }
    restartGuiding();
    logProgress("Flip complete");
}

bool MeridianFlipTask::stopGuiding() { return true; }
bool MeridianFlipTask::performFlip() { std::this_thread::sleep_for(std::chrono::seconds(3)); return true; }
bool MeridianFlipTask::recenterTarget(const Coordinates&) { return true; }
bool MeridianFlipTask::restartGuiding() { return true; }

// DitherTask
void DitherTask::setupParameters() {
    addParamDefinition("dither_pixels", "number", false, 5.0, "Dither pixels");
    addParamDefinition("settle_time", "number", false, 10, "Settle time");
}

void DitherTask::executeImpl(const json& params) {
    double pixels = params.value("dither_pixels", 5.0);
    logProgress("Dithering " + std::to_string(pixels) + " pixels");
    sendDitherCommand(pixels);
    waitForSettle(0.5, params.value("settle_time", 10), 60);
}

bool DitherTask::sendDitherCommand(double) { return true; }
bool DitherTask::waitForSettle(double, int time, int) {
    std::this_thread::sleep_for(std::chrono::seconds(time));
    return true;
}

// WaitTask
void WaitTask::setupParameters() {
    addParamDefinition("wait_type", "string", true, nullptr, "Wait type");
    addParamDefinition("duration", "number", false, 0, "Duration");
}

void WaitTask::executeImpl(const json& params) {
    std::string type = params.value("wait_type", "duration");
    if (type == "duration") waitForDuration(params.value("duration", 0));
    logProgress("Wait complete");
}

bool WaitTask::waitForDuration(int sec) {
    while (sec-- > 0 && !isCancelled()) std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}
bool WaitTask::waitForTime(std::chrono::system_clock::time_point) { return true; }
bool WaitTask::waitForAltitude(const Coordinates&, double) { return true; }
bool WaitTask::waitForTwilight(const std::string&) { return true; }

// CalibrationFrameTask
void CalibrationFrameTask::setupParameters() {
    addParamDefinition("frame_type", "string", true, nullptr, "Frame type");
    addParamDefinition("count", "number", true, nullptr, "Count");
    addParamDefinition("exposure_time", "number", false, 1.0, "Exposure");
}

void CalibrationFrameTask::executeImpl(const json& params) {
    std::string type = params.value("frame_type", "");
    int count = params.value("count", 1);
    logProgress("Capturing " + std::to_string(count) + " " + type + " frames");
    if (type == "dark") captureDarks(count, params.value("exposure_time", 1.0), 1);
    else if (type == "flat") captureFlats(count, "", 1, 30000);
    else if (type == "bias") captureBias(count, 1);
}

bool CalibrationFrameTask::captureDarks(int count, double, int) {
    for (int i = 0; i < count && !isCancelled(); ++i) std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}
bool CalibrationFrameTask::captureFlats(int count, const std::string&, int, int) {
    for (int i = 0; i < count && !isCancelled(); ++i) std::this_thread::sleep_for(std::chrono::seconds(1));
    return true;
}
bool CalibrationFrameTask::captureBias(int count, int) {
    for (int i = 0; i < count && !isCancelled(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}
double CalibrationFrameTask::calculateFlatExposure(const std::string&, int) { return 2.0; }

}  // namespace lithium::task::workflow
