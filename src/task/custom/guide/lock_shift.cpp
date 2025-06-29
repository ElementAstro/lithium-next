#include "lock_shift.hpp"
#include "atom/error/exception.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

// ==================== GetLockShiftEnabledTask Implementation
// ====================

GetLockShiftEnabledTask::GetLockShiftEnabledTask()
    : Task("GetLockShiftEnabled",
           [this](const json& params) { getLockShiftEnabled(params); }) {
    setTaskType("GetLockShiftEnabled");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for status retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetLockShiftEnabledTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting lock shift enabled status");

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock shift status: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock shift status: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get lock shift status: {}", e.what());
    }
}

void GetLockShiftEnabledTask::getLockShiftEnabled(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting lock shift enabled status");
    addHistoryEntry("Getting lock shift enabled status");

    bool enabled = phd2_client.value()->getLockShiftEnabled();

    spdlog::info("Lock shift enabled: {}", enabled);
    addHistoryEntry("Lock shift enabled: " +
                    std::string(enabled ? "yes" : "no"));

    setResult({{"enabled", enabled}});
}

// ==================== SetLockShiftEnabledTask Implementation
// ====================

SetLockShiftEnabledTask::SetLockShiftEnabledTask()
    : Task("SetLockShiftEnabled",
           [this](const json& params) { setLockShiftEnabled(params); }) {
    setTaskType("SetLockShiftEnabled");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for settings
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("enabled", "boolean", true, json(true),
                       "Enable or disable lock shift");
}

void SetLockShiftEnabledTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting lock shift enabled status");

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
        addHistoryEntry("Failed to set lock shift status: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set lock shift status: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set lock shift status: {}", e.what());
    }
}

void SetLockShiftEnabledTask::setLockShiftEnabled(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    bool enabled = params.value("enabled", true);

    spdlog::info("Setting lock shift enabled: {}", enabled);
    addHistoryEntry("Setting lock shift enabled: " +
                    std::string(enabled ? "yes" : "no"));

    phd2_client.value()->setLockShiftEnabled(enabled);

    spdlog::info("Lock shift status set successfully");
    addHistoryEntry("Lock shift status set successfully");
}

// ==================== GetLockShiftParamsTask Implementation
// ====================

GetLockShiftParamsTask::GetLockShiftParamsTask()
    : Task("GetLockShiftParams",
           [this](const json& params) { getLockShiftParams(params); }) {
    setTaskType("GetLockShiftParams");

    // Set default priority and timeout
    setPriority(4);  // Lower priority for parameter retrieval
    setTimeout(std::chrono::seconds(10));
}

void GetLockShiftParamsTask::execute(const json& params) {
    try {
        addHistoryEntry("Getting lock shift parameters");

        Task::execute(params);

    } catch (const atom::error::Exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock shift parameters: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to get lock shift parameters: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to get lock shift parameters: {}",
                            e.what());
    }
}

void GetLockShiftParamsTask::getLockShiftParams(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    spdlog::info("Getting lock shift parameters");
    addHistoryEntry("Getting lock shift parameters");

    json lock_shift_params = phd2_client.value()->getLockShiftParams();

    spdlog::info("Lock shift parameters retrieved successfully");
    addHistoryEntry("Lock shift parameters retrieved");

    setResult(lock_shift_params);
}

// ==================== SetLockShiftParamsTask Implementation
// ====================

SetLockShiftParamsTask::SetLockShiftParamsTask()
    : Task("SetLockShiftParams",
           [this](const json& params) { setLockShiftParams(params); }) {
    setTaskType("SetLockShiftParams");

    // Set default priority and timeout
    setPriority(6);  // Medium priority for settings
    setTimeout(std::chrono::seconds(10));

    // Add parameter definitions
    addParamDefinition("params", "object", true, json::object(),
                       "Lock shift parameters object");
}

void SetLockShiftParamsTask::execute(const json& params) {
    try {
        addHistoryEntry("Setting lock shift parameters");

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
        addHistoryEntry("Failed to set lock shift parameters: " +
                        std::string(e.what()));
        throw;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Failed to set lock shift parameters: " +
                        std::string(e.what()));
        THROW_RUNTIME_ERROR("Failed to set lock shift parameters: {}",
                            e.what());
    }
}

void SetLockShiftParamsTask::setLockShiftParams(const json& params) {
    auto phd2_client = GetPtr<phd2::Client>(Constants::PHD2_CLIENT);
    if (!phd2_client) {
        THROW_OBJ_NOT_EXIST("PHD2 client not found in global manager");
    }

    json lock_shift_params = params.value("params", json::object());

    if (lock_shift_params.empty()) {
        THROW_INVALID_ARGUMENT("Lock shift parameters cannot be empty");
    }

    spdlog::info("Setting lock shift parameters");
    addHistoryEntry("Setting lock shift parameters");

    phd2_client.value()->setLockShiftParams(lock_shift_params);

    spdlog::info("Lock shift parameters set successfully");
    addHistoryEntry("Lock shift parameters set successfully");
}

}  // namespace lithium::task::guide
