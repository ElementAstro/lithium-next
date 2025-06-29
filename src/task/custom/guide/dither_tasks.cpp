#include "atom/error/exception.hpp"
#include "dither.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <random>
#include <thread>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "client/phd2/types.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// Helper function to create SettleParams
static phd2::SettleParams createSettleParams(double tolerance, int time,
                                             int timeout = 60) {
    phd2::SettleParams params;
    params.pixels = tolerance;
    params.time = time;
    params.timeout = timeout;
    return params;
}

// ==================== GuiderDitherTask Implementation ====================

GuiderDitherTask::GuiderDitherTask()
    : Task("GuiderDither",
           [this](const json& params) { performDither(params); }) {
    setTaskType("GuiderDither");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for dithering
    setTimeout(std::chrono::seconds(60));

    // Add parameter definitions
    addParamDefinition("amount", "number", false, json(5.0),
                       "Dither amount in pixels");
    addParamDefinition("ra_only", "boolean", false, json(false),
                       "Dither only in RA direction");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void GuiderDitherTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting dither operation");

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
        addHistoryEntry("Failed to perform dither: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform dither: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform dither: {}", e.what());
    }
}

void GuiderDitherTask::performDither(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    double amount = params.value("amount", 5.0);
    bool ra_only = params.value("ra_only", false);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);

    if (amount < 1.0 || amount > 50.0) {
        THROW_INVALID_ARGUMENT(
            "Dither amount must be between 1.0 and 50.0 pixels (got {})",
            amount);
    }

    if (settle_tolerance < 0.1 || settle_tolerance > 10.0) {
        THROW_INVALID_ARGUMENT(
            "Settle tolerance must be between 0.1 and 10.0 pixels (got {})",
            settle_tolerance);
    }

    if (settle_time < 1 || settle_time > 300) {
        THROW_INVALID_ARGUMENT(
            "Settle time must be between 1 and 300 seconds (got {})",
            settle_time);
    }

    spdlog::info(
        "Performing dither: amount={}px, ra_only={}, settle_tolerance={}px, "
        "settle_time={}s",
        amount, ra_only, settle_tolerance, settle_time);
    addHistoryEntry("Dither configuration: amount=" + std::to_string(amount) +
                    "px, RA only=" + (ra_only ? "yes" : "no"));

    auto settle_params = createSettleParams(settle_tolerance, settle_time);
    auto future = phd2_client.value()->dither(amount, ra_only, settle_params);
    if (!future.get()) {
        THROW_RUNTIME_ERROR("Failed to perform dither");
    }

    spdlog::info("Dither completed successfully");
    addHistoryEntry("Dither operation completed successfully");
}

std::string GuiderDitherTask::taskName() { return "GuiderDither"; }

std::unique_ptr<Task> GuiderDitherTask::createEnhancedTask() {
    return std::make_unique<GuiderDitherTask>();
}

// ==================== DitherSequenceTask Implementation ====================

DitherSequenceTask::DitherSequenceTask()
    : Task("DitherSequence",
           [this](const json& params) { performDitherSequence(params); }) {
    setTaskType("DitherSequence");

    // Set default priority and timeout
    setPriority(5);                         // Medium priority for sequence
    setTimeout(std::chrono::seconds(300));  // Longer timeout for sequence

    // Add parameter definitions
    addParamDefinition("count", "integer", true, json(5),
                       "Number of dithers to perform");
    addParamDefinition("amount", "number", false, json(5.0),
                       "Dither amount in pixels");
    addParamDefinition("interval", "integer", false, json(30),
                       "Interval between dithers in seconds");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void DitherSequenceTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting dither sequence");

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
        addHistoryEntry("Failed to perform dither sequence: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform dither sequence: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform dither sequence: {}", e.what());
    }
}

