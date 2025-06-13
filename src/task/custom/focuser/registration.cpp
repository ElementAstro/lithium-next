//
// Created by max on 2025-06-13.
//

#include "factory.hpp"
#include "base.hpp"
#include "position.hpp" 
#include "autofocus.hpp"
#include "temperature.hpp"
#include "validation.hpp"
#include "backlash.hpp"
#include "calibration.hpp"
#include "star_analysis.hpp"

namespace lithium::task::focuser {

namespace {
using namespace lithium::task;

// Register FocuserPositionTask
AUTO_REGISTER_TASK(
    FocuserPositionTask, "FocuserPosition",
    (TaskInfo{
        .name = "FocuserPosition",
        .description = "Control focuser position (absolute/relative moves, sync)",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"move_absolute", "move_relative", "sync", "get_position", "halt"})},
                                          {"description", "Position operation to perform"}}},
                       {"position", json{{"type", "integer"},
                                         {"minimum", 0},
                                         {"description", "Target position for absolute move or sync"}}},
                       {"steps", json{{"type", "integer"},
                                      {"description", "Steps for relative move (positive=outward, negative=inward)"}}},
                       {"timeout", json{{"type", "number"},
                                        {"minimum", 1.0},
                                        {"default", 30.0},
                                        {"description", "Movement timeout in seconds"}}},
                       {"wait_for_completion", json{{"type", "boolean"},
                                                    {"default", true},
                                                    {"description", "Wait for movement to complete"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register AutofocusTask
AUTO_REGISTER_TASK(
    AutofocusTask, "Autofocus",
    (TaskInfo{
        .name = "Autofocus",
        .description = "Automatic focusing with multiple algorithms and quality assessment",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"algorithm", json{{"type", "string"},
                                          {"enum", json::array({"vcurve", "hyperbolic", "polynomial", "simple"})},
                                          {"default", "vcurve"},
                                          {"description", "Autofocus algorithm to use"}}},
                       {"initial_step_size", json{{"type", "integer"},
                                                  {"minimum", 1},
                                                  {"default", 100},
                                                  {"description", "Initial step size for coarse focusing"}}},
                       {"fine_step_size", json{{"type", "integer"},
                                               {"minimum", 1},
                                               {"default", 20},
                                               {"description", "Step size for fine focusing"}}},
                       {"search_range", json{{"type", "integer"},
                                             {"minimum", 100},
                                             {"default", 1000},
                                             {"description", "Total search range in steps"}}},
                       {"max_iterations", json{{"type", "integer"},
                                               {"minimum", 3},
                                               {"maximum", 50},
                                               {"default", 20},
                                               {"description", "Maximum focusing iterations"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"default", 5.0},
                                              {"description", "Exposure time for focus frames"}}},
                       {"tolerance", json{{"type", "number"},
                                          {"minimum", 0.01},
                                          {"default", 0.1},
                                          {"description", "Focus quality tolerance"}}},
                       {"use_subframe", json{{"type", "boolean"},
                                             {"default", true},
                                             {"description", "Use subframe for faster focusing"}}},
                       {"subframe_size", json{{"type", "integer"},
                                              {"minimum", 100},
                                              {"default", 512},
                                              {"description", "Subframe size in pixels"}}},
                       {"filter", json{{"type", "string"},
                                       {"description", "Filter to use for focusing"}}},
                       {"binning", json{{"type", "integer"},
                                        {"minimum", 1},
                                        {"default", 2},
                                        {"description", "Camera binning for focus frames"}}}}}}},
        .version = "1.0.0",
        .dependencies = {"FocuserPosition", "StarAnalysis"}}));

// Register TemperatureCompensationTask
AUTO_REGISTER_TASK(
    TemperatureCompensationTask, "TemperatureCompensation",
    (TaskInfo{
        .name = "TemperatureCompensation",
        .description = "Temperature compensation and monitoring for focus drift",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"enable", "disable", "calibrate", "monitor"})},
                                          {"description", "Temperature compensation operation"}}},
                       {"compensation_rate", json{{"type", "number"},
                                                  {"description", "Steps per degree Celsius (if known)"}}},
                       {"temperature_tolerance", json{{"type", "number"},
                                                      {"minimum", 0.1},
                                                      {"default", 1.0},
                                                      {"description", "Temperature change threshold for compensation"}}},
                       {"monitor_interval", json{{"type", "number"},
                                                 {"minimum", 1.0},
                                                 {"default", 60.0},
                                                 {"description", "Temperature monitoring interval in seconds"}}},
                       {"calibration_temp_range", json{{"type", "number"},
                                                       {"minimum", 1.0},
                                                       {"default", 10.0},
                                                       {"description", "Temperature range for calibration"}}},
                       {"use_predictive", json{{"type", "boolean"},
                                               {"default", true},
                                               {"description", "Use predictive compensation based on trends"}}}}}}},
        .version = "1.0.0",
        .dependencies = {"FocuserPosition", "Autofocus"}}));

