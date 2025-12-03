/*
 * task.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Task data models for HTTP/WebSocket responses

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_TASK_HPP
#define LITHIUM_SERVER_MODELS_TASK_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::models::task {

using json = nlohmann::json;

/**
 * @brief Task status enumeration
 */
enum class TaskStatus {
    Pending,    ///< Task is waiting to be executed
    Running,    ///< Task is currently executing
    Completed,  ///< Task completed successfully
    Failed,     ///< Task failed with an error
    Cancelled   ///< Task was cancelled
};

/**
 * @brief Convert TaskStatus to string
 */
inline auto statusToString(TaskStatus status) -> std::string {
    switch (status) {
        case TaskStatus::Pending:
            return "pending";
        case TaskStatus::Running:
            return "running";
        case TaskStatus::Completed:
            return "completed";
        case TaskStatus::Failed:
            return "failed";
        case TaskStatus::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

/**
 * @brief Parse string to TaskStatus
 */
inline auto stringToStatus(const std::string& str)
    -> std::optional<TaskStatus> {
    if (str == "pending")
        return TaskStatus::Pending;
    if (str == "running")
        return TaskStatus::Running;
    if (str == "completed")
        return TaskStatus::Completed;
    if (str == "failed")
        return TaskStatus::Failed;
    if (str == "cancelled")
        return TaskStatus::Cancelled;
    return std::nullopt;
}

/**
 * @brief Task summary for list responses
 */
struct TaskSummary {
    std::string id;
    std::string type;
    TaskStatus status;
    int priority;
    double progress;
    std::string progressMessage;
    std::string error;
    bool cancelRequested;
    int64_t createdAt;
    int64_t updatedAt;

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["id"] = id;
        j["type"] = type;
        j["status"] = statusToString(status);
        j["priority"] = priority;
        j["progress"] = progress;

        if (!progressMessage.empty()) {
            j["progressMessage"] = progressMessage;
        }
        if (!error.empty()) {
            j["error"] = error;
        }

        j["cancelRequested"] = cancelRequested;
        j["createdAt"] = createdAt;
        j["updatedAt"] = updatedAt;

        return j;
    }
};

/**
 * @brief Task statistics
 */
struct TaskStats {
    size_t totalTasks;
    size_t pending;
    size_t running;
    size_t completed;
    size_t failed;
    size_t cancelled;
    size_t totalSubmitted;
    size_t totalCompleted;
    size_t totalFailed;
    size_t totalCancelled;
    size_t periodicTasks;

    [[nodiscard]] auto toJson() const -> json {
        return {{"total_tasks", totalTasks},
                {"pending", pending},
                {"running", running},
                {"completed", completed},
                {"failed", failed},
                {"cancelled", cancelled},
                {"total_submitted", totalSubmitted},
                {"total_completed", totalCompleted},
                {"total_failed", totalFailed},
                {"total_cancelled", totalCancelled},
                {"periodic_tasks", periodicTasks}};
    }
};

/**
 * @brief Task update event for WebSocket broadcast
 */
struct TaskUpdateEvent {
    std::string taskId;
    std::string taskType;
    TaskStatus status;
    int priority;
    double progress;
    std::string progressMessage;
    std::string error;
    bool cancelRequested;
    json result;
    int64_t createdAt;
    int64_t updatedAt;

    [[nodiscard]] auto toJson() const -> json {
        json event;
        event["type"] = "event";
        event["event"] = "taskUpdated";

        json task;
        task["id"] = taskId;
        task["taskType"] = taskType;
        task["status"] = statusToString(status);
        task["priority"] = priority;
        task["progress"] = progress;

        if (!progressMessage.empty()) {
            task["progressMessage"] = progressMessage;
        }
        if (!error.empty()) {
            task["error"] = error;
        }

        task["cancelRequested"] = cancelRequested;
        task["createdAt"] = createdAt;
        task["updatedAt"] = updatedAt;

        if (!result.is_null()) {
            task["result"] = result;
        }

        event["task"] = task;
        return event;
    }
};

/**
 * @brief Task list response
 */
inline auto makeTaskListResponse(const std::vector<TaskSummary>& tasks,
                                 size_t limit, size_t offset) -> json {
    json taskList = json::array();
    for (const auto& task : tasks) {
        taskList.push_back(task.toJson());
    }

    return {{"tasks", taskList},
            {"count", taskList.size()},
            {"limit", limit},
            {"offset", offset}};
}

}  // namespace lithium::models::task

#endif  // LITHIUM_SERVER_MODELS_TASK_HPP
