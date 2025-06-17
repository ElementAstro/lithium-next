#include "device_config.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GetDeviceConfigTask Implementation ====================

GetDeviceConfigTask::GetDeviceConfigTask()
    : Task("GetDeviceConfig",
           [this](const json& params) { getDeviceConfig(params); }) {
    setTaskType("GetDeviceConfig");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for configuration retrieval
    setTimeout(std::chrono::seconds(15));

    // Add parameter definitions
    addParamDefinition(
        "device_type", "string", false, json("all"),
        "Device type to get config for (camera, mount, ao, all)");
}

void GetDeviceConfigTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting device configuration");

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
        addHistoryEntry("Failed to get device configuration: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get device configuration: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get device configuration: {}", e.what());
    }
}

void GetDeviceConfigTask::getDeviceConfig(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    std::string device_type = params.value("device_type", "all");

    spdlog::info("Getting device configuration for: {}", device_type);
    addHistoryEntry("Getting device configuration for: " + device_type);

    json config;

    if (device_type == "all" || device_type == "camera") {
        try {
            config["camera"] = {
                {"exposure_ms", phd2_client.value()->getExposure()},
                {"use_subframes", phd2_client.value()->getUseSubframes()}};
        } catch (const std::exception& e) {
            config["camera"] = {{"error", e.what()}};
        }
    }

    if (device_type == "all" || device_type == "mount") {
        try {
            config["mount"] = {
                {"calibration_data",
                 phd2_client.value()->getCalibrationData("Mount")},
                {"dec_guide_mode", phd2_client.value()->getDecGuideMode()}};
        } catch (const std::exception& e) {
            config["mount"] = {{"error", e.what()}};
        }
    }

    if (device_type == "all" || device_type == "ao") {
        try {
            config["ao"] = {{"calibration_data",
                             phd2_client.value()->getCalibrationData("AO")}};
        } catch (const std::exception& e) {
            config["ao"] = {{"error", e.what()}};
        }
    }

    // Add general system info
    config["system"] = {
        {"app_state", static_cast<int>(phd2_client.value()->getAppState())},
        {"pixel_scale", phd2_client.value()->getPixelScale()},
        {"search_region", phd2_client.value()->getSearchRegion()},
        {"guide_output_enabled", phd2_client.value()->getGuideOutputEnabled()},
        {"paused", phd2_client.value()->getPaused()}};

    spdlog::info("Device configuration retrieved successfully");
    addHistoryEntry("Device configuration retrieved for " + device_type);

    // Store result for retrieval
    setResult(config);
}

std::string GetDeviceConfigTask::taskName() { return "GetDeviceConfig"; }

std::unique_ptr<Task> GetDeviceConfigTask::createEnhancedTask() {
    return std::make_unique<GetDeviceConfigTask>();
}

// ==================== SetDeviceConfigTask Implementation ====================

SetDeviceConfigTask::SetDeviceConfigTask()
    : Task("SetDeviceConfig",
           [this](const json& params) { setDeviceConfig(params); }) {
    setTaskType("SetDeviceConfig");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for configuration changes
    setTimeout(std::chrono::seconds(30));

    // Add parameter definitions
    addParamDefinition("config", "object", true, json::object(),
                       "Device configuration object");
}

void SetDeviceConfigTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting device configuration");

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
        addHistoryEntry("Failed to set device configuration: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set device configuration: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set device configuration: {}", e.what());
    }
}

