#include "task_manager.hpp"

#include <algorithm>

#include <spdlog/spdlog.h>

namespace lithium::server {

TaskManager::TaskManager(std::shared_ptr<app::EventLoop> event_loop)
    : event_loop_(std::move(event_loop)) {}

auto TaskManager::generateId() -> std::string {
    return atom::utils::UUID().toString();
}

void TaskManager::notifyStatus(const TaskInfoPtr& task) {
    StatusCallback cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb = status_callback_;
    }
    if (cb) {
        cb(*task);
    }
}

auto TaskManager::submitTask(const std::string& type, const json& params,
                             Runner runner, int priority) -> std::string {
    auto task = std::make_shared<TaskInfo>();
    task->id = generateId();
    task->type = type;
    task->status = Status::Pending;
    task->error.clear();
    task->params = params;
    task->priority = priority;
    task->progress = 0.0;
    task->createdAt = std::chrono::system_clock::now();
    task->updatedAt = task->createdAt;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task->id] = task;
        task_order_.push_back(task->id);
    }

    total_submitted_++;

    // Notify observers about new Pending task
    notifyStatus(task);

    auto loop = event_loop_.lock();
    if (!loop) {
        spdlog::error("TaskManager::submitTask called without valid EventLoop");
        task->status = Status::Failed;
        task->error = "EventLoop not available";
        total_failed_++;
        notifyStatus(task);
        return task->id;
    }

    // Execute on EventLoop thread pool with priority
    loop->post(priority, [this, task, runner]() {
        if (task->cancelRequested.load()) {
            task->status = Status::Cancelled;
            task->updatedAt = std::chrono::system_clock::now();
            total_cancelled_++;
            spdlog::info("Task {} cancelled before start", task->id);
            notifyStatus(task);
            return;
        }

        task->status = Status::Running;
        task->updatedAt = std::chrono::system_clock::now();
        notifyStatus(task);
        try {
            runner(task);
            if (task->cancelRequested.load() &&
                task->status != Status::Failed) {
                task->status = Status::Cancelled;
                total_cancelled_++;
            } else if (task->status == Status::Running ||
                       task->status == Status::Pending) {
                // If runner did not explicitly change status, mark as completed
                task->status = Status::Completed;
                task->progress = 100.0;
                total_completed_++;
            }
        } catch (const std::exception& e) {
            task->status = Status::Failed;
            task->error = e.what();
            total_failed_++;
            spdlog::error("Task {} failed: {}", task->id, e.what());
        }
        task->updatedAt = std::chrono::system_clock::now();
        notifyStatus(task);
    });

    return task->id;
}

auto TaskManager::submitDelayedTask(const std::string& type, const json& params,
                                    Runner runner,
                                    std::chrono::milliseconds delay,
                                    int priority) -> std::string {
    auto task = std::make_shared<TaskInfo>();
    task->id = generateId();
    task->type = type;
    task->status = Status::Pending;
    task->error.clear();
    task->params = params;
    task->priority = priority;
    task->progress = 0.0;
    task->progressMessage = "Waiting for delayed start";
    task->createdAt = std::chrono::system_clock::now();
    task->updatedAt = task->createdAt;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task->id] = task;
        task_order_.push_back(task->id);
    }

    total_submitted_++;
    notifyStatus(task);

    auto loop = event_loop_.lock();
    if (!loop) {
        spdlog::error(
            "TaskManager::submitDelayedTask called without valid EventLoop");
        task->status = Status::Failed;
        task->error = "EventLoop not available";
        total_failed_++;
        notifyStatus(task);
        return task->id;
    }

    // Execute on EventLoop thread pool after delay
    loop->postDelayed(delay, priority, [this, task, runner]() {
        if (task->cancelRequested.load()) {
            task->status = Status::Cancelled;
            task->updatedAt = std::chrono::system_clock::now();
            total_cancelled_++;
            spdlog::info("Task {} cancelled before start", task->id);
            notifyStatus(task);
            return;
        }

        task->status = Status::Running;
        task->progressMessage.clear();
        task->updatedAt = std::chrono::system_clock::now();
        notifyStatus(task);
        try {
            runner(task);
            if (task->cancelRequested.load() &&
                task->status != Status::Failed) {
                task->status = Status::Cancelled;
                total_cancelled_++;
            } else if (task->status == Status::Running ||
                       task->status == Status::Pending) {
                task->status = Status::Completed;
                task->progress = 100.0;
                total_completed_++;
            }
        } catch (const std::exception& e) {
            task->status = Status::Failed;
            task->error = e.what();
            total_failed_++;
            spdlog::error("Task {} failed: {}", task->id, e.what());
        }
        task->updatedAt = std::chrono::system_clock::now();
        notifyStatus(task);
    });

    return task->id;
}

