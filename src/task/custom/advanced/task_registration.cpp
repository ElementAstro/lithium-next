#include "smart_exposure_task.hpp"
#include "deep_sky_sequence_task.hpp"
#include "planetary_imaging_task.hpp"
#include "timelapse_task.hpp"
#include "meridian_flip_task.hpp"
#include "intelligent_sequence_task.hpp"
#include "auto_calibration_task.hpp"
#include "../factory.hpp"

#include "atom/type/json.hpp"

namespace lithium::task::task {

// ==================== Task Registration ====================

namespace {
using namespace lithium::task;

// Register SmartExposureTask
AUTO_REGISTER_TASK(
    SmartExposureTask, "SmartExposure",
    (TaskInfo{
        .name = "SmartExposure",
        .description =
            "Automatically optimizes exposure time to achieve target SNR",
        .category = "Advanced",
        .requiredParameters = {"target_snr"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target_snr", json{{"type", "number"},
                                           {"minimum", 0},
                                           {"maximum", 1000}}},
                       {"max_exposure", json{{"type", "number"},
                                             {"minimum", 0},
                                             {"maximum", 3600}}},
                       {"min_exposure", json{{"type", "number"},
                                             {"minimum", 0},
                                             {"maximum", 300}}},
                       {"max_attempts", json{{"type", "integer"},
                                             {"minimum", 1},
                                             {"maximum", 20}}},
                       {"binning", json{{"type", "integer"}, {"minimum", 1}}},
                       {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                       {"offset", json{{"type", "integer"}, {"minimum", 0}}}}},
                 {"required", json::array({"target_snr"})}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register DeepSkySequenceTask
AUTO_REGISTER_TASK(
    DeepSkySequenceTask, "DeepSkySequence",
    (TaskInfo{.name = "DeepSkySequence",
              .description = "Performs automated deep sky imaging sequence "
                             "with multiple filters",
              .category = "Advanced",
              .requiredParameters = {"total_exposures", "exposure_time"},
              .parameterSchema =
                  json{
                      {"type", "object"},
                      {"properties",
                       json{{"target_name", json{{"type", "string"}}},
                            {"total_exposures", json{{"type", "integer"},
                                                     {"minimum", 1},
                                                     {"maximum", 1000}}},
                            {"exposure_time", json{{"type", "number"},
                                                   {"minimum", 0},
                                                   {"maximum", 3600}}},
                            {"filters",
                             json{{"type", "array"},
                                  {"items", json{{"type", "string"}}}}},
                            {"dithering", json{{"type", "boolean"}}},
                            {"dither_pixels", json{{"type", "integer"},
                                                   {"minimum", 0},
                                                   {"maximum", 100}}},
                            {"dither_interval", json{{"type", "number"},
                                                     {"minimum", 0},
                                                     {"maximum", 50}}},
                            {"binning",
                             json{{"type", "integer"}, {"minimum", 1}}},
                            {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                            {"offset",
                             json{{"type", "integer"}, {"minimum", 0}}}}},
                      {"required", json::array({"total_exposures",
                                                "exposure_time"})}},
              .version = "1.0.0",
              .dependencies = {"TakeExposure"}}));

// Register PlanetaryImagingTask
AUTO_REGISTER_TASK(
    PlanetaryImagingTask, "PlanetaryImaging",
    (TaskInfo{
        .name = "PlanetaryImaging",
        .description =
            "High-speed planetary imaging with lucky imaging support",
        .category = "Advanced",
        .requiredParameters = {"video_length"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"planet", json{{"type", "string"}}},
                       {"video_length", json{{"type", "integer"},
                                             {"minimum", 1},
                                             {"maximum", 1800}}},
                       {"frame_rate", json{{"type", "number"},
                                           {"minimum", 0},
                                           {"maximum", 120}}},
                       {"filters", json{{"type", "array"},
                                        {"items", json{{"type", "string"}}}}},
                       {"binning", json{{"type", "integer"}, {"minimum", 1}}},
                       {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                       {"offset", json{{"type", "integer"}, {"minimum", 0}}},
                       {"high_speed", json{{"type", "boolean"}}}}},
                 {"required", json::array({"video_length"})}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register TimelapseTask
AUTO_REGISTER_TASK(
    TimelapseTask, "Timelapse",
    (TaskInfo{.name = "Timelapse",
              .description =
                  "Captures timelapse sequences with configurable intervals",
              .category = "Advanced",
              .requiredParameters = {"total_frames", "interval"},
              .parameterSchema =
                  json{
                      {"type", "object"},
                      {"properties",
                       json{{"total_frames", json{{"type", "integer"},
                                                  {"minimum", 1},
                                                  {"maximum", 10000}}},
                            {"interval", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 3600}}},
                            {"exposure_time",
                             json{{"type", "number"}, {"minimum", 0}}},
                            {"type",
                             json{{"type", "string"},
                                  {"enum", json::array({"sunset", "lunar",
                                                        "star_trails"})}}},
                            {"binning",
                             json{{"type", "integer"}, {"minimum", 1}}},
                            {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                            {"offset",
                             json{{"type", "integer"}, {"minimum", 0}}},
                            {"auto_exposure", json{{"type", "boolean"}}}}},
                      {"required", json::array({"total_frames", "interval"})}},
              .version = "1.0.0",
              .dependencies = {"TakeExposure"}}));

// Register MeridianFlipTask
AUTO_REGISTER_TASK(
    MeridianFlipTask, "MeridianFlip",
    (TaskInfo{
        .name = "MeridianFlip",
        .description = "Automated meridian flip with plate solving and autofocus",
        .category = "Advanced",
        .requiredParameters = {"target_ra", "target_dec"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target_ra", json{{"type", "number"},
                                          {"minimum", 0},
                                          {"maximum", 24}}},
                       {"target_dec", json{{"type", "number"},
                                           {"minimum", -90},
                                           {"maximum", 90}}},
                       {"flip_offset_minutes", json{{"type", "number"},
                                                   {"minimum", 0},
                                                   {"maximum", 60}}},
                       {"autofocus_after_flip", json{{"type", "boolean"}}},
                       {"platesolve_after_flip", json{{"type", "boolean"}}},
                       {"rotate_after_flip", json{{"type", "boolean"}}},
                       {"target_rotation", json{{"type", "number"}}},
                       {"pause_before_flip", json{{"type", "number"}}}}},
                 {"required", json::array({"target_ra", "target_dec"})}},
        .version = "1.0.0",
        .dependencies = {"PlateSolve", "Autofocus"}}));

// Register IntelligentSequenceTask
AUTO_REGISTER_TASK(
    IntelligentSequenceTask, "IntelligentSequence",
    (TaskInfo{
        .name = "IntelligentSequence",
        .description = "Intelligent multi-target imaging with weather monitoring",
        .category = "Advanced",
        .requiredParameters = {"targets"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"targets", json{{"type", "array"},
                                       {"items", json{{"type", "object"},
                                                     {"properties", json{
                                                       {"name", json{{"type", "string"}}},
                                                       {"ra", json{{"type", "number"}}},
                                                       {"dec", json{{"type", "number"}}}}},
                                                     {"required", json::array({"name", "ra", "dec"})}}}}},
                       {"session_duration_hours", json{{"type", "number"),
                                                      {"minimum", 0},
                                                      {"maximum", 24}}},
                       {"min_altitude", json{{"type", "number"},
                                            {"minimum", 0},
                                            {"maximum", 90}}},
                       {"weather_monitoring", json{{"type", "boolean"}}},
                       {"dynamic_target_selection", json{{"type", "boolean"}}}}},
                 {"required", json::array({"targets"})}},
        .version = "1.0.0",
        .dependencies = {"DeepSkySequence", "WeatherMonitor"}}));

// Register AutoCalibrationTask
AUTO_REGISTER_TASK(
    AutoCalibrationTask, "AutoCalibration",
    (TaskInfo{
        .name = "AutoCalibration",
        .description = "Automated calibration frame capture and organization",
        .category = "Advanced",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"output_directory", json{{"type", "string"}}},
                       {"skip_existing", json{{"type", "boolean"}}},
                       {"organize_folders", json{{"type", "boolean"}}},
                       {"filters", json{{"type", "array"},
                                       {"items", json{{"type", "string"}}}}},
                       {"dark_frame_count", json{{"type", "integer"},
                                                {"minimum", 1},
                                                {"maximum", 200}}},
                       {"bias_frame_count", json{{"type", "integer"},
                                                {"minimum", 1},
                                                {"maximum", 500}}},
                       {"flat_frame_count", json{{"type", "integer"},
                                                {"minimum", 1},
                                                {"maximum", 100}}},
                       {"temperature", json{{"type", "number"},
                                           {"minimum", -40},
                                           {"maximum", 20}}}}},
                 {"required", json::array()}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));
}  // namespace

}  // namespace lithium::task::task
