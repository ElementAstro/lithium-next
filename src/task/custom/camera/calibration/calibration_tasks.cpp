/**
 * @file calibration_tasks.cpp
 * @brief Implementation of calibration tasks
 */

#include "calibration_tasks.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include "../exposure/exposure_tasks.hpp"

namespace lithium::task::camera {

// ============================================================================
// AutoCalibrationTask Implementation
// ============================================================================

void AutoCalibrationTask::setupParameters() {
    addParamDefinition("dark_count", "integer", false, 20,
                       "Number of dark frames");
    addParamDefinition("bias_count", "integer", false, 50,
                       "Number of bias frames");
    addParamDefinition("flat_count", "integer", false, 20,
                       "Number of flat frames per filter");
    addParamDefinition("dark_exposures", "array", false,
                       json::array({60, 120, 300}), "Dark exposure times");
    addParamDefinition("filters", "array", false,
                       json::array({"L", "R", "G", "B"}), "Filters for flats");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("offset", "integer", false, 10, "Camera offset");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
}

void AutoCalibrationTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    int darkCount = params.value("dark_count", 20);
    int biasCount = params.value("bias_count", 50);
    int flatCount = params.value("flat_count", 20);

    validateCount(darkCount, 100);
    validateCount(biasCount, 200);
    validateCount(flatCount, 100);
}

void AutoCalibrationTask::executeImpl(const json& params) {
    logProgress("Starting automatic calibration sequence");

    // Acquire bias frames first
    acquireBiasFrames(params);

    // Acquire dark frames at different exposures
    acquireDarkFrames(params);

    // Acquire flat frames for each filter
    acquireFlatFrames(params);

    logProgress("Calibration sequence complete", 1.0);
}

void AutoCalibrationTask::acquireDarkFrames(const json& params) {
    int darkCount = params.value("dark_count", 20);
    auto exposures =
        params.value("dark_exposures", std::vector<double>{60, 120, 300});

    logProgress("Acquiring dark frames");

    for (double exposure : exposures) {
        logProgress("Taking " + std::to_string(darkCount) + " darks at " +
                    std::to_string(exposure) + "s");

        json darkParams = {
            {"exposure", exposure},
            {"count", darkCount},
            {"type", "dark"},
            {"gain", params.value("gain", 100)},
            {"offset", params.value("offset", 10)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

        TakeManyExposureTask darkTask;
        darkTask.execute(darkParams);
    }
}

void AutoCalibrationTask::acquireBiasFrames(const json& params) {
    int biasCount = params.value("bias_count", 50);

    logProgress("Acquiring " + std::to_string(biasCount) + " bias frames");

    json biasParams = {
        {"exposure", 0.0001},  // Minimum exposure for bias
        {"count", biasCount},
        {"type", "bias"},
        {"gain", params.value("gain", 100)},
        {"offset", params.value("offset", 10)},
        {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

    TakeManyExposureTask biasTask;
    biasTask.execute(biasParams);
}

void AutoCalibrationTask::acquireFlatFrames(const json& params) {
    int flatCount = params.value("flat_count", 20);
    auto filters =
        params.value("filters", std::vector<std::string>{"L", "R", "G", "B"});

    logProgress("Acquiring flat frames for " + std::to_string(filters.size()) +
                " filters");

    for (const auto& filter : filters) {
        logProgress("Taking " + std::to_string(flatCount) +
                    " flats with filter " + filter);

        json flatParams = params;
        flatParams["count"] = flatCount;
        flatParams["filter"] = filter;

        FlatFieldSequenceTask flatTask;
        flatTask.execute(flatParams);
    }
}

// ============================================================================
// ThermalCycleTask Implementation
// ============================================================================

void ThermalCycleTask::setupParameters() {
    addParamDefinition("start_temp", "number", true, nullptr,
                       "Starting temperature");
    addParamDefinition("end_temp", "number", true, nullptr,
                       "Ending temperature");
    addParamDefinition("temp_step", "number", false, 5.0, "Temperature step");
    addParamDefinition("dark_count", "integer", false, 10,
                       "Darks per temperature");
    addParamDefinition("exposure", "number", false, 60.0, "Exposure time");
    addParamDefinition("settle_time", "number", false, 300.0,
                       "Temperature settling time");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
}

void ThermalCycleTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "start_temp");
    validateRequired(params, "end_temp");

    double startTemp = params["start_temp"].get<double>();
    double endTemp = params["end_temp"].get<double>();

    validateTemperature(startTemp);
    validateTemperature(endTemp);
}

void ThermalCycleTask::executeImpl(const json& params) {
    double startTemp = params["start_temp"].get<double>();
    double endTemp = params["end_temp"].get<double>();
    double tempStep = params.value("temp_step", 5.0);
    int darkCount = params.value("dark_count", 10);
    double exposure = params.value("exposure", 60.0);
    double settleTime = params.value("settle_time", 300.0);

    logProgress("Starting thermal cycle from " + std::to_string(startTemp) +
                "°C to " + std::to_string(endTemp) + "°C");

    double currentTemp = startTemp;
    int steps = static_cast<int>(std::abs(endTemp - startTemp) / tempStep) + 1;
    int step = 0;

    while ((startTemp < endTemp && currentTemp <= endTemp) ||
           (startTemp > endTemp && currentTemp >= endTemp)) {
        double progress = static_cast<double>(step) / steps;
        logProgress(
            "Setting temperature to " + std::to_string(currentTemp) + "°C",
            progress);

        // Wait for temperature to settle (simulated)
        logProgress("Waiting for temperature to stabilize...");
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(settleTime * 10)));

        // Take dark frames at this temperature
        json darkParams = {{"exposure", exposure},
                           {"count", darkCount},
                           {"type", "dark"},
                           {"gain", params.value("gain", 100)}};

        TakeManyExposureTask darkTask;
        darkTask.execute(darkParams);

        currentTemp += (startTemp < endTemp) ? tempStep : -tempStep;
        step++;
    }

    logProgress("Thermal cycle complete", 1.0);
}

