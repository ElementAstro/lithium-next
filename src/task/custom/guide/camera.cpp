#include "camera.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== SetCameraExposureTask Implementation
// ====================

SetCameraExposureTask::SetCameraExposureTask()
    : Task("SetCameraExposure",
           [this](const json& params) { setCameraExposure(params); }) {
    setTaskType("SetCameraExposure");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for camera settings
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("exposure_ms", "integer", true, json(1000),
                       "Exposure time in milliseconds");
}

void SetCameraExposureTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting camera exposure");

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
        addHistoryEntry("Failed to set camera exposure: " +
                        std::string(e.what()));
        throw;
    }
}

void SetCameraExposureTask::setCameraExposure(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    int exposure_ms = params.value("exposure_ms", 1000);

    // Validate exposure time
    if (exposure_ms < 100 || exposure_ms > 60000) {
        throw std::runtime_error(
            "Exposure time must be between 100ms and 60000ms");
    }

    spdlog::info("Setting camera exposure to: {}ms", exposure_ms);
    addHistoryEntry(
        "Setting camera exposure to: " + std::to_string(exposure_ms) + "ms");

    // Set camera exposure using PHD2 client
    phd2_client.value()->setExposure(exposure_ms);

    spdlog::info("Camera exposure set successfully");
    addHistoryEntry("Camera exposure set to " + std::to_string(exposure_ms) +
                    "ms");
}

std::string SetCameraExposureTask::taskName() { return "SetCameraExposure"; }

std::unique_ptr<Task> SetCameraExposureTask::createEnhancedTask() {
    return std::make_unique<SetCameraExposureTask>();
}

// ==================== GetCameraExposureTask Implementation
// ====================

GetCameraExposureTask::GetCameraExposureTask()
    : Task("GetCameraExposure",
           [this](const json& params) { getCameraExposure(params); }) {
    setTaskType("GetCameraExposure");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for getting settings
    setTimeout(std::chrono::seconds(10));
}

void GetCameraExposureTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting camera exposure");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get camera exposure: " +
                        std::string(e.what()));
        throw;
    }
}

void GetCameraExposureTask::getCameraExposure(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    spdlog::info("Getting current camera exposure");
    addHistoryEntry("Getting current camera exposure");

    // Get camera exposure using PHD2 client
    int exposure_ms = phd2_client.value()->getExposure();

    spdlog::info("Current camera exposure: {}ms", exposure_ms);
    addHistoryEntry("Current camera exposure: " + std::to_string(exposure_ms) +
                    "ms");

    // Store result for retrieval
    setResult({{"exposure_ms", exposure_ms},
               {"exposure_seconds", exposure_ms / 1000.0}});
}

std::string GetCameraExposureTask::taskName() { return "GetCameraExposure"; }

std::unique_ptr<Task> GetCameraExposureTask::createEnhancedTask() {
    return std::make_unique<GetCameraExposureTask>();
}

// ==================== CaptureSingleFrameTask Implementation
// ====================

CaptureSingleFrameTask::CaptureSingleFrameTask()
    : Task("CaptureSingleFrame",
           [this](const json& params) { captureSingleFrame(params); }) {
    setTaskType("CaptureSingleFrame");

    // Set default priority and timeout
    setPriority(7);  // High priority for frame capture
    setTimeout(std::chrono::seconds(30));

    // Add parameter definitions
    addParamDefinition("exposure_ms", "integer", false, json(-1),
                       "Optional exposure time in ms (-1 for current setting)");
    addParamDefinition("subframe_x", "integer", false, json(-1),
                       "Subframe X coordinate (-1 for full frame)");
    addParamDefinition("subframe_y", "integer", false, json(-1),
                       "Subframe Y coordinate (-1 for full frame)");
    addParamDefinition("subframe_width", "integer", false, json(-1),
                       "Subframe width (-1 for full frame)");
    addParamDefinition("subframe_height", "integer", false, json(-1),
                       "Subframe height (-1 for full frame)");
}

void CaptureSingleFrameTask::execute(const json& params) {
    try {
        addHistoryEntry("Capturing single frame");

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
        addHistoryEntry("Failed to capture frame: " + std::string(e.what()));
        throw;
    }
}

