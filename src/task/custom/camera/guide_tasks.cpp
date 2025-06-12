// ==================== Includes and Declarations ====================
#include "guide_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <thread>
#include "../factory.hpp"
#include "atom/error/exception.hpp"

namespace lithium::task::task {

// ==================== Mock Guider Class ====================
#ifdef MOCK_CAMERA
class MockGuider {
public:
    MockGuider() = default;

    bool isGuiding() const { return guiding_; }
    void startGuiding() { guiding_ = true; }
    void stopGuiding() { guiding_ = false; }
    void dither(double pixels) {
        spdlog::info("Dithering by {} pixels", pixels);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    bool calibrate() {
        spdlog::info("Calibrating guider");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true;
    }

private:
    bool guiding_{false};
};
#endif

// ==================== GuidedExposureTask Implementation ====================

auto GuidedExposureTask::taskName() -> std::string { return "GuidedExposure"; }

void GuidedExposureTask::execute(const json& params) { executeImpl(params); }

void GuidedExposureTask::executeImpl(const json& params) {
    spdlog::info("Executing GuidedExposure task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double exposureTime = params.at("exposure").get<double>();
        ExposureType type = params.value("type", ExposureType::LIGHT);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);
        bool useGuiding = params.value("guiding", true);

        spdlog::info("Starting guided exposure for {} seconds with guiding {}",
                     exposureTime, useGuiding ? "enabled" : "disabled");

#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif

        if (useGuiding) {
#ifdef MOCK_CAMERA
            if (!guider->isGuiding()) {
                spdlog::info("Starting guiding");
                guider->startGuiding();
                // Wait for guiding to stabilize
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
#endif
        }

        // Simulate exposure execution
        spdlog::info("Taking {} second exposure", exposureTime);
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<long>(exposureTime * 100)));  // Simulated exposure

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("GuidedExposure task completed in {} ms",
                     duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("GuidedExposure task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto GuidedExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            GuidedExposureTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced GuidedExposure task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);  // High priority for guided exposure
    task->setTimeout(std::chrono::seconds(600));  // 10 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void GuidedExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", true, 1.0,
                            "Exposure time in seconds");
    task.addParamDefinition("type", "string", false, "light", "Exposure type");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset value");
    task.addParamDefinition("guiding", "bool", false, true,
                            "Enable autoguiding");
}

void GuidedExposureTask::validateGuidingParameters(const json& params) {
    if (!params.contains("exposure") || !params["exposure"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure parameter");
    }

    double exposure = params["exposure"].get<double>();
    if (exposure <= 0 || exposure > 3600) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0 and 3600 seconds");
    }
}

// ==================== DitherSequenceTask Implementation ====================

auto DitherSequenceTask::taskName() -> std::string { return "DitherSequence"; }

void DitherSequenceTask::execute(const json& params) { executeImpl(params); }

void DitherSequenceTask::executeImpl(const json& params) {
    spdlog::info("Executing DitherSequence task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int count = params.at("count").get<int>();
        double exposure = params.at("exposure").get<double>();
        double ditherPixels = params.value("dither_pixels", 5.0);
        int settleTime = params.value("settle_time", 5);

        spdlog::info(
            "Starting dither sequence with {} exposures, {} pixel dither, {} "
            "second settle",
            count, ditherPixels, settleTime);

#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif

        // Start guiding if not already active
#ifdef MOCK_CAMERA
        if (!guider->isGuiding()) {
            guider->startGuiding();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
#endif

        int totalFrames = 0;

        for (int i = 0; i < count; ++i) {
            spdlog::info("Taking dithered exposure {} of {}", i + 1, count);

            // Dither before each exposure (except first)
            if (i > 0) {
#ifdef MOCK_CAMERA
                spdlog::info("Dithering by {} pixels", ditherPixels);
                guider->dither(ditherPixels);
#endif
                // Wait for settling
                spdlog::info("Waiting {} seconds for guiding to settle",
                             settleTime);
                std::this_thread::sleep_for(std::chrono::seconds(settleTime));
            }

            // Take the exposure - simulate exposure
            spdlog::info("Taking {} second exposure", exposure);
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<long>(exposure * 100)));

            totalFrames++;
            spdlog::info("Dithered exposure {} of {} completed", i + 1, count);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("DitherSequence task completed {} exposures in {} ms",
                     totalFrames, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("DitherSequence task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto DitherSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            DitherSequenceTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced DitherSequence task failed: {}", e.what());
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

void DitherSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("count", "int", true, 1,
                            "Number of dithered exposures");
    task.addParamDefinition("exposure", "double", true, 1.0,
                            "Exposure time per frame");
    task.addParamDefinition("dither_pixels", "double", false, 5.0,
                            "Dither distance in pixels");
    task.addParamDefinition("settle_time", "int", false, 5,
                            "Settling time after dither");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void DitherSequenceTask::validateDitheringParameters(const json& params) {
    if (!params.contains("count") || !params["count"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid count parameter");
    }

    int count = params["count"].get<int>();
    if (count <= 0 || count > 1000) {
        THROW_INVALID_ARGUMENT("Count must be between 1 and 1000");
    }

    if (params.contains("dither_pixels")) {
        double pixels = params["dither_pixels"].get<double>();
        if (pixels < 0 || pixels > 50) {
            THROW_INVALID_ARGUMENT("Dither pixels must be between 0 and 50");
        }
    }
}

// ==================== AutoGuidingTask Implementation ====================

auto AutoGuidingTask::taskName() -> std::string { return "AutoGuiding"; }

void AutoGuidingTask::execute(const json& params) { executeImpl(params); }

void AutoGuidingTask::executeImpl(const json& params) {
    spdlog::info("Executing AutoGuiding task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        bool calibrate = params.value("calibrate", true);
        double tolerance = params.value("tolerance", 1.0);
        int maxAttempts = params.value("max_attempts", 3);

        spdlog::info(
            "Setting up autoguiding with calibration {}, tolerance {} pixels",
            calibrate ? "enabled" : "disabled", tolerance);

#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif

        if (calibrate) {
            spdlog::info("Starting guider calibration");

            for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
                spdlog::info("Calibration attempt {} of {}", attempt,
                             maxAttempts);

#ifdef MOCK_CAMERA
                if (guider->calibrate()) {
                    spdlog::info("Guider calibration successful");
                    break;
                }
#endif

                if (attempt == maxAttempts) {
                    THROW_RUNTIME_ERROR(
                        "Guider calibration failed after {} attempts",
                        maxAttempts);
                }

                spdlog::warn("Calibration attempt {} failed, retrying...",
                             attempt);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }

        // Start guiding
        spdlog::info("Starting autoguiding");
#ifdef MOCK_CAMERA
        guider->startGuiding();
#endif

        // Wait for guiding to stabilize
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("AutoGuiding task completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("AutoGuiding task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto AutoGuidingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            AutoGuidingTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced AutoGuiding task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);                         // Medium-high priority
    task->setTimeout(std::chrono::seconds(300));  // 5 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void AutoGuidingTask::defineParameters(Task& task) {
    task.addParamDefinition("calibrate", "bool", false, true,
                            "Perform calibration before guiding");
    task.addParamDefinition("tolerance", "double", false, 1.0,
                            "Guiding tolerance in pixels");
    task.addParamDefinition("max_attempts", "int", false, 3,
                            "Maximum calibration attempts");
}

void AutoGuidingTask::validateAutoGuidingParameters(const json& params) {
    if (params.contains("tolerance")) {
        double tolerance = params["tolerance"].get<double>();
        if (tolerance < 0.1 || tolerance > 10.0) {
            THROW_INVALID_ARGUMENT(
                "Tolerance must be between 0.1 and 10.0 pixels");
        }
    }

    if (params.contains("max_attempts")) {
        int attempts = params["max_attempts"].get<int>();
        if (attempts < 1 || attempts > 10) {
            THROW_INVALID_ARGUMENT("Max attempts must be between 1 and 10");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register GuidedExposureTask
AUTO_REGISTER_TASK(
    GuidedExposureTask, "GuidedExposure",
    (TaskInfo{
        .name = "GuidedExposure",
        .description = "Exposure with autoguiding support",
        .category = "Guiding",
        .requiredParameters = {"exposure"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"exposure", json{{"type", "number"},
                                         {"minimum", 0},
                                         {"maximum", 3600}}},
                       {"type", json{{"type", "string"}}},
                       {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                       {"offset", json{{"type", "integer"}, {"minimum", 0}}},
                       {"guiding", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register DitherSequenceTask
AUTO_REGISTER_TASK(
    DitherSequenceTask, "DitherSequence",
    (TaskInfo{.name = "DitherSequence",
              .description = "Sequence of exposures with dithering",
              .category = "Guiding",
              .requiredParameters = {"count", "exposure"},
              .parameterSchema =
                  json{
                      {"type", "object"},
                      {"properties",
                       json{{"count", json{{"type", "integer"},
                                           {"minimum", 1},
                                           {"maximum", 1000}}},
                            {"exposure", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 3600}}},
                            {"dither_pixels", json{{"type", "number"},
                                                   {"minimum", 0},
                                                   {"maximum", 50}}},
                            {"settle_time", json{{"type", "integer"},
                                                 {"minimum", 0},
                                                 {"maximum", 60}}},
                            {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                            {"offset",
                             json{{"type", "integer"}, {"minimum", 0}}}}}},
              .version = "1.0.0",
              .dependencies = {}}));

// Register AutoGuidingTask
AUTO_REGISTER_TASK(
    AutoGuidingTask, "AutoGuiding",
    (TaskInfo{
        .name = "AutoGuiding",
        .description = "Start and calibrate autoguiding",
        .category = "Guiding",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties", json{{"calibrate", json{{"type", "boolean"}}},
                                     {"tolerance", json{{"type", "number"},
                                                        {"minimum", 0.1},
                                                        {"maximum", 10.0}}},
                                     {"max_attempts", json{{"type", "integer"},
                                                           {"minimum", 1},
                                                           {"maximum", 10}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
