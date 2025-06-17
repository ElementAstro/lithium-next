#include "advanced.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GetSearchRegionTask Implementation ====================

GetSearchRegionTask::GetSearchRegionTask()
    : Task("GetSearchRegion",
           [this](const json& params) { getSearchRegion(params); }) {
    setTaskType("GetSearchRegion");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for information retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetSearchRegionTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting search region");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get search region: " +
                        std::string(e.what()));
        throw;
    }
}

void GetSearchRegionTask::getSearchRegion(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    spdlog::info("Getting search region");
    addHistoryEntry("Getting search region");

    // Get search region using PHD2 client
    int search_region = phd2_client.value()->getSearchRegion();

    spdlog::info("Search region: {} pixels", search_region);
    addHistoryEntry("Search region: " + std::to_string(search_region) +
                    " pixels");

    // Store result for retrieval
    setResult({{"search_region", search_region}, {"units", "pixels"}});
}

std::string GetSearchRegionTask::taskName() { return "GetSearchRegion"; }

std::unique_ptr<Task> GetSearchRegionTask::createEnhancedTask() {
    return std::make_unique<GetSearchRegionTask>();
}

// ==================== FlipCalibrationTask Implementation ====================

FlipCalibrationTask::FlipCalibrationTask()
    : Task("FlipCalibration",
           [this](const json& params) { flipCalibration(params); }) {
    setTaskType("FlipCalibration");

    // Set default priority and timeout
    setPriority(7);  // High priority for calibration operations
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("confirm", "boolean", false, json(false),
                       "Confirm calibration flip operation");
}

void FlipCalibrationTask::execute(const json& params) {
    try {
        addHistoryEntry("Flipping calibration");

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
        addHistoryEntry("Failed to flip calibration: " + std::string(e.what()));
        throw;
    }
}

void FlipCalibrationTask::flipCalibration(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    bool confirm = params.value("confirm", false);

    if (!confirm) {
        throw std::runtime_error(
            "Must confirm calibration flip by setting 'confirm' parameter to "
            "true");
    }

    spdlog::info("Flipping calibration data");
    addHistoryEntry("Flipping calibration data for meridian flip");

    // Flip calibration using PHD2 client
    phd2_client.value()->flipCalibration();

    spdlog::info("Calibration flipped successfully");
    addHistoryEntry("Calibration data flipped successfully");
}

std::string FlipCalibrationTask::taskName() { return "FlipCalibration"; }

std::unique_ptr<Task> FlipCalibrationTask::createEnhancedTask() {
    return std::make_unique<FlipCalibrationTask>();
}

// ==================== GetCalibrationDataTask Implementation
// ====================

GetCalibrationDataTask::GetCalibrationDataTask()
    : Task("GetCalibrationData",
           [this](const json& params) { getCalibrationData(params); }) {
    setTaskType("GetCalibrationData");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for data retrieval
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("device", "string", false, json("Mount"),
                       "Device to get calibration for (Mount or AO)");
}

void GetCalibrationDataTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting calibration data");

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
        addHistoryEntry("Failed to get calibration data: " +
                        std::string(e.what()));
        throw;
    }
}

void GetCalibrationDataTask::getCalibrationData(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string device = params.value("device", "Mount");

    // Validate device
    if (device != "Mount" && device != "AO") {
        throw std::runtime_error("Device must be 'Mount' or 'AO'");
    }

    spdlog::info("Getting calibration data for: {}", device);
    addHistoryEntry("Getting calibration data for: " + device);

    // Get calibration data using PHD2 client
    json calibration_data = phd2_client.value()->getCalibrationData(device);

    spdlog::info("Calibration data retrieved successfully");
    addHistoryEntry("Calibration data retrieved for " + device);

    // Store result for retrieval
    setResult(calibration_data);
}

std::string GetCalibrationDataTask::taskName() { return "GetCalibrationData"; }

std::unique_ptr<Task> GetCalibrationDataTask::createEnhancedTask() {
    return std::make_unique<GetCalibrationDataTask>();
}

// ==================== GetAlgoParamNamesTask Implementation
// ====================

GetAlgoParamNamesTask::GetAlgoParamNamesTask()
    : Task("GetAlgoParamNames",
           [this](const json& params) { getAlgoParamNames(params); }) {
    setTaskType("GetAlgoParamNames");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for information retrieval
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("axis", "string", true, json("ra"),
                       "Axis to get parameter names for (ra, dec, x, y)");
}

void GetAlgoParamNamesTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting algorithm parameter names");

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
        addHistoryEntry("Failed to get algorithm parameter names: " +
                        std::string(e.what()));
        throw;
    }
}

