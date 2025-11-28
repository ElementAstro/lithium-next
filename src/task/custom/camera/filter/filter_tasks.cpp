/**
 * @file filter_tasks.cpp
 * @brief Implementation of filter sequence tasks
 */

#include "filter_tasks.hpp"
#include "../exposure/exposure_tasks.hpp"
#include <thread>

namespace lithium::task::camera {

// ============================================================================
// FilterSequenceTask Implementation
// ============================================================================

void FilterSequenceTask::setupParameters() {
    addParamDefinition("filters", "array", true, nullptr, "List of filter names");
    addParamDefinition("exposures_per_filter", "integer", false, 10, "Exposures per filter");
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}}, "Binning");
    addParamDefinition("dither", "boolean", false, false, "Enable dithering");
    addParamDefinition("dither_every", "integer", false, 1, "Dither every N frames");
}

void FilterSequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "filters");
    validateRequired(params, "exposure");
    validateType(params, "filters", "array");

    if (params["filters"].empty()) {
        throw ValidationError("At least one filter must be specified");
    }

    double exposure = params["exposure"].get<double>();
    validateExposure(exposure);
}

void FilterSequenceTask::executeImpl(const json& params) {
    auto filters = params["filters"].get<std::vector<std::string>>();
    int countPerFilter = params.value("exposures_per_filter", 10);
    double exposure = params["exposure"].get<double>();
    bool dither = params.value("dither", false);

    int totalFrames = filters.size() * countPerFilter;
    int frameCount = 0;

    logProgress("Starting filter sequence with " + std::to_string(filters.size()) + " filters");

    for (const auto& filter : filters) {
        logProgress("Switching to filter: " + filter);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Filter change time

        for (int i = 0; i < countPerFilter; ++i) {
            double progress = static_cast<double>(frameCount) / totalFrames;
            logProgress("Filter " + filter + " frame " + std::to_string(i + 1) + "/" +
                       std::to_string(countPerFilter), progress);

            json exposureParams = {
                {"exposure", exposure},
                {"type", "light"},
                {"filter", filter},
                {"gain", params.value("gain", 100)},
                {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}
            };

            TakeExposureTask frameExposure;
            frameExposure.execute(exposureParams);

            if (dither && (i + 1) % params.value("dither_every", 1) == 0) {
                logProgress("Dithering...");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            frameCount++;
        }
    }

    logProgress("Filter sequence complete: " + std::to_string(totalFrames) + " frames", 1.0);
}

// ============================================================================
// RGBSequenceTask Implementation
// ============================================================================

void RGBSequenceTask::setupParameters() {
    addParamDefinition("r_exposure", "number", true, nullptr, "Red filter exposure");
    addParamDefinition("g_exposure", "number", true, nullptr, "Green filter exposure");
    addParamDefinition("b_exposure", "number", true, nullptr, "Blue filter exposure");
    addParamDefinition("r_count", "integer", false, 10, "Red frame count");
    addParamDefinition("g_count", "integer", false, 10, "Green frame count");
    addParamDefinition("b_count", "integer", false, 10, "Blue frame count");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}}, "Binning");
    addParamDefinition("interleave", "boolean", false, false, "Interleave RGB frames");
}

void RGBSequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "r_exposure");
    validateRequired(params, "g_exposure");
    validateRequired(params, "b_exposure");

    validateExposure(params["r_exposure"].get<double>());
    validateExposure(params["g_exposure"].get<double>());
    validateExposure(params["b_exposure"].get<double>());
}

