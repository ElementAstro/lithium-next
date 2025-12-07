/*
 * server_status.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: Server Status Controller - Provides HTTP endpoints for server
             health, WebSocket stats, task manager stats, and event loop status

**************************************************/

#ifndef LITHIUM_SERVER_CONTROLLER_SERVER_STATUS_HPP
#define LITHIUM_SERVER_CONTROLLER_SERVER_STATUS_HPP

#include "../controller.hpp"
#include "server/models/server.hpp"
#include "server/models/task.hpp"
#include "server/utils/response.hpp"

#include <chrono>
#include <memory>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

// Forward declarations
class WebSocketServer;

namespace lithium::server {
class TaskManager;
}

namespace lithium::app {
class EventLoop;
}

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for server status, health checks, and component statistics
 *
 * Provides HTTP endpoints to query:
 * - Server health and uptime
 * - WebSocket server statistics
 * - Task manager statistics
 * - Event loop status
 */
class ServerStatusController : public Controller {
private:
    static std::weak_ptr<WebSocketServer> websocket_server_;
    static std::weak_ptr<TaskManager> task_manager_;
    static std::weak_ptr<app::EventLoop> event_loop_;
    static std::chrono::steady_clock::time_point start_time_;

public:
    /**
     * @brief Set the WebSocket server instance
     */
    static void setWebSocketServer(std::shared_ptr<WebSocketServer> ws) {
        websocket_server_ = ws;
    }

    /**
     * @brief Set the TaskManager instance
     */
    static void setTaskManager(std::shared_ptr<TaskManager> tm) {
        task_manager_ = tm;
    }

    /**
     * @brief Set the EventLoop instance
     */
    static void setEventLoop(std::shared_ptr<app::EventLoop> el) {
        event_loop_ = el;
    }

