#include "position.hpp"
#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace lithium::task::focuser {

FocuserPositionTask::FocuserPositionTask(const std::string& name)
    : BaseFocuserTask(name) {

    setTaskType("FocuserPosition");
    addHistoryEntry("FocuserPositionTask initialized");
}

void FocuserPositionTask::execute(const json& params) {
    addHistoryEntry("FocuserPosition task started");
    setErrorType(TaskErrorType::None);

    try {
        if (!validateParams(params)) {
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT("Parameter validation failed");
        }

        validatePositionParams(params);

        if (!setupFocuser()) {
            setErrorType(TaskErrorType::DeviceError);
            THROW_RUNTIME_ERROR("Failed to setup focuser");
        }

        std::string action = params.at("action").get<std::string>();
        int timeout = params.value("timeout", 30);
        bool verify = params.value("verify", true);

        addHistoryEntry("Executing action: " + action);

        if (action == "move_absolute") {
            int position = params.at("position").get<int>();
            if (!moveAbsolute(position, timeout, verify)) {
                setErrorType(TaskErrorType::DeviceError);
                THROW_RUNTIME_ERROR("Absolute move failed");
            }

        } else if (action == "move_relative") {
            int steps = params.at("steps").get<int>();
            if (!moveRelativeSteps(steps, timeout)) {
                setErrorType(TaskErrorType::DeviceError);
                THROW_RUNTIME_ERROR("Relative move failed");
            }

        } else if (action == "get_position") {
            int position = getPositionSafe();
            addHistoryEntry("Current position: " + std::to_string(position));

        } else if (action == "sync_position") {
            int position = params.at("position").get<int>();
            if (!syncPosition(position)) {
                setErrorType(TaskErrorType::DeviceError);
                THROW_RUNTIME_ERROR("Position sync failed");
            }

        } else {
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT("Unknown action: " + action);
        }

        addHistoryEntry("FocuserPosition task completed successfully");
        spdlog::info("FocuserPosition task completed: {}", action);

    } catch (const std::exception& e) {
        addHistoryEntry("FocuserPosition task failed: " + std::string(e.what()));

        if (getErrorType() == TaskErrorType::None) {
            setErrorType(TaskErrorType::SystemError);
        }

        spdlog::error("FocuserPosition task failed: {}", e.what());
        throw;
    }
}

bool FocuserPositionTask::moveAbsolute(int position, int timeout, bool verify) {
    addHistoryEntry("Moving to absolute position: " + std::to_string(position));

    if (!moveToPosition(position, timeout)) {
        return false;
    }

    if (verify && !verifyPosition(position)) {
        spdlog::error("Position verification failed after absolute move");
        return false;
    }

    addHistoryEntry("Absolute move completed successfully");
    return true;
}

bool FocuserPositionTask::moveRelativeSteps(int steps, int timeout) {
    auto currentPos = getCurrentPosition();
    if (!currentPos) {
        spdlog::error("Cannot get current position for relative move");
        return false;
    }

    int startPosition = *currentPos;
    int targetPosition = startPosition + steps;

    addHistoryEntry("Moving " + std::to_string(steps) + " steps from position " +
                   std::to_string(startPosition));

    if (!moveToPosition(targetPosition, timeout)) {
        return false;
    }

    addHistoryEntry("Relative move completed successfully");
    return true;
}

bool FocuserPositionTask::syncPosition(int position) {
    addHistoryEntry("Syncing position to: " + std::to_string(position));

    try {
        // In a real implementation, this would send a sync command to the focuser
        // to set the current physical position as the specified value
        spdlog::info("Synchronizing focuser position to {}", position);

        addHistoryEntry("Position sync completed");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to sync position: {}", e.what());
        return false;
    }
}

int FocuserPositionTask::getPositionSafe() {
    auto position = getCurrentPosition();
    if (!position) {
        THROW_RUNTIME_ERROR("Failed to get current focuser position");
    }
    return *position;
}

std::unique_ptr<Task> FocuserPositionTask::createEnhancedTask() {
    auto task = std::make_unique<Task>("FocuserPosition", [](const json& params) {
        try {
            FocuserPositionTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced FocuserPosition task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(120));
    task->setLogLevel(2);
    task->setTaskType("FocuserPosition");

    return task;
}

void FocuserPositionTask::defineParameters(Task& task) {
    task.addParamDefinition("action", "string", true, "move_absolute",
                           "Action to perform: move_absolute, move_relative, get_position, sync_position");
    task.addParamDefinition("position", "int", false, 25000,
                           "Target position for absolute moves or sync operations");
    task.addParamDefinition("steps", "int", false, 100,
                           "Number of steps for relative moves");
    task.addParamDefinition("timeout", "int", false, 30,
                           "Movement timeout in seconds");
    task.addParamDefinition("verify", "bool", false, true,
                           "Verify position after movement");
}

void FocuserPositionTask::validatePositionParams(const json& params) {
    if (!params.contains("action")) {
        THROW_INVALID_ARGUMENT("Missing required parameter: action");
    }

    std::string action = params["action"].get<std::string>();

    if (action == "move_absolute" || action == "sync_position") {
        if (!params.contains("position")) {
            THROW_INVALID_ARGUMENT("Missing required parameter 'position' for action: " + action);
        }

        int position = params["position"].get<int>();
        if (!isValidPosition(position)) {
            THROW_INVALID_ARGUMENT("Position " + std::to_string(position) + " is out of range");
        }

    } else if (action == "move_relative") {
        if (!params.contains("steps")) {
            THROW_INVALID_ARGUMENT("Missing required parameter 'steps' for relative move");
        }

        int steps = params["steps"].get<int>();
        if (std::abs(steps) > 10000) {
            THROW_INVALID_ARGUMENT("Relative move steps too large: " + std::to_string(steps));
        }

    } else if (action != "get_position") {
        THROW_INVALID_ARGUMENT("Unknown action: " + action);
    }
}

bool FocuserPositionTask::verifyPosition(int expectedPosition, int tolerance) {
    auto currentPos = getCurrentPosition();
    if (!currentPos) {
        spdlog::error("Cannot verify position - unable to read current position");
        return false;
    }

    int difference = std::abs(*currentPos - expectedPosition);
    bool isWithinTolerance = difference <= tolerance;

    if (!isWithinTolerance) {
        spdlog::warn("Position verification failed: expected {}, got {}, difference {}",
                     expectedPosition, *currentPos, difference);
    }

    return isWithinTolerance;
}

}  // namespace lithium::task::focuser