void RGBSequenceTask::executeImpl(const json& params) {
    double rExp = params["r_exposure"].get<double>();
    double gExp = params["g_exposure"].get<double>();
    double bExp = params["b_exposure"].get<double>();
    int rCount = params.value("r_count", 10);
    int gCount = params.value("g_count", 10);
    int bCount = params.value("b_count", 10);
    bool interleave = params.value("interleave", false);

    logProgress("Starting RGB sequence");

    if (interleave) {
        int maxCount = std::max({rCount, gCount, bCount});
        for (int i = 0; i < maxCount; ++i) {
            double progress = static_cast<double>(i) / maxCount;
            
            if (i < rCount) {
                logProgress("R frame " + std::to_string(i + 1), progress);
                json rParams = {{"exposure", rExp}, {"type", "light"}, {"filter", "R"},
                               {"gain", params.value("gain", 100)}};
                TakeExposureTask rExposure;
                rExposure.execute(rParams);
            }
            if (i < gCount) {
                json gParams = {{"exposure", gExp}, {"type", "light"}, {"filter", "G"},
                               {"gain", params.value("gain", 100)}};
                TakeExposureTask gExposure;
                gExposure.execute(gParams);
            }
            if (i < bCount) {
                json bParams = {{"exposure", bExp}, {"type", "light"}, {"filter", "B"},
                               {"gain", params.value("gain", 100)}};
                TakeExposureTask bExposure;
                bExposure.execute(bParams);
            }
        }
    } else {
        // Sequential RGB
        json filterParams = {
            {"filters", json::array({"R", "G", "B"})},
            {"exposures_per_filter", rCount},
            {"exposure", rExp},
            {"gain", params.value("gain", 100)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}
        };

        FilterSequenceTask seqTask;
        seqTask.execute(filterParams);
    }

    logProgress("RGB sequence complete", 1.0);
}

// ============================================================================
// NarrowbandSequenceTask Implementation
// ============================================================================

void NarrowbandSequenceTask::setupParameters() {
    addParamDefinition("ha_exposure", "number", false, 300.0, "Ha filter exposure");
    addParamDefinition("oiii_exposure", "number", false, 300.0, "OIII filter exposure");
    addParamDefinition("sii_exposure", "number", false, 300.0, "SII filter exposure");
    addParamDefinition("ha_count", "integer", false, 20, "Ha frame count");
    addParamDefinition("oiii_count", "integer", false, 20, "OIII frame count");
    addParamDefinition("sii_count", "integer", false, 20, "SII frame count");
    addParamDefinition("palette", "string", false, "sho", "Color palette (sho/hoo/hos)");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning", "object", false, json{{"x", 1}, {"y", 1}}, "Binning");
    addParamDefinition("dither", "boolean", false, true, "Enable dithering");
}

void NarrowbandSequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);

    if (params.contains("ha_exposure"))
        validateExposure(params["ha_exposure"].get<double>());
    if (params.contains("oiii_exposure"))
        validateExposure(params["oiii_exposure"].get<double>());
    if (params.contains("sii_exposure"))
        validateExposure(params["sii_exposure"].get<double>());
}

void NarrowbandSequenceTask::executeImpl(const json& params) {
    double haExp = params.value("ha_exposure", 300.0);
    double oiiiExp = params.value("oiii_exposure", 300.0);
    double siiExp = params.value("sii_exposure", 300.0);
    int haCount = params.value("ha_count", 20);
    int oiiiCount = params.value("oiii_count", 20);
    int siiCount = params.value("sii_count", 20);
    std::string palette = params.value("palette", "sho");

    logProgress("Starting narrowband sequence with " + palette + " palette");

    // Build filter sequence based on palette
    std::vector<std::tuple<std::string, double, int>> sequence;

    if (palette == "sho" || palette == "hubble") {
        sequence = {{"SII", siiExp, siiCount}, {"Ha", haExp, haCount}, {"OIII", oiiiExp, oiiiCount}};
    } else if (palette == "hoo") {
        sequence = {{"Ha", haExp, haCount}, {"OIII", oiiiExp, oiiiCount}};
    } else if (palette == "hos") {
        sequence = {{"Ha", haExp, haCount}, {"OIII", oiiiExp, oiiiCount}, {"SII", siiExp, siiCount}};
    }

    int totalFrames = 0;
    for (const auto& [filter, exp, count] : sequence) {
        totalFrames += count;
    }

    int framesDone = 0;
    for (const auto& [filter, exposure, count] : sequence) {
        logProgress("Acquiring " + std::to_string(count) + " " + filter + " frames");

        json filterParams = {
            {"exposure", exposure},
            {"count", count},
            {"type", "light"},
            {"filter", filter},
            {"gain", params.value("gain", 100)},
            {"binning", params.value("binning", json{{"x", 1}, {"y", 1}})}
        };

        TakeManyExposureTask manyExposure;
        manyExposure.execute(filterParams);

        framesDone += count;
        logProgress("Completed " + filter, static_cast<double>(framesDone) / totalFrames);
    }

    logProgress("Narrowband sequence complete: " + std::to_string(totalFrames) + " frames", 1.0);
}

}  // namespace lithium::task::camera
