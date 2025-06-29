#include "meridian_flip_task.hpp"
#include "intelligent_sequence_task.hpp"
#include "auto_calibration_task.hpp"
#include "weather_monitor_task.hpp"
#include "observatory_automation_task.hpp"
#include "mosaic_imaging_task.hpp"
#include "focus_optimization_task.hpp"
#include "../factory.hpp"

#include "atom/type/json.hpp"

namespace lithium::task::task {

namespace {
using namespace lithium::task;

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
                       {"session_duration_hours", json{{"type", "number"},
                                                      {"minimum", 0},
                                                      {"maximum", 24}}},
                       {"min_altitude", json{{"type", "number"},
                                            {"minimum", 0},
                                            {"maximum", 90}}},
                       {"weather_monitoring", json{{"type", "boolean"}}},
                       {"dynamic_target_selection", json{{"type", "boolean"}}}}},
                 {"required", json::array({"targets"})}},
        .version = "1.0.0",
        .dependencies = {"DeepSkySequence"}}));

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

// Register WeatherMonitorTask
AUTO_REGISTER_TASK(
    WeatherMonitorTask, "WeatherMonitor",
    (TaskInfo{
        .name = "WeatherMonitor",
        .description = "Continuous weather monitoring with safety responses",
        .category = "Advanced",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"monitor_interval_minutes", json{{"type", "number"},
                                                        {"minimum", 0.5},
                                                        {"maximum", 60}}},
                       {"cloud_cover_limit", json{{"type", "number"},
                                                 {"minimum", 0},
                                                 {"maximum", 100}}},
                       {"wind_speed_limit", json{{"type", "number"},
                                                {"minimum", 0}}},
                       {"humidity_limit", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 100}}},
                       {"rain_detection", json{{"type", "boolean"}}},
                       {"email_alerts", json{{"type", "boolean"}}}}},
                 {"required", json::array()}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register ObservatoryAutomationTask
AUTO_REGISTER_TASK(
    ObservatoryAutomationTask, "ObservatoryAutomation",
    (TaskInfo{
        .name = "ObservatoryAutomation",
        .description = "Complete observatory startup, operation, and shutdown",
        .category = "Advanced",
        .requiredParameters = {"operation"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                         {"enum", json::array({"startup", "shutdown", "emergency_stop"})}}},
                       {"enable_roof_control", json{{"type", "boolean"}}},
                       {"enable_telescope_control", json{{"type", "boolean"}}},
                       {"enable_camera_control", json{{"type", "boolean"}}},
                       {"camera_temperature", json{{"type", "number"},
                                                  {"minimum", -50},
                                                  {"maximum", 20}}},
                       {"perform_safety_check", json{{"type", "boolean"}}}}},
                 {"required", json::array({"operation"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register MosaicImagingTask
AUTO_REGISTER_TASK(
    MosaicImagingTask, "MosaicImaging",
    (TaskInfo{
        .name = "MosaicImaging",
        .description = "Automated large field-of-view mosaic imaging",
        .category = "Advanced",
        .requiredParameters = {"center_ra", "center_dec"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"center_ra", json{{"type", "number"},
                                         {"minimum", 0},
                                         {"maximum", 24}}},
                       {"center_dec", json{{"type", "number"},
                                          {"minimum", -90},
                                          {"maximum", 90}}},
                       {"mosaic_width_degrees", json{{"type", "number"},
                                                    {"minimum", 0.1}}},
                       {"mosaic_height_degrees", json{{"type", "number"},
                                                     {"minimum", 0.1}}},
                       {"tiles_x", json{{"type", "integer"},
                                       {"minimum", 1},
                                       {"maximum", 10}}},
                       {"tiles_y", json{{"type", "integer"},
                                       {"minimum", 1},
                                       {"maximum", 10}}},
                       {"overlap_percent", json{{"type", "number"},
                                               {"minimum", 0},
                                               {"maximum", 50}}}}},
                 {"required", json::array({"center_ra", "center_dec"})}},
        .version = "1.0.0",
        .dependencies = {"DeepSkySequence", "PlateSolve"}}));

// Register FocusOptimizationTask
AUTO_REGISTER_TASK(
    FocusOptimizationTask, "FocusOptimization",
    (TaskInfo{
        .name = "FocusOptimization",
        .description = "Advanced focus optimization with temperature compensation",
        .category = "Advanced",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"focus_mode", json{{"type", "string"},
                                          {"enum", json::array({"initial", "periodic", "temperature_compensation", "continuous"})}}},
                       {"algorithm", json{{"type", "string"},
                                         {"enum", json::array({"hfr", "fwhm", "star_count"})}}},
                       {"step_size", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 1000}}},
                       {"max_steps", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 100}}},
                       {"target_hfr", json{{"type", "number"},
                                          {"minimum", 0},
                                          {"maximum", 10}}},
                       {"temperature_compensation", json{{"type", "boolean"}}}}},
                 {"required", json::array()}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure", "Focuser"}}));

}  // namespace

}  // namespace lithium::task::task