auto TaskManager::getTask(const std::string& id) const -> TaskInfoPtr {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return nullptr;
    }
    return it->second;
}

auto TaskManager::listActiveTasks() const -> std::vector<TaskInfoPtr> {
    std::vector<TaskInfoPtr> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& id : task_order_) {
        auto it = tasks_.find(id);
        if (it != tasks_.end()) {
            const auto& task = it->second;
            if (task->status == Status::Pending ||
                task->status == Status::Running) {
                result.push_back(task);
            }
        }
    }
    return result;
}

auto TaskManager::listAllTasks(size_t limit, size_t offset) const
    -> std::vector<TaskInfoPtr> {
    std::vector<TaskInfoPtr> result;
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = 0;
    size_t skipped = 0;

    // Iterate in reverse order (newest first)
    for (auto it = task_order_.rbegin(); it != task_order_.rend(); ++it) {
        if (skipped < offset) {
            skipped++;
            continue;
        }

        auto task_it = tasks_.find(*it);
        if (task_it != tasks_.end()) {
            result.push_back(task_it->second);
            count++;
            if (limit > 0 && count >= limit) {
                break;
            }
        }
    }

    return result;
}

auto TaskManager::listTasksByStatus(Status status) const
    -> std::vector<TaskInfoPtr> {
    std::vector<TaskInfoPtr> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& id : task_order_) {
        auto it = tasks_.find(id);
        if (it != tasks_.end() && it->second->status == status) {
            result.push_back(it->second);
        }
    }
    return result;
}

auto TaskManager::listTasksByType(const std::string& type) const
    -> std::vector<TaskInfoPtr> {
    std::vector<TaskInfoPtr> result;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& id : task_order_) {
        auto it = tasks_.find(id);
        if (it != tasks_.end() && it->second->type == type) {
            result.push_back(it->second);
        }
    }
    return result;
}

auto TaskManager::cancelTask(const std::string& id) -> bool {
    TaskInfoPtr task;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return false;
        }

        task = it->second;
        task->cancelRequested.store(true);

        // If task has not started yet, mark as cancelled immediately
        if (task->status == Status::Pending) {
            task->status = Status::Cancelled;
            task->updatedAt = std::chrono::system_clock::now();
            total_cancelled_++;
        }
    }

    notifyStatus(task);
    return true;
}

auto TaskManager::updateProgress(const std::string& id, double progress,
                                 const std::string& message) -> bool {
    TaskInfoPtr task;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return false;
        }
        task = it->second;
    }

    task->progress = std::clamp(progress, 0.0, 100.0);
    if (!message.empty()) {
        task->progressMessage = message;
    }
    task->updatedAt = std::chrono::system_clock::now();

    notifyStatus(task);
    return true;
}

auto TaskManager::setResult(const std::string& id, const json& result) -> bool {
    TaskInfoPtr task;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return false;
        }
        task = it->second;
    }

    task->result = result;
    task->updatedAt = std::chrono::system_clock::now();

    notifyStatus(task);
    return true;
}

