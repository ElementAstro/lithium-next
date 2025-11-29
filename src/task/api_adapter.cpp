/**
 * @file api_adapter.cpp
 * @brief Implementation of Task Engine API Data Types and Utilities
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "api_adapter.hpp"
#include "custom/factory.hpp"
#include "target.hpp"
#include "task.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace lithium::task::api {

// ============================================================================
// Utility Functions
// ============================================================================

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return oss.str();
}

std::string generateUniqueId(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch())
                  .count();
    return prefix + "_" + std::to_string(ms);
}

// ============================================================================
// ApiResponse Implementation
// ============================================================================

ApiResponse ApiResponse::success(const json& data, const std::string& message) {
    ApiResponse response;
    response.statusCode = 200;
    response.status = "success";
    response.data = data;
    response.message = message;
    response.hasError = false;
    return response;
}

ApiResponse ApiResponse::accepted(const json& data,
                                  const std::string& message) {
    ApiResponse response;
    response.statusCode = 202;
    response.status = "success";
    response.data = data;
    response.message = message;
    response.hasError = false;
    return response;
}

ApiResponse ApiResponse::makeError(int statusCode, const std::string& errorCode,
                                   const std::string& errorMessage,
                                   const json& details) {
    ApiResponse response;
    response.statusCode = statusCode;
    response.status = "error";
    response.hasError = true;
    response.error.code = errorCode;
    response.error.message = errorMessage;
    response.error.details = details;
    return response;
}

json ApiResponse::toJson() const {
    json j;
    j["statusCode"] = statusCode;
    j["status"] = status;

    if (!data.is_null()) {
        j["data"] = data;
    }

    if (!message.empty()) {
        j["message"] = message;
    }

    if (hasError) {
        j["error"] = {{"code", error.code}, {"message", error.message}};
        if (!error.details.is_null()) {
            j["error"]["details"] = error.details;
        }
    }

    return j;
}

// ============================================================================
// WsEventType String Conversion
// ============================================================================

std::string wsEventTypeToString(WsEventType type) {
    switch (type) {
        case WsEventType::SequenceStart:
            return "sequence.start";
        case WsEventType::SequenceProgress:
            return "sequence.progress";
        case WsEventType::SequencePaused:
            return "sequence.paused";
        case WsEventType::SequenceResumed:
            return "sequence.resumed";
        case WsEventType::SequenceComplete:
            return "sequence.complete";
        case WsEventType::SequenceAborted:
            return "sequence.aborted";

        case WsEventType::TargetStart:
            return "target.start";
        case WsEventType::TargetProgress:
            return "target.progress";
        case WsEventType::TargetComplete:
            return "target.complete";
        case WsEventType::TargetFailed:
            return "target.failed";

        case WsEventType::TaskStart:
            return "task.start";
        case WsEventType::TaskProgress:
            return "task.progress";
        case WsEventType::TaskComplete:
            return "task.complete";
        case WsEventType::TaskFailed:
            return "task.failed";

        case WsEventType::ExposureStarted:
            return "exposure.started";
        case WsEventType::ExposureProgress:
            return "exposure.progress";
        case WsEventType::ExposureFinished:
            return "exposure.finished";
        case WsEventType::ExposureAborted:
            return "exposure.aborted";

        case WsEventType::DeviceConnected:
            return "device.connected";
        case WsEventType::DeviceDisconnected:
            return "device.disconnected";
        case WsEventType::DeviceStatusUpdate:
            return "device.status";

        case WsEventType::Notification:
            return "notification";
        case WsEventType::Error:
            return "error";

        default:
            return "unknown";
    }
}

// ============================================================================
// WsEvent Implementation
// ============================================================================

json WsEvent::toJson() const {
    json j;
    j["type"] = eventType;
    j["timestamp"] = timestamp;
    j["data"] = data;
    if (!correlationId.empty()) {
        j["correlationId"] = correlationId;
    }
    return j;
}

WsEvent WsEvent::fromJson(const json& j) {
    WsEvent event;
    event.eventType = j.value("type", "");
    event.timestamp = j.value("timestamp", "");
    event.data = j.value("data", json::object());
    event.correlationId = j.value("correlationId", "");
    return event;
}

WsEvent WsEvent::create(WsEventType type, const json& data,
                        const std::string& correlationId) {
    WsEvent event;
    event.type = type;
    event.eventType = wsEventTypeToString(type);
    event.timestamp = getCurrentTimestamp();
    event.data = data;
    event.correlationId = correlationId;
    return event;
}

// ============================================================================
// SequenceConverter Implementation
// ============================================================================

std::shared_ptr<Target> SequenceConverter::fromApiJson(
    const json& sequenceJson) {
    try {
        std::string name = sequenceJson.value("name", "Unnamed Sequence");
        auto target = std::make_shared<Target>(name);

        // Parse trigger if present
        if (sequenceJson.contains("trigger")) {
            const auto& trigger = sequenceJson["trigger"];
            json params = target->getParams();
            params["trigger"] = trigger;
            target->setParams(params);
        }

        // Parse tasks
        if (sequenceJson.contains("tasks")) {
            const auto& tasks = sequenceJson["tasks"];

            for (const auto& taskDef : tasks) {
                std::string taskType = taskDef.value("taskType", "");
                json params = taskDef.value("parameters", json::object());
                int count = taskDef.value("count", 1);

                // Map API task types to internal task types
                std::string internalType;
                if (taskType == "exposure") {
                    internalType = "camera.exposure";
                } else if (taskType == "autofocus") {
                    internalType = "camera.autofocus";
                } else if (taskType == "meridian_flip") {
                    internalType = "mount.meridian_flip";
                } else if (taskType == "park") {
                    internalType = "mount.park";
                } else if (taskType == "slew") {
                    internalType = "mount.slew";
                } else if (taskType == "dither") {
                    internalType = "guider.dither";
                } else {
                    internalType = taskType;
                }

                // Create tasks
                auto& factory = TaskFactory::getInstance();
                for (int i = 0; i < count; ++i) {
                    auto task = factory.createTask(
                        internalType, internalType + "_" + std::to_string(i),
                        params);
                    if (task) {
                        target->addTask(std::move(task));
                    }
                }
            }
        }

        return target;

    } catch (const std::exception& e) {
        spdlog::error("Failed to convert sequence from API JSON: {}", e.what());
        return nullptr;
    }
}

json SequenceConverter::toApiJson(const std::shared_ptr<Target>& target) {
    try {
        json sequenceJson;
        sequenceJson["name"] = target->getName();

        // Convert tasks
        json tasks = json::array();
        const auto& targetTasks = target->getTasks();
        for (const auto& task : targetTasks) {
            tasks.push_back(task->toJson());
        }
        sequenceJson["tasks"] = tasks;

        return sequenceJson;

    } catch (const std::exception& e) {
        spdlog::error("Failed to convert target to API JSON: {}", e.what());
        return json();
    }
}

json SequenceConverter::convertTaskParams(const std::string& taskType,
                                          const json& apiParams) {
    json internalParams = apiParams;

    // Convert camera exposure parameters
    if (taskType == "exposure" || taskType == "camera.exposure") {
        if (apiParams.contains("duration")) {
            internalParams["exposure"] = apiParams["duration"];
        }
        if (apiParams.contains("frameType")) {
            internalParams["type"] = apiParams["frameType"];
        }
    }
    // Convert mount slew parameters
    else if (taskType == "slew" || taskType == "mount.slew") {
        if (apiParams.contains("ra") && apiParams.contains("dec")) {
            internalParams["coordinates"] = {{"ra", apiParams["ra"]},
                                             {"dec", apiParams["dec"]}};
        }
    }
    // Convert focuser parameters
    else if (taskType == "move_focuser" || taskType == "focuser.move") {
        if (apiParams.contains("relative")) {
            internalParams["isRelative"] = apiParams["relative"];
        }
    }

    return internalParams;
}

std::vector<std::string> SequenceConverter::validateSequence(
    const json& sequenceJson) {
    std::vector<std::string> errors;

    // Validate required fields
    if (!sequenceJson.contains("name") || !sequenceJson["name"].is_string()) {
        errors.push_back("Missing or invalid 'name' field");
    }

    if (!sequenceJson.contains("tasks") || !sequenceJson["tasks"].is_array()) {
        errors.push_back("Missing or invalid 'tasks' field (must be array)");
        return errors;
    }

    // Validate trigger if present
    if (sequenceJson.contains("trigger")) {
        const auto& trigger = sequenceJson["trigger"];

        if (!trigger.contains("type")) {
            errors.push_back("Trigger missing 'type' field");
        } else {
            std::string triggerType = trigger["type"];

            if (triggerType != "altitude" && triggerType != "time" &&
                triggerType != "immediate") {
                errors.push_back("Invalid trigger type: " + triggerType);
            }

            if (triggerType == "altitude" && !trigger.contains("target")) {
                errors.push_back("Altitude trigger requires 'target' field");
            }
        }
    }

    // Validate tasks
    const auto& tasks = sequenceJson["tasks"];
    int taskIndex = 0;

    for (const auto& task : tasks) {
        std::string prefix = "Task[" + std::to_string(taskIndex) + "]: ";

        if (!task.contains("taskType")) {
            errors.push_back(prefix + "Missing 'taskType' field");
            taskIndex++;
            continue;
        }

        std::string taskType = task["taskType"];

        // Validate exposure tasks
        if (taskType == "exposure") {
            if (task.contains("parameters")) {
                const auto& params = task["parameters"];
                if (!params.contains("duration")) {
                    errors.push_back(prefix +
                                     "Exposure requires 'duration' parameter");
                }
            } else {
                errors.push_back(prefix + "Exposure task missing 'parameters'");
            }
        }

        // Validate slew tasks
        if (taskType == "slew") {
            if (task.contains("parameters")) {
                const auto& params = task["parameters"];
                if (!params.contains("ra") || !params.contains("dec")) {
                    errors.push_back(prefix +
                                     "Slew requires 'ra' and 'dec' parameters");
                }
            } else {
                errors.push_back(prefix + "Slew task missing 'parameters'");
            }
        }

        // Validate count field if present
        if (task.contains("count")) {
            if (!task["count"].is_number_integer() ||
                task["count"].get<int>() < 1) {
                errors.push_back(
                    prefix +
                    "Invalid 'count' value (must be positive integer)");
            }
        }

        taskIndex++;
    }

    return errors;
}

// ============================================================================
// ErrorMapper Implementation
// ============================================================================

ErrorMapper::ErrorInfo ErrorMapper::mapException(
    const std::exception& exception) {
    ErrorInfo info;
    std::string message = exception.what();

    // Map common exception types
    if (message.find("not found") != std::string::npos) {
        info.httpStatus = 404;
        info.errorCode = "not_found";
        info.message = message;
    } else if (message.find("invalid") != std::string::npos) {
        info.httpStatus = 400;
        info.errorCode = "invalid_parameter";
        info.message = message;
    } else if (message.find("timeout") != std::string::npos) {
        info.httpStatus = 504;
        info.errorCode = "timeout";
        info.message = "Operation timed out: " + message;
    } else if (message.find("busy") != std::string::npos) {
        info.httpStatus = 409;
        info.errorCode = "device_busy";
        info.message = message;
    } else if (message.find("not connected") != std::string::npos) {
        info.httpStatus = 503;
        info.errorCode = "device_not_connected";
        info.message = message;
    } else {
        info.httpStatus = 500;
        info.errorCode = "internal_error";
        info.message = "An unexpected error occurred: " + message;
    }

    return info;
}

ApiResponse ErrorMapper::createErrorResponse(const std::exception& exception) {
    auto info = mapException(exception);
    return ApiResponse::makeError(info.httpStatus, info.errorCode, info.message,
                                  info.details);
}

ApiResponse ErrorMapper::createValidationError(const std::string& field,
                                               const std::string& message,
                                               const json& value) {
    json details;
    details["field"] = field;
    details["message"] = message;
    if (!value.is_null()) {
        details["value"] = value;
    }

    return ApiResponse::makeError(400, "missing_required_field", message,
                                  details);
}

}  // namespace lithium::task::api