void SetDeviceConfigTask::setDeviceConfig(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    json config = params.value("config", json::object());

    if (config.empty()) {
        THROW_INVALID_ARGUMENT("Configuration cannot be empty");
    }

    spdlog::info("Setting device configuration");
    addHistoryEntry("Setting device configuration");

    int changes_applied = 0;

    // Apply camera configuration
    if (config.contains("camera")) {
        auto camera_config = config["camera"];

        if (camera_config.contains("exposure_ms")) {
            int exposure = camera_config["exposure_ms"];
            phd2_client.value()->setExposure(exposure);
            spdlog::info("Set camera exposure to {}ms", exposure);
            changes_applied++;
        }
    }

    // Apply mount configuration
    if (config.contains("mount")) {
        auto mount_config = config["mount"];

        if (mount_config.contains("dec_guide_mode")) {
            std::string mode = mount_config["dec_guide_mode"];
            phd2_client.value()->setDecGuideMode(mode);
            spdlog::info("Set Dec guide mode to {}", mode);
            changes_applied++;
        }
    }

    // Apply system configuration
    if (config.contains("system")) {
        auto system_config = config["system"];

        if (system_config.contains("guide_output_enabled")) {
            bool enabled = system_config["guide_output_enabled"];
            phd2_client.value()->setGuideOutputEnabled(enabled);
            spdlog::info("Set guide output enabled to {}", enabled);
            changes_applied++;
        }
    }

    spdlog::info("Device configuration applied successfully ({} changes)",
                 changes_applied);
    addHistoryEntry("Device configuration applied (" +
                    std::to_string(changes_applied) + " changes)");

    // Store result for retrieval
    setResult({{"changes_applied", changes_applied}});
}

std::string SetDeviceConfigTask::taskName() { return "SetDeviceConfig"; }

std::unique_ptr<Task> SetDeviceConfigTask::createEnhancedTask() {
    return std::make_unique<SetDeviceConfigTask>();
}

// ==================== GetMountPositionTask Implementation ====================

GetMountPositionTask::GetMountPositionTask()
    : Task("GetMountPosition",
           [this](const json& params) { getMountPosition(params); }) {
    setTaskType("GetMountPosition");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for information retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetMountPositionTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting mount position");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get mount position: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get mount position: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get mount position: {}", e.what());
    }
}

void GetMountPositionTask::getMountPosition(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting mount position information");
    addHistoryEntry("Getting mount position information");

    json position_info;

    // Get various position-related information
    try {
        // Get lock position if available
        auto lock_pos = phd2_client.value()->getLockPosition();
        if (lock_pos.has_value()) {
            position_info["lock_position"] = {{"x", lock_pos.value()[0]},
                                              {"y", lock_pos.value()[1]}};
        } else {
            position_info["lock_position"] = nullptr;
        }

        // Get pixel scale for conversions
        position_info["pixel_scale"] = phd2_client.value()->getPixelScale();

        // Get calibration data which contains angle and step size info
        auto calibration = phd2_client.value()->getCalibrationData("Mount");
        position_info["calibration"] = calibration;

        // Get current app state
        position_info["app_state"] =
            static_cast<int>(phd2_client.value()->getAppState());

    } catch (const std::exception& e) {
        position_info["error"] = e.what();
    }

    spdlog::info("Mount position information retrieved");
    addHistoryEntry("Mount position information retrieved");

    // Store result for retrieval
    setResult(position_info);
}

std::string GetMountPositionTask::taskName() { return "GetMountPosition"; }

std::unique_ptr<Task> GetMountPositionTask::createEnhancedTask() {
    return std::make_unique<GetMountPositionTask>();
}

// ==================== PHD2HealthCheckTask Implementation ====================

PHD2HealthCheckTask::PHD2HealthCheckTask()
    : Task("PHD2HealthCheck",
           [this](const json& params) { performHealthCheck(params); }) {
    setTaskType("PHD2HealthCheck");

    // Set default priority and timeout
    setPriority(5);  // Medium priority for health checks
    setTimeout(std::chrono::seconds(30));

    // Add parameter definitions
    addParamDefinition(
        "quick", "boolean", false, json(false),
        "Perform quick health check (faster, less comprehensive)");
}

void PHD2HealthCheckTask::execute(const json& params) {
    try {
        addHistoryEntry("Performing PHD2 health check");

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
        addHistoryEntry("Health check failed: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Health check failed: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Health check failed: {}", e.what());
    }
}

