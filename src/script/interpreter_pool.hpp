/*
 * interpreter_pool.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file interpreter_pool.hpp
 * @brief Python Interpreter Pool for Concurrent Execution
 * @date 2024
 * @version 1.0.0
 *
 * This module provides a pool of Python interpreters for concurrent script execution:
 * - Thread-safe interpreter acquisition/release
 * - Automatic GIL management
 * - Task queue with priority support
 * - Resource limiting and monitoring
 * - Support for Python 3.12+ sub-interpreters
 */

#ifndef LITHIUM_SCRIPT_INTERPRETER_POOL_HPP
#define LITHIUM_SCRIPT_INTERPRETER_POOL_HPP

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace py = pybind11;

namespace lithium {

/**
 * @brief Error codes for interpreter pool operations
 */
enum class InterpreterPoolError {
    Success = 0,
    PoolNotInitialized,
    PoolExhausted,
    TaskQueueFull,
    ExecutionFailed,
    Timeout,
    Cancelled,
    GILAcquisitionFailed,
    InvalidInterpreter,
    ShutdownInProgress,
    UnknownError
};

/**
 * @brief Get string representation of InterpreterPoolError
 */
[[nodiscard]] constexpr std::string_view interpreterPoolErrorToString(
    InterpreterPoolError error) noexcept {
    switch (error) {
        case InterpreterPoolError::Success: return "Success";
        case InterpreterPoolError::PoolNotInitialized: return "Pool not initialized";
        case InterpreterPoolError::PoolExhausted: return "Pool exhausted";
        case InterpreterPoolError::TaskQueueFull: return "Task queue full";
        case InterpreterPoolError::ExecutionFailed: return "Execution failed";
        case InterpreterPoolError::Timeout: return "Operation timed out";
        case InterpreterPoolError::Cancelled: return "Operation cancelled";
        case InterpreterPoolError::GILAcquisitionFailed: return "GIL acquisition failed";
        case InterpreterPoolError::InvalidInterpreter: return "Invalid interpreter";
        case InterpreterPoolError::ShutdownInProgress: return "Shutdown in progress";
        case InterpreterPoolError::UnknownError: return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for interpreter pool operations
 */
template<typename T>
using InterpreterResult = std::expected<T, InterpreterPoolError>;

/**
 * @brief Configuration for the interpreter pool
 */
struct InterpreterPoolConfig {
    size_t poolSize{4};                        ///< Number of interpreters in pool
    size_t maxQueuedTasks{1000};               ///< Maximum queued tasks
    std::chrono::milliseconds taskTimeout{30000};  ///< Default task timeout
    std::chrono::milliseconds acquireTimeout{5000};  ///< Interpreter acquire timeout
    bool enableStatistics{true};               ///< Enable execution statistics
    bool preloadModules{false};                ///< Preload common modules
    std::vector<std::string> modulesToPreload; ///< Modules to preload
    bool useSubinterpreters{false};            ///< Use Python 3.12+ sub-interpreters
    size_t workerThreads{0};                   ///< Worker threads (0 = poolSize)
};

/**
 * @brief Statistics for the interpreter pool
 */
struct InterpreterPoolStats {
    size_t totalTasks{0};              ///< Total tasks executed
    size_t successfulTasks{0};         ///< Successful executions
    size_t failedTasks{0};             ///< Failed executions
    size_t timedOutTasks{0};           ///< Timed out tasks
    size_t cancelledTasks{0};          ///< Cancelled tasks
    size_t currentQueuedTasks{0};      ///< Currently queued tasks
    size_t availableInterpreters{0};   ///< Available interpreters
    size_t busyInterpreters{0};        ///< Busy interpreters
    double averageExecutionTimeMs{0};  ///< Average execution time
    double maxExecutionTimeMs{0};      ///< Maximum execution time
    std::chrono::system_clock::time_point lastTaskTime;  ///< Last task completion
};

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    Low = 0,
    Normal = 5,
    High = 10,
    Critical = 15
};

/**
 * @brief Execution result for a Python task
 */
struct PythonTaskResult {
    bool success{false};
    py::object result;
    std::string error;
    std::chrono::milliseconds executionTime{0};
    std::string interpreterInfo;
};

/**
 * @brief Concept for callable Python tasks
 */
template<typename F>
concept PythonCallable = std::invocable<F> &&
    (std::same_as<std::invoke_result_t<F>, py::object> ||
     std::same_as<std::invoke_result_t<F>, void>);

/**
 * @brief RAII guard for interpreter acquisition
 */
class InterpreterGuard {
public:
    InterpreterGuard() = default;
    InterpreterGuard(InterpreterGuard&& other) noexcept;
    InterpreterGuard& operator=(InterpreterGuard&& other) noexcept;
    ~InterpreterGuard();

    // Disable copy
    InterpreterGuard(const InterpreterGuard&) = delete;
    InterpreterGuard& operator=(const InterpreterGuard&) = delete;

    /**
     * @brief Check if guard holds a valid interpreter
     */
    [[nodiscard]] bool isValid() const noexcept;

    /**
     * @brief Get the interpreter index
     */
    [[nodiscard]] size_t getIndex() const noexcept;

    /**
     * @brief Execute a callable with this interpreter
     */
    template<PythonCallable F>
    auto execute(F&& func) -> std::invoke_result_t<F>;

    /**
     * @brief Release the interpreter early
     */
    void release();

private:
    friend class InterpreterPool;

    InterpreterGuard(class InterpreterPool* pool, size_t index,
                     std::unique_ptr<py::gil_scoped_acquire> gil);

    class InterpreterPool* pool_{nullptr};
    size_t index_{0};
    std::unique_ptr<py::gil_scoped_acquire> gil_;
    bool valid_{false};
};

/**
 * @brief Python Interpreter Pool
 *
 * Manages a pool of Python interpreters for concurrent execution.
 * Provides thread-safe task submission and automatic GIL management.
 */
class InterpreterPool {
public:
    /**
     * @brief Constructs an InterpreterPool with default configuration
     */
    InterpreterPool();

    /**
     * @brief Constructs an InterpreterPool with specified configuration
     * @param config Pool configuration
     */
    explicit InterpreterPool(const InterpreterPoolConfig& config);

    /**
     * @brief Destructor - shuts down pool and waits for pending tasks
     */
    ~InterpreterPool();

    // Disable copy and move
    InterpreterPool(const InterpreterPool&) = delete;
    InterpreterPool& operator=(const InterpreterPool&) = delete;
    InterpreterPool(InterpreterPool&&) = delete;
    InterpreterPool& operator=(InterpreterPool&&) = delete;

    // =========================================================================
    // Interpreter Management
    // =========================================================================

    /**
     * @brief Initializes the interpreter pool
     * @return Success or error
     */
    [[nodiscard]] InterpreterResult<void> initialize();

    /**
     * @brief Shuts down the interpreter pool
     * @param waitForTasks Whether to wait for pending tasks
     */
    void shutdown(bool waitForTasks = true);

    /**
     * @brief Checks if the pool is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Checks if the pool is shutting down
     */
    [[nodiscard]] bool isShuttingDown() const noexcept;

    /**
     * @brief Acquires an interpreter from the pool
     * @param timeout Maximum time to wait for an interpreter
     * @return InterpreterGuard on success, error on failure
     */
    [[nodiscard]] InterpreterResult<InterpreterGuard> acquire(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    /**
     * @brief Releases an interpreter back to the pool
     * @param index Interpreter index
     */
    void release(size_t index);

    /**
     * @brief Gets the number of available interpreters
     */
    [[nodiscard]] size_t availableCount() const noexcept;

    /**
     * @brief Gets the number of busy interpreters
     */
    [[nodiscard]] size_t busyCount() const noexcept;

    /**
     * @brief Gets the total pool size
     */
    [[nodiscard]] size_t poolSize() const noexcept;

    // =========================================================================
    // Task Execution
    // =========================================================================

    /**
     * @brief Submits a task for execution
     * @param func Callable to execute
     * @param priority Task priority
     * @return Future containing the result
     */
    template<PythonCallable F>
    [[nodiscard]] std::future<PythonTaskResult> submit(
        F&& func,
        TaskPriority priority = TaskPriority::Normal);

    /**
     * @brief Submits a task and waits for completion
     * @param func Callable to execute
     * @param timeout Maximum execution time
     * @return Result of the execution
     */
    template<PythonCallable F>
    [[nodiscard]] InterpreterResult<PythonTaskResult> execute(
        F&& func,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

    /**
     * @brief Executes a Python script string
     * @param script Python code to execute
     * @param globals Optional global namespace
     * @param locals Optional local namespace
     * @param priority Task priority
     * @return Future containing the result
     */
    [[nodiscard]] std::future<PythonTaskResult> executeScript(
        std::string_view script,
        const py::dict& globals = py::dict(),
        const py::dict& locals = py::dict(),
        TaskPriority priority = TaskPriority::Normal);

    /**
     * @brief Executes a Python function from a module
     * @param moduleName Name of the Python module
     * @param functionName Name of the function
     * @param args Arguments to pass
     * @param kwargs Keyword arguments
     * @param priority Task priority
     * @return Future containing the result
     */
    [[nodiscard]] std::future<PythonTaskResult> executeFunction(
        std::string_view moduleName,
        std::string_view functionName,
        const py::tuple& args = py::tuple(),
        const py::dict& kwargs = py::dict(),
        TaskPriority priority = TaskPriority::Normal);

    /**
     * @brief Cancels all pending tasks
     * @return Number of cancelled tasks
     */
    size_t cancelAllTasks();

    /**
     * @brief Gets the number of pending tasks
     */
    [[nodiscard]] size_t pendingTaskCount() const noexcept;

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Gets current pool statistics
     */
    [[nodiscard]] InterpreterPoolStats getStatistics() const;

    /**
     * @brief Resets pool statistics
     */
    void resetStatistics();

    /**
     * @brief Sets the maximum queue size
     * @param size Maximum number of queued tasks
     */
    void setMaxQueueSize(size_t size);

    /**
     * @brief Sets the default task timeout
     * @param timeout Default timeout for tasks
     */
    void setDefaultTimeout(std::chrono::milliseconds timeout);

    // =========================================================================
    // Module Management
    // =========================================================================

    /**
     * @brief Preloads a module in all interpreters
     * @param moduleName Name of the module to preload
     * @return Success or error
     */
    [[nodiscard]] InterpreterResult<void> preloadModule(std::string_view moduleName);

    /**
     * @brief Adds a path to sys.path for all interpreters
     * @param path Path to add
     */
    void addSysPath(const std::string& path);

    /**
     * @brief Sets environment variable for all interpreters
     * @param name Variable name
     * @param value Variable value
     */
    void setEnvironmentVariable(const std::string& name, const std::string& value);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// =========================================================================
// Template Implementation
// =========================================================================

template<PythonCallable F>
auto InterpreterGuard::execute(F&& func) -> std::invoke_result_t<F> {
    if (!isValid()) {
        if constexpr (std::same_as<std::invoke_result_t<F>, void>) {
            return;
        } else {
            return py::none();
        }
    }
    return std::forward<F>(func)();
}

}  // namespace lithium

#endif  // LITHIUM_SCRIPT_INTERPRETER_POOL_HPP
