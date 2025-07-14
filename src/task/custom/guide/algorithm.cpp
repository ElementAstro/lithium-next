#include "algorithm.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== SetAlgoParamTask Implementation ====================

SetAlgoParamTask::SetAlgoParamTask()
    : Task("SetAlgoParam",
           [this](const json& params) { setAlgorithmParameter(params); }) {
    setTaskType("SetAlgoParam");

    // Set default priority and timeout
    setPriority(5);  // Medium priority for parameter setting
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("axis", "string", true, json("ra"),
                       "Axis to set parameter for (ra, dec, x, y)");
    addParamDefinition("name", "string", true, json(""), "Parameter name");
    addParamDefinition("value", "number", true, json(0.0), "Parameter value");
}

void SetAlgoParamTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting algorithm parameter");

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
        addHistoryEntry("Failed to set algorithm parameter: " +
                        std::string(e.what()));
        throw;
    }
}

void SetAlgoParamTask::setAlgorithmParameter(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string axis = params.value("axis", "ra");
    std::string name = params.value("name", "");
    double value = params.value("value", 0.0);

    // Validate parameters
    if (axis != "ra" && axis != "dec" && axis != "x" && axis != "y") {
        throw std::runtime_error("Axis must be one of: ra, dec, x, y");
    }

    if (name.empty()) {
        throw std::runtime_error("Parameter name cannot be empty");
    }

    spdlog::info("Setting algorithm parameter: axis={}, name={}, value={}",
                 axis, name, value);
    addHistoryEntry("Setting " + axis + "." + name + " = " +
                    std::to_string(value));

    // Set algorithm parameter using PHD2 client
    phd2_client.value()->setAlgoParam(axis, name, value);

    spdlog::info("Algorithm parameter set successfully");
    addHistoryEntry("Algorithm parameter set successfully");
}

std::string SetAlgoParamTask::taskName() { return "SetAlgoParam"; }

std::unique_ptr<Task> SetAlgoParamTask::createEnhancedTask() {
    return std::make_unique<SetAlgoParamTask>();
}

// ==================== GetAlgoParamTask Implementation ====================

GetAlgoParamTask::GetAlgoParamTask()
    : Task("GetAlgoParam",
           [this](const json& params) { getAlgorithmParameter(params); }) {
    setTaskType("GetAlgoParam");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for parameter getting
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("axis", "string", true, json("ra"),
                       "Axis to get parameter from (ra, dec, x, y)");
    addParamDefinition("name", "string", true, json(""), "Parameter name");
}

void GetAlgoParamTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting algorithm parameter");

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
        addHistoryEntry("Failed to get algorithm parameter: " +
                        std::string(e.what()));
        throw;
    }
}

void GetAlgoParamTask::getAlgorithmParameter(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string axis = params.value("axis", "ra");
    std::string name = params.value("name", "");

    // Validate parameters
    if (axis != "ra" && axis != "dec" && axis != "x" && axis != "y") {
        throw std::runtime_error("Axis must be one of: ra, dec, x, y");
    }

    if (name.empty()) {
        throw std::runtime_error("Parameter name cannot be empty");
    }

    spdlog::info("Getting algorithm parameter: axis={}, name={}", axis, name);
    addHistoryEntry("Getting " + axis + "." + name);

    // Get algorithm parameter using PHD2 client
    double value = phd2_client.value()->getAlgoParam(axis, name);

    spdlog::info("Algorithm parameter value: {}", value);
    addHistoryEntry("Parameter value: " + std::to_string(value));

    // Store result for retrieval
    setResult({{"axis", axis}, {"name", name}, {"value", value}});
}

std::string GetAlgoParamTask::taskName() { return "GetAlgoParam"; }

std::unique_ptr<Task> GetAlgoParamTask::createEnhancedTask() {
    return std::make_unique<GetAlgoParamTask>();
}

// ==================== SetDecGuideModeTask Implementation ====================

SetDecGuideModeTask::SetDecGuideModeTask()
    : Task("SetDecGuideMode",
           [this](const json& params) { setDecGuideMode(params); }) {
    setTaskType("SetDecGuideMode");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for mode setting
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("mode", "string", true, json("Auto"),
                       "Dec guide mode (Off, Auto, North, South)");
}

void SetDecGuideModeTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting Dec guide mode");

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
        addHistoryEntry("Failed to set Dec guide mode: " +
                        std::string(e.what()));
        throw;
    }
}

void SetDecGuideModeTask::setDecGuideMode(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    std::string mode = params.value("mode", "Auto");

    // Validate mode
    if (mode != "Off" && mode != "Auto" && mode != "North" && mode != "South") {
        throw std::runtime_error(
            "Mode must be one of: Off, Auto, North, South");
    }

    spdlog::info("Setting Dec guide mode to: {}", mode);
    addHistoryEntry("Setting Dec guide mode to: " + mode);

    // Set Dec guide mode using PHD2 client
    phd2_client.value()->setDecGuideMode(mode);

    spdlog::info("Dec guide mode set successfully");
    addHistoryEntry("Dec guide mode set to: " + mode);
}

std::string SetDecGuideModeTask::taskName() { return "SetDecGuideMode"; }

std::unique_ptr<Task> SetDecGuideModeTask::createEnhancedTask() {
    return std::make_unique<SetDecGuideModeTask>();
}

// ==================== GetDecGuideModeTask Implementation ====================

GetDecGuideModeTask::GetDecGuideModeTask()
    : Task("GetDecGuideMode",
           [this](const json& params) { getDecGuideMode(params); }) {
    setTaskType("GetDecGuideMode");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for mode getting
    setTimeout(std::chrono::seconds(10));
}

void GetDecGuideModeTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting Dec guide mode");

        // Call the parent execute method which will call our lambda
        Task::execute(params);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get Dec guide mode: " +
                        std::string(e.what()));
        throw;
    }
}

void GetDecGuideModeTask::getDecGuideMode(const json& params) {
    // Get PHD2 client from global manager
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        throw std::runtime_error("PHD2 client not found in global manager");
    }

    spdlog::info("Getting current Dec guide mode");
    addHistoryEntry("Getting current Dec guide mode");

    // Get Dec guide mode using PHD2 client
    std::string mode = phd2_client.value()->getDecGuideMode();

    spdlog::info("Current Dec guide mode: {}", mode);
    addHistoryEntry("Current Dec guide mode: " + mode);

    // Store result for retrieval
    setResult({{"mode", mode}});
}

std::string GetDecGuideModeTask::taskName() { return "GetDecGuideMode"; }

std::unique_ptr<Task> GetDecGuideModeTask::createEnhancedTask() {
    return std::make_unique<GetDecGuideModeTask>();
}

}  // namespace lithium::task::guide
