/*
 * interpreter_pool.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "interpreter_pool.hpp"

#include <spdlog/spdlog.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace lithium {

// Task wrapper for priority queue
struct PrioritizedTask {
    std::function<PythonTaskResult()> task;
    TaskPriority priority;
    std::chrono::steady_clock::time_point submitTime;
    size_t sequenceNumber;

    bool operator<(const PrioritizedTask& other) const {
        // Higher priority first, then earlier submission time
        if (static_cast<int>(priority) != static_cast<int>(other.priority)) {
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
        return submitTime > other.submitTime;
    }
};

// InterpreterGuard Implementation
InterpreterGuard::InterpreterGuard(InterpreterPool* pool, size_t index,
                                   std::unique_ptr<py::gil_scoped_acquire> gil)
    : pool_(pool), index_(index), gil_(std::move(gil)), valid_(true) {}

InterpreterGuard::InterpreterGuard(InterpreterGuard&& other) noexcept
    : pool_(other.pool_),
      index_(other.index_),
      gil_(std::move(other.gil_)),
      valid_(other.valid_) {
    other.pool_ = nullptr;
    other.valid_ = false;
}

InterpreterGuard& InterpreterGuard::operator=(InterpreterGuard&& other) noexcept {
    if (this != &other) {
        release();
        pool_ = other.pool_;
        index_ = other.index_;
        gil_ = std::move(other.gil_);
        valid_ = other.valid_;
        other.pool_ = nullptr;
        other.valid_ = false;
    }
    return *this;
}

InterpreterGuard::~InterpreterGuard() {
    release();
}

bool InterpreterGuard::isValid() const noexcept {
    return valid_ && pool_ != nullptr;
}

size_t InterpreterGuard::getIndex() const noexcept {
    return index_;
}

void InterpreterGuard::release() {
    if (valid_ && pool_) {
        gil_.reset();  // Release GIL first
        pool_->release(index_);
        valid_ = false;
        pool_ = nullptr;
    }
}

// InterpreterPool Implementation
class InterpreterPool::Impl {
public:
    explicit Impl(const InterpreterPoolConfig& config)
        : config_(config),
          initialized_(false),
          shuttingDown_(false),
          sequenceCounter_(0) {
        spdlog::info("InterpreterPool created with pool size: {}", config.poolSize);
    }

    ~Impl() {
        shutdown(false);
    }

    InterpreterResult<void> initialize() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_) {
            return {};  // Already initialized
        }

        spdlog::info("Initializing interpreter pool...");

        try {
            // Ensure Python is initialized (only once in main thread)
            if (!Py_IsInitialized()) {
                spdlog::error("Python interpreter not initialized. "
                              "Please use py::scoped_interpreter in main.");
                return std::unexpected(InterpreterPoolError::PoolNotInitialized);
            }

            // Initialize interpreter availability tracking
            for (size_t i = 0; i < config_.poolSize; ++i) {
                availableInterpreters_.insert(i);
            }

            // Preload modules if configured
            if (config_.preloadModules && !config_.modulesToPreload.empty()) {
                py::gil_scoped_acquire gil;
                for (const auto& moduleName : config_.modulesToPreload) {
                    try {
                        py::module::import(moduleName.c_str());
                        spdlog::debug("Preloaded module: {}", moduleName);
                    } catch (const py::error_already_set& e) {
                        spdlog::warn("Failed to preload module {}: {}", moduleName, e.what());
                    }
                }
            }

            // Start worker threads
            size_t numWorkers = config_.workerThreads > 0 ?
                                config_.workerThreads : config_.poolSize;

            for (size_t i = 0; i < numWorkers; ++i) {
                workers_.emplace_back([this, i] { workerThread(i); });
            }

            initialized_ = true;
            spdlog::info("Interpreter pool initialized with {} workers", numWorkers);
            return {};

        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize interpreter pool: {}", e.what());
            return std::unexpected(InterpreterPoolError::PoolNotInitialized);
        }
    }

    void shutdown(bool waitForTasks) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!initialized_ || shuttingDown_) {
                return;
            }
            shuttingDown_ = true;
        }

        spdlog::info("Shutting down interpreter pool (wait={})", waitForTasks);

        // Notify all workers to stop
        taskCondition_.notify_all();

        // Wait for workers to finish
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();

        // Clear task queue if not waiting
        if (!waitForTasks) {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!taskQueue_.empty()) {
                taskQueue_.pop();
                stats_.cancelledTasks++;
            }
        }

        initialized_ = false;
        spdlog::info("Interpreter pool shut down");
    }

    bool isInitialized() const noexcept {
        return initialized_;
    }

    bool isShuttingDown() const noexcept {
        return shuttingDown_;
    }

    InterpreterResult<InterpreterGuard> acquire(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!initialized_) {
            return std::unexpected(InterpreterPoolError::PoolNotInitialized);
        }

        if (shuttingDown_) {
            return std::unexpected(InterpreterPoolError::ShutdownInProgress);
        }

        // Wait for an available interpreter
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (availableInterpreters_.empty()) {
            if (interpreterCondition_.wait_until(lock, deadline) ==
                std::cv_status::timeout) {
                return std::unexpected(InterpreterPoolError::Timeout);
            }

            if (shuttingDown_) {
                return std::unexpected(InterpreterPoolError::ShutdownInProgress);
            }
        }

        // Get an available interpreter
        auto it = availableInterpreters_.begin();
        size_t index = *it;
        availableInterpreters_.erase(it);
        busyInterpreters_.insert(index);

        // Create GIL guard
        auto gil = std::make_unique<py::gil_scoped_acquire>();

        return InterpreterGuard(
            reinterpret_cast<InterpreterPool*>(this), index, std::move(gil));
    }

    void release(size_t index) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = busyInterpreters_.find(index);
        if (it != busyInterpreters_.end()) {
            busyInterpreters_.erase(it);
            availableInterpreters_.insert(index);
            interpreterCondition_.notify_one();
        }
    }

    size_t availableCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return availableInterpreters_.size();
    }

    size_t busyCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return busyInterpreters_.size();
    }

    size_t poolSize() const noexcept {
        return config_.poolSize;
    }

    std::future<PythonTaskResult> submitTask(
        std::function<PythonTaskResult()> task,
        TaskPriority priority) {

        auto promise = std::make_shared<std::promise<PythonTaskResult>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!initialized_) {
                PythonTaskResult result;
                result.success = false;
                result.error = "Pool not initialized";
                promise->set_value(result);
                return future;
            }

            if (shuttingDown_) {
                PythonTaskResult result;
                result.success = false;
                result.error = "Pool is shutting down";
                promise->set_value(result);
                return future;
            }

            if (taskQueue_.size() >= config_.maxQueuedTasks) {
                PythonTaskResult result;
                result.success = false;
                result.error = "Task queue full";
                promise->set_value(result);
                return future;
            }

            PrioritizedTask ptask;
            ptask.task = [task = std::move(task), promise]() mutable {
                try {
                    auto result = task();
                    promise->set_value(std::move(result));
                    return result;
                } catch (const std::exception& e) {
                    PythonTaskResult result;
                    result.success = false;
                    result.error = e.what();
                    promise->set_value(result);
                    return result;
                }
            };
            ptask.priority = priority;
            ptask.submitTime = std::chrono::steady_clock::now();
            ptask.sequenceNumber = sequenceCounter_++;

            taskQueue_.push(std::move(ptask));
        }

        taskCondition_.notify_one();
        return future;
    }

    std::future<PythonTaskResult> executeScript(
        std::string_view script,
        const py::dict& globals,
        const py::dict& locals,
        TaskPriority priority) {

        std::string scriptStr(script);

        return submitTask([scriptStr, globals, locals]() {
            PythonTaskResult result;
            auto start = std::chrono::steady_clock::now();

            try {
                py::gil_scoped_acquire gil;

                py::dict g = globals.is_none() ? py::globals() : globals;
                py::dict l = locals.is_none() ? py::dict() : locals;

                py::exec(scriptStr.c_str(), g, l);

                result.success = true;
                result.result = l.contains("result") ? l["result"] : py::none();

            } catch (const py::error_already_set& e) {
                result.success = false;
                result.error = e.what();
            } catch (const std::exception& e) {
                result.success = false;
                result.error = e.what();
            }

            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            return result;
        }, priority);
    }

    std::future<PythonTaskResult> executeFunction(
        std::string_view moduleName,
        std::string_view functionName,
        const py::tuple& args,
        const py::dict& kwargs,
        TaskPriority priority) {

        std::string modName(moduleName);
        std::string funcName(functionName);

        return submitTask([modName, funcName, args, kwargs]() {
            PythonTaskResult result;
            auto start = std::chrono::steady_clock::now();

            try {
                py::gil_scoped_acquire gil;

                py::module mod = py::module::import(modName.c_str());
                py::object func = mod.attr(funcName.c_str());

                result.result = func(*args, **kwargs);
                result.success = true;

            } catch (const py::error_already_set& e) {
                result.success = false;
                result.error = e.what();
            } catch (const std::exception& e) {
                result.success = false;
                result.error = e.what();
            }

            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            return result;
        }, priority);
    }

    size_t cancelAllTasks() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = taskQueue_.size();

        while (!taskQueue_.empty()) {
            taskQueue_.pop();
        }

        stats_.cancelledTasks += count;
        return count;
    }

    size_t pendingTaskCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return taskQueue_.size();
    }

    InterpreterPoolStats getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        InterpreterPoolStats stats = stats_;
        stats.currentQueuedTasks = taskQueue_.size();
        stats.availableInterpreters = availableInterpreters_.size();
        stats.busyInterpreters = busyInterpreters_.size();

        return stats;
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = InterpreterPoolStats{};
    }

    void setMaxQueueSize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.maxQueuedTasks = size;
    }

    void setDefaultTimeout(std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.taskTimeout = timeout;
    }

    InterpreterResult<void> preloadModule(std::string_view moduleName) {
        if (!initialized_) {
            return std::unexpected(InterpreterPoolError::PoolNotInitialized);
        }

        try {
            py::gil_scoped_acquire gil;
            py::module::import(std::string(moduleName).c_str());
            spdlog::debug("Preloaded module: {}", moduleName);
            return {};
        } catch (const py::error_already_set& e) {
            spdlog::error("Failed to preload module {}: {}", moduleName, e.what());
            return std::unexpected(InterpreterPoolError::ExecutionFailed);
        }
    }

    void addSysPath(const std::string& path) {
        if (!initialized_) return;

        try {
            py::gil_scoped_acquire gil;
            py::module sys = py::module::import("sys");
            py::list sysPath = sys.attr("path");
            sysPath.append(path);
            spdlog::debug("Added to sys.path: {}", path);
        } catch (const py::error_already_set& e) {
            spdlog::warn("Failed to add sys.path: {}", e.what());
        }
    }

    void setEnvironmentVariable(const std::string& name, const std::string& value) {
        if (!initialized_) return;

        try {
            py::gil_scoped_acquire gil;
            py::module os = py::module::import("os");
            os.attr("environ")[name.c_str()] = value;
        } catch (const py::error_already_set& e) {
            spdlog::warn("Failed to set env var: {}", e.what());
        }
    }

private:
    void workerThread(size_t workerId) {
        spdlog::debug("Worker {} started", workerId);

        while (true) {
            PrioritizedTask task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                taskCondition_.wait(lock, [this] {
                    return shuttingDown_ || !taskQueue_.empty();
                });

                if (shuttingDown_ && taskQueue_.empty()) {
                    break;
                }

                if (taskQueue_.empty()) {
                    continue;
                }

                task = std::move(const_cast<PrioritizedTask&>(taskQueue_.top()));
                taskQueue_.pop();
            }

            // Execute the task
            auto start = std::chrono::steady_clock::now();

            try {
                auto result = task.task();

                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start);

                // Update statistics
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    stats_.totalTasks++;
                    if (result.success) {
                        stats_.successfulTasks++;
                    } else {
                        stats_.failedTasks++;
                    }

                    // Update average execution time
                    double elapsedMs = static_cast<double>(elapsed.count());
                    stats_.averageExecutionTimeMs =
                        (stats_.averageExecutionTimeMs * (stats_.totalTasks - 1) +
                         elapsedMs) / stats_.totalTasks;

                    if (elapsedMs > stats_.maxExecutionTimeMs) {
                        stats_.maxExecutionTimeMs = elapsedMs;
                    }

                    stats_.lastTaskTime = std::chrono::system_clock::now();
                }

            } catch (const std::exception& e) {
                spdlog::error("Worker {} task failed: {}", workerId, e.what());

                std::lock_guard<std::mutex> lock(mutex_);
                stats_.totalTasks++;
                stats_.failedTasks++;
            }
        }

        spdlog::debug("Worker {} stopped", workerId);
    }

    InterpreterPoolConfig config_;
    std::atomic<bool> initialized_;
    std::atomic<bool> shuttingDown_;

    mutable std::mutex mutex_;
    std::condition_variable taskCondition_;
    std::condition_variable interpreterCondition_;

    std::unordered_set<size_t> availableInterpreters_;
    std::unordered_set<size_t> busyInterpreters_;

    std::priority_queue<PrioritizedTask> taskQueue_;
    std::vector<std::thread> workers_;

    InterpreterPoolStats stats_;
    std::atomic<size_t> sequenceCounter_;
};

// InterpreterPool public methods forwarding to Impl

InterpreterPool::InterpreterPool()
    : pImpl_(std::make_unique<Impl>(InterpreterPoolConfig{})) {}

InterpreterPool::InterpreterPool(const InterpreterPoolConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

InterpreterPool::~InterpreterPool() = default;

InterpreterResult<void> InterpreterPool::initialize() {
    return pImpl_->initialize();
}

void InterpreterPool::shutdown(bool waitForTasks) {
    pImpl_->shutdown(waitForTasks);
}

bool InterpreterPool::isInitialized() const noexcept {
    return pImpl_->isInitialized();
}

bool InterpreterPool::isShuttingDown() const noexcept {
    return pImpl_->isShuttingDown();
}

InterpreterResult<InterpreterGuard> InterpreterPool::acquire(
    std::chrono::milliseconds timeout) {
    return pImpl_->acquire(timeout);
}

void InterpreterPool::release(size_t index) {
    pImpl_->release(index);
}

size_t InterpreterPool::availableCount() const noexcept {
    return pImpl_->availableCount();
}

size_t InterpreterPool::busyCount() const noexcept {
    return pImpl_->busyCount();
}

size_t InterpreterPool::poolSize() const noexcept {
    return pImpl_->poolSize();
}

std::future<PythonTaskResult> InterpreterPool::executeScript(
    std::string_view script,
    const py::dict& globals,
    const py::dict& locals,
    TaskPriority priority) {
    return pImpl_->executeScript(script, globals, locals, priority);
}

std::future<PythonTaskResult> InterpreterPool::executeFunction(
    std::string_view moduleName,
    std::string_view functionName,
    const py::tuple& args,
    const py::dict& kwargs,
    TaskPriority priority) {
    return pImpl_->executeFunction(moduleName, functionName, args, kwargs, priority);
}

size_t InterpreterPool::cancelAllTasks() {
    return pImpl_->cancelAllTasks();
}

size_t InterpreterPool::pendingTaskCount() const noexcept {
    return pImpl_->pendingTaskCount();
}

InterpreterPoolStats InterpreterPool::getStatistics() const {
    return pImpl_->getStatistics();
}

void InterpreterPool::resetStatistics() {
    pImpl_->resetStatistics();
}

void InterpreterPool::setMaxQueueSize(size_t size) {
    pImpl_->setMaxQueueSize(size);
}

void InterpreterPool::setDefaultTimeout(std::chrono::milliseconds timeout) {
    pImpl_->setDefaultTimeout(timeout);
}

InterpreterResult<void> InterpreterPool::preloadModule(std::string_view moduleName) {
    return pImpl_->preloadModule(moduleName);
}

void InterpreterPool::addSysPath(const std::string& path) {
    pImpl_->addSysPath(path);
}

void InterpreterPool::setEnvironmentVariable(const std::string& name,
                                              const std::string& value) {
    pImpl_->setEnvironmentVariable(name, value);
}

// Template implementations need explicit instantiation or inline
template<PythonCallable F>
std::future<PythonTaskResult> InterpreterPool::submit(F&& func, TaskPriority priority) {
    return pImpl_->submitTask(
        [func = std::forward<F>(func)]() {
            PythonTaskResult result;
            auto start = std::chrono::steady_clock::now();

            try {
                py::gil_scoped_acquire gil;

                if constexpr (std::same_as<std::invoke_result_t<F>, void>) {
                    func();
                    result.result = py::none();
                } else {
                    result.result = func();
                }
                result.success = true;

            } catch (const py::error_already_set& e) {
                result.success = false;
                result.error = e.what();
            } catch (const std::exception& e) {
                result.success = false;
                result.error = e.what();
            }

            result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            return result;
        },
        priority);
}

template<PythonCallable F>
InterpreterResult<PythonTaskResult> InterpreterPool::execute(
    F&& func, std::chrono::milliseconds timeout) {

    auto future = submit(std::forward<F>(func));

    if (future.wait_for(timeout) == std::future_status::timeout) {
        return std::unexpected(InterpreterPoolError::Timeout);
    }

    return future.get();
}

}  // namespace lithium