// ============================================================================
// FlatFieldSequenceTask Implementation
// ============================================================================

void FlatFieldSequenceTask::setupParameters() {
    addParamDefinition("count", "integer", false, 20, "Number of flat frames");
    addParamDefinition("target_adu", "integer", false, 30000,
                       "Target ADU level");
    addParamDefinition("tolerance", "number", false, 0.1,
                       "ADU tolerance fraction");
    addParamDefinition("min_exposure", "number", false, 0.1,
                       "Minimum exposure");
    addParamDefinition("max_exposure", "number", false, 30.0,
                       "Maximum exposure");
    addParamDefinition("filter", "string", false, "L", "Filter name");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}},
                       "Binning");
}

void FlatFieldSequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    int targetADU = params.value("target_adu", 30000);
    if (targetADU < 1000 || targetADU > 65000) {
        throw ValidationError("Target ADU must be between 1000 and 65000");
    }
}

void FlatFieldSequenceTask::executeImpl(const json& params) {
    int count = params.value("count", 20);
    int targetADU = params.value("target_adu", 30000);
    double tolerance = params.value("tolerance", 0.1);
    double minExposure = params.value("min_exposure", 0.1);
    double maxExposure = params.value("max_exposure", 30.0);
    std::string filter = params.value("filter", "L");

    logProgress("Starting flat field sequence for filter " + filter);
    logProgress("Target ADU: " + std::to_string(targetADU));

    // Find optimal exposure
    double currentExposure = (minExposure + maxExposure) / 2.0;
    int measuredADU = 0;

    for (int attempt = 0; attempt < 5; ++attempt) {
        logProgress("Test exposure: " + std::to_string(currentExposure) + "s");

        // Take test exposure
        json testParams = {
            {"exposure", currentExposure},
            {"type", "flat"},
            {"filter", filter},
            {"gain", params.value("gain", 100)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

        TakeExposureTask testExposure;
        testExposure.execute(testParams);

        // Simulate ADU measurement
        measuredADU = static_cast<int>(currentExposure * 10000 + 5000);

        logProgress("Measured ADU: " + std::to_string(measuredADU));

        if (std::abs(measuredADU - targetADU) <= targetADU * tolerance) {
            logProgress("Optimal exposure found: " +
                        std::to_string(currentExposure) + "s");
            break;
        }

        currentExposure =
            calculateFlatExposure(currentExposure, measuredADU, targetADU);
        currentExposure = std::clamp(currentExposure, minExposure, maxExposure);
    }

    // Take the flat sequence
    logProgress("Taking " + std::to_string(count) + " flat frames");

    json flatParams = {
        {"exposure", currentExposure},
        {"count", count},
        {"type", "flat"},
        {"filter", filter},
        {"gain", params.value("gain", 100)},
        {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}};

    TakeManyExposureTask flatTask;
    flatTask.execute(flatParams);

    logProgress("Flat field sequence complete", 1.0);
}

double FlatFieldSequenceTask::calculateFlatExposure(double currentExposure,
                                                    int measuredADU,
                                                    int targetADU) {
    // Linear scaling for flat exposure
    return currentExposure * static_cast<double>(targetADU) / measuredADU;
}

}  // namespace lithium::task::camera