void PHD2HealthCheckTask::performHealthCheck(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    bool quick = params.value("quick", false);

    spdlog::info("Performing {} PHD2 health check",
                 quick ? "quick" : "comprehensive");
    addHistoryEntry("Performing " +
                    std::string(quick ? "quick" : "comprehensive") +
                    " health check");

    json health_report;
    int checks_passed = 0;
    int total_checks = 0;

    // Basic connectivity check
    total_checks++;
    try {
        auto state = phd2_client.value()->getAppState();
        health_report["connectivity"] = {
            {"status", "OK"}, {"app_state", static_cast<int>(state)}};
        checks_passed++;
    } catch (const std::exception& e) {
        health_report["connectivity"] = {{"status", "FAILED"},
                                         {"error", e.what()}};
    }

    // Camera configuration check
    total_checks++;
    try {
        int exposure = phd2_client.value()->getExposure();
        bool subframes = phd2_client.value()->getUseSubframes();

        health_report["camera"] = {{"status", "OK"},
                                   {"exposure_ms", exposure},
                                   {"use_subframes", subframes}};
        checks_passed++;
    } catch (const std::exception& e) {
        health_report["camera"] = {{"status", "FAILED"}, {"error", e.what()}};
    }

    // Guide output status check
    total_checks++;
    try {
        bool output_enabled = phd2_client.value()->getGuideOutputEnabled();
        bool paused = phd2_client.value()->getPaused();

        health_report["guide_output"] = {
            {"status", "OK"}, {"enabled", output_enabled}, {"paused", paused}};
        checks_passed++;
    } catch (const std::exception& e) {
        health_report["guide_output"] = {{"status", "FAILED"},
                                         {"error", e.what()}};
    }

    if (!quick) {
        // Calibration status check (comprehensive only)
        total_checks++;
        try {
            auto calibration = phd2_client.value()->getCalibrationData("Mount");
            health_report["calibration"] = {{"status", "OK"},
                                            {"data", calibration}};
            checks_passed++;
        } catch (const std::exception& e) {
            health_report["calibration"] = {{"status", "FAILED"},
                                            {"error", e.what()}};
        }

        // System parameters check (comprehensive only)
        total_checks++;
        try {
            double pixel_scale = phd2_client.value()->getPixelScale();
            int search_region = phd2_client.value()->getSearchRegion();

            health_report["system_params"] = {{"status", "OK"},
                                              {"pixel_scale", pixel_scale},
                                              {"search_region", search_region}};
            checks_passed++;
        } catch (const std::exception& e) {
            health_report["system_params"] = {{"status", "FAILED"},
                                              {"error", e.what()}};
        }
    }

    // Overall health assessment
    double health_percentage =
        (static_cast<double>(checks_passed) / total_checks) * 100.0;
    std::string overall_status;

    if (health_percentage >= 90.0) {
        overall_status = "EXCELLENT";
    } else if (health_percentage >= 75.0) {
        overall_status = "GOOD";
    } else if (health_percentage >= 50.0) {
        overall_status = "WARNING";
    } else {
        overall_status = "CRITICAL";
    }

    health_report["overall"] = {
        {"status", overall_status},
        {"health_percentage", health_percentage},
        {"checks_passed", checks_passed},
        {"total_checks", total_checks},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count()}};

    spdlog::info("Health check completed: {} ({:.1f}% healthy)", overall_status,
                 health_percentage);
    addHistoryEntry("Health check completed: " + overall_status + " (" +
                    std::to_string(static_cast<int>(health_percentage)) +
                    "% healthy)");

    // Store result for retrieval
    setResult(health_report);
}

std::string PHD2HealthCheckTask::taskName() { return "PHD2HealthCheck"; }

std::unique_ptr<Task> PHD2HealthCheckTask::createEnhancedTask() {
    return std::make_unique<PHD2HealthCheckTask>();
}

}  // namespace lithium::task::guide
