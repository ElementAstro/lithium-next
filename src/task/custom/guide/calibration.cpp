#include "calibration.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GuiderCalibrateTask Implementation ====================

GuiderCalibrateTask::GuiderCalibrateTask()
    : Task("GuiderCalibrate",
           [this](const json& params) { performCalibration(params); }) {
    setTaskType("GuiderCalibrate");

    // Set default priority and timeout
    setPriority(8);                         // High priority for calibration
    setTimeout(std::chrono::seconds(180));  // Longer timeout for calibration

    // Add parameter definitions
    addParamDefinition("steps", "integer", false, json(25),
                       "Number of calibration steps");
    addParamDefinition("distance", "number", false, json(25.0),
                       "Calibration distance in pixels");
    addParamDefinition("use_existing", "boolean", false, json(false),
                       "Use existing calibration if available");
    addParamDefinition("clear_existing", "boolean", false, json(false),
                       "Clear existing calibration before starting");
}

void GuiderCalibrateTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting guider calibration");

        // Validate parameters
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to calibrate guider: " + std::string(e.what()));
        throw;
    }
}

void GuiderCalibrateTask::performCalibration(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    int steps = params.value("steps", 25);
    double distance = params.value("distance", 25.0);
    bool use_existing = params.value("use_existing", false);
    bool clear_existing = params.value("clear_existing", false);

    // Validate parameters
    if (steps < 5 || steps > 100) {
        throw std::runtime_error("Calibration steps must be between 5 and 100");
    }

    if (distance < 5.0 || distance > 100.0) {
        throw std::runtime_error(
            "Calibration distance must be between 5.0 and 100.0 pixels");
    }

    if (use_existing && clear_existing) {
        throw std::runtime_error(
            "Cannot use existing and clear existing calibration at the same "
            "time");
    }

    spdlog::info(
        "Starting calibration: steps={}, distance={}px, use_existing={}, "
        "clear_existing={}",
        steps, distance, use_existing, clear_existing);
    addHistoryEntry("Calibration configuration: " + std::to_string(steps) +
                    " steps, " + std::to_string(distance) + "px distance");

    // Clear existing calibration if requested
    if (clear_existing) {
        spdlog::info("Clearing existing calibration");
        addHistoryEntry("Clearing existing calibration data");
        phd2_client.value()->clearCalibration();
    }

    // Check for existing calibration
    if (use_existing && phd2_client.value()->isCalibrated()) {
        spdlog::info("Using existing calibration");
        addHistoryEntry("Using existing calibration data");
        return;
    }

    // Perform calibration (PHD2 handles this automatically when guiding starts)
    spdlog::info(
        "Calibration will be performed automatically when guiding starts");
    addHistoryEntry(
        "Calibration setup completed - will calibrate when guiding starts");
}

std::string GuiderCalibrateTask::taskName() { return "GuiderCalibrate"; }

std::unique_ptr<Task> GuiderCalibrateTask::createEnhancedTask() {
    return std::make_unique<GuiderCalibrateTask>();
}

// ==================== GuiderClearCalibrationTask Implementation
// ====================

GuiderClearCalibrationTask::GuiderClearCalibrationTask()
    : Task("GuiderClearCalibration",
           [this](const json& params) { clearCalibration(params); }) {
    setTaskType("GuiderClearCalibration");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for clearing calibration
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("confirm", "boolean", false, json(false),
                       "Confirm clearing calibration data");
}

void GuiderClearCalibrationTask::execute(const json& params) {
    try {
        addHistoryEntry("Clearing guider calibration");

        // Validate parameters
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to clear calibration: " +
                        std::string(e.what()));
        throw;
    }
}

void GuiderClearCalibrationTask::clearCalibration(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    bool confirm = params.value("confirm", false);

    if (!confirm) {
        throw std::runtime_error(
            "Must confirm clearing calibration by setting 'confirm' parameter "
            "to true");
    }

    spdlog::info("Clearing guider calibration data");
    addHistoryEntry("Clearing all calibration data");

    // Clear calibration using PHD2 client
    phd2_client.value()->clearCalibration();
    spdlog::info("Calibration data cleared successfully");
    addHistoryEntry("Calibration data cleared successfully");
}

std::string GuiderClearCalibrationTask::taskName() {
    return "GuiderClearCalibration";
}

std::unique_ptr<Task> GuiderClearCalibrationTask::createEnhancedTask() {
    return std::make_unique<GuiderClearCalibrationTask>();
}

}  // namespace lithium::task::guide