void CaptureSingleFrameTask::captureSingleFrame(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    int exposure_ms = params.value("exposure_ms", -1);
    int subframe_x = params.value("subframe_x", -1);
    int subframe_y = params.value("subframe_y", -1);
    int subframe_width = params.value("subframe_width", -1);
    int subframe_height = params.value("subframe_height", -1);

    std::optional<int> exposure_opt =
        (exposure_ms > 0) ? std::make_optional(exposure_ms) : std::nullopt;
    std::optional<std::array<int, 4>> subframe_opt = std::nullopt;

    // Create subframe if specified
    if (subframe_x >= 0 && subframe_y >= 0 && subframe_width > 0 &&
        subframe_height > 0) {
        subframe_opt = std::array<int, 4>{subframe_x, subframe_y,
                                          subframe_width, subframe_height};
        spdlog::info("Capturing frame with subframe: ({}, {}, {}, {})",
                     subframe_x, subframe_y, subframe_width, subframe_height);
        addHistoryEntry("Capturing frame with subframe");
    } else {
        spdlog::info("Capturing full frame");
        addHistoryEntry("Capturing full frame");
    }

    if (exposure_ms > 0) {
        spdlog::info("Using exposure time: {}ms", exposure_ms);
        addHistoryEntry("Using exposure time: " + std::to_string(exposure_ms) +
                        "ms");
    }

    // Capture single frame using PHD2 client
    phd2_client.value()->captureSingleFrame(exposure_opt, subframe_opt);

    spdlog::info("Frame captured successfully");
    addHistoryEntry("Frame captured successfully");
}

std::string CaptureSingleFrameTask::taskName() { return "CaptureSingleFrame"; }

std::unique_ptr<Task> CaptureSingleFrameTask::createEnhancedTask() {
    return std::make_unique<CaptureSingleFrameTask>();
}

// ==================== StartLoopTask Implementation ====================

StartLoopTask::StartLoopTask()
    : Task("StartLoop", [this](const json& params) { startLoop(params); }) {
    setTaskType("StartLoop");

    // Set default priority and timeout
    setPriority(7);  // High priority for starting loop
    setTimeout(std::chrono::seconds(10));
}

void StartLoopTask::execute(const json& params) {
    try {
        addHistoryEntry("Starting exposure loop");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to start loop: " + std::string(e.what()));
        throw;
    }
}

void StartLoopTask::startLoop(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    spdlog::info("Starting exposure loop");
    addHistoryEntry("Starting continuous exposure loop");

    // Start looping using PHD2 client
    phd2_client.value()->loop();

    spdlog::info("Exposure loop started successfully");
    addHistoryEntry("Exposure loop started successfully");
}

std::string StartLoopTask::taskName() { return "StartLoop"; }

std::unique_ptr<Task> StartLoopTask::createEnhancedTask() {
    return std::make_unique<StartLoopTask>();
}

// ==================== GetSubframeStatusTask Implementation
// ====================

GetSubframeStatusTask::GetSubframeStatusTask()
    : Task("GetSubframeStatus",
           [this](const json& params) { getSubframeStatus(params); }) {
    setTaskType("GetSubframeStatus");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for status retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetSubframeStatusTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting subframe status");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get subframe status: " +
                        std::string(e.what()));
        throw;
    }
}

void GetSubframeStatusTask::getSubframeStatus(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    spdlog::info("Getting subframe status");
    addHistoryEntry("Getting subframe status");

    // Get subframe status using PHD2 client
    bool use_subframes = phd2_client.value()->getUseSubframes();

    spdlog::info("Subframes enabled: {}", use_subframes);
    addHistoryEntry("Subframes enabled: " +
                    std::string(use_subframes ? "yes" : "no"));

    // Store result for retrieval
    setResult({{"use_subframes", use_subframes}});
}

std::string GetSubframeStatusTask::taskName() { return "GetSubframeStatus"; }

std::unique_ptr<Task> GetSubframeStatusTask::createEnhancedTask() {
    return std::make_unique<GetSubframeStatusTask>();
}

}  // namespace lithium::task::guide
