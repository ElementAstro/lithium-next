#include "deep_sky_sequence_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include "../camera/basic_exposure.hpp"
#include "../camera/common.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto DeepSkySequenceTask::taskName() -> std::string { return "DeepSkySequence"; }

void DeepSkySequenceTask::execute(const json& params) { executeImpl(params); }

void DeepSkySequenceTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing DeepSkySequence task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string targetName = params.value("target_name", "Unknown");
        int totalExposures = params.value("total_exposures", 20);
        double exposureTime = params.value("exposure_time", 300.0);
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"L"});
        bool dithering = params.value("dithering", true);
        int ditherPixels = params.value("dither_pixels", 10);
        double ditherInterval = params.value("dither_interval", 5);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        LOG_F(INFO,
              "Starting deep sky sequence for target '{}' with {} exposures of "
              "{} seconds",
              targetName, totalExposures, exposureTime);

        int exposuresPerFilter = totalExposures / filters.size();
        int remainingExposures = totalExposures % filters.size();

        for (size_t filterIndex = 0; filterIndex < filters.size();
             ++filterIndex) {
            const std::string& filter = filters[filterIndex];
            int exposuresForThisFilter =
                exposuresPerFilter + (filterIndex < remainingExposures ? 1 : 0);

            LOG_F(INFO, "Taking {} exposures with filter {}",
                  exposuresForThisFilter, filter);

            for (int exp = 1; exp <= exposuresForThisFilter; ++exp) {
                if (dithering && exp > 1 &&
                    (exp - 1) % static_cast<int>(ditherInterval) == 0) {
                    LOG_F(INFO, "Performing dither of {} pixels", ditherPixels);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }

                LOG_F(INFO, "Taking exposure {} of {} for filter {}", exp,
                      exposuresForThisFilter, filter);

                json exposureParams = {{"exposure", exposureTime},
                                       {"type", ExposureType::LIGHT},
                                       {"binning", binning},
                                       {"gain", gain},
                                       {"offset", offset}};
                auto exposureTask = TakeExposureTask::createEnhancedTask();
                exposureTask->execute(exposureParams);

                if (exp % 10 == 0) {
                    LOG_F(INFO, "Progress: {}/{} exposures completed for filter {}",
                          exp, exposuresForThisFilter, filter);
                }
            }

            LOG_F(INFO, "Completed all {} exposures for filter {}",
                  exposuresForThisFilter, filter);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "DeepSkySequence task '{}' completed {} exposures in {} ms",
              getName(), totalExposures, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "DeepSkySequence task '{}' failed after {} ms: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void DeepSkySequenceTask::validateDeepSkyParameters(const json& params) {
    if (!params.contains("total_exposures") ||
        !params["total_exposures"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid total_exposures parameter");
    }

    if (!params.contains("exposure_time") ||
        !params["exposure_time"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure_time parameter");
    }

    int totalExposures = params["total_exposures"].get<int>();
    if (totalExposures <= 0 || totalExposures > 1000) {
        THROW_INVALID_ARGUMENT("Total exposures must be between 1 and 1000");
    }

    double exposureTime = params["exposure_time"].get<double>();
    if (exposureTime <= 0 || exposureTime > 3600) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0 and 3600 seconds");
    }

    if (params.contains("dither_pixels")) {
        int pixels = params["dither_pixels"].get<int>();
        if (pixels < 0 || pixels > 100) {
            THROW_INVALID_ARGUMENT("Dither pixels must be between 0 and 100");
        }
    }

    if (params.contains("dither_interval")) {
        double interval = params["dither_interval"].get<double>();
        if (interval <= 0 || interval > 50) {
            THROW_INVALID_ARGUMENT("Dither interval must be between 0 and 50");
        }
    }
}

auto DeepSkySequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<DeepSkySequenceTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced DeepSkySequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(7200));  // 2 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void DeepSkySequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("target_name", "string", false, "Unknown",
                            "Name of the target object");
    task.addParamDefinition("total_exposures", "int", true, 20,
                            "Total number of exposures to take");
    task.addParamDefinition("exposure_time", "double", true, 300.0,
                            "Exposure time in seconds");
    task.addParamDefinition("filters", "array", false, json::array({"L"}),
                            "List of filters to use");
    task.addParamDefinition("dithering", "bool", false, true,
                            "Enable dithering between exposures");
    task.addParamDefinition("dither_pixels", "int", false, 10,
                            "Dither distance in pixels");
    task.addParamDefinition("dither_interval", "double", false, 5.0,
                            "Number of exposures between dithers");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

}  // namespace lithium::task::task
