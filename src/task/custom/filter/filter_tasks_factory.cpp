#include "filter_change_task.hpp"
#include "lrgb_sequence_task.hpp"
#include "narrowband_sequence_task.hpp"
#include "filter_calibration_task.hpp"
#include "../factory.hpp"

namespace lithium::task::filter {

// Register FilterChangeTask
namespace {
static auto filter_change_registrar = TaskRegistrar<FilterChangeTask>(
    "filter_change",
    TaskInfo{
        .name = "filter_change",
        .description = "Change individual filters on the filter wheel",
        .category = "imaging",
        .requiredParameters = {"filterName"},
        .parameterSchema = json{
            {"filterName", {{"type", "string"}, {"description", "Name of filter to change to"}}},
            {"timeout", {{"type", "number"}, {"description", "Timeout in seconds"}, {"default", 30}}},
            {"verify", {{"type", "boolean"}, {"description", "Verify position after change"}, {"default", true}}},
            {"retries", {{"type", "number"}, {"description", "Number of retry attempts"}, {"default", 3}}}
        },
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true
    },
    [](const std::string& name, const json& config) -> std::unique_ptr<FilterChangeTask> {
        return std::make_unique<FilterChangeTask>(name);
    }
);

static auto lrgb_sequence_registrar = TaskRegistrar<LRGBSequenceTask>(
    "lrgb_sequence",
    TaskInfo{
        .name = "lrgb_sequence",
        .description = "Execute LRGB imaging sequences",
        .category = "imaging",
        .requiredParameters = {},
        .parameterSchema = json{
            {"luminance_exposure", {{"type", "number"}, {"default", 60.0}}},
            {"red_exposure", {{"type", "number"}, {"default", 60.0}}},
            {"green_exposure", {{"type", "number"}, {"default", 60.0}}},
            {"blue_exposure", {{"type", "number"}, {"default", 60.0}}},
            {"luminance_count", {{"type", "number"}, {"default", 10}}},
            {"red_count", {{"type", "number"}, {"default", 5}}},
            {"green_count", {{"type", "number"}, {"default", 5}}},
            {"blue_count", {{"type", "number"}, {"default", 5}}},
            {"gain", {{"type", "number"}, {"default", 100}}},
            {"offset", {{"type", "number"}, {"default", 10}}}
        },
        .version = "1.0.0",
        .dependencies = {"filter_change"},
        .isEnabled = true
    },
    [](const std::string& name, const json& config) -> std::unique_ptr<LRGBSequenceTask> {
        return std::make_unique<LRGBSequenceTask>(name);
    }
);

static auto narrowband_sequence_registrar = TaskRegistrar<NarrowbandSequenceTask>(
    "narrowband_sequence", 
    TaskInfo{
        .name = "narrowband_sequence",
        .description = "Execute narrowband imaging sequences",
        .category = "imaging",
        .requiredParameters = {},
        .parameterSchema = json{
            {"ha_exposure", {{"type", "number"}, {"default", 300.0}}},
            {"oiii_exposure", {{"type", "number"}, {"default", 300.0}}},
            {"sii_exposure", {{"type", "number"}, {"default", 300.0}}},
            {"ha_count", {{"type", "number"}, {"default", 10}}},
            {"oiii_count", {{"type", "number"}, {"default", 10}}},
            {"sii_count", {{"type", "number"}, {"default", 10}}},
            {"gain", {{"type", "number"}, {"default", 200}}},
            {"offset", {{"type", "number"}, {"default", 10}}}
        },
        .version = "1.0.0", 
        .dependencies = {"filter_change"},
        .isEnabled = true
    },
    [](const std::string& name, const json& config) -> std::unique_ptr<NarrowbandSequenceTask> {
        return std::make_unique<NarrowbandSequenceTask>(name);
    }
);

static auto filter_calibration_registrar = TaskRegistrar<FilterCalibrationTask>(
    "filter_calibration",
    TaskInfo{
        .name = "filter_calibration", 
        .description = "Perform filter calibration sequences",
        .category = "calibration",
        .requiredParameters = {"calibration_type"},
        .parameterSchema = json{
            {"calibration_type", {{"type", "string"}, {"enum", json::array({"dark", "flat", "bias", "all"})}}},
            {"filters", {{"type", "array"}, {"items", {{"type", "string"}}}}},
            {"dark_count", {{"type", "number"}, {"default", 10}}},
            {"flat_count", {{"type", "number"}, {"default", 10}}},
            {"bias_count", {{"type", "number"}, {"default", 50}}}
        },
        .version = "1.0.0",
        .dependencies = {"filter_change"},
        .isEnabled = true
    },
    [](const std::string& name, const json& config) -> std::unique_ptr<FilterCalibrationTask> {
        return std::make_unique<FilterCalibrationTask>(name);
    }
);
}

}  // namespace lithium::task::filter