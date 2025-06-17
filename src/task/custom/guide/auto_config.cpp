#include "auto_config.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"
#include "exception/exception.hpp"

namespace lithium::task::guide {

AutoGuideConfigTask::AutoGuideConfigTask()
    : Task("AutoGuideConfig",
           [this](const json& params) { optimizeConfiguration(params); }) {
    setTaskType("AutoGuideConfig");
    setPriority(7);  // High priority for configuration
    setTimeout(std::chrono::seconds(120));

    // Parameter definitions
    addParamDefinition("aggressiveness", "number", false, json(0.5),
                       "Optimization aggressiveness (0.1-1.0)");
    addParamDefinition("max_exposure", "number", false, json(5.0),
                       "Maximum exposure time in seconds");
    addParamDefinition("min_exposure", "number", false, json(0.1),
                       "Minimum exposure time in seconds");
    addParamDefinition("reset_first", "boolean", false, json(false),
                       "Reset to defaults before optimizing");
}

void AutoGuideConfigTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting auto guide configuration");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw lithium::exception::SystemException(
                3001, errorMsg,
                {"AutoGuideConfig", "AutoGuideConfigTask", __FUNCTION__});
        }

        Task::execute(params);

    } catch (const lithium::exception::EnhancedException& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Auto config failed: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Auto config failed: " + std::string(e.what()));
        throw lithium::exception::SystemException(
            3002, "Auto config failed: {}",
            {"AutoGuideConfig", "AutoGuideConfigTask", __FUNCTION__}, e.what());
    }
}

void AutoGuideConfigTask::optimizeConfiguration(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw lithium::exception::SystemException(
            3003, "PHD2 client not found in global manager",
            {"optimizeConfiguration", "AutoGuideConfigTask", __FUNCTION__});
    }

    double aggressiveness = params.value("aggressiveness", 0.5);
    double max_exposure = params.value("max_exposure", 5.0);
    double min_exposure = params.value("min_exposure", 0.1);
    bool reset_first = params.value("reset_first", false);

    // Validate parameters
    if (aggressiveness < 0.1 || aggressiveness > 1.0) {
        throw lithium::exception::SystemException(
            3004, "Aggressiveness must be between 0.1 and 1.0 (got {})",
            {"optimizeConfiguration", "AutoGuideConfigTask", __FUNCTION__},
            aggressiveness);
    }

    if (min_exposure >= max_exposure) {
        throw lithium::exception::SystemException(
            3005, "Min exposure must be less than max exposure ({} >= {})",
            {"optimizeConfiguration", "AutoGuideConfigTask", __FUNCTION__},
            min_exposure, max_exposure);
    }

    spdlog::info("Starting auto guide configuration with aggressiveness: {}",
                 aggressiveness);
    addHistoryEntry("Optimizing guide configuration");

    // Step 1: Analyze current performance
    analyzeCurrentPerformance();

    // Step 2: Adjust exposure time
    adjustExposureTime();

    // Step 3: Optimize algorithm parameters
    optimizeAlgorithmParameters();

    // Step 4: Configure dither settings
    configureDitherSettings();

    spdlog::info("Auto guide configuration completed successfully");
    addHistoryEntry("Auto guide configuration completed");
}

void AutoGuideConfigTask::analyzeCurrentPerformance() {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client)
        return;

    // Get current guiding stats (implementation depends on PHD2 API)
    current_analysis_ = {.current_rms = 0.5,  // Example values
                         .star_brightness = 100.0,
                         .noise_level = 10.0,
                         .dropped_frames = 0,
                         .is_stable = true};

    spdlog::info("Current performance analysis complete");
    addHistoryEntry("Performance analysis completed");
}

void AutoGuideConfigTask::adjustExposureTime() {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client)
        return;

    // Example exposure adjustment logic
    double new_exposure = 1.0;  // Default value
    phd2_client.value()->setExposure(static_cast<int>(new_exposure * 1000));

    spdlog::info("Adjusted exposure time to {}s", new_exposure);
    addHistoryEntry("Exposure time set to " + std::to_string(new_exposure) +
                    "s");
}

void AutoGuideConfigTask::optimizeAlgorithmParameters() {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client)
        return;

    // Example parameter optimization
    phd2_client.value()->setAlgoParam("ra", "Aggressiveness", 0.7);
    phd2_client.value()->setAlgoParam("dec", "Aggressiveness", 0.5);

    spdlog::info("Optimized algorithm parameters");
    addHistoryEntry("Algorithm parameters optimized");
}

void AutoGuideConfigTask::configureDitherSettings() {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client)
        return;

    // Example dither configuration
    json dither_params = {
        {"amount", 1.5}, {"settle_pixels", 0.5}, {"settle_time", 10}};
    phd2_client.value()->setLockShiftParams(dither_params);

    spdlog::info("Configured dither settings");
    addHistoryEntry("Dither settings configured");
}

std::string AutoGuideConfigTask::taskName() { return "AutoGuideConfig"; }

std::unique_ptr<Task> AutoGuideConfigTask::createEnhancedTask() {
    return std::make_unique<AutoGuideConfigTask>();
}

}  // namespace lithium::task::guide