void DitherSequenceTask::performDitherSequence(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    int count = params.value("count", 5);
    double amount = params.value("amount", 5.0);
    int interval = params.value("interval", 30);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);

    if (count < 1 || count > 100) {
        THROW_INVALID_ARGUMENT(
            "Dither count must be between 1 and 100 (got {})", count);
    }

    if (amount < 1.0 || amount > 50.0) {
        THROW_INVALID_ARGUMENT(
            "Dither amount must be between 1.0 and 50.0 pixels (got {})",
            amount);
    }

    if (interval < 5 || interval > 3600) {
        THROW_INVALID_ARGUMENT(
            "Interval must be between 5 and 3600 seconds (got {})", interval);
    }

    spdlog::info(
        "Starting dither sequence: count={}, amount={}px, interval={}s", count,
        amount, interval);
    addHistoryEntry("Sequence configuration: " + std::to_string(count) +
                    " dithers, " + std::to_string(amount) + "px amount, " +
                    std::to_string(interval) + "s interval");

    for (int i = 0; i < count; ++i) {
        spdlog::info("Performing dither {}/{}", i + 1, count);
        addHistoryEntry("Performing dither " + std::to_string(i + 1) + "/" +
                        std::to_string(count));

        auto settle_params = createSettleParams(settle_tolerance, settle_time);
        auto future = phd2_client.value()->dither(amount, false, settle_params);
        if (!future.get()) {
            THROW_RUNTIME_ERROR("Failed to perform dither {}", i + 1);
        }

        addHistoryEntry("Dither " + std::to_string(i + 1) +
                        " completed successfully");

        if (i < count - 1) {
            spdlog::info("Waiting {}s before next dither", interval);
            addHistoryEntry("Waiting " + std::to_string(interval) +
                            "s before next dither");
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
    }

    spdlog::info("Dither sequence completed successfully");
    addHistoryEntry("All dithers completed successfully");
}

// ==================== GuiderRandomDitherTask Implementation
// ====================

GuiderRandomDitherTask::GuiderRandomDitherTask()
    : Task("GuiderRandomDither",
           [this](const json& params) { performRandomDither(params); }) {
    setTaskType("GuiderRandomDither");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for random dithering
    setTimeout(std::chrono::seconds(60));

    // Add parameter definitions
    addParamDefinition("min_amount", "number", false, json(2.0),
                       "Minimum dither amount in pixels");
    addParamDefinition("max_amount", "number", false, json(10.0),
                       "Maximum dither amount in pixels");
    addParamDefinition("settle_tolerance", "number", false, json(2.0),
                       "Settling tolerance in pixels");
    addParamDefinition("settle_time", "integer", false, json(10),
                       "Minimum settle time in seconds");
}

void GuiderRandomDitherTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting random dither operation");

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
        addHistoryEntry("Failed to perform random dither: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to perform random dither: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to perform random dither: {}", e.what());
    }
}

void GuiderRandomDitherTask::performRandomDither(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    double min_amount = params.value("min_amount", 2.0);
    double max_amount = params.value("max_amount", 10.0);
    double settle_tolerance = params.value("settle_tolerance", 2.0);
    int settle_time = params.value("settle_time", 10);

    if (min_amount < 1.0 || min_amount > 50.0) {
        THROW_INVALID_ARGUMENT(
            "Min amount must be between 1.0 and 50.0 pixels (got {})",
            min_amount);
    }

    if (max_amount < 1.0 || max_amount > 50.0) {
        THROW_INVALID_ARGUMENT(
            "Max amount must be between 1.0 and 50.0 pixels (got {})",
            max_amount);
    }

    if (min_amount >= max_amount) {
        THROW_INVALID_ARGUMENT(
            "Min amount must be less than max amount ({} >= {})", min_amount,
            max_amount);
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min_amount, max_amount);
    double amount = dis(gen);

    spdlog::info(
        "Performing random dither: amount={}px (range: {}-{}px), "
        "settle_tolerance={}px, settle_time={}s",
        amount, min_amount, max_amount, settle_tolerance, settle_time);
    addHistoryEntry("Random dither: amount=" + std::to_string(amount) +
                    "px (range: " + std::to_string(min_amount) + "-" +
                    std::to_string(max_amount) + "px)");

    auto settle_params = createSettleParams(settle_tolerance, settle_time);
    auto future = phd2_client.value()->dither(amount, false, settle_params);
    if (!future.get()) {
        THROW_RUNTIME_ERROR("Failed to perform random dither");
    }

    spdlog::info("Random dither completed successfully");
    addHistoryEntry("Random dither operation completed successfully");
}

std::string GuiderRandomDitherTask::taskName() { return "GuiderRandomDither"; }

std::unique_ptr<Task> GuiderRandomDitherTask::createEnhancedTask() {
    return std::make_unique<GuiderRandomDitherTask>();
}

}  // namespace lithium::task::guide