auto TaskManager::failTask(const std::string& id, const std::string& error)
    -> bool {
    TaskInfoPtr task;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return false;
        }
        task = it->second;
    }

    task->status = Status::Failed;
    task->error = error;
    task->updatedAt = std::chrono::system_clock::now();
    total_failed_++;

    notifyStatus(task);
    return true;
}

auto TaskManager::cleanupOldTasks(std::chrono::seconds max_age) -> size_t {
    auto now = std::chrono::system_clock::now();
    size_t removed = 0;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = task_order_.begin();
    while (it != task_order_.end()) {
        auto task_it = tasks_.find(*it);
        if (task_it == tasks_.end()) {
            it = task_order_.erase(it);
            continue;
        }

        const auto& task = task_it->second;
        // Only cleanup finished tasks
        if (task->status != Status::Pending && task->status != Status::Running) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                now - task->updatedAt);
            if (age > max_age) {
                tasks_.erase(task_it);
                it = task_order_.erase(it);
                removed++;
                continue;
            }
        }
        ++it;
    }

    if (removed > 0) {
        spdlog::info("TaskManager: cleaned up {} old tasks", removed);
    }

    return removed;
}

auto TaskManager::getStats() const -> json {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t pending = 0, running = 0, completed = 0, failed = 0, cancelled = 0;
    for (const auto& [id, task] : tasks_) {
        switch (task->status) {
            case Status::Pending:
                pending++;
                break;
            case Status::Running:
                running++;
                break;
            case Status::Completed:
                completed++;
                break;
            case Status::Failed:
                failed++;
                break;
            case Status::Cancelled:
                cancelled++;
                break;
        }
    }

    return {{"total_tasks", tasks_.size()},
            {"pending", pending},
            {"running", running},
            {"completed", completed},
            {"failed", failed},
            {"cancelled", cancelled},
            {"total_submitted", total_submitted_.load()},
            {"total_completed", total_completed_.load()},
            {"total_failed", total_failed_.load()},
            {"total_cancelled", total_cancelled_.load()},
            {"periodic_tasks", periodic_tasks_.size()}};
}

auto TaskManager::getEventLoop() const -> std::shared_ptr<app::EventLoop> {
    return event_loop_.lock();
}

auto TaskManager::schedulePeriodicTask(const std::string& type,
                                       std::chrono::milliseconds interval,
                                       std::function<void()> runner)
    -> std::string {
    auto id = generateId();

    auto info = std::make_shared<PeriodicTaskInfo>();
    info->type = type;
    info->interval = interval;
    info->runner = std::move(runner);
    info->cancelled.store(false);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        periodic_tasks_[id] = info;
    }

    spdlog::info("Scheduled periodic task {} of type {} with interval {}ms", id,
                 type, interval.count());

    // Start the periodic execution
    runPeriodicTask(id);

    return id;
}

auto TaskManager::cancelPeriodicTask(const std::string& id) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = periodic_tasks_.find(id);
    if (it == periodic_tasks_.end()) {
        return false;
    }

    it->second->cancelled.store(true);
    periodic_tasks_.erase(it);
    spdlog::info("Cancelled periodic task {}", id);
    return true;
}

void TaskManager::runPeriodicTask(const std::string& id) {
    std::shared_ptr<PeriodicTaskInfo> info;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = periodic_tasks_.find(id);
        if (it == periodic_tasks_.end()) {
            return;
        }
        info = it->second;
    }

    if (info->cancelled.load()) {
        return;
    }

    auto loop = event_loop_.lock();
    if (!loop) {
        spdlog::error("EventLoop not available for periodic task {}", id);
        return;
    }

    // Schedule the task execution
    loop->postDelayed(info->interval, [this, id, info]() {
        if (info->cancelled.load()) {
            return;
        }

        try {
            info->runner();
        } catch (const std::exception& e) {
            spdlog::error("Periodic task {} failed: {}", id, e.what());
        }

        // Schedule next execution
        runPeriodicTask(id);
    });
}

}  // namespace lithium::server
