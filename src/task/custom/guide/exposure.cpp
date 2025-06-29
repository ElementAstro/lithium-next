#include "exposure.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "client/phd2/types.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GuidedExposureTask Implementation ====================

GuidedExposureTask::GuidedExposureTask()
    : Task("GuidedExposure",
           [this](const json& params) { performGuidedExposure(params); }) {
    setTaskType("GuidedExposure");

    // Set default priority and timeout
    setPriority(7);                         // High priority for guided exposure
    setTimeout(std::chrono::seconds(600));  // 10 minutes for long exposures

    // Add parameter definitions
    addParamDefinition("exposure_time", "number", true, json(60.0),
                       "Exposure time in seconds");
    addParamDefinition("dither_before", "boolean", false, json(false),
                       "Perform dither before exposure");
    addParamDefinition("dither_after", "boolean", false, json(false),
                       "Perform dither after exposure");
    addParamDefinition("dither_amount", "number", false, json(5.0),
                       "Dither amount in pixels");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void GuidedExposureTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting guided exposure");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT(errorMsg);
        }

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform guided exposure: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform guided exposure: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform guided exposure: {}", e.what());
    }
}

void GuidedExposureTask::performGuidedExposure(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    double exposure_time = params.value("exposure_time", 60.0);
    bool dither_before = params.value("dither_before", false);
    bool dither_after = params.value("dither_after", false);
    double dither_amount = params.value("dither_amount", 5.0);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);

    if (exposure_time < 0.1 || exposure_time > 3600.0) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0.1 and 3600.0 seconds (got {})",
            exposure_time);
    }

    if (dither_amount < 1.0 || dither_amount > 50.0) {
        THROW_INVALID_ARGUMENT(
            "Dither amount must be between 1.0 and 50.0 pixels (got {})",
            dither_amount);
    }

    spdlog::info(
        "Starting guided exposure: {}s, dither_before={}, dither_after={}",
        exposure_time, dither_before, dither_after);
    addHistoryEntry("Exposure configuration: " + std::to_string(exposure_time) +
                    "s");

    if (phd2_client.value()->getAppState() != phd2::AppStateType::Guiding) {
        THROW_RUNTIME_ERROR(
            "Guiding is not active. Please start guiding first.");
    }

    if (dither_before) {
        spdlog::info("Performing dither before exposure");
        addHistoryEntry("Dithering before exposure");
        phd2::SettleParams settle_params{settle_tolerance, settle_time};
        auto dither_future =
            phd2_client.value()->dither(dither_amount, false, settle_params);
        if (!dither_future.get()) {
            THROW_RUNTIME_ERROR("Failed to dither before exposure");
        }
    }

    spdlog::info("Starting exposure monitoring for {}s", exposure_time);
    addHistoryEntry("Starting exposure monitoring");

    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::milliseconds(
                                     static_cast<int>(exposure_time * 1000));

    while (std::chrono::steady_clock::now() < end_time) {
        if (phd2_client.value()->getAppState() != phd2::AppStateType::Guiding) {
            THROW_RUNTIME_ERROR("Guiding stopped during exposure");
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    spdlog::info("Exposure completed successfully");
    addHistoryEntry("Exposure completed successfully");

    if (dither_after) {
        spdlog::info("Performing dither after exposure");
        addHistoryEntry("Dithering after exposure");
        phd2::SettleParams settle_params{settle_tolerance, settle_time};
        auto dither_future =
            phd2_client.value()->dither(dither_amount, false, settle_params);
        if (!dither_future.get()) {
            THROW_RUNTIME_ERROR("Failed to dither after exposure");
        }
    }
}

std::string GuidedExposureTask::taskName() { return "GuidedExposure"; }

std::unique_ptr<Task> GuidedExposureTask::createEnhancedTask() {
    return std::make_unique<GuidedExposureTask>();
}

// ==================== AutoGuidingTask Implementation ====================

AutoGuidingTask::AutoGuidingTask()
    : Task("AutoGuiding",
           [this](const json& params) { performAutoGuiding(params); }) {
    setTaskType("AutoGuiding");

    // Set default priority and timeout
    setPriority(8);                          // High priority for auto guiding
    setTimeout(std::chrono::seconds(7200));  // 2 hours for long sessions

    // Add parameter definitions
    addParamDefinition("duration", "number", false, json(3600.0),
                       "Guiding duration in seconds (0 = indefinite)");
    addParamDefinition("exposure_time", "number", false, json(2.0),
                       "Guide exposure time in seconds");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("auto_select_star", "boolean", false, json(true),
                       "Automatically select guide star");
    addParamDefinition("check_interval", "integer", false, json(30),
                       "Status check interval in seconds");
}

void AutoGuidingTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting auto guiding session");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT(errorMsg);
        }

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform auto guiding: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform auto guiding: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform auto guiding: {}", e.what());
    }
}

