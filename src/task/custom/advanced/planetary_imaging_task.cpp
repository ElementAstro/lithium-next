#include "planetary_imaging_task.hpp"
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

PlanetaryImagingTask::PlanetaryImagingTask()
    : Task("PlanetaryImaging",
           [this](const json& params) { this->executeImpl(params); }) {}

auto PlanetaryImagingTask::taskName() -> std::string { return "PlanetaryImaging"; }

void PlanetaryImagingTask::execute(const json& params) { executeImpl(params); }

void PlanetaryImagingTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing PlanetaryImaging task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string planet = params.value("planet", "Mars");
        int videoLength = params.value("video_length", 120);
        double frameRate = params.value("frame_rate", 30.0);
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"R", "G", "B"});
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 400);
        int offset = params.value("offset", 10);
        bool highSpeed = params.value("high_speed", true);

        LOG_F(INFO, "Starting planetary imaging of {} for {} seconds at {} fps",
              planet, videoLength, frameRate);

        double frameExposure = 1.0 / frameRate;
        int totalFrames = static_cast<int>(videoLength * frameRate);

        for (const std::string& filter : filters) {
            LOG_F(INFO,
                  "Recording {} frames with filter {} at {} second exposures",
                  totalFrames, filter, frameExposure);

            for (int frame = 1; frame <= totalFrames; ++frame) {
                json exposureParams = {{"exposure", frameExposure},
                                       {"type", ExposureType::LIGHT},
                                       {"binning", binning},
                                       {"gain", gain},
                                       {"offset", offset}};
                auto exposureTask = TakeExposureTask::createEnhancedTask();
                exposureTask->execute(exposureParams);

                if (frame % 100 == 0) {
                    LOG_F(INFO, "Progress: {}/{} frames completed for filter {}",
                          frame, totalFrames, filter);
                }
            }

            LOG_F(INFO, "Completed {} frames for filter {}", totalFrames,
                  filter);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO,
              "PlanetaryImaging task '{}' completed {} total frames in {} ms",
              getName(), totalFrames * filters.size(), duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "PlanetaryImaging task '{}' failed after {} ms: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void PlanetaryImagingTask::validatePlanetaryParameters(const json& params) {
    if (!params.contains("video_length") ||
        !params["video_length"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid video_length parameter");
    }

    int videoLength = params["video_length"].get<int>();
    if (videoLength <= 0 || videoLength > 1800) {
        THROW_INVALID_ARGUMENT(
            "Video length must be between 1 and 1800 seconds");
    }

    if (params.contains("frame_rate")) {
        double frameRate = params["frame_rate"].get<double>();
        if (frameRate <= 0 || frameRate > 120) {
            THROW_INVALID_ARGUMENT("Frame rate must be between 0 and 120 fps");
        }
    }
}

auto PlanetaryImagingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<PlanetaryImagingTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced PlanetaryImaging task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void PlanetaryImagingTask::defineParameters(Task& task) {
    task.addParamDefinition("planet", "string", false, "Mars",
                            "Name of the planet being imaged");
    task.addParamDefinition("video_length", "int", true, 120,
                            "Length of video in seconds");
    task.addParamDefinition("frame_rate", "double", false, 30.0,
                            "Frame rate in frames per second");
    task.addParamDefinition("filters", "array", false, json::array({"R", "G", "B"}),
                            "List of filters to use");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 400, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
    task.addParamDefinition("high_speed", "bool", false, true,
                            "Enable high-speed mode");
}

}  // namespace lithium::task::task
