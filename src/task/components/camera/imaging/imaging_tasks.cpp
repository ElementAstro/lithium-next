/**
 * @file imaging_tasks.cpp
 * @brief Implementation of advanced imaging tasks
 */

#include "imaging_tasks.hpp"
#include <cmath>
#include <thread>
#include "../exposure/exposure_tasks.hpp"
#include "../guiding/guiding_tasks.hpp"

namespace lithium::task::camera {

// DeepSkySequenceTask
void DeepSkySequenceTask::setupParameters() {
    addParamDefinition("target_name", "string", false, "", "Target name");
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("count", "integer", true, nullptr, "Frame count");
    addParamDefinition("filter", "string", false, "L", "Filter");
    addParamDefinition("gain", "integer", false, 100, "Gain");
    addParamDefinition("dither", "boolean", false, true, "Enable dither");
    addParamDefinition("guiding", "boolean", false, true, "Enable guiding");
}

void DeepSkySequenceTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateRequired(params, "count");
}

void DeepSkySequenceTask::executeImpl(const json& params) {
    std::string target = params.value("target_name", "Unknown");
    int count = params["count"].get<int>();
    double exposure = params["exposure"].get<double>();

    logProgress("Deep sky sequence: " + target);

    json seqParams = params;
    seqParams["type"] = "light";

    TakeManyExposureTask seq;
    seq.execute(seqParams);

    logProgress("Deep sky sequence complete", 1.0);
}

// PlanetaryImagingTask
void PlanetaryImagingTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 0.01, "Exposure (ms)");
    addParamDefinition("frame_count", "integer", false, 5000, "Frames");
    addParamDefinition("gain", "integer", false, 300, "Gain");
    addParamDefinition("roi_size", "integer", false, 1024, "ROI size");
}

void PlanetaryImagingTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
}

void PlanetaryImagingTask::executeImpl(const json& params) {
    int frameCount = params.value("frame_count", 5000);
    logProgress("Planetary imaging: " + std::to_string(frameCount) + " frames");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    logProgress("Planetary imaging complete", 1.0);
}

// TimelapseTask
void TimelapseTask::setupParameters() {
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("interval", "number", true, nullptr,
                       "Interval between frames");
    addParamDefinition("count", "integer", true, nullptr, "Frame count");
    addParamDefinition("gain", "integer", false, 100, "Gain");
}

void TimelapseTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "exposure");
    validateRequired(params, "interval");
    validateRequired(params, "count");
}

void TimelapseTask::executeImpl(const json& params) {
    double exposure = params["exposure"].get<double>();
    double interval = params["interval"].get<double>();
    int count = params["count"].get<int>();

    logProgress("Timelapse: " + std::to_string(count) + " frames");

    for (int i = 0; i < count; ++i) {
        double progress = static_cast<double>(i) / count;
        logProgress("Frame " + std::to_string(i + 1), progress);

        json expParams = {{"exposure", exposure}, {"type", "light"}};
        TakeExposureTask exp;
        exp.execute(expParams);

        if (i < count - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(interval * 1000)));
        }
    }

    logProgress("Timelapse complete", 1.0);
}

// MosaicTask
void MosaicTask::setupParameters() {
    addParamDefinition("rows", "integer", true, nullptr, "Mosaic rows");
    addParamDefinition("cols", "integer", true, nullptr, "Mosaic columns");
    addParamDefinition("overlap", "number", false, 0.2, "Panel overlap");
    addParamDefinition("exposure", "number", true, nullptr, "Exposure time");
    addParamDefinition("count_per_panel", "integer", false, 10,
                       "Frames per panel");
}

void MosaicTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "rows");
    validateRequired(params, "cols");
    validateRequired(params, "exposure");
}

void MosaicTask::executeImpl(const json& params) {
    int rows = params["rows"].get<int>();
    int cols = params["cols"].get<int>();
    int totalPanels = rows * cols;

    logProgress("Mosaic: " + std::to_string(rows) + "x" + std::to_string(cols));

    for (int i = 0; i < totalPanels; ++i) {
        double progress = static_cast<double>(i) / totalPanels;
        logProgress("Panel " + std::to_string(i + 1) + "/" +
                        std::to_string(totalPanels),
                    progress);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    logProgress("Mosaic complete", 1.0);
}

void MosaicTask::calculatePanelPositions(int rows, int cols, double overlap,
                                         double fovWidth, double fovHeight) {
    // Panel position calculation placeholder
}

}  // namespace lithium::task::camera
