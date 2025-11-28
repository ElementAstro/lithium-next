#include "task_manager.hpp"

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
                             Runner runner) -> std::string {
    auto task = std::make_shared<TaskInfo>();
    task->id = generateId();
    task->type = type;
    task->status = Status::Pending;
    task->error.clear();
    task->params = params;
    task->createdAt = std::chrono::system_clock::now();
    task->updatedAt = task->createdAt;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_[task->id] = task;
    }

    // Notify observers about new Pending task
    notifyStatus(task);

    auto loop = event_loop_.lock();
    if (!loop) {
        spdlog::error("TaskManager::submitTask called without valid EventLoop");
        task->status = Status::Failed;
        task->error = "EventLoop not available";
        return task->id;
    }

    // Execute on EventLoop thread pool
    loop->post([this, task, runner]() {
        if (task->cancelRequested.load()) {
            task->status = Status::Cancelled;
            task->updatedAt = std::chrono::system_clock::now();
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
            } else if (task->status == Status::Running ||
                       task->status == Status::Pending) {
                // If runner did not explicitly change status, mark as completed
                task->status = Status::Completed;
            }
        } catch (const std::exception& e) {
            task->status = Status::Failed;
            task->error = e.what();
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
    for (const auto& [id, task] : tasks_) {
        if (task->status == Status::Pending ||
            task->status == Status::Running) {
            result.push_back(task);
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
        }
    }

    notifyStatus(task);
    return true;
}

}  // namespace lithium::server
