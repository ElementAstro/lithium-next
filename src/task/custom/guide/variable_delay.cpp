#include "variable_delay.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GetVariableDelaySettingsTask Implementation
// ====================

GetVariableDelaySettingsTask::GetVariableDelaySettingsTask()
    : Task("GetVariableDelaySettings",
           [this](const json& params) { getVariableDelaySettings(params); }) {
    setTaskType("GetVariableDelaySettings");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for settings retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetVariableDelaySettingsTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting variable delay settings");

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get variable delay settings: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get variable delay settings: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get variable delay settings: {}",
                            e.what());
    }
}

void GetVariableDelaySettingsTask::getVariableDelaySettings(
    const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting variable delay settings");
    addHistoryEntry("Getting variable delay settings");

    json settings = phd2_client.value()->getVariableDelaySettings();

    spdlog::info("Variable delay settings retrieved successfully");
    addHistoryEntry("Variable delay settings retrieved");

    setResult(settings);
}

std::string GetVariableDelaySettingsTask::taskName() {
    return "GetVariableDelaySettings";
}

std::unique_ptr<Task> GetVariableDelaySettingsTask::createEnhancedTask() {
    return std::make_unique<GetVariableDelaySettingsTask>();
}

// ==================== SetVariableDelaySettingsTask Implementation
// ====================

SetVariableDelaySettingsTask::SetVariableDelaySettingsTask()
    : Task("SetVariableDelaySettings",
           [this](const json& params) { setVariableDelaySettings(params); }) {
    setTaskType("SetVariableDelaySettings");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for settings
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("settings", "object", true, json::object(),
                       "Variable delay settings object");
}

void SetVariableDelaySettingsTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting variable delay settings");

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
        addHistoryEntry("Failed to set variable delay settings: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set variable delay settings: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set variable delay settings: {}",
                            e.what());
    }
}

void SetVariableDelaySettingsTask::setVariableDelaySettings(
    const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    json settings = params.value("settings", json::object());

    if (settings.empty()) {
        THROW_INVALID_ARGUMENT("Variable delay settings cannot be empty");
    }

    spdlog::info("Setting variable delay settings");
    addHistoryEntry("Setting variable delay settings");

    phd2_client.value()->setVariableDelaySettings(settings);

    spdlog::info("Variable delay settings set successfully");
    addHistoryEntry("Variable delay settings set successfully");
}

std::string SetVariableDelaySettingsTask::taskName() {
    return "SetVariableDelaySettings";
}

std::unique_ptr<Task> SetVariableDelaySettingsTask::createEnhancedTask() {
    return std::make_unique<SetVariableDelaySettingsTask>();
}

}  // namespace lithium::task::guide
