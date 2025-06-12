// ==================== Includes and Declarations ====================
#include "filter_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "basic_exposure.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::task::task {

// ==================== Mock Filter Wheel Class ====================
#ifdef MOCK_CAMERA
class MockFilterWheel {
public:
    MockFilterWheel() = default;

    void setFilter(const std::string& filterName) {
        currentFilter_ = filterName;
        spdlog::info("Filter wheel set to: {}", filterName);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(500));  // Simulate movement
    }

    std::string getCurrentFilter() const { return currentFilter_; }
    bool isMoving() const { return false; }

    std::vector<std::string> getAvailableFilters() const {
        return {"Red", "Green", "Blue", "Luminance",
                "Ha",  "OIII",  "SII",  "Clear"};
    }

private:
    std::string currentFilter_{"Luminance"};
};
#endif

// ==================== FilterSequenceTask Implementation ====================

auto FilterSequenceTask::taskName() -> std::string { return "FilterSequence"; }

void FilterSequenceTask::execute(const json& params) { executeImpl(params); }

void FilterSequenceTask::executeImpl(const json& params) {
    spdlog::info("Executing FilterSequence task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        auto filters = params.at("filters").get<std::vector<std::string>>();
        double exposure = params.at("exposure").get<double>();
        int count = params.value("count", 1);

        spdlog::info(
            "Starting filter sequence with {} filters, {} second exposures, {} "
            "frames per filter",
            filters.size(), exposure, count);

#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif

        int totalFrames = 0;

        for (const auto& filter : filters) {
            spdlog::info("Switching to filter: {}", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif

            // Wait for filter wheel to settle
            std::this_thread::sleep_for(std::chrono::seconds(1));

            for (int i = 0; i < count; ++i) {
                spdlog::info("Taking frame {} of {} with filter {}", i + 1,
                             count, filter);

                // Take exposure with current filter
                json exposureParams = {{"exposure", exposure},
                                       {"type", ExposureType::LIGHT},
                                       {"gain", params.value("gain", 100)},
                                       {"offset", params.value("offset", 10)},
                                       {"filter", filter}};

                TakeExposureTask exposureTask("TakeExposure",
                                              [](const json&) {});
                exposureTask.execute(exposureParams);
                totalFrames++;

                spdlog::info("Frame {} with filter {} completed", i + 1,
                             filter);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("FilterSequence completed {} total frames in {} ms",
                     totalFrames, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("FilterSequence task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto FilterSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            FilterSequenceTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced FilterSequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void FilterSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("filters", "array", true,
                            json::array({"Red", "Green", "Blue"}),
                            "List of filters to use");
    task.addParamDefinition("exposure", "double", true, 60.0,
                            "Exposure time per frame");
    task.addParamDefinition("count", "int", false, 1,
                            "Number of frames per filter");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void FilterSequenceTask::validateFilterSequenceParameters(const json& params) {
    if (!params.contains("filters") || !params["filters"].is_array()) {
        THROW_INVALID_ARGUMENT("Missing or invalid filters parameter");
    }

    auto filters = params["filters"];
    if (filters.empty() || filters.size() > 10) {
        THROW_INVALID_ARGUMENT("Filter list must contain 1-10 filters");
    }

    if (!params.contains("exposure") || !params["exposure"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure parameter");
    }

    double exposure = params["exposure"].get<double>();
    if (exposure <= 0 || exposure > 3600) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0 and 3600 seconds");
    }
}

// ==================== RGBSequenceTask Implementation ====================

auto RGBSequenceTask::taskName() -> std::string { return "RGBSequence"; }

void RGBSequenceTask::execute(const json& params) { executeImpl(params); }

void RGBSequenceTask::executeImpl(const json& params) {
    spdlog::info("Executing RGBSequence task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double redExposure = params.value("red_exposure", 60.0);
        double greenExposure = params.value("green_exposure", 60.0);
        double blueExposure = params.value("blue_exposure", 60.0);
        int count = params.value("count", 5);

        spdlog::info(
            "Starting RGB sequence: R={:.1f}s, G={:.1f}s, B={:.1f}s, {} frames "
            "each",
            redExposure, greenExposure, blueExposure, count);

#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif

        std::vector<std::pair<std::string, double>> rgbSequence = {
            {"Red", redExposure},
            {"Green", greenExposure},
            {"Blue", blueExposure}};

        int totalFrames = 0;

        for (const auto& [filter, exposure] : rgbSequence) {
            spdlog::info("Switching to {} filter", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif
            std::this_thread::sleep_for(std::chrono::seconds(1));

            for (int i = 0; i < count; ++i) {
                spdlog::info("Taking {} frame {} of {}", filter, i + 1, count);

                json exposureParams = {{"exposure", exposure},
                                       {"type", ExposureType::LIGHT},
                                       {"gain", params.value("gain", 100)},
                                       {"offset", params.value("offset", 10)},
                                       {"filter", filter}};

                TakeExposureTask exposureTask("TakeExposure",
                                              [](const json&) {});
                exposureTask.execute(exposureParams);
                totalFrames++;

                spdlog::info("{} frame {} of {} completed", filter, i + 1,
                             count);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("RGBSequence completed {} total frames in {} ms",
                     totalFrames, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("RGBSequence task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto RGBSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            RGBSequenceTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced RGBSequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(7200));  // 2 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void RGBSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("red_exposure", "double", false, 60.0,
                            "Red filter exposure time");
    task.addParamDefinition("green_exposure", "double", false, 60.0,
                            "Green filter exposure time");
    task.addParamDefinition("blue_exposure", "double", false, 60.0,
                            "Blue filter exposure time");
    task.addParamDefinition("count", "int", false, 5,
                            "Number of frames per filter");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void RGBSequenceTask::validateRGBParameters(const json& params) {
    std::vector<std::string> exposureParams = {"red_exposure", "green_exposure",
                                               "blue_exposure"};

    for (const auto& param : exposureParams) {
        if (params.contains(param)) {
            double exposure = params[param].get<double>();
            if (exposure <= 0 || exposure > 3600) {
                THROW_INVALID_ARGUMENT(
                    "RGB exposure times must be between 0 and 3600 seconds");
            }
        }
    }

    if (params.contains("count")) {
        int count = params["count"].get<int>();
        if (count < 1 || count > 100) {
            THROW_INVALID_ARGUMENT("Frame count must be between 1 and 100");
        }
    }
}

// ==================== NarrowbandSequenceTask Implementation
// ====================

auto NarrowbandSequenceTask::taskName() -> std::string {
    return "NarrowbandSequence";
}

void NarrowbandSequenceTask::execute(const json& params) {
    executeImpl(params);
}

void NarrowbandSequenceTask::executeImpl(const json& params) {
    spdlog::info("Executing NarrowbandSequence task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double haExposure = params.value("ha_exposure", 300.0);
        double oiiiExposure = params.value("oiii_exposure", 300.0);
        double siiExposure = params.value("sii_exposure", 300.0);
        int count = params.value("count", 10);
        bool useHOS =
            params.value("use_hos", true);  // H-alpha, OIII, SII sequence

        spdlog::info(
            "Starting narrowband sequence: Ha={:.1f}s, OIII={:.1f}s, "
            "SII={:.1f}s, {} frames each",
            haExposure, oiiiExposure, siiExposure, count);

#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif

        std::vector<std::pair<std::string, double>> narrowbandSequence;

        if (useHOS) {
            narrowbandSequence = {{"Ha", haExposure},
                                  {"OIII", oiiiExposure},
                                  {"SII", siiExposure}};
        } else {
            if (params.contains("ha_exposure"))
                narrowbandSequence.emplace_back("Ha", haExposure);
            if (params.contains("oiii_exposure"))
                narrowbandSequence.emplace_back("OIII", oiiiExposure);
            if (params.contains("sii_exposure"))
                narrowbandSequence.emplace_back("SII", siiExposure);
        }

        int totalFrames = 0;

        for (const auto& [filter, exposure] : narrowbandSequence) {
            spdlog::info("Switching to {} filter", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif
            std::this_thread::sleep_for(
                std::chrono::seconds(2));  // Longer settle time for narrowband

            for (int i = 0; i < count; ++i) {
                spdlog::info("Taking {} frame {} of {}", filter, i + 1, count);

                json exposureParams = {
                    {"exposure", exposure},
                    {"type", ExposureType::LIGHT},
                    {"gain",
                     params.value("gain", 200)},  // Higher gain for narrowband
                    {"offset", params.value("offset", 10)},
                    {"filter", filter}};

                TakeExposureTask exposureTask("TakeExposure",
                                              [](const json&) {});
                exposureTask.execute(exposureParams);
                totalFrames++;

                spdlog::info("{} frame {} of {} completed", filter, i + 1,
                             count);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("NarrowbandSequence completed {} total frames in {} ms",
                     totalFrames, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("NarrowbandSequence task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto NarrowbandSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            NarrowbandSequenceTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced NarrowbandSequence task failed: {}",
                          e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(14400));  // 4 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void NarrowbandSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("ha_exposure", "double", false, 300.0,
                            "H-alpha exposure time");
    task.addParamDefinition("oiii_exposure", "double", false, 300.0,
                            "OIII exposure time");
    task.addParamDefinition("sii_exposure", "double", false, 300.0,
                            "SII exposure time");
    task.addParamDefinition("count", "int", false, 10,
                            "Number of frames per filter");
    task.addParamDefinition("use_hos", "bool", false, true,
                            "Use H-alpha, OIII, SII sequence");
    task.addParamDefinition("gain", "int", false, 200,
                            "Camera gain for narrowband");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void NarrowbandSequenceTask::validateNarrowbandParameters(const json& params) {
    std::vector<std::string> exposureParams = {"ha_exposure", "oiii_exposure",
                                               "sii_exposure"};

    for (const auto& param : exposureParams) {
        if (params.contains(param)) {
            double exposure = params[param].get<double>();
            if (exposure <= 0 || exposure > 1800) {  // Max 30 minutes
                THROW_INVALID_ARGUMENT(
                    "Narrowband exposure times must be between 0 and 1800 "
                    "seconds");
            }
        }
    }

    if (params.contains("count")) {
        int count = params["count"].get<int>();
        if (count < 1 || count > 200) {
            THROW_INVALID_ARGUMENT("Frame count must be between 1 and 200");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register FilterSequenceTask
AUTO_REGISTER_TASK(
    FilterSequenceTask, "FilterSequence",
    (TaskInfo{
        .name = "FilterSequence",
        .description = "Sequence exposures for a list of filters",
        .category = "Imaging",
        .requiredParameters = {"filters", "exposure"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"filters", json{{"type", "array"},
                                        {"items", json{{"type", "string"}}}}},
                       {"exposure", json{{"type", "number"},
                                         {"minimum", 0},
                                         {"maximum", 3600}}},
                       {"count", json{{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 100}}},
                       {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                       {"offset", json{{"type", "integer"}, {"minimum", 0}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register RGBSequenceTask
AUTO_REGISTER_TASK(
    RGBSequenceTask, "RGBSequence",
    (TaskInfo{.name = "RGBSequence",
              .description = "Sequence exposures for RGB filters",
              .category = "Imaging",
              .requiredParameters = {},
              .parameterSchema =
                  json{
                      {"type", "object"},
                      {"properties",
                       json{{"red_exposure", json{{"type", "number"},
                                                  {"minimum", 0},
                                                  {"maximum", 3600}}},
                            {"green_exposure", json{{"type", "number"},
                                                    {"minimum", 0},
                                                    {"maximum", 3600}}},
                            {"blue_exposure", json{{"type", "number"},
                                                   {"minimum", 0},
                                                   {"maximum", 3600}}},
                            {"count", json{{"type", "integer"},
                                           {"minimum", 1},
                                           {"maximum", 100}}},
                            {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                            {"offset",
                             json{{"type", "integer"}, {"minimum", 0}}}}}},
              .version = "1.0.0",
              .dependencies = {"TakeExposure"}}));

// Register NarrowbandSequenceTask
AUTO_REGISTER_TASK(
    NarrowbandSequenceTask, "NarrowbandSequence",
    (TaskInfo{
        .name = "NarrowbandSequence",
        .description =
            "Sequence exposures for narrowband filters (Ha, OIII, SII)",
        .category = "Imaging",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"ha_exposure", json{{"type", "number"},
                                            {"minimum", 0},
                                            {"maximum", 1800}}},
                       {"oiii_exposure", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 1800}}},
                       {"sii_exposure", json{{"type", "number"},
                                             {"minimum", 0},
                                             {"maximum", 1800}}},
                       {"count", json{{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 200}}},
                       {"use_hos", json{{"type", "boolean"}}},
                       {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                       {"offset", json{{"type", "integer"}, {"minimum", 0}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

}  // namespace