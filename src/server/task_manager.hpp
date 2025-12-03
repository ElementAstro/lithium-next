#ifndef LITHIUM_SERVER_TASK_MANAGER_HPP
#define LITHIUM_SERVER_TASK_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
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
        int priority{0};  ///< Task priority (higher = more urgent)
        double progress{0.0};  ///< Progress percentage 0-100
        std::string progressMessage;  ///< Human-readable progress message
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
     * @param priority Optional priority (default 0, higher = more urgent)
     * @return Generated task ID
     */
    auto submitTask(const std::string& type, const json& params, Runner runner,
                    int priority = 0) -> std::string;

    /**
     * @brief Submit a delayed task that starts after specified duration.
     *
     * @param type   Logical task type name
     * @param params Parameter snapshot for this task
     * @param runner Callable that performs the actual work
     * @param delay  Delay before task execution
     * @param priority Optional priority
     * @return Generated task ID
     */
    auto submitDelayedTask(const std::string& type, const json& params,
                           Runner runner, std::chrono::milliseconds delay,
                           int priority = 0) -> std::string;

    /**
     * @brief Look up a task by ID.
     */
    auto getTask(const std::string& id) const -> TaskInfoPtr;

    /**
     * @brief List tasks that are still active (Pending or Running).
     */
    auto listActiveTasks() const -> std::vector<TaskInfoPtr>;

    /**
     * @brief List all tasks (including completed/failed/cancelled).
     *
     * @param limit Maximum number of tasks to return (0 = all)
     * @param offset Number of tasks to skip
     * @return Vector of task info pointers
     */
    auto listAllTasks(size_t limit = 0, size_t offset = 0) const
        -> std::vector<TaskInfoPtr>;

    /**
     * @brief List tasks by status.
     */
    auto listTasksByStatus(Status status) const -> std::vector<TaskInfoPtr>;

    /**
     * @brief List tasks by type.
     */
    auto listTasksByType(const std::string& type) const
        -> std::vector<TaskInfoPtr>;

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
     * @brief Update task progress.
     *
     * @param id Task ID
     * @param progress Progress percentage (0-100)
     * @param message Optional progress message
     * @return true if task exists and was updated
     */
    auto updateProgress(const std::string& id, double progress,
                        const std::string& message = "") -> bool;

    /**
     * @brief Set task result.
     *
     * @param id Task ID
     * @param result Result JSON
     * @return true if task exists and was updated
     */
    auto setResult(const std::string& id, const json& result) -> bool;

    /**
     * @brief Mark task as failed with error message.
     *
     * @param id Task ID
     * @param error Error message
     * @return true if task exists and was updated
     */
    auto failTask(const std::string& id, const std::string& error) -> bool;

    /**
     * @brief Remove completed/failed/cancelled tasks older than specified age.
     *
     * @param max_age Maximum age of tasks to keep
     * @return Number of tasks removed
     */
    auto cleanupOldTasks(std::chrono::seconds max_age) -> size_t;

    /**
     * @brief Get task statistics.
     */
    auto getStats() const -> json;

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

    /**
     * @brief Get the shared EventLoop instance.
     */
    auto getEventLoop() const -> std::shared_ptr<app::EventLoop>;

    /**
     * @brief Schedule a periodic task.
     *
     * @param type Task type name
     * @param interval Interval between executions
     * @param runner Task runner
     * @return Periodic task ID (can be used to cancel)
     */
    auto schedulePeriodicTask(const std::string& type,
                              std::chrono::milliseconds interval,
                              std::function<void()> runner) -> std::string;

    /**
     * @brief Cancel a periodic task.
     */
    auto cancelPeriodicTask(const std::string& id) -> bool;

private:
    std::weak_ptr<app::EventLoop> event_loop_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, TaskInfoPtr> tasks_;
    std::deque<std::string> task_order_;  ///< Maintains insertion order
    StatusCallback status_callback_;

    // Periodic tasks
    struct PeriodicTaskInfo {
        std::string type;
        std::chrono::milliseconds interval;
        std::function<void()> runner;
        std::atomic<bool> cancelled{false};
    };
    std::unordered_map<std::string, std::shared_ptr<PeriodicTaskInfo>>
        periodic_tasks_;

    // Statistics
    std::atomic<size_t> total_submitted_{0};
    std::atomic<size_t> total_completed_{0};
    std::atomic<size_t> total_failed_{0};
    std::atomic<size_t> total_cancelled_{0};

    void notifyStatus(const TaskInfoPtr& task);
    static auto generateId() -> std::string;
    void runPeriodicTask(const std::string& id);
};

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_TASK_MANAGER_HPP