    /**
     * @brief Initialize start time (call once at server startup)
     */
    static void initializeStartTime() {
        start_time_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Register all server status routes
     */
    void registerRoutes(ServerApp& app) override {
        // Initialize start time if not already done
        static bool initialized = false;
        if (!initialized) {
            initializeStartTime();
            initialized = true;
        }

        // ===== HEALTH CHECK =====

        // Simple health check endpoint
        CROW_ROUTE(app, "/api/v1/health").methods("GET"_method)([]() {
            nlohmann::json result = {{"status", "healthy"},
                                     {"timestamp", getCurrentTimestamp()}};
            return ResponseBuilder::success(result);
        });

        // Detailed health check with component status
        CROW_ROUTE(app, "/api/v1/health/detailed").methods("GET"_method)([]() {
            nlohmann::json result;
            result["status"] = "healthy";
            result["timestamp"] = getCurrentTimestamp();
            result["uptime_seconds"] = getUptimeSeconds();

            // Check WebSocket server
            nlohmann::json ws_status;
            if (auto ws = websocket_server_.lock()) {
                ws_status["available"] = true;
                ws_status["running"] = ws->is_running();
                ws_status["active_connections"] = ws->get_active_connections();
            } else {
                ws_status["available"] = false;
            }
            result["websocket"] = ws_status;

            // Check TaskManager
            nlohmann::json tm_status;
            if (auto tm = task_manager_.lock()) {
                tm_status["available"] = true;
                auto stats = tm->getStats();
                tm_status["active_tasks"] = stats["pending"].get<size_t>() +
                                            stats["running"].get<size_t>();
            } else {
                tm_status["available"] = false;
            }
            result["task_manager"] = tm_status;

            // Check EventLoop
            nlohmann::json el_status;
            if (auto el = event_loop_.lock()) {
                el_status["available"] = true;
            } else {
                el_status["available"] = false;
            }
            result["event_loop"] = el_status;

            return ResponseBuilder::success(result);
        });

        // ===== SERVER UPTIME =====

        CROW_ROUTE(app, "/api/v1/server/uptime").methods("GET"_method)([]() {
            auto uptime = getUptimeSeconds();
            nlohmann::json result = {{"uptime_seconds", uptime},
                                     {"uptime_formatted", formatUptime(uptime)},
                                     {"start_time", getStartTimeISO()}};
            return ResponseBuilder::success(result);
        });

        // ===== WEBSOCKET STATISTICS =====

        CROW_ROUTE(app, "/api/v1/websocket/stats").methods("GET"_method)([]() {
            auto ws = websocket_server_.lock();
            if (!ws) {
                return ResponseBuilder::serviceUnavailable(
                    "WebSocket server not available");
            }

            nlohmann::json result = ws->get_stats();
            result["running"] = ws->is_running();
            result["subscribed_topics"] = ws->get_subscribed_topics();

            return ResponseBuilder::success(result);
        });

        CROW_ROUTE(app, "/api/v1/websocket/connections")
            .methods("GET"_method)([]() {
                auto ws = websocket_server_.lock();
                if (!ws) {
                    return ResponseBuilder::serviceUnavailable(
                        "WebSocket server not available");
                }

                nlohmann::json result = {
                    {"active_connections", ws->get_active_connections()},
                    {"running", ws->is_running()}};

                return ResponseBuilder::success(result);
            });

        // ===== TASK MANAGER STATISTICS =====

        CROW_ROUTE(app, "/api/v1/tasks/stats").methods("GET"_method)([]() {
            auto tm = task_manager_.lock();
            if (!tm) {
                return ResponseBuilder::serviceUnavailable(
                    "TaskManager not available");
            }

            return ResponseBuilder::success(tm->getStats());
        });

        // List all tasks with pagination
        CROW_ROUTE(app, "/api/v1/tasks")
            .methods("GET"_method)([](const crow::request& req) {
                auto tm = task_manager_.lock();
                if (!tm) {
                    return ResponseBuilder::serviceUnavailable(
                        "TaskManager not available");
                }

                // Parse query parameters
                size_t limit = 50;
                size_t offset = 0;
                std::string status_filter;
                std::string type_filter;

                auto url_params = crow::query_string(req.url_params);
                if (url_params.get("limit")) {
                    limit = std::stoul(url_params.get("limit"));
                }
                if (url_params.get("offset")) {
                    offset = std::stoul(url_params.get("offset"));
                }
                if (url_params.get("status")) {
                    status_filter = url_params.get("status");
                }
                if (url_params.get("type")) {
                    type_filter = url_params.get("type");
                }

                std::vector<std::shared_ptr<TaskManager::TaskInfo>> tasks;

                if (!status_filter.empty()) {
                    TaskManager::Status status;
                    if (status_filter == "pending") {
                        status = TaskManager::Status::Pending;
                    } else if (status_filter == "running") {
                        status = TaskManager::Status::Running;
                    } else if (status_filter == "completed") {
                        status = TaskManager::Status::Completed;
                    } else if (status_filter == "failed") {
                        status = TaskManager::Status::Failed;
                    } else if (status_filter == "cancelled") {
                        status = TaskManager::Status::Cancelled;
                    } else {
                        return ResponseBuilder::badRequest(
                            "Invalid status filter: " + status_filter);
                    }
                    tasks = tm->listTasksByStatus(status);
                } else if (!type_filter.empty()) {
                    tasks = tm->listTasksByType(type_filter);
                } else {
                    tasks = tm->listAllTasks(limit, offset);
                }

                nlohmann::json task_list = nlohmann::json::array();
                for (const auto& task : tasks) {
                    task_list.push_back(taskToJson(task));
                }

                nlohmann::json result = {{"tasks", task_list},
                                         {"count", task_list.size()},
                                         {"limit", limit},
                                         {"offset", offset}};

                return ResponseBuilder::success(result);
            });

        // Get single task details
        CROW_ROUTE(app, "/api/v1/tasks/<string>")
            .methods("GET"_method)([](const std::string& task_id) {
                auto tm = task_manager_.lock();
                if (!tm) {
                    return ResponseBuilder::serviceUnavailable(
                        "TaskManager not available");
                }

                auto task = tm->getTask(task_id);
                if (!task) {
                    return ResponseBuilder::notFound("Task not found: " +
                                                     task_id);
                }

                return ResponseBuilder::success(taskToJson(task));
            });

        // Update task progress (for internal use)
        CROW_ROUTE(app, "/api/v1/tasks/<string>/progress")
            .methods("PUT"_method)([](const crow::request& req,
                                      const std::string& task_id) {
                auto tm = task_manager_.lock();
                if (!tm) {
                    return ResponseBuilder::serviceUnavailable(
                        "TaskManager not available");
                }

                try {
                    auto body = nlohmann::json::parse(req.body);
                    double progress = body.value("progress", 0.0);
                    std::string message = body.value("message", "");

                    if (tm->updateProgress(task_id, progress, message)) {
                        return ResponseBuilder::success(
                            {{"updated", true}, {"task_id", task_id}});
                    } else {
                        return ResponseBuilder::notFound("Task not found: " +
                                                         task_id);
                    }
                } catch (const std::exception& e) {
                    return ResponseBuilder::badRequest(
                        std::string("Invalid request: ") + e.what());
                }
            });

        // Cancel a task
        CROW_ROUTE(app, "/api/v1/tasks/<string>/cancel")
            .methods("POST"_method)([](const std::string& task_id) {
                auto tm = task_manager_.lock();
                if (!tm) {
                    return ResponseBuilder::serviceUnavailable(
                        "TaskManager not available");
                }

                if (tm->cancelTask(task_id)) {
                    return ResponseBuilder::success(
                        {{"cancelled", true}, {"task_id", task_id}});
                } else {
                    return ResponseBuilder::notFound("Task not found: " +
                                                     task_id);
                }
            });

        // Cleanup old tasks
        CROW_ROUTE(app, "/api/v1/tasks/cleanup")
            .methods("POST"_method)([](const crow::request& req) {
                auto tm = task_manager_.lock();
                if (!tm) {
                    return ResponseBuilder::serviceUnavailable(
                        "TaskManager not available");
                }

                try {
                    int max_age_seconds = 3600;  // Default 1 hour
                    if (!req.body.empty()) {
                        auto body = nlohmann::json::parse(req.body);
                        max_age_seconds = body.value("max_age_seconds", 3600);
                    }

                    size_t removed = tm->cleanupOldTasks(
                        std::chrono::seconds(max_age_seconds));

                    return ResponseBuilder::success(
                        {{"removed", removed},
                         {"max_age_seconds", max_age_seconds}});
                } catch (const std::exception& e) {
                    return ResponseBuilder::badRequest(
                        std::string("Invalid request: ") + e.what());
                }
            });

        LOG_INFO("ServerStatusController routes registered");
    }

private:
    static auto getCurrentTimestamp() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    static auto getUptimeSeconds() -> int64_t {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now -
                                                                start_time_)
            .count();
    }