// Register FocusValidationTask
AUTO_REGISTER_TASK(
    FocusValidationTask, "FocusValidation",
    (TaskInfo{
        .name = "FocusValidation",
        .description = "Focus quality validation and drift monitoring",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"validate", "monitor", "auto_correct"})},
                                          {"description", "Validation operation to perform"}}},
                       {"quality_threshold", json{{"type", "number"},
                                                  {"minimum", 0.1},
                                                  {"default", 0.8},
                                                  {"description", "Minimum acceptable focus quality (0-1)"}}},
                       {"drift_threshold", json{{"type", "number"},
                                                {"minimum", 0.01},
                                                {"default", 0.2},
                                                {"description", "Focus drift threshold for auto-correction"}}},
                       {"monitor_interval", json{{"type", "number"},
                                                 {"minimum", 10.0},
                                                 {"default", 300.0},
                                                 {"description", "Monitoring interval in seconds"}}},
                       {"validation_frames", json{{"type", "integer"},
                                                  {"minimum", 1},
                                                  {"default", 3},
                                                  {"description", "Number of frames for validation"}}},
                       {"auto_refocus", json{{"type", "boolean"},
                                             {"default", true},
                                             {"description", "Automatically refocus if drift detected"}}}}}}},
        .version = "1.0.0",
        .dependencies = {"StarAnalysis", "Autofocus"}}));

// Register BacklashCompensationTask
AUTO_REGISTER_TASK(
    BacklashCompensationTask, "BacklashCompensation",
    (TaskInfo{
        .name = "BacklashCompensation",
        .description = "Backlash measurement and compensation for precise focusing",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"measure", "enable", "disable", "calibrate"})},
                                          {"description", "Backlash operation to perform"}}},
                       {"measurement_range", json{{"type", "integer"},
                                                  {"minimum", 50},
                                                  {"default", 200},
                                                  {"description", "Range for backlash measurement"}}},
                       {"measurement_steps", json{{"type", "integer"},
                                                  {"minimum", 5},
                                                  {"default", 20},
                                                  {"description", "Number of steps for measurement"}}},
                       {"compensation_steps", json{{"type", "integer"},
                                                   {"minimum", 0},
                                                   {"description", "Manual backlash compensation amount"}}},
                       {"auto_compensate", json{{"type", "boolean"},
                                                {"default", true},
                                                {"description", "Automatically apply compensation"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"default", 3.0},
                                              {"description", "Exposure time for measurement frames"}}}}}}},
        .version = "1.0.0",
        .dependencies = {"FocuserPosition", "StarAnalysis"}}));

// Register FocusCalibrationTask
AUTO_REGISTER_TASK(
    FocusCalibrationTask, "FocusCalibration",
    (TaskInfo{
        .name = "FocusCalibration",
        .description = "Comprehensive focus system calibration and optimization",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"full", "quick", "temperature", "backlash", "validation"})},
                                          {"description", "Calibration type to perform"}}},
                       {"calibration_range", json{{"type", "integer"},
                                                  {"minimum", 500},
                                                  {"default", 2000},
                                                  {"description", "Focus range for calibration"}}},
                       {"temperature_points", json{{"type", "integer"},
                                                   {"minimum", 3},
                                                   {"default", 5},
                                                   {"description", "Number of temperature points for calibration"}}},
                       {"filter_list", json{{"type", "array"},
                                            {"items", json{{"type", "string"}}},
                                            {"description", "Filters to calibrate (empty = all available)"}}},
                       {"save_profile", json{{"type", "boolean"},
                                             {"default", true},
                                             {"description", "Save calibration profile"}}},
                       {"profile_name", json{{"type", "string"},
                                             {"description", "Name for calibration profile"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"default", 5.0},
                                              {"description", "Exposure time for calibration frames"}}}}}}},
        .version = "1.0.0",
        .dependencies = {"Autofocus", "TemperatureCompensation", "BacklashCompensation"}}));

// Register StarAnalysisTask
AUTO_REGISTER_TASK(
    StarAnalysisTask, "StarAnalysis",
    (TaskInfo{
        .name = "StarAnalysis",
        .description = "Advanced star detection and quality analysis for focusing",
        .category = "Focuser",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"operation", json{{"type", "string"},
                                          {"enum", json::array({"detect", "measure", "analyze", "hfd"})},
                                          {"description", "Star analysis operation"}}},
                       {"detection_threshold", json{{"type", "number"},
                                                    {"minimum", 0.1},
                                                    {"default", 3.0},
                                                    {"description", "Star detection threshold (sigma)"}}},
                       {"min_star_size", json{{"type", "integer"},
                                              {"minimum", 3},
                                              {"default", 5},
                                              {"description", "Minimum star size in pixels"}}},
                       {"max_star_size", json{{"type", "integer"},
                                              {"minimum", 10},
                                              {"default", 50},
                                              {"description", "Maximum star size in pixels"}}},
                       {"roi_size", json{{"type", "integer"},
                                         {"minimum", 50},
                                         {"default", 100},
                                         {"description", "Region of interest size around stars"}}},
                       {"max_stars", json{{"type", "integer"},
                                          {"minimum", 1},
                                          {"default", 20},
                                          {"description", "Maximum number of stars to analyze"}}},
                       {"quality_metric", json{{"type", "string"},
                                               {"enum", json::array({"hfd", "fwhm", "eccentricity", "snr"})},
                                               {"default", "hfd"},
                                               {"description", "Primary quality metric"}}},
                       {"image_path", json{{"type", "string"},
                                           {"description", "Path to image file for analysis"}}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

} // anonymous namespace

} // namespace lithium::task::focuser