void AutoGuidingTask::performAutoGuiding(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    double duration = params.value("duration", 3600.0);
    double exposure_time = params.value("exposure_time", 2.0);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    bool auto_select_star = params.value("auto_select_star", true);
    int check_interval = params.value("check_interval", 30);

    if (duration < 0) {
        THROW_INVALID_ARGUMENT("Duration cannot be negative (got {})",
                               duration);
    }

    if (exposure_time < 0.1 || exposure_time > 60.0) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0.1 and 60.0 seconds (got {})",
            exposure_time);
    }

    if (check_interval < 5 || check_interval > 300) {
        THROW_INVALID_ARGUMENT(
            "Check interval must be between 5 and 300 seconds (got {})",
            check_interval);
    }

    spdlog::info(
        "Starting auto guiding session: duration={}s, exposure_time={}s",
        duration, exposure_time);
    addHistoryEntry("Auto guiding configuration: " + std::to_string(duration) +
                    "s duration");

    if (phd2_client.value()->getAppState() != phd2::AppStateType::Guiding) {
        phd2::SettleParams settle_params{settle_tolerance, 10};
        auto guide_future = phd2_client.value()->startGuiding(
            settle_params, false, std::nullopt);
        if (!guide_future.get()) {
            THROW_RUNTIME_ERROR("Failed to start guiding");
        }

        spdlog::info("Guiding started successfully");
        addHistoryEntry("Guiding started successfully");
    } else {
        spdlog::info("Guiding already active, continuing");
        addHistoryEntry("Guiding already active");
    }

    auto start_time = std::chrono::steady_clock::now();
    auto end_time = (duration > 0)
                        ? start_time + std::chrono::milliseconds(
                                           static_cast<int>(duration * 1000))
                        : std::chrono::steady_clock::time_point::max();

    while (std::chrono::steady_clock::now() < end_time) {
        if (phd2_client.value()->getAppState() != phd2::AppStateType::Guiding) {
            THROW_RUNTIME_ERROR("Guiding stopped unexpectedly");
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::steady_clock::now() - start_time)
                           .count();

        if (elapsed % 300 == 0) {
            spdlog::info("Auto guiding running for {}s", elapsed);
            addHistoryEntry("Guiding active for " + std::to_string(elapsed) +
                            "s");
        }

        std::this_thread::sleep_for(std::chrono::seconds(check_interval));
    }

    spdlog::info("Auto guiding session completed");
    addHistoryEntry("Auto guiding session completed successfully");
}

// ==================== GuidedSequenceTask Implementation ====================

GuidedSequenceTask::GuidedSequenceTask()
    : Task("GuidedSequence",
           [this](const json& params) { performGuidedSequence(params); }) {
    setTaskType("GuidedSequence");

    // Set default priority and timeout
    setPriority(6);                          // Medium priority for sequence
    setTimeout(std::chrono::seconds(7200));  // 2 hours for sequences

    // Add parameter definitions
    addParamDefinition("count", "integer", true, json(10),
                       "Number of exposures in sequence");
    addParamDefinition("exposure_time", "number", true, json(60.0),
                       "Exposure time per frame in seconds");
    addParamDefinition("dither_interval", "integer", false, json(5),
                       "Dither every N exposures (0 = no dithering)");
    addParamDefinition("dither_amount", "number", false, json(5.0),
                       "Dither amount in pixels");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void GuidedSequenceTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting guided sequence");

        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT(errorMsg);
        }

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform guided sequence: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform guided sequence: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform guided sequence: {}", e.what());
    }
}

void GuidedSequenceTask::performGuidedSequence(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    int count = params.value("count", 10);
    double exposure_time = params.value("exposure_time", 60.0);
    int dither_interval = params.value("dither_interval", 5);
    double dither_amount = params.value("dither_amount", 5.0);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);

    if (count < 1 || count > 1000) {
        THROW_INVALID_ARGUMENT("Count must be between 1 and 1000 (got {})",
                               count);
    }

    if (exposure_time < 0.1 || exposure_time > 3600.0) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0.1 and 3600.0 seconds (got {})",
            exposure_time);
    }

    if (dither_interval < 0 || dither_interval > count) {
        THROW_INVALID_ARGUMENT(
            "Dither interval must be between 0 and count (got {})",
            dither_interval);
    }

    spdlog::info("Starting guided sequence: {} exposures of {}s each", count,
                 exposure_time);
    addHistoryEntry("Sequence configuration: " + std::to_string(count) + " × " +
                    std::to_string(exposure_time) + "s");

    if (phd2_client.value()->getAppState() != phd2::AppStateType::Guiding) {
        THROW_RUNTIME_ERROR(
            "Guiding is not active. Please start guiding first.");
    }

    for (int i = 0; i < count; ++i) {
        spdlog::info("Starting exposure {}/{}", i + 1, count);
        addHistoryEntry("Starting exposure " + std::to_string(i + 1) + "/" +
                        std::to_string(count));

        if (dither_interval > 0 && i > 0 && (i % dither_interval) == 0) {
            spdlog::info("Performing dither before exposure {}", i + 1);
            addHistoryEntry("Dithering before exposure " +
                            std::to_string(i + 1));

            phd2::SettleParams settle_params{settle_tolerance, settle_time};
            auto dither_future = phd2_client.value()->dither(
                dither_amount, false, settle_params);
            if (!dither_future.get()) {
                THROW_RUNTIME_ERROR("Failed to dither before exposure {}",
                                    i + 1);
            }
        }

        auto start_time = std::chrono::steady_clock::now();
        auto end_time =
            start_time +
            std::chrono::milliseconds(static_cast<int>(exposure_time * 1000));

        while (std::chrono::steady_clock::now() < end_time) {
            if (phd2_client.value()->getAppState() !=
                phd2::AppStateType::Guiding) {
                THROW_RUNTIME_ERROR("Guiding stopped during exposure {}",
                                    i + 1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        spdlog::info("Exposure {}/{} completed", i + 1, count);
        addHistoryEntry("Exposure " + std::to_string(i + 1) +
                        " completed successfully");
    }

    spdlog::info("Guided sequence completed successfully");
    addHistoryEntry("All exposures completed successfully");
}

std::string GuidedSequenceTask::taskName() { return "GuidedSequence"; }

std::unique_ptr<Task> GuidedSequenceTask::createEnhancedTask() {
    return std::make_unique<GuidedSequenceTask>();
}

}  // namespace lithium::task::guide
