#include "system.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <set>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "client/phd2/types.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GetAppStateTask Implementation ====================

GetAppStateTask::GetAppStateTask()
    : Task("GetAppState", [this](const json& params) { getAppState(params); }) {
    setTaskType("GetAppState");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for state retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetAppStateTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting PHD2 app state");
        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get app state: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get app state: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get app state: {}", e.what());
    }
}

void GetAppStateTask::getAppState(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting PHD2 application state");
    addHistoryEntry("Getting PHD2 application state");

    auto app_state = phd2_client.value()->getAppState();

    std::string state_str;
    switch (app_state) {
        case phd2::AppStateType::Stopped:
            state_str = "Stopped";
            break;
        case phd2::AppStateType::Selected:
            state_str = "Selected";
            break;
        case phd2::AppStateType::Calibrating:
            state_str = "Calibrating";
            break;
        case phd2::AppStateType::Guiding:
            state_str = "Guiding";
            break;
        case phd2::AppStateType::LostLock:
            state_str = "LostLock";
            break;
        case phd2::AppStateType::Paused:
            state_str = "Paused";
            break;
        case phd2::AppStateType::Looping:
            state_str = "Looping";
            break;
        default:
            state_str = "Unknown";
            break;
    }

    spdlog::info("Current PHD2 state: {}", state_str);
    addHistoryEntry("Current PHD2 state: " + state_str);

    setResult(
        {{"state", state_str}, {"state_code", static_cast<int>(app_state)}});
}

// ==================== GetGuideOutputEnabledTask Implementation
// ====================

GetGuideOutputEnabledTask::GetGuideOutputEnabledTask()
    : Task("GetGuideOutputEnabled",
           [this](const json& params) { getGuideOutputEnabled(params); }) {
    setTaskType("GetGuideOutputEnabled");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for status retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetGuideOutputEnabledTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting guide output status");
        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get guide output status: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get guide output status: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get guide output status: {}", e.what());
    }
}

void GetGuideOutputEnabledTask::getGuideOutputEnabled(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting guide output status");
    addHistoryEntry("Getting guide output status");

    bool enabled = phd2_client.value()->getGuideOutputEnabled();

    spdlog::info("Guide output enabled: {}", enabled);
    addHistoryEntry("Guide output enabled: " +
                    std::string(enabled ? "yes" : "no"));

    setResult({{"enabled", enabled}});
}

// ==================== SetGuideOutputEnabledTask Implementation
// ====================

SetGuideOutputEnabledTask::SetGuideOutputEnabledTask()
    : Task("SetGuideOutputEnabled",
           [this](const json& params) { setGuideOutputEnabled(params); }) {
    setTaskType("SetGuideOutputEnabled");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for settings
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("enabled", "boolean", true, json(true),
                       "Enable or disable guide output");
}

void SetGuideOutputEnabledTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting guide output status");

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
        addHistoryEntry("Failed to set guide output status: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set guide output status: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set guide output status: {}", e.what());
    }
}

void SetGuideOutputEnabledTask::setGuideOutputEnabled(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    bool enabled = params.value("enabled", true);

    spdlog::info("Setting guide output enabled: {}", enabled);
    addHistoryEntry("Setting guide output enabled: " +
                    std::string(enabled ? "yes" : "no"));

    phd2_client.value()->setGuideOutputEnabled(enabled);

    spdlog::info("Guide output status set successfully");
    addHistoryEntry("Guide output status set successfully");
}

// ==================== GetPausedStatusTask Implementation ====================

GetPausedStatusTask::GetPausedStatusTask()
    : Task("GetPausedStatus",
           [this](const json& params) { getPausedStatus(params); }) {
    setTaskType("GetPausedStatus");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for status retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetPausedStatusTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting paused status");
        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get paused status: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get paused status: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get paused status: {}", e.what());
    }
}

