#include "timelapse_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include "../camera/basic_exposure.hpp"
#include "../camera/common.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto TimelapseTask::taskName() -> std::string { return "Timelapse"; }

void TimelapseTask::execute(const json& params) { executeImpl(params); }

void TimelapseTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing Timelapse task '{}' with params: {}", getName(),
          params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int totalFrames = params.value("total_frames", 100);
        double interval = params.value("interval", 30.0);
        double exposureTime = params.value("exposure_time", 10.0);
        std::string timelapseType = params.value("type", "sunset");
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);
        bool autoExposure = params.value("auto_exposure", false);

        LOG_F(INFO,
              "Starting {} timelapse with {} frames at {} second intervals",
              timelapseType, totalFrames, interval);

        for (int frame = 1; frame <= totalFrames; ++frame) {
            auto frameStartTime = std::chrono::steady_clock::now();

            LOG_F(INFO, "Capturing timelapse frame {} of {}", frame,
                  totalFrames);

            double currentExposure = exposureTime;
            if (autoExposure && timelapseType == "sunset") {
                // Gradually increase exposure as it gets darker
                double progress = static_cast<double>(frame) / totalFrames;
                currentExposure = exposureTime * (1.0 + progress * 2.0);
            }

            json exposureParams = {{"exposure", currentExposure},
                                   {"type", ExposureType::LIGHT},
                                   {"binning", binning},
                                   {"gain", gain},
                                   {"offset", offset}};
            auto exposureTask = TakeExposureTask::createEnhancedTask();
            exposureTask->execute(exposureParams);

            auto frameEndTime = std::chrono::steady_clock::now();
            auto frameElapsed =
                std::chrono::duration_cast<std::chrono::seconds>(
                    frameEndTime - frameStartTime);
            auto remainingTime =
                std::chrono::seconds(static_cast<int>(interval)) - frameElapsed;

            if (remainingTime.count() > 0 && frame < totalFrames) {
                LOG_F(INFO, "Waiting {} seconds until next frame",
                      remainingTime.count());
                std::this_thread::sleep_for(remainingTime);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "Timelapse task '{}' completed {} frames in {} ms",
              getName(), totalFrames, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "Timelapse task '{}' failed after {} ms: {}", getName(),
              duration.count(), e.what());
        throw;
    }
}

void TimelapseTask::validateTimelapseParameters(const json& params) {
    if (!params.contains("total_frames") ||
        !params["total_frames"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid total_frames parameter");
    }

    if (!params.contains("interval") || !params["interval"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid interval parameter");
    }

    int totalFrames = params["total_frames"].get<int>();
    if (totalFrames <= 0 || totalFrames > 10000) {
        THROW_INVALID_ARGUMENT("Total frames must be between 1 and 10000");
    }

    double interval = params["interval"].get<double>();
    if (interval <= 0 || interval > 3600) {
        THROW_INVALID_ARGUMENT("Interval must be between 0 and 3600 seconds");
    }

    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"].get<double>();
        if (exposure <= 0 || exposure > interval) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be positive and less than interval");
        }
    }
}

auto TimelapseTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<TimelapseTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced Timelapse task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(5);
    task->setTimeout(std::chrono::seconds(36000));  // 10 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void TimelapseTask::defineParameters(Task& task) {
    task.addParamDefinition("total_frames", "int", true, 100,
                            "Total number of frames to capture");
    task.addParamDefinition("interval", "double", true, 30.0,
                            "Interval between frames in seconds");
    task.addParamDefinition("exposure_time", "double", false, 10.0,
                            "Exposure time in seconds");
    task.addParamDefinition("type", "string", false, "sunset",
                            "Type of timelapse (sunset, lunar, star_trails)");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
    task.addParamDefinition("auto_exposure", "bool", false, false,
                            "Enable automatic exposure adjustment");
}

}  // namespace lithium::task::task
