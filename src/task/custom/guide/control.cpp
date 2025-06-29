#include "control.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "client/phd2/types.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

static phd2::SettleParams createSettleParams(double tolerance, int time,
                                             int timeout = 60) {
    phd2::SettleParams params;
    params.pixels = tolerance;
    params.time = time;
    params.timeout = timeout;
    return params;
}

GuiderStartTask::GuiderStartTask()
    : Task("GuiderStart",
           [this](const json& params) { startGuiding(params); }) {
    setTaskType("GuiderStart");
    setPriority(8);
    setTimeout(std::chrono::seconds(60));
    addParamDefinition("auto_select_star", "boolean", false, json(true),
                       "Automatically select guide star");
    addParamDefinition("exposure_time", "number", false, json(2.0),
                       "Guide exposure time in seconds");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void GuiderStartTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting autoguiding");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }
        Task::execute(params);
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to start guiding: " + std::string(e.what()));
        throw;
    }
}

void GuiderStartTask::startGuiding(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }
    bool auto_select_star = params.value("auto_select_star", true);
    double exposure_time = params.value("exposure_time", 2.0);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);
    if (exposure_time < 0.1 || exposure_time > 60.0) {
        throw std::runtime_error(
            "Exposure time must be between 0.1 and 60.0 seconds");
    }
    if (settle_tolerance < 0.1 || settle_tolerance > 10.0) {
        throw std::runtime_error(
            "Settle tolerance must be between 0.1 and 10.0 pixels");
    }
    if (settle_time < 1 || settle_time > 300) {
        throw std::runtime_error(
            "Settle time must be between 1 and 300 seconds");
    }
    spdlog::info(
        "Starting guiding with exposure_time={}s, auto_select_star={}, "
        "settle_tolerance={}, settle_time={}s",
        exposure_time, auto_select_star, settle_tolerance, settle_time);
    addHistoryEntry("Configuration: exposure=" + std::to_string(exposure_time) +
                    "s, auto_select=" + (auto_select_star ? "yes" : "no"));
    if (auto_select_star) {
        try {
            auto star_pos = phd2_client.value()->findStar();
            spdlog::info("Guide star automatically selected at ({}, {})",
                         star_pos[0], star_pos[1]);
            addHistoryEntry("Guide star automatically selected");
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to auto-select guide star: " +
                                     std::string(e.what()));
        }
    }
    auto settle_params = createSettleParams(settle_tolerance, settle_time);
    auto future = phd2_client.value()->startGuiding(settle_params);
    if (future.get()) {
        spdlog::info("Guiding started successfully");
        addHistoryEntry("Autoguiding started successfully");
    } else {
        throw std::runtime_error("Failed to start guiding");
    }
}

std::string GuiderStartTask::taskName() { return "GuiderStart"; }

std::unique_ptr<Task> GuiderStartTask::createEnhancedTask() {
    return std::make_unique<GuiderStartTask>();
}

GuiderStopTask::GuiderStopTask()
    : Task("GuiderStop", [this](const json& params) { stopGuiding(params); }) {
    setTaskType("GuiderStop");
    setPriority(7);
    setTimeout(std::chrono::seconds(30));
    addParamDefinition("force", "boolean", false, json(false),
                       "Force stop even if calibration is in progress");
}

void GuiderStopTask::execute(const json& params) {
    try {
        addHistoryEntry("Stopping autoguiding");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }
        Task::execute(params);
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to stop guiding: " + std::string(e.what()));
        throw;
    }
}

void GuiderStopTask::stopGuiding(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }
    bool force = params.value("force", false);
    spdlog::info("Stopping guiding (force={})", force);
    addHistoryEntry("Stopping guiding" + std::string(force ? " (forced)" : ""));
    phd2_client.value()->stopCapture();
    spdlog::info("Guiding stopped successfully");
    addHistoryEntry("Autoguiding stopped successfully");
}

std::string GuiderStopTask::taskName() { return "GuiderStop"; }

std::unique_ptr<Task> GuiderStopTask::createEnhancedTask() {
    return std::make_unique<GuiderStopTask>();
}

GuiderPauseTask::GuiderPauseTask()
    : Task("GuiderPause",
           [this](const json& params) { pauseGuiding(params); }) {
    setTaskType("GuiderPause");
    setPriority(6);
    setTimeout(std::chrono::seconds(10));
}

void GuiderPauseTask::execute(const json& params) {
    try {
        addHistoryEntry("Pausing autoguiding");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }
        Task::execute(params);
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to pause guiding: " + std::string(e.what()));
        throw;
    }
}

void GuiderPauseTask::pauseGuiding(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }
    spdlog::info("Pausing guiding");
    addHistoryEntry("Pausing guiding");
    phd2_client.value()->setPaused(true);
    spdlog::info("Guiding paused successfully");
    addHistoryEntry("Autoguiding paused successfully");
}

std::string GuiderPauseTask::taskName() { return "GuiderPause"; }

std::unique_ptr<Task> GuiderPauseTask::createEnhancedTask() {
    return std::make_unique<GuiderPauseTask>();
}

GuiderResumeTask::GuiderResumeTask()
    : Task("GuiderResume",
           [this](const json& params) { resumeGuiding(params); }) {
    setTaskType("GuiderResume");
    setPriority(6);
    setTimeout(std::chrono::seconds(10));
}

void GuiderResumeTask::execute(const json& params) {
    try {
        addHistoryEntry("Resuming autoguiding");
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }
        Task::execute(params);
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to resume guiding: " + std::string(e.what()));
        throw;
    }
}

void GuiderResumeTask::resumeGuiding(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }
    spdlog::info("Resuming guiding");
    addHistoryEntry("Resuming guiding");
    phd2_client.value()->setPaused(false);
    spdlog::info("Guiding resumed successfully");
    addHistoryEntry("Autoguiding resumed successfully");
}

std::string GuiderResumeTask::taskName() { return "GuiderResume"; }

std::unique_ptr<Task> GuiderResumeTask::createEnhancedTask() {
    return std::make_unique<GuiderResumeTask>();
}

}  // namespace lithium::task::guide
