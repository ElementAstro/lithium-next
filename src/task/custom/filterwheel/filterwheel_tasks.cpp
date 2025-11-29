/**
 * @file filterwheel_tasks.cpp
 * @brief Implementation of filter wheel tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "filterwheel_tasks.hpp"
#include <algorithm>
#include <thread>

namespace lithium::task::filterwheel {

// ============================================================================
// ChangeFilterTask Implementation
// ============================================================================

void ChangeFilterTask::setupParameters() {
    addParamDefinition("filter", "string", false, nullptr, "Filter name");
    addParamDefinition("position", "integer", false, nullptr,
                       "Filter position (1-based)");
}

void ChangeFilterTask::executeImpl(const json& params) {
    std::string filterName;
    int position = -1;

    if (params.contains("filter")) {
        filterName = params["filter"].get<std::string>();
        logProgress("Changing to filter: " + filterName);
    } else if (params.contains("position")) {
        position = params["position"].get<int>();
        logProgress("Changing to filter position: " + std::to_string(position));
    } else {
        throw std::invalid_argument(
            "Either filter name or position must be specified");
    }

    // Simulate filter change
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    logProgress("Filter change complete", 1.0);
}

// ============================================================================
// FilterSequenceTask Implementation
// ============================================================================

void FilterSequenceTask::setupParameters() {
    addParamDefinition("filters", "array", true, nullptr,
                       "List of filter names");
    addParamDefinition("exposures_per_filter", "integer", false, 10,
                       "Exposures per filter");
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("dither", "boolean", false, false, "Enable dithering");
    addParamDefinition("dither_every", "integer", false, 1,
                       "Dither every N frames");
}

void FilterSequenceTask::executeImpl(const json& params) {
    auto result = ParamValidator::required(params, "filters");
    if (!result) {
        throw std::invalid_argument("Filters list is required");
    }

    auto filters = params["filters"].get<std::vector<std::string>>();
    if (filters.empty()) {
        throw std::invalid_argument("At least one filter must be specified");
    }

    int countPerFilter = params.value("exposures_per_filter", 10);
    double exposure = params.value("exposure", 60.0);
    bool dither = params.value("dither", false);

    int totalFrames = static_cast<int>(filters.size()) * countPerFilter;
    int frameCount = 0;

    logProgress("Starting filter sequence with " +
                std::to_string(filters.size()) + " filters");

    for (const auto& filter : filters) {
        if (!shouldContinue()) {
            logProgress("Filter sequence cancelled");
            return;
        }

        logProgress("Switching to filter: " + filter);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));  // Filter change time

        for (int i = 0; i < countPerFilter; ++i) {
            if (!shouldContinue()) {
                logProgress("Filter sequence cancelled");
                return;
            }

            double progress = static_cast<double>(frameCount) / totalFrames;
            logProgress("Filter " + filter + " frame " + std::to_string(i + 1) +
                            "/" + std::to_string(countPerFilter),
                        progress);

            // Simulate exposure
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(exposure * 100)));

            if (dither && (i + 1) % params.value("dither_every", 1) == 0) {
                logProgress("Dithering...");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            frameCount++;
        }
    }

    logProgress(
        "Filter sequence complete: " + std::to_string(totalFrames) + " frames",
        1.0);
}

// ============================================================================
// RGBSequenceTask Implementation
// ============================================================================

void RGBSequenceTask::setupParameters() {
    addParamDefinition("r_exposure", "number", true, nullptr,
                       "Red filter exposure");
    addParamDefinition("g_exposure", "number", true, nullptr,
                       "Green filter exposure");
    addParamDefinition("b_exposure", "number", true, nullptr,
                       "Blue filter exposure");
    addParamDefinition("r_count", "integer", false, 10, "Red frame count");
    addParamDefinition("g_count", "integer", false, 10, "Green frame count");
    addParamDefinition("b_count", "integer", false, 10, "Blue frame count");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("interleave", "boolean", false, false,
                       "Interleave RGB frames");
}

void RGBSequenceTask::executeImpl(const json& params) {
    auto rResult = ParamValidator::required(params, "r_exposure");
    auto gResult = ParamValidator::required(params, "g_exposure");
    auto bResult = ParamValidator::required(params, "b_exposure");

    if (!rResult || !gResult || !bResult) {
        throw std::invalid_argument("All RGB exposures are required");
    }

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
            if (!shouldContinue()) {
                logProgress("RGB sequence cancelled");
                return;
            }

            double progress = static_cast<double>(i) / maxCount;

            if (i < rCount) {
                logProgress("R frame " + std::to_string(i + 1), progress);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(rExp * 100)));
            }
            if (i < gCount) {
                logProgress("G frame " + std::to_string(i + 1), progress);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(gExp * 100)));
            }
            if (i < bCount) {
                logProgress("B frame " + std::to_string(i + 1), progress);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(static_cast<int>(bExp * 100)));
            }
        }
    } else {
        // Sequential RGB
        int totalFrames = rCount + gCount + bCount;
        int framesDone = 0;

        // Red
        logProgress("Acquiring Red frames");
        for (int i = 0; i < rCount && shouldContinue(); ++i) {
            double progress = static_cast<double>(framesDone) / totalFrames;
            logProgress("R frame " + std::to_string(i + 1) + "/" +
                            std::to_string(rCount),
                        progress);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(rExp * 100)));
            framesDone++;
        }

        // Green
        logProgress("Acquiring Green frames");
        for (int i = 0; i < gCount && shouldContinue(); ++i) {
            double progress = static_cast<double>(framesDone) / totalFrames;
            logProgress("G frame " + std::to_string(i + 1) + "/" +
                            std::to_string(gCount),
                        progress);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(gExp * 100)));
            framesDone++;
        }

        // Blue
        logProgress("Acquiring Blue frames");
        for (int i = 0; i < bCount && shouldContinue(); ++i) {
            double progress = static_cast<double>(framesDone) / totalFrames;
            logProgress("B frame " + std::to_string(i + 1) + "/" +
                            std::to_string(bCount),
                        progress);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(bExp * 100)));
            framesDone++;
        }
    }

    logProgress("RGB sequence complete", 1.0);
}

// ============================================================================
// NarrowbandSequenceTask Implementation
// ============================================================================

void NarrowbandSequenceTask::setupParameters() {
    addParamDefinition("ha_exposure", "number", false, 300.0,
                       "Ha filter exposure");
    addParamDefinition("oiii_exposure", "number", false, 300.0,
                       "OIII filter exposure");
    addParamDefinition("sii_exposure", "number", false, 300.0,
                       "SII filter exposure");
    addParamDefinition("ha_count", "integer", false, 20, "Ha frame count");
    addParamDefinition("oiii_count", "integer", false, 20, "OIII frame count");
    addParamDefinition("sii_count", "integer", false, 20, "SII frame count");
    addParamDefinition("palette", "string", false, "sho",
                       "Color palette (sho/hoo/hos)");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("dither", "boolean", false, true, "Enable dithering");
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
        sequence = {{"SII", siiExp, siiCount},
                    {"Ha", haExp, haCount},
                    {"OIII", oiiiExp, oiiiCount}};
    } else if (palette == "hoo") {
        sequence = {{"Ha", haExp, haCount}, {"OIII", oiiiExp, oiiiCount}};
    } else if (palette == "hos") {
        sequence = {{"Ha", haExp, haCount},
                    {"OIII", oiiiExp, oiiiCount},
                    {"SII", siiExp, siiCount}};
    } else {
        // Default to SHO
        sequence = {{"SII", siiExp, siiCount},
                    {"Ha", haExp, haCount},
                    {"OIII", oiiiExp, oiiiCount}};
    }

    int totalFrames = 0;
    for (const auto& [filter, exp, count] : sequence) {
        totalFrames += count;
    }

    int framesDone = 0;
    for (const auto& [filter, exposure, count] : sequence) {
        if (!shouldContinue()) {
            logProgress("Narrowband sequence cancelled");
            return;
        }

        logProgress("Switching to " + filter + " filter");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        logProgress("Acquiring " + std::to_string(count) + " " + filter +
                    " frames");

        for (int i = 0; i < count && shouldContinue(); ++i) {
            double progress = static_cast<double>(framesDone) / totalFrames;
            logProgress(filter + " frame " + std::to_string(i + 1) + "/" +
                            std::to_string(count),
                        progress);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(exposure * 10)));
            framesDone++;
        }

        logProgress("Completed " + filter,
                    static_cast<double>(framesDone) / totalFrames);
    }

    logProgress("Narrowband sequence complete: " + std::to_string(totalFrames) +
                    " frames",
                1.0);
}

// ============================================================================
// LRGBSequenceTask Implementation
// ============================================================================

void LRGBSequenceTask::setupParameters() {
    addParamDefinition("l_exposure", "number", false, 120.0,
                       "Luminance exposure");
    addParamDefinition("r_exposure", "number", false, 60.0, "Red exposure");
    addParamDefinition("g_exposure", "number", false, 60.0, "Green exposure");
    addParamDefinition("b_exposure", "number", false, 60.0, "Blue exposure");
    addParamDefinition("l_count", "integer", false, 30,
                       "Luminance frame count");
    addParamDefinition("r_count", "integer", false, 10, "Red frame count");
    addParamDefinition("g_count", "integer", false, 10, "Green frame count");
    addParamDefinition("b_count", "integer", false, 10, "Blue frame count");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("l_priority", "boolean", false, true,
                       "Prioritize luminance acquisition");
}

void LRGBSequenceTask::executeImpl(const json& params) {
    double lExp = params.value("l_exposure", 120.0);
    double rExp = params.value("r_exposure", 60.0);
    double gExp = params.value("g_exposure", 60.0);
    double bExp = params.value("b_exposure", 60.0);
    int lCount = params.value("l_count", 30);
    int rCount = params.value("r_count", 10);
    int gCount = params.value("g_count", 10);
    int bCount = params.value("b_count", 10);
    bool lPriority = params.value("l_priority", true);

    int totalFrames = lCount + rCount + gCount + bCount;
    int framesDone = 0;

    logProgress("Starting LRGB sequence");

    auto acquireFrames = [&](const std::string& filter, double exposure,
                             int count) {
        logProgress("Switching to " + filter + " filter");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        for (int i = 0; i < count && shouldContinue(); ++i) {
            double progress = static_cast<double>(framesDone) / totalFrames;
            logProgress(filter + " frame " + std::to_string(i + 1) + "/" +
                            std::to_string(count),
                        progress);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(exposure * 10)));
            framesDone++;
        }
    };

    if (lPriority) {
        // Acquire L first, then RGB
        acquireFrames("L", lExp, lCount);
        if (shouldContinue())
            acquireFrames("R", rExp, rCount);
        if (shouldContinue())
            acquireFrames("G", gExp, gCount);
        if (shouldContinue())
            acquireFrames("B", bExp, bCount);
    } else {
        // Interleave L with RGB
        acquireFrames("L", lExp, lCount / 2);
        if (shouldContinue())
            acquireFrames("R", rExp, rCount);
        if (shouldContinue())
            acquireFrames("G", gExp, gCount);
        if (shouldContinue())
            acquireFrames("B", bExp, bCount);
        if (shouldContinue())
            acquireFrames("L", lExp, lCount - lCount / 2);
    }

    logProgress(
        "LRGB sequence complete: " + std::to_string(framesDone) + " frames",
        1.0);
}

}  // namespace lithium::task::filterwheel
