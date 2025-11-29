#include "registration.hpp"
#include "custom/camera/camera_tasks.hpp"
#include "custom/task_wrappers.hpp"
#include "custom/workflow/workflow_tasks.hpp"
#include "spdlog/spdlog.h"

// Use the workflow namespace
namespace workflow = lithium::task::workflow;

// Use the new camera namespace
namespace camera = lithium::task::camera;

namespace lithium::task {

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

    factory.registerTask<camera::TakeExposureTask>(
        "TakeExposure", createTaskWrapper<camera::TakeExposureTask>,
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

    factory.registerTask<camera::TakeManyExposureTask>(
        "TakeManyExposure", createTaskWrapper<camera::TakeManyExposureTask>,
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

    factory.registerTask<camera::SubframeExposureTask>(
        "SubframeExposure", createTaskWrapper<camera::SubframeExposureTask>,
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

    factory.registerTask<camera::CameraSettingsTask>(
        "CameraSettings", createTaskWrapper<camera::CameraSettingsTask>,
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

    factory.registerTask<camera::CameraPreviewTask>(
        "CameraPreview", createTaskWrapper<camera::CameraPreviewTask>,
        cameraPreviewInfo);

    // Register sequence tasks
    TaskInfo smartExposureInfo{
        .name = "SmartExposure",
        .description = "Intelligent exposure optimization based on target SNR",
        .category = "Sequence",
        .requiredParameters = {},
        .parameterSchema =
            json{{"target_snr",
                  {{"type", "number"}, {"minimum", 1.0}, {"default", 50.0}}},
                 {"max_exposure",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 300.0}}},
                 {"min_exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 1.0}}},
                 {"max_attempts",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 5}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
                 {"offset",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 10}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::SmartExposureTask>(
        "SmartExposure", createTaskWrapper<camera::SmartExposureTask>,
        smartExposureInfo);

    TaskInfo deepSkySequenceInfo{
        .name = "DeepSkySequence",
        .description =
            "Automated deep sky imaging sequence with multiple filters",
        .category = "Sequence",
        .requiredParameters = {"total_exposures", "exposure_time"},
        .parameterSchema =
            json{{"target_name", {{"type", "string"}, {"default", "Unknown"}}},
                 {"total_exposures", {{"type", "integer"}, {"minimum", 1}}},
                 {"exposure_time", {{"type", "number"}, {"minimum", 0.1}}},
                 {"filters",
                  {{"type", "array"},
                   {"items", {{"type", "string"}}},
                   {"default", json::array({"L"})}}},
                 {"dithering", {{"type", "boolean"}, {"default", true}}},
                 {"dither_pixels",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 10}}},
                 {"dither_interval",
                  {{"type", "number"}, {"minimum", 1.0}, {"default", 5.0}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
                 {"offset",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 10}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::DeepSkySequenceTask>(
        "DeepSkySequence", createTaskWrapper<camera::DeepSkySequenceTask>,
        deepSkySequenceInfo);

    TaskInfo planetaryImagingInfo{
        .name = "PlanetaryImaging",
        .description =
            "High-speed planetary imaging with lucky imaging technique",
        .category = "Sequence",
        .requiredParameters = {"video_length"},
        .parameterSchema =
            json{{"planet", {{"type", "string"}, {"default", "Mars"}}},
                 {"video_length", {{"type", "integer"}, {"minimum", 1}}},
                 {"frame_rate",
                  {{"type", "number"}, {"minimum", 1.0}, {"default", 30.0}}},
                 {"filters",
                  {{"type", "array"},
                   {"items", {{"type", "string"}}},
                   {"default", json::array({"R", "G", "B"})}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 400}}},
                 {"offset",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 10}}},
                 {"high_speed", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::PlanetaryImagingTask>(
        "PlanetaryImaging", createTaskWrapper<camera::PlanetaryImagingTask>,
        planetaryImagingInfo);

    TaskInfo timelapseInfo{
        .name = "Timelapse",
        .description = "Time-lapse imaging with adjustable intervals",
        .category = "Sequence",
        .requiredParameters = {"total_frames", "interval"},
        .parameterSchema =
            json{{"total_frames", {{"type", "integer"}, {"minimum", 1}}},
                 {"interval", {{"type", "number"}, {"minimum", 0.1}}},
                 {"exposure_time",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 10.0}}},
                 {"type",
                  {{"type", "string"},
                   {"enum", json::array({"sunset", "lunar", "star_trails"})},
                   {"default", "sunset"}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 100}}},
                 {"offset",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 10}}},
                 {"auto_exposure", {{"type", "boolean"}, {"default", false}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::TimelapseTask>(
        "Timelapse", createTaskWrapper<camera::TimelapseTask>, timelapseInfo);

    // Register calibration tasks
    TaskInfo autoCalibrationInfo{
        .name = "AutoCalibration",
        .description =
            "Automatically acquire calibration frames (dark, bias, flat)",
        .category = "Calibration",
        .requiredParameters = {},
        .parameterSchema =
            json{{"dark_count",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 10}}},
                 {"bias_count",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 20}}},
                 {"flat_count",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 10}}},
                 {"dark_exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"flat_exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 100}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::AutoCalibrationTask>(
        "AutoCalibration", createTaskWrapper<camera::AutoCalibrationTask>,
        autoCalibrationInfo);

    TaskInfo thermalCycleInfo{
        .name = "ThermalCycle",
        .description =
            "Acquire dark frames across temperature range for thermal "
            "calibration",
        .category = "Calibration",
        .requiredParameters = {"exposure", "count"},
        .parameterSchema =
            json{{"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"count", {{"type", "integer"}, {"minimum", 1}}},
                 {"start_temp", {{"type", "number"}, {"default", -10.0}}},
                 {"end_temp", {{"type", "number"}, {"default", 20.0}}},
                 {"temp_step",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 5.0}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::ThermalCycleTask>(
        "ThermalCycle", createTaskWrapper<camera::ThermalCycleTask>,
        thermalCycleInfo);

    TaskInfo flatFieldSequenceInfo{
        .name = "FlatFieldSequence",
        .description =
            "Automated flat field frame acquisition with exposure control",
        .category = "Calibration",
        .requiredParameters = {},
        .parameterSchema =
            json{{"filter", {{"type", "string"}, {"default", "L"}}},
                 {"count",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 10}}},
                 {"target_adu",
                  {{"type", "integer"}, {"minimum", 1000}, {"default", 30000}}},
                 {"tolerance",
                  {{"type", "number"}, {"minimum", 0.01}, {"default", 0.1}}},
                 {"max_exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 30.0}}},
                 {"min_exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 0.1}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::FlatFieldSequenceTask>(
        "FlatFieldSequence", createTaskWrapper<camera::FlatFieldSequenceTask>,
        flatFieldSequenceInfo);

    // Register focus tasks
    TaskInfo autoFocusInfo{
        .name = "AutoFocus",
        .description = "Perform automatic focusing using star analysis",
        .category = "Focus",
        .requiredParameters = {},
        .parameterSchema =
            json{{"exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 3.0}}},
                 {"step_size",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 100}}},
                 {"max_steps",
                  {{"type", "integer"}, {"minimum", 3}, {"default", 15}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"method",
                  {{"type", "string"},
                   {"enum", json::array({"hfd", "fwhm", "contrast"})},
                   {"default", "hfd"}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::AutoFocusTask>(
        "AutoFocus", createTaskWrapper<camera::AutoFocusTask>, autoFocusInfo);

    TaskInfo focusSeriesInfo{
        .name = "FocusSeries",
        .description = "Perform focus test series for manual focus adjustment",
        .category = "Focus",
        .requiredParameters = {"start_position", "end_position", "step_size"},
        .parameterSchema =
            json{{"start_position", {{"type", "integer"}, {"minimum", 0}}},
                 {"end_position", {{"type", "integer"}, {"minimum", 0}}},
                 {"step_size", {{"type", "integer"}, {"minimum", 1}}},
                 {"exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 3.0}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::FocusSeriesTask>(
        "FocusSeries", createTaskWrapper<camera::FocusSeriesTask>,
        focusSeriesInfo);

    TaskInfo temperatureFocusInfo{
        .name = "TemperatureFocus",
        .description = "Temperature-compensated focus adjustment",
        .category = "Focus",
        .requiredParameters = {},
        .parameterSchema =
            json{{"coefficient", {{"type", "number"}, {"default", -1.5}}},
                 {"reference_temp", {{"type", "number"}, {"default", 20.0}}},
                 {"current_temp", {{"type", "number"}}},
                 {"max_adjustment",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 500}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::TemperatureFocusTask>(
        "TemperatureFocus", createTaskWrapper<camera::TemperatureFocusTask>,
        temperatureFocusInfo);

    // Register filter tasks

    TaskInfo filterSequenceInfo{
        .name = "FilterSequence",
        .description = "Execute imaging sequence with multiple filters",
        .category = "Filter",
        .requiredParameters = {"filters", "exposures_per_filter"},
        .parameterSchema =
            json{
                {"filters",
                 {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"exposures_per_filter", {{"type", "integer"}, {"minimum", 1}}},
                {"exposure_time", {{"type", "number"}, {"minimum", 0.001}}},
                {"binning",
                 {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                {"gain",
                 {{"type", "integer"}, {"minimum", 0}, {"default", 100}}}},
        .version = "1.0.0",
        .dependencies = {"FilterChange"},
        .isEnabled = true};

    factory.registerTask<camera::FilterSequenceTask>(
        "FilterSequence", createTaskWrapper<camera::FilterSequenceTask>,
        filterSequenceInfo);

    TaskInfo rgbSequenceInfo{
        .name = "RGBSequence",
        .description = "RGB color imaging sequence",
        .category = "Filter",
        .requiredParameters = {"exposures_per_filter"},
        .parameterSchema =
            json{
                {"exposures_per_filter", {{"type", "integer"}, {"minimum", 1}}},
                {"exposure_time", {{"type", "number"}, {"minimum", 0.001}}},
                {"binning",
                 {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                {"gain",
                 {{"type", "integer"}, {"minimum", 0}, {"default", 100}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::RGBSequenceTask>(
        "RGBSequence", createTaskWrapper<camera::RGBSequenceTask>,
        rgbSequenceInfo);

    TaskInfo narrowbandSequenceInfo{
        .name = "NarrowbandSequence",
        .description = "Narrowband filter imaging sequence (Ha, OIII, SII)",
        .category = "Filter",
        .requiredParameters = {"filters", "exposures_per_filter"},
        .parameterSchema =
            json{
                {"filters",
                 {{"type", "array"}, {"items", {{"type", "string"}}}}},
                {"exposures_per_filter", {{"type", "integer"}, {"minimum", 1}}},
                {"exposure_time", {{"type", "number"}, {"minimum", 0.001}}},
                {"binning",
                 {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                {"gain",
                 {{"type", "integer"}, {"minimum", 0}, {"default", 100}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::NarrowbandSequenceTask>(
        "NarrowbandSequence", createTaskWrapper<camera::NarrowbandSequenceTask>,
        narrowbandSequenceInfo);

    // Register guide tasks
    TaskInfo autoGuidingInfo{
        .name = "AutoGuiding",
        .description = "Setup and calibrate autoguiding system",
        .category = "Guide",
        .requiredParameters = {},
        .parameterSchema =
            json{{"settle_time",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 10.0}}},
                 {"settle_pixels",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 1.5}}},
                 {"settle_timeout",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 30.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::AutoGuidingTask>(
        "AutoGuiding", createTaskWrapper<camera::AutoGuidingTask>,
        autoGuidingInfo);

    TaskInfo guidedExposureInfo{
        .name = "GuidedExposure",
        .description = "Perform guided exposure with autoguiding integration",
        .category = "Guide",
        .requiredParameters = {"exposure"},
        .parameterSchema =
            json{{"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}},
                 {"gain",
                  {{"type", "integer"}, {"minimum", 0}, {"default", 100}}}},
        .version = "1.0.0",
        .dependencies = {"AutoGuiding"},
        .isEnabled = true};

    factory.registerTask<camera::GuidedExposureTask>(
        "GuidedExposure", createTaskWrapper<camera::GuidedExposureTask>,
        guidedExposureInfo);

    TaskInfo ditherSequenceInfo{
        .name = "DitherSequence",
        .description = "Perform dithering sequence for improved image quality",
        .category = "Guide",
        .requiredParameters = {},
        .parameterSchema =
            json{{"pixels",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 5.0}}},
                 {"settle_time",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 10.0}}},
                 {"settle_pixels",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 1.5}}},
                 {"dither_count",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 5}}}},
        .version = "1.0.0",
        .dependencies = {"AutoGuiding"},
        .isEnabled = true};

    factory.registerTask<camera::DitherSequenceTask>(
        "DitherSequence", createTaskWrapper<camera::DitherSequenceTask>,
        ditherSequenceInfo);

    // Register safety tasks
    TaskInfo weatherMonitorInfo{
        .name = "WeatherMonitor",
        .description = "Monitor weather conditions and perform safety imaging",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{{"cloud_threshold",
                  {{"type", "number"},
                   {"minimum", 0.0},
                   {"maximum", 100.0},
                   {"default", 30.0}}},
                 {"wind_threshold",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 35.0}}},
                 {"rain_threshold",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 0.1}}},
                 {"check_interval",
                  {{"type", "number"}, {"minimum", 1.0}, {"default", 60.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::WeatherMonitorTask>(
        "WeatherMonitor", createTaskWrapper<camera::WeatherMonitorTask>,
        weatherMonitorInfo);

    TaskInfo cloudDetectionInfo{
        .name = "CloudDetection",
        .description = "Perform cloud detection using all-sky camera",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{{"exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 5.0}}},
                 {"threshold",
                  {{"type", "number"},
                   {"minimum", 0.0},
                   {"maximum", 100.0},
                   {"default", 30.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::CloudDetectionTask>(
        "CloudDetection", createTaskWrapper<camera::CloudDetectionTask>,
        cloudDetectionInfo);

    TaskInfo safetyShutdownInfo{
        .name = "SafetyShutdown",
        .description = "Perform safe shutdown of imaging equipment",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{{"park_mount", {{"type", "boolean"}, {"default", true}}},
                 {"warm_camera", {{"type", "boolean"}, {"default", true}}},
                 {"close_roof", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::SafetyShutdownTask>(
        "SafetyShutdown", createTaskWrapper<camera::SafetyShutdownTask>,
        safetyShutdownInfo);

    // Register platesolve tasks
    TaskInfo plateSolveExposureInfo{
        .name = "PlateSolveExposure",
        .description = "Take exposure and perform plate solving for astrometry",
        .category = "Platesolve",
        .requiredParameters = {},
        .parameterSchema =
            json{{"exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 3.0}}},
                 {"timeout",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 60}}},
                 {"search_radius",
                  {{"type", "number"}, {"minimum", 0.1}, {"default", 5.0}}},
                 {"binning",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<camera::PlateSolveExposureTask>(
        "PlateSolveExposure", createTaskWrapper<camera::PlateSolveExposureTask>,
        plateSolveExposureInfo);

    TaskInfo centeringInfo{
        .name = "Centering",
        .description =
            "Center target object in field of view using plate solving",
        .category = "Platesolve",
        .requiredParameters = {},
        .parameterSchema =
            json{{"exposure",
                  {{"type", "number"}, {"minimum", 0.001}, {"default", 3.0}}},
                 {"max_iterations",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 5}}},
                 {"tolerance",
                  {{"type", "number"}, {"minimum", 0.0}, {"default", 60.0}}}},
        .version = "1.0.0",
        .dependencies = {"PlateSolveExposure"},
        .isEnabled = true};

    factory.registerTask<camera::CenteringTask>(
        "Centering", createTaskWrapper<camera::CenteringTask>, centeringInfo);

    TaskInfo mosaicInfo{
        .name = "Mosaic",
        .description =
            "Automated mosaic imaging with plate solving and positioning",
        .category = "Platesolve",
        .requiredParameters = {"panels"},
        .parameterSchema =
            json{{"panels",
                  {{"type", "array"}, {"items", {{"type", "object"}}}}},
                 {"exposure", {{"type", "number"}, {"minimum", 0.001}}},
                 {"overlap",
                  {{"type", "number"},
                   {"minimum", 0.0},
                   {"maximum", 1.0},
                   {"default", 0.1}}}},
        .version = "1.0.0",
        .dependencies = {"PlateSolveExposure", "Centering"},
        .isEnabled = true};

    factory.registerTask<camera::MosaicTask>(
        "Mosaic", createTaskWrapper<camera::MosaicTask>, mosaicInfo);

    // Register device tasks
    TaskInfo deviceConnectInfo{
        .name = "DeviceConnect",
        .description = "Connect to a device",
        .category = "Device",
        .requiredParameters = {"device_name"},
        .parameterSchema =
            json{{"device_name", {{"type", "string"}}},
                 {"device_type", {{"type", "string"}}},
                 {"timeout",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 30}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<DeviceConnectTask>(
        "DeviceConnect", createTaskWrapper<DeviceConnectTask>,
        deviceConnectInfo);

    // Register config tasks
    TaskInfo loadConfigInfo{
        .name = "LoadConfig",
        .description = "Load configuration from file",
        .category = "Config",
        .requiredParameters = {"config_path"},
        .parameterSchema =
            json{{"config_path", {{"type", "string"}}},
                 {"profile", {{"type", "string"}, {"default", "default"}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<LoadConfigTask>(
        "LoadConfig", createTaskWrapper<LoadConfigTask>, loadConfigInfo);

    // Register script tasks
    TaskInfo runScriptInfo{
        .name = "RunScript",
        .description = "Execute a script with parameters",
        .category = "Script",
        .requiredParameters = {"script_path"},
        .parameterSchema =
            json{{"script_path", {{"type", "string"}}},
                 {"script_type",
                  {{"type", "string"},
                   {"enum", json::array({"python", "shell"})},
                   {"default", "python"}}},
                 {"timeout",
                  {{"type", "integer"}, {"minimum", 1}, {"default", 300}}},
                 {"parameters",
                  {{"type", "object"}, {"default", json::object()}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<RunScriptTask>(
        "RunScript", createTaskWrapper<RunScriptTask>, runScriptInfo);

    // Register search tasks
    TaskInfo targetSearchInfo{
        .name = "TargetSearch",
        .description = "Search for astronomical targets in catalog",
        .category = "Search",
        .requiredParameters = {},
        .parameterSchema =
            json{{"target_name", {{"type", "string"}}},
                 {"catalog", {{"type", "string"}, {"default", "NGC"}}},
                 {"magnitude_limit", {{"type", "number"}, {"default", 15.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<TargetSearchTask>(
        "TargetSearch", createTaskWrapper<TargetSearchTask>, targetSearchInfo);

    // Register additional device tasks
    TaskInfo deviceDisconnectInfo{
        .name = "DeviceDisconnect",
        .description = "Safely disconnect a device",
        .category = "Device",
        .requiredParameters = {"device_name"},
        .parameterSchema =
            json{{"device_name", {{"type", "string"}}},
                 {"safe_shutdown", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<DeviceDisconnectTask>(
        "DeviceDisconnect", createTaskWrapper<DeviceDisconnectTask>,
        deviceDisconnectInfo);

    // Register additional config tasks
    TaskInfo saveConfigInfo{
        .name = "SaveConfig",
        .description = "Save current configuration to file",
        .category = "Config",
        .requiredParameters = {"config_path"},
        .parameterSchema =
            json{{"config_path", {{"type", "string"}}},
                 {"profile", {{"type", "string"}, {"default", "default"}}},
                 {"format",
                  {{"type", "string"},
                   {"enum", json::array({"json", "yaml"})},
                   {"default", "json"}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<SaveConfigTask>(
        "SaveConfig", createTaskWrapper<SaveConfigTask>, saveConfigInfo);

    // Register workflow task
    TaskInfo runWorkflowInfo{
        .name = "RunWorkflow",
        .description = "Execute a multi-step script workflow",
        .category = "Script",
        .requiredParameters = {"workflow_name"},
        .parameterSchema =
            json{{"workflow_name", {{"type", "string"}}},
                 {"steps", {{"type", "array"}, {"default", json::array()}}},
                 {"parallel", {{"type", "boolean"}, {"default", false}}},
                 {"stop_on_error", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<RunWorkflowTask>(
        "RunWorkflow", createTaskWrapper<RunWorkflowTask>, runWorkflowInfo);

    // Register mount tasks
    TaskInfo mountSlewInfo{
        .name = "MountSlew",
        .description = "Slew mount to specified coordinates",
        .category = "Mount",
        .requiredParameters = {"ra", "dec"},
        .parameterSchema =
            json{
                {"ra",
                 {{"type", "number"}, {"minimum", 0.0}, {"maximum", 24.0}}},
                {"dec",
                 {{"type", "number"}, {"minimum", -90.0}, {"maximum", 90.0}}},
                {"target_name", {{"type", "string"}, {"default", ""}}},
                {"sync_before_slew", {{"type", "boolean"}, {"default", false}}},
                {"wait_for_settle", {{"type", "boolean"}, {"default", true}}},
                {"settle_time",
                 {{"type", "number"}, {"minimum", 0.0}, {"default", 5.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<MountSlewTask>(
        "MountSlew", createTaskWrapper<MountSlewTask>, mountSlewInfo);

    TaskInfo mountParkInfo{
        .name = "MountPark",
        .description = "Park mount at specified position",
        .category = "Mount",
        .requiredParameters = {},
        .parameterSchema = json{{"park_position",
                                 {{"type", "string"}, {"default", "default"}}},
                                {"wait_for_complete",
                                 {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<MountParkTask>(
        "MountPark", createTaskWrapper<MountParkTask>, mountParkInfo);

    TaskInfo mountTrackInfo{
        .name = "MountTrack",
        .description = "Control mount tracking",
        .category = "Mount",
        .requiredParameters = {},
        .parameterSchema =
            json{{"tracking_mode",
                  {{"type", "string"},
                   {"enum",
                    json::array({"sidereal", "lunar", "solar", "custom"})},
                   {"default", "sidereal"}}},
                 {"enabled", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<MountTrackTask>(
        "MountTrack", createTaskWrapper<MountTrackTask>, mountTrackInfo);

    // Register focuser tasks
    TaskInfo focuserMoveInfo{
        .name = "FocuserMove",
        .description = "Move focuser to specified position",
        .category = "Focuser",
        .requiredParameters = {"position"},
        .parameterSchema =
            json{{"position", {{"type", "integer"}, {"minimum", 0}}},
                 {"absolute", {{"type", "boolean"}, {"default", true}}},
                 {"wait_for_complete",
                  {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<FocuserMoveTask>(
        "FocuserMove", createTaskWrapper<FocuserMoveTask>, focuserMoveInfo);

    // ========================================================================
    // Register Workflow Tasks
    // ========================================================================

    TaskInfo targetAcquisitionInfo{
        .name = "TargetAcquisition",
        .description =
            "Complete target acquisition workflow (slew, plate solve, center, "
            "guide, focus)",
        .category = "Workflow",
        .requiredParameters = {"target_name", "coordinates"},
        .parameterSchema =
            json{{"target_name", {{"type", "string"}}},
                 {"coordinates", {{"type", "object"}}},
                 {"settle_time", {{"type", "integer"}, {"default", 5}}},
                 {"start_guiding", {{"type", "boolean"}, {"default", true}}},
                 {"perform_autofocus",
                  {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::TargetAcquisitionTask>(
        "TargetAcquisition", createTaskWrapper<workflow::TargetAcquisitionTask>,
        targetAcquisitionInfo);

    TaskInfo exposureSequenceInfo{
        .name = "ExposureSequence",
        .description =
            "Execute exposure sequence for a target with filter changes and "
            "dithering",
        .category = "Workflow",
        .requiredParameters = {"target_name", "exposure_plans"},
        .parameterSchema =
            json{{"target_name", {{"type", "string"}}},
                 {"exposure_plans", {{"type", "array"}}},
                 {"dither_enabled", {{"type", "boolean"}, {"default", true}}},
                 {"dither_pixels", {{"type", "number"}, {"default", 5.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::ExposureSequenceTask>(
        "ExposureSequence", createTaskWrapper<workflow::ExposureSequenceTask>,
        exposureSequenceInfo);

    TaskInfo sessionInfo{
        .name = "Session",
        .description = "Complete observation session management",
        .category = "Workflow",
        .requiredParameters = {"session_name", "targets"},
        .parameterSchema = json{{"session_name", {{"type", "string"}}},
                                {"targets", {{"type", "array"}}},
                                {"camera_cooling_temp",
                                 {{"type", "number"}, {"default", -10.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::SessionTask>(
        "Session", createTaskWrapper<workflow::SessionTask>, sessionInfo);

    TaskInfo safetyCheckInfo{
        .name = "SafetyCheck",
        .description = "Weather and equipment safety monitoring",
        .category = "Workflow",
        .requiredParameters = {},
        .parameterSchema =
            json{{"check_weather", {{"type", "boolean"}, {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::SafetyCheckTask>(
        "SafetyCheck", createTaskWrapper<workflow::SafetyCheckTask>,
        safetyCheckInfo);

    TaskInfo meridianFlipInfo{
        .name = "MeridianFlip",
        .description = "Automated meridian flip handling",
        .category = "Workflow",
        .requiredParameters = {"target_coordinates"},
        .parameterSchema =
            json{{"target_coordinates", {{"type", "object"}}},
                 {"settle_time", {{"type", "integer"}, {"default", 10}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::MeridianFlipTask>(
        "MeridianFlip", createTaskWrapper<workflow::MeridianFlipTask>,
        meridianFlipInfo);

    TaskInfo ditherInfo{
        .name = "Dither",
        .description = "Dithering between exposures",
        .category = "Workflow",
        .requiredParameters = {},
        .parameterSchema =
            json{{"dither_pixels", {{"type", "number"}, {"default", 5.0}}},
                 {"settle_time", {{"type", "integer"}, {"default", 10}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::DitherTask>(
        "Dither", createTaskWrapper<workflow::DitherTask>, ditherInfo);

    TaskInfo waitInfo{
        .name = "Wait",
        .description = "Configurable wait conditions",
        .category = "Workflow",
        .requiredParameters = {"wait_type"},
        .parameterSchema =
            json{{"wait_type",
                  {{"type", "string"},
                   {"enum", json::array({"duration", "time", "altitude",
                                         "twilight"})}}},
                 {"duration", {{"type", "integer"}, {"default", 0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::WaitTask>(
        "Wait", createTaskWrapper<workflow::WaitTask>, waitInfo);

    TaskInfo calibrationFrameInfo{
        .name = "CalibrationFrame",
        .description = "Calibration frame acquisition (darks, flats, bias)",
        .category = "Workflow",
        .requiredParameters = {"frame_type", "count"},
        .parameterSchema =
            json{{"frame_type",
                  {{"type", "string"},
                   {"enum", json::array({"dark", "flat", "bias"})}}},
                 {"count", {{"type", "integer"}, {"minimum", 1}}},
                 {"exposure_time", {{"type", "number"}, {"default", 1.0}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true};

    factory.registerTask<workflow::CalibrationFrameTask>(
        "CalibrationFrame", createTaskWrapper<workflow::CalibrationFrameTask>,
        calibrationFrameInfo);

    spdlog::info("Successfully registered {} built-in tasks", 50);
}

std::vector<std::string> getRegisteredTaskTypes() {
    return TaskFactory::getInstance().getRegisteredTaskTypes();
}

bool isTaskTypeRegistered(const std::string& taskType) {
    return TaskFactory::getInstance().isTaskRegistered(taskType);
}

}  // namespace lithium::task