    static auto getStartTimeISO() -> std::string {
        // Approximate start time in system clock
        auto now_steady = std::chrono::steady_clock::now();
        auto now_system = std::chrono::system_clock::now();
        auto uptime = now_steady - start_time_;
        auto start_system = now_system - uptime;
        auto time_t = std::chrono::system_clock::to_time_t(start_system);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    // Use model's formatUptime
    static auto formatUptime(int64_t seconds) -> std::string {
        return models::server::UptimeInfo::formatUptime(seconds);
    }

    // Convert TaskManager::Status to model TaskStatus
    static auto toModelTaskStatus(TaskManager::Status status)
        -> models::task::TaskStatus {
        using TMStatus = TaskManager::Status;
        using ModelStatus = models::task::TaskStatus;
        switch (status) {
            case TMStatus::Pending:
                return ModelStatus::Pending;
            case TMStatus::Running:
                return ModelStatus::Running;
            case TMStatus::Completed:
                return ModelStatus::Completed;
            case TMStatus::Failed:
                return ModelStatus::Failed;
            case TMStatus::Cancelled:
                return ModelStatus::Cancelled;
        }
        return ModelStatus::Pending;
    }

    // Convert TaskManager::TaskInfo to TaskSummary model
    static auto toTaskSummary(
        const std::shared_ptr<TaskManager::TaskInfo>& task)
        -> models::task::TaskSummary {
        auto toMs = [](auto tp) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       tp.time_since_epoch())
                .count();
        };

        models::task::TaskSummary summary;
        summary.id = task->id;
        summary.type = task->type;
        summary.status = toModelTaskStatus(task->status);
        summary.priority = task->priority;
        summary.progress = task->progress;
        summary.progressMessage = task->progressMessage;
        summary.error = task->error;
        summary.cancelRequested = task->cancelRequested.load();
        summary.createdAt = toMs(task->createdAt);
        summary.updatedAt = toMs(task->updatedAt);

        return summary;
    }

    static auto taskToJson(const std::shared_ptr<TaskManager::TaskInfo>& task)
        -> nlohmann::json {
        return toTaskSummary(task).toJson();
    }
};

// Static member definitions (in header for simplicity, should be in .cpp)
inline std::weak_ptr<WebSocketServer> ServerStatusController::websocket_server_;
inline std::weak_ptr<TaskManager> ServerStatusController::task_manager_;
inline std::weak_ptr<app::EventLoop> ServerStatusController::event_loop_;
inline std::chrono::steady_clock::time_point
    ServerStatusController::start_time_ = std::chrono::steady_clock::now();

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SERVER_STATUS_HPP