void GetPausedStatusTask::getPausedStatus(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting paused status");
    addHistoryEntry("Getting paused status");

    bool paused = phd2_client.value()->getPaused();

    spdlog::info("PHD2 paused: {}", paused);
    addHistoryEntry("PHD2 paused: " + std::string(paused ? "yes" : "no"));

    setResult({{"paused", paused}});
}

// ==================== ShutdownPHD2Task Implementation ====================

ShutdownPHD2Task::ShutdownPHD2Task()
    : Task("ShutdownPHD2",
           [this](const json& params) { shutdownPHD2(params); }) {
    setTaskType("ShutdownPHD2");

    // Set default priority and timeout
    setPriority(9);  // Very high priority for shutdown
    setTimeout(std::chrono::seconds(30));

    // Add parameter definitions
    addParamDefinition("confirm", "boolean", false, json(false),
                       "Confirm shutdown of PHD2");
}

void ShutdownPHD2Task::execute(const json& params) {
    try {
        addHistoryEntry("Shutting down PHD2");

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
        addHistoryEntry("Failed to shutdown PHD2: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to shutdown PHD2: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to shutdown PHD2: {}", e.what());
    }
}

void ShutdownPHD2Task::shutdownPHD2(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    bool confirm = params.value("confirm", false);

    if (!confirm) {
        THROW_INVALID_ARGUMENT(
            "Must confirm PHD2 shutdown by setting 'confirm' parameter to "
            "true");
    }

    spdlog::warn("Shutting down PHD2 application");
    addHistoryEntry("Shutting down PHD2 application");

    phd2_client.value()->shutdown();

    spdlog::info("PHD2 shutdown command sent");
    addHistoryEntry("PHD2 shutdown command sent");
}

// ==================== SendGuidePulseTask Implementation ====================

SendGuidePulseTask::SendGuidePulseTask()
    : Task("SendGuidePulse",
           [this](const json& params) { sendGuidePulse(params); }) {
    setTaskType("SendGuidePulse");

    // Set default priority and timeout
    setPriority(8);  // High priority for direct guide commands
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("amount", "integer", true, json(100),
                       "Pulse duration in milliseconds or AO step count");
    addParamDefinition("direction", "string", true, json("N"),
                       "Direction (N/S/E/W/Up/Down/Left/Right)");
    addParamDefinition("device", "string", false, json("Mount"),
                       "Device to pulse (Mount or AO)");
}

void SendGuidePulseTask::execute(const json& params) {
    try {
        addHistoryEntry("Sending guide pulse");

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
        addHistoryEntry("Failed to send guide pulse: " + std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to send guide pulse: " + std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to send guide pulse: {}", e.what());
    }
}

void SendGuidePulseTask::sendGuidePulse(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    int amount = params.value("amount", 100);
    std::string direction = params.value("direction", "N");
    std::string device = params.value("device", "Mount");

    if (amount < 1 || amount > 10000) {
        THROW_INVALID_ARGUMENT("Amount must be between 1 and 10000");
    }

    std::set<std::string> valid_directions = {"N",  "S",    "E",    "W",
                                              "Up", "Down", "Left", "Right"};
    if (valid_directions.find(direction) == valid_directions.end()) {
        THROW_INVALID_ARGUMENT(
            "Invalid direction. Must be one of: N, S, E, W, Up, Down, Left, "
            "Right");
    }

    if (device != "Mount" && device != "AO") {
        THROW_INVALID_ARGUMENT("Device must be 'Mount' or 'AO'");
    }

    spdlog::info("Sending guide pulse: {} {} for {}ms/steps on {}", direction,
                 amount, amount, device);
    addHistoryEntry("Sending " + direction + " pulse for " +
                    std::to_string(amount) + "ms on " + device);

    phd2_client.value()->guidePulse(amount, direction, device);

    spdlog::info("Guide pulse sent successfully");
    addHistoryEntry("Guide pulse sent successfully");
}

}  // namespace lithium::task::guide
