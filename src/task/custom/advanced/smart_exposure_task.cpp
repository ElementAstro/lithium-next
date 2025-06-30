#include "smart_exposure_task.hpp"
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

auto SmartExposureTask::taskName() -> std::string { return "SmartExposure"; }

void SmartExposureTask::execute(const json& params) { executeImpl(params); }

void SmartExposureTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing SmartExposure task '{}' with params: {}", getName(),
          params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double targetSNR = params.value("target_snr", 50.0);
        double maxExposure = params.value("max_exposure", 300.0);
        double minExposure = params.value("min_exposure", 1.0);
        int maxAttempts = params.value("max_attempts", 5);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        LOG_F(INFO,
              "Starting smart exposure targeting SNR {} with max exposure {} "
              "seconds",
              targetSNR, maxExposure);

        double currentExposure = (maxExposure + minExposure) / 2.0;
        double achievedSNR = 0.0;

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            LOG_F(INFO, "Smart exposure attempt {} with {} seconds", attempt,
                  currentExposure);

            // Take test exposure
            json exposureParams = {{"exposure", currentExposure},
                                   {"type", ExposureType::LIGHT},
                                   {"binning", binning},
                                   {"gain", gain},
                                   {"offset", offset}};

            // Create and execute TakeExposureTask
            auto exposureTask = TakeExposureTask::createEnhancedTask();
            exposureTask->execute(exposureParams);

            // In a real implementation, we would analyze the image for SNR
            achievedSNR =
                std::min(targetSNR * 1.2, currentExposure * 0.5 + 20.0);

            LOG_F(INFO, "Achieved SNR: {:.2f}, Target: {:.2f}", achievedSNR,
                  targetSNR);

            if (std::abs(achievedSNR - targetSNR) <= targetSNR * 0.1) {
                LOG_F(INFO, "Target SNR achieved within 10% tolerance");
                break;
            }

            if (attempt < maxAttempts) {
                double ratio = targetSNR / achievedSNR;
                currentExposure = std::clamp(currentExposure * ratio * ratio,
                                             minExposure, maxExposure);
                LOG_F(INFO, "Adjusting exposure to {} seconds for next attempt",
                      currentExposure);
            }
        }

        // Take final exposure with optimal settings
        LOG_F(INFO, "Taking final smart exposure with {} seconds",
              currentExposure);
        json finalParams = {{"exposure", currentExposure},
                            {"type", ExposureType::LIGHT},
                            {"binning", binning},
                            {"gain", gain},
                            {"offset", offset}};
        auto finalTask = TakeExposureTask::createEnhancedTask();
        finalTask->execute(finalParams);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(
            INFO,
            "SmartExposure task '{}' completed in {} ms with final SNR {:.2f}",
            getName(), duration.count(), achievedSNR);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "SmartExposure task '{}' failed after {} ms: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void SmartExposureTask::validateSmartExposureParameters(const json& params) {
    if (params.contains("target_snr")) {
        double snr = params["target_snr"].get<double>();
        if (snr <= 0 || snr > 1000) {
            THROW_INVALID_ARGUMENT("Target SNR must be between 0 and 1000");
        }
    }

    if (params.contains("max_exposure")) {
        double exposure = params["max_exposure"].get<double>();
        if (exposure <= 0 || exposure > 3600) {
            THROW_INVALID_ARGUMENT(
                "Max exposure must be between 0 and 3600 seconds");
        }
    }

    if (params.contains("min_exposure")) {
        double exposure = params["min_exposure"].get<double>();
        if (exposure <= 0 || exposure > 300) {
            THROW_INVALID_ARGUMENT(
                "Min exposure must be between 0 and 300 seconds");
        }
    }

    if (params.contains("max_attempts")) {
        int attempts = params["max_attempts"].get<int>();
        if (attempts <= 0 || attempts > 20) {
            THROW_INVALID_ARGUMENT("Max attempts must be between 1 and 20");
        }
    }
}

auto SmartExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<SmartExposureTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced SmartExposure task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(1800));  // 30 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void SmartExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("target_snr", "double", true, 50.0,
                            "Target signal-to-noise ratio");
    task.addParamDefinition("max_exposure", "double", false, 300.0,
                            "Maximum exposure time in seconds");
    task.addParamDefinition("min_exposure", "double", false, 1.0,
                            "Minimum exposure time in seconds");
    task.addParamDefinition("max_attempts", "int", false, 5,
                            "Maximum optimization attempts");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

}  // namespace lithium::task::task
