#include "registration.hpp"
#include "custom/camera/basic_exposure.hpp"
#include "custom/camera/sequence_tasks.hpp"
#include "spdlog/spdlog.h"

namespace lithium::sequencer {

void registerBuiltInTasks() {
    auto& factory = TaskFactory::getInstance();

    spdlog::info("Registering built-in tasks with TaskFactory");

    // Register camera tasks
    TaskInfo takeExposureInfo{
        .name = "TakeExposure",
        .description = "Capture a single astronomical exposure",
        .category = "Camera",
        .requiredParameters = {"exposure", "type", "binning", "gain", "offset"},
        .parameterSchema =
            json{{"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"type",
                  {{"type", "string"},
                   {"enum", json::array({"light", "dark", "bias", "flat",
                                         "snapshot"})}}},
                 {"binning", {{"type", "integer"}, {"minimum", 1}}},
                 {"gain", {{"type", "integer"}, {"minimum", 0}}},
                 {"offset", {{"type", "integer"}, {"minimum", 0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<task::TakeExposureTask>(
        "TakeExposure", createTaskWrapper<task::TakeExposureTask>,
        takeExposureInfo);

    TaskInfo takeManyExposureInfo{
        .name = "TakeManyExposure",
        .description = "Capture multiple astronomical exposures in sequence",
        .category = "Camera",
        .requiredParameters = {"count", "exposure", "type", "binning", "gain",
                               "offset"},
        .parameterSchema =
            json{{"count", {{"type", "integer"}, {"minimum", 1}}},
                 {"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"type",
                  {{"type", "string"},
                   {"enum", json::array({"light", "dark", "bias", "flat",
                                         "snapshot"})}}},
                 {"binning", {{"type", "integer"}, {"minimum", 1}}},
                 {"gain", {{"type", "integer"}, {"minimum", 0}}},
                 {"offset", {{"type", "integer"}, {"minimum", 0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<task::TakeManyExposureTask>(
        "TakeManyExposure", createTaskWrapper<task::TakeManyExposureTask>,
        takeManyExposureInfo);

    TaskInfo subframeExposureInfo{
        .name = "SubframeExposure",
        .description = "Capture an exposure of a specific region of interest",
        .category = "Camera",
        .requiredParameters = {"exposure", "type", "binning", "gain", "offset",
                               "x", "y", "width", "height"},
        .parameterSchema =
            json{{"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"type",
                  {{"type", "string"},
                   {"enum", json::array({"light", "dark", "bias", "flat",
                                         "snapshot"})}}},
                 {"binning", {{"type", "integer"}, {"minimum", 1}}},
                 {"gain", {{"type", "integer"}, {"minimum", 0}}},
                 {"offset", {{"type", "integer"}, {"minimum", 0}}},
                 {"x", {{"type", "integer"}, {"minimum", 0}}},
                 {"y", {{"type", "integer"}, {"minimum", 0}}},
                 {"width", {{"type", "integer"}, {"minimum", 1}}},
                 {"height", {{"type", "integer"}, {"minimum", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<task::SubframeExposureTask>(
        "SubframeExposure", createTaskWrapper<task::SubframeExposureTask>,
        subframeExposureInfo);

    TaskInfo cameraSettingsInfo{
        .name = "CameraSettings",
        .description =
            "Configure camera settings like gain, offset, and binning",
        .category = "Camera",
        .requiredParameters = {},
        .parameterSchema =
            json{{"gain", {{"type", "integer"}, {"minimum", 0}}},
                 {"offset", {{"type", "integer"}, {"minimum", 0}}},
                 {"binning", {{"type", "integer"}, {"minimum", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<task::CameraSettingsTask>(
        "CameraSettings", createTaskWrapper<task::CameraSettingsTask>,
        cameraSettingsInfo);

    TaskInfo cameraPreviewInfo{
        .name = "CameraPreview",
        .description = "Take a quick preview snapshot",
        .category = "Camera",
        .requiredParameters = {"exposure"},
        .parameterSchema =
            json{
                {"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                {"binning",
                 {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                {"gain", {{"type", "integer"}, {"minimum", 0}, {"default", 0}}},
                {"offset",
                 {{"type", "integer"}, {"minimum", 0}, {"default", 0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<task::CameraPreviewTask>(
        "CameraPreview", createTaskWrapper<task::CameraPreviewTask>,
        cameraPreviewInfo);

    // Register sequence tasks
    TaskInfo smartExposureInfo{
        .name = "SmartExposure",
        .description = "Intelligent exposure optimization based on target SNR",
        .category = "Sequence",
        .requiredParameters = {},
        .parameterSchema = json{
            {"target_snr", {{"type", "number"}, {"minimum", 1.0}, {"default", 50.0}}},
            {"max_exposure", {{"type", "number"}, {"minimum", 0.1}, {"default", 300.0}}},
            {"min_exposure", {{"type", "number"}, {"minimum", 0.001}, {"default", 1.0}}},
            {"max_attempts", {{"type", "integer"}, {"minimum", 1}, {"default", 5}}},
            {"binning", {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
            {"gain", {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
            {"offset", {{"type", "integer"}, {"minimum", 0}, {"default", 10}}}
        },
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true
    };

    factory.registerTask<task::SmartExposureTask>(
        "SmartExposure", createTaskWrapper<task::SmartExposureTask>,
        smartExposureInfo);

    TaskInfo deepSkySequenceInfo{
        .name = "DeepSkySequence",
        .description = "Automated deep sky imaging sequence with multiple filters",
        .category = "Sequence",
        .requiredParameters = {"total_exposures", "exposure_time"},
        .parameterSchema = json{
            {"target_name", {{"type", "string"}, {"default", "Unknown"}}},
            {"total_exposures", {{"type", "integer"}, {"minimum", 1}}},
            {"exposure_time", {{"type", "number"}, {"minimum", 0.1}}},
            {"filters", {{"type", "array"}, {"items", {{"type", "string"}}}, {"default", json::array({"L"})}}},
            {"dithering", {{"type", "boolean"}, {"default", true}}},
            {"dither_pixels", {{"type", "integer"}, {"minimum", 1}, {"default", 10}}},
            {"dither_interval", {{"type", "number"}, {"minimum", 1.0}, {"default", 5.0}}},
            {"binning", {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
            {"gain", {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
            {"offset", {{"type", "integer"}, {"minimum", 0}, {"default", 10}}}
        },
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true
    };

    factory.registerTask<task::DeepSkySequenceTask>(
        "DeepSkySequence", createTaskWrapper<task::DeepSkySequenceTask>,
        deepSkySequenceInfo);

    TaskInfo planetaryImagingInfo{
        .name = "PlanetaryImaging",
        .description = "High-speed planetary imaging with lucky imaging technique",
        .category = "Sequence",
        .requiredParameters = {"video_length"},
        .parameterSchema = json{
            {"planet", {{"type", "string"}, {"default", "Mars"}}},
            {"video_length", {{"type", "integer"}, {"minimum", 1}}},
            {"frame_rate", {{"type", "number"}, {"minimum", 1.0}, {"default", 30.0}}},
            {"filters", {{"type", "array"}, {"items", {{"type", "string"}}}, {"default", json::array({"R", "G", "B"})}}},
            {"binning", {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
            {"gain", {{"type", "integer"}, {"minimum", 0}, {"default", 400}}},
            {"offset", {{"type", "integer"}, {"minimum", 0}, {"default", 10}}},
            {"high_speed", {{"type", "boolean"}, {"default", true}}}
        },
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true
    };

    factory.registerTask<task::PlanetaryImagingTask>(
        "PlanetaryImaging", createTaskWrapper<task::PlanetaryImagingTask>,
        planetaryImagingInfo);

    TaskInfo timelapseInfo{
        .name = "Timelapse",
        .description = "Time-lapse imaging with adjustable intervals",
        .category = "Sequence",
        .requiredParameters = {"total_frames", "interval"},
        .parameterSchema = json{
            {"total_frames", {{"type", "integer"}, {"minimum", 1}}},
            {"interval", {{"type", "number"}, {"minimum", 0.1}}},
            {"exposure_time", {{"type", "number"}, {"minimum", 0.001}, {"default", 10.0}}},
            {"type", {{"type", "string"}, {"enum", json::array({"sunset", "lunar", "star_trails"})}, {"default", "sunset"}}},
            {"binning", {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
            {"gain", {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
            {"offset", {{"type", "integer"}, {"minimum", 0}, {"default", 10}}},
            {"auto_exposure", {{"type", "boolean"}, {"default", false}}}
        },
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true
    };

    factory.registerTask<task::TimelapseTask>(
        "Timelapse", createTaskWrapper<task::TimelapseTask>,
        timelapseInfo);

    spdlog::info("Successfully registered {} built-in tasks", 10);
}

}  // namespace lithium::sequencer
