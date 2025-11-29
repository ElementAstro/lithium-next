#ifndef LITHIUM_SERVER_TASK_MANAGER_HPP
#define LITHIUM_SERVER_TASK_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "atom/utils/uuid.hpp"

#include "eventloop.hpp"

namespace lithium::server {

/**
 * @brief Asynchronous task manager built on top of EventLoop.
 *
 * This component runs blocking camera/device/script operations on the shared
 * EventLoop thread pool, tracks their status, and exposes basic lifecycle
 * controls for the REST API layer.
 */
class TaskManager {
public:
    using json = nlohmann::json;

    enum class Status { Pending, Running, Completed, Failed, Cancelled };

    struct TaskInfo {
        std::string id;
        std::string type;
        Status status{Status::Pending};
        std::string error;
        json params;
        json result;
        std::chrono::system_clock::time_point createdAt;
        std::chrono::system_clock::time_point updatedAt;
        std::atomic<bool> cancelRequested{false};
    };

    using TaskInfoPtr = std::shared_ptr<TaskInfo>;
    using Runner = std::function<void(const TaskInfoPtr&)>;
    using StatusCallback = std::function<void(const TaskInfo&)>;

    explicit TaskManager(std::shared_ptr<app::EventLoop> event_loop);

    /**
     * @brief Submit a new asynchronous task.
     *
     * The provided runner will be executed on the EventLoop thread pool.
     *
     * @param type   Logical task type name
     * @param params Parameter snapshot for this task
     * @param runner Callable that performs the actual work
     * @return Generated task ID
     */
    auto submitTask(const std::string& type, const json& params, Runner runner)
        -> std::string;

    /**
     * @brief Look up a task by ID.
     */
    auto getTask(const std::string& id) const -> TaskInfoPtr;

    /**
     * @brief List tasks that are still active (Pending or Running).
     */
    auto listActiveTasks() const -> std::vector<TaskInfoPtr>;

    /**
     * @brief Request cancellation of a task.
     *
     * Cancellation is best-effort: if the task has not yet started it will be
     * marked Cancelled; if it is already running, the cancel flag is set and it
     * is up to the underlying operation to honour it.
     *
     * @return true if the task existed, false otherwise
     */
    auto cancelTask(const std::string& id) -> bool;

    /**
     * @brief Install a callback invoked whenever a task status changes.
     *
     * The callback is called from the EventLoop worker thread that executes
     * the task, so implementations must be thread-safe and non-blocking.
     */
    void setStatusCallback(StatusCallback cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_callback_ = std::move(cb);
    }

private:
    std::weak_ptr<app::EventLoop> event_loop_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, TaskInfoPtr> tasks_;
    StatusCallback status_callback_;

    void notifyStatus(const TaskInfoPtr& task);
    static auto generateId() -> std::string;
};

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_TASK_MANAGER_HPP