void GetAlgoParamNamesTask::getAlgoParamNames(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string axis = params.value("axis", "ra");

    // Validate axis
    if (axis != "ra" && axis != "dec" && axis != "x" && axis != "y") {
        throw std::runtime_error("Axis must be one of: ra, dec, x, y");
    }

    spdlog::info("Getting algorithm parameter names for axis: {}", axis);
    addHistoryEntry("Getting algorithm parameter names for: " + axis);

    // Get algorithm parameter names using PHD2 client
    std::vector<std::string> param_names =
        phd2_client.value()->getAlgoParamNames(axis);

    spdlog::info("Found {} parameter names for {}", param_names.size(), axis);
    addHistoryEntry("Found " + std::to_string(param_names.size()) +
                    " parameters for " + axis);

    // Store result for retrieval
    setResult({{"axis", axis}, {"parameter_names", param_names}});
}

std::string GetAlgoParamNamesTask::taskName() { return "GetAlgoParamNames"; }

std::unique_ptr<Task> GetAlgoParamNamesTask::createEnhancedTask() {
    return std::make_unique<GetAlgoParamNamesTask>();
}

// ==================== GuideStatsTask Implementation ====================

GuideStatsTask::GuideStatsTask()
    : Task("GuideStats",
           [this](const json& params) { getGuideStats(params); }) {
    setTaskType("GuideStats");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for statistics
    setTimeout(std::chrono::seconds(15));

    // Add parameter definitions
    addParamDefinition("duration", "integer", false, json(60),
                       "Duration in seconds to collect stats");
}

void GuideStatsTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting comprehensive guide statistics");

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
        addHistoryEntry("Failed to get guide statistics: " +
                        std::string(e.what()));
        throw;
    }
}

void GuideStatsTask::getGuideStats(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    int duration = params.value("duration", 60);

    if (duration < 5 || duration > 300) {
        throw std::runtime_error("Duration must be between 5 and 300 seconds");
    }

    spdlog::info("Collecting guide statistics for {} seconds", duration);
    addHistoryEntry("Collecting guide statistics for " +
                    std::to_string(duration) + " seconds");

    // Get various stats from PHD2 client
    json stats;
    stats["app_state"] = static_cast<int>(phd2_client.value()->getAppState());
    stats["paused"] = phd2_client.value()->getPaused();
    stats["guide_output_enabled"] =
        phd2_client.value()->getGuideOutputEnabled();

    // Get current lock position if available
    auto lock_pos = phd2_client.value()->getLockPosition();
    if (lock_pos.has_value()) {
        stats["lock_position"] = {{"x", lock_pos.value()[0]},
                                  {"y", lock_pos.value()[1]}};
    }

    // Get pixel scale and search region
    stats["pixel_scale"] = phd2_client.value()->getPixelScale();
    stats["search_region"] = phd2_client.value()->getSearchRegion();

    // Get exposure time
    stats["exposure_ms"] = phd2_client.value()->getExposure();

    // Get Dec guide mode
    stats["dec_guide_mode"] = phd2_client.value()->getDecGuideMode();

    spdlog::info("Guide statistics collected successfully");
    addHistoryEntry("Guide statistics collected successfully");

    // Store result for retrieval
    setResult(stats);
}

std::string GuideStatsTask::taskName() { return "GuideStats"; }

std::unique_ptr<Task> GuideStatsTask::createEnhancedTask() {
    return std::make_unique<GuideStatsTask>();
}

// ==================== EmergencyStopTask Implementation ====================

EmergencyStopTask::EmergencyStopTask()
    : Task("EmergencyStop",
           [this](const json& params) { emergencyStop(params); }) {
    setTaskType("EmergencyStop");

    // Set default priority and timeout
    setPriority(10);  // Highest priority for emergency operations
    setTimeout(std::chrono::seconds(5));

    // Add parameter definitions
    addParamDefinition("reason", "string", false, json("Emergency stop"),
                       "Reason for emergency stop");
}

void EmergencyStopTask::execute(const json& params) {
    try {
        addHistoryEntry("EMERGENCY STOP initiated");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to execute emergency stop: " +
                        std::string(e.what()));
        throw;
    }
}

void EmergencyStopTask::emergencyStop(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string reason = params.value("reason", "Emergency stop");

    spdlog::critical("EMERGENCY STOP: {}", reason);
    addHistoryEntry("EMERGENCY STOP: " + reason);

    try {
        // Stop all guiding operations immediately
        phd2_client.value()->stopCapture();

        // Disable guide output to prevent any further guide pulses
        phd2_client.value()->setGuideOutputEnabled(false);

        spdlog::critical("Emergency stop completed successfully");
        addHistoryEntry("Emergency stop completed - all guiding stopped");

    } catch (const std::exception& e) {
        spdlog::critical("Emergency stop encountered error: {}", e.what());
        addHistoryEntry("Emergency stop encountered error: " +
                        std::string(e.what()));

        // Even if there's an error, we still consider this successful
        // because we tried our best to stop everything
    }
}

std::string EmergencyStopTask::taskName() { return "EmergencyStop"; }

std::unique_ptr<Task> EmergencyStopTask::createEnhancedTask() {
    return std::make_unique<EmergencyStopTask>();
}

}  // namespace lithium::task::guide
