/*
 * task.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "task.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "server/command.hpp"
#include "server/models/task.hpp"
#include "server/task_manager.hpp"

namespace lithium::app {

namespace {

using namespace lithium::models::task;

// Convert TaskManager::Status to models::task::TaskStatus
auto toModelStatus(lithium::server::TaskManager::Status status) -> TaskStatus {
    using TMStatus = lithium::server::TaskManager::Status;
    switch (status) {
        case TMStatus::Pending:
            return TaskStatus::Pending;
        case TMStatus::Running:
            return TaskStatus::Running;
        case TMStatus::Completed:
            return TaskStatus::Completed;
        case TMStatus::Failed:
            return TaskStatus::Failed;
        case TMStatus::Cancelled:
            return TaskStatus::Cancelled;
    }
    return TaskStatus::Pending;
}

// Convert string to TaskManager::Status
auto stringToTMStatus(const std::string& str)
    -> std::optional<lithium::server::TaskManager::Status> {
    auto modelStatus = stringToStatus(str);
    if (!modelStatus) {
        return std::nullopt;
    }

    using TMStatus = lithium::server::TaskManager::Status;
    switch (*modelStatus) {
        case TaskStatus::Pending:
            return TMStatus::Pending;
        case TaskStatus::Running:
            return TMStatus::Running;
        case TaskStatus::Completed:
            return TMStatus::Completed;
        case TaskStatus::Failed:
            return TMStatus::Failed;
        case TaskStatus::Cancelled:
            return TMStatus::Cancelled;
    }
    return std::nullopt;
}

// Convert TaskManager::TaskInfo to TaskSummary model
auto toTaskSummary(
    const std::shared_ptr<lithium::server::TaskManager::TaskInfo>& task)
    -> TaskSummary {
    auto toMs = [](auto tp) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   tp.time_since_epoch())
            .count();
    };

    TaskSummary summary;
    summary.id = task->id;
    summary.type = task->type;
    summary.status = toModelStatus(task->status);
    summary.priority = task->priority;
    summary.progress = task->progress;
    summary.progressMessage = task->progressMessage;
    summary.error = task->error;
    summary.cancelRequested = task->cancelRequested.load();
    summary.createdAt = toMs(task->createdAt);
    summary.updatedAt = toMs(task->updatedAt);

    return summary;
}

}  // namespace

void registerTaskCommands(
    std::shared_ptr<CommandDispatcher> dispatcher,
    std::shared_ptr<lithium::server::TaskManager> task_manager) {
    if (!dispatcher || !task_manager) {
        spdlog::error(
            "registerTaskCommands: dispatcher or task_manager is null");
        return;
    }

    // task.list - List tasks with optional filters
    dispatcher->registerCommand<nlohmann::json>(
        "task.list",
        [task_manager](const nlohmann::json& payload) -> nlohmann::json {
            std::vector<std::shared_ptr<lithium::server::TaskManager::TaskInfo>>
                tasks;

            size_t limit = payload.value("limit", 50);
            size_t offset = payload.value("offset", 0);

            if (payload.contains("status")) {
                auto status = stringToTMStatus(payload["status"]);
                if (status) {
                    tasks = task_manager->listTasksByStatus(*status);
                } else {
                    return {{"status", "error"},
                            {"error",
                             {{"code", "invalid_status"},
                              {"message", "Invalid status filter"}}}};
                }
            } else if (payload.contains("type")) {
                tasks = task_manager->listTasksByType(payload["type"]);
            } else if (payload.value("active", false)) {
                tasks = task_manager->listActiveTasks();
            } else {
                tasks = task_manager->listAllTasks(limit, offset);
            }

            // Convert to TaskSummary models
            std::vector<TaskSummary> summaries;
            summaries.reserve(tasks.size());
            for (const auto& task : tasks) {
                summaries.push_back(toTaskSummary(task));
            }

            return {{"status", "success"},
                    {"data", makeTaskListResponse(summaries, limit, offset)}};
        });
    spdlog::info("Registered command: task.list");

    // task.get - Get single task details
    dispatcher->registerCommand<nlohmann::json>(
        "task.get",
        [task_manager](const nlohmann::json& payload) -> nlohmann::json {
            if (!payload.contains("taskId")) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "missing_parameter"},
                          {"message", "taskId is required"}}}};
            }

            std::string task_id = payload["taskId"];
            auto task = task_manager->getTask(task_id);

            if (!task) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "not_found"},
                          {"message", "Task not found: " + task_id}}}};
            }

            return {{"status", "success"},
                    {"data", toTaskSummary(task).toJson()}};
        });
    spdlog::info("Registered command: task.get");

    // task.cancel - Cancel a task
    dispatcher->registerCommand<nlohmann::json>(
        "task.cancel",
        [task_manager](const nlohmann::json& payload) -> nlohmann::json {
            if (!payload.contains("taskId")) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "missing_parameter"},
                          {"message", "taskId is required"}}}};
            }

            std::string task_id = payload["taskId"];
            bool cancelled = task_manager->cancelTask(task_id);

            if (!cancelled) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "not_found"},
                          {"message", "Task not found: " + task_id}}}};
            }

            return {{"status", "success"},
                    {"data", {{"cancelled", true}, {"taskId", task_id}}}};
        });
    spdlog::info("Registered command: task.cancel");

    // task.stats - Get task manager statistics
    dispatcher->registerCommand<nlohmann::json>(
        "task.stats",
        [task_manager](const nlohmann::json& /*payload*/) -> nlohmann::json {
            return {{"status", "success"}, {"data", task_manager->getStats()}};
        });
    spdlog::info("Registered command: task.stats");

    // task.cleanup - Clean up old completed tasks
    dispatcher->registerCommand<nlohmann::json>(
        "task.cleanup",
        [task_manager](const nlohmann::json& payload) -> nlohmann::json {
            int max_age_seconds = payload.value("maxAgeSeconds", 3600);

            size_t removed = task_manager->cleanupOldTasks(
                std::chrono::seconds(max_age_seconds));

            return {
                {"status", "success"},
                {"data",
                 {{"removed", removed}, {"maxAgeSeconds", max_age_seconds}}}};
        });
    spdlog::info("Registered command: task.cleanup");

    // task.progress - Update task progress
    dispatcher->registerCommand<nlohmann::json>(
        "task.progress",
        [task_manager](const nlohmann::json& payload) -> nlohmann::json {
            if (!payload.contains("taskId") || !payload.contains("progress")) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "missing_parameter"},
                          {"message", "taskId and progress are required"}}}};
            }

            std::string task_id = payload["taskId"];
            double progress = payload["progress"];
            std::string message = payload.value("message", "");

            bool updated =
                task_manager->updateProgress(task_id, progress, message);

            if (!updated) {
                return {{"status", "error"},
                        {"error",
                         {{"code", "not_found"},
                          {"message", "Task not found: " + task_id}}}};
            }

            return {{"status", "success"},
                    {"data",
                     {{"updated", true},
                      {"taskId", task_id},
                      {"progress", progress}}}};
        });
    spdlog::info("Registered command: task.progress");

    spdlog::info("Task management commands registered");
}

}  // namespace lithium::app
