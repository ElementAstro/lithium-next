/**
 * @file task_base.hpp
 * @brief Base class for all device-related tasks with common functionality
 *
 * This provides shared utilities for parameter validation, logging,
 * timing, and progress tracking that all device tasks can inherit from.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMMON_TASK_BASE_HPP
#define LITHIUM_TASK_COMMON_TASK_BASE_HPP

#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include "../../task.hpp"

namespace lithium::task {

/**
 * @brief Base class for device tasks with common functionality
 *
 * Provides shared utilities for parameter validation, logging,
 * timing, and progress tracking.
 */
class TaskBase : public Task {
public:
    using Task::Task;

    /**
     * @brief Constructor with task type name
     * @param taskType The task type identifier
     */
    explicit TaskBase(const std::string& taskType)
        : Task(taskType, [this](const json& p) { this->executeImpl(p); }),
          taskTypeName_(taskType) {
        setupBaseParameters();
        setTaskType(taskType);
    }

    /**
     * @brief Constructor with name and config
     * @param name Task instance name
     * @param config Configuration parameters
     */
    TaskBase(const std::string& name, const json& config)
        : Task(name, [this](const json& p) { this->executeImpl(p); }),
          taskTypeName_(name),
          config_(config) {
        setupBaseParameters();
        setTaskType(name);
    }

    virtual ~TaskBase() = default;

    /**
     * @brief Get task type name
     */
    [[nodiscard]] std::string getTaskTypeName() const { return taskTypeName_; }

    /**
     * @brief Execute the task with timing and error handling
     */
    void execute(const json& params) override {
        startTimer();
        markStartTime();

        try {
            spdlog::info("[{}] Starting execution", taskTypeName_);

            // Check if cancelled before starting
            if (isCancelled()) {
                setStatus(TaskStatus::Failed);
                setErrorType(TaskErrorType::Cancelled);
                setError("Task was cancelled before execution");
                return;
            }

            // Validate parameters first
            if (!validateParams(params)) {
                setStatus(TaskStatus::Failed);
                setErrorType(TaskErrorType::InvalidParameter);
                auto errors = getParamErrors();
                if (!errors.empty()) {
                    setError("Parameter validation failed: " + errors[0]);
                }
                return;
            }

            setStatus(TaskStatus::InProgress);
            executeImpl(params);

            if (!isCancelled()) {
                setStatus(TaskStatus::Completed);
                logCompletion();
            }
        } catch (const std::exception& e) {
            spdlog::error("[{}] Failed: {}", taskTypeName_, e.what());
            setStatus(TaskStatus::Failed);
            setErrorType(TaskErrorType::ExecutionFailed);
            setError(e.what());
            throw;
        }

        markEndTime();
    }

    /**
     * @brief Cancel the task
     */
    bool cancel() override {
        spdlog::info("[{}] Cancellation requested", taskTypeName_);
        return Task::cancel();
    }

protected:
    /**
     * @brief Implementation method to be overridden by derived classes
     */
    virtual void executeImpl(const json& params) = 0;

    /**
     * @brief Setup base parameters common to all tasks
     */
    virtual void setupBaseParameters() {
        addParamDefinition("timeout", "integer", false, 3600,
                           "Task timeout (seconds)");
        addParamDefinition("retry_count", "integer", false, 0,
                           "Retry count on failure");
        addParamDefinition("retry_delay", "integer", false, 1000,
                           "Delay between retries (ms)");
    }

    /**
     * @brief Log task progress
     * @param message Progress message
     * @param progress Progress value (0.0 to 1.0), -1 for no percentage
     */
    void logProgress(const std::string& message, double progress = -1.0) {
        addHistoryEntry(message);
        if (progress >= 0.0) {
            spdlog::info("[{}] Progress {:.1f}%: {}", taskTypeName_,
                         progress * 100, message);
        } else {
            spdlog::info("[{}] {}", taskTypeName_, message);
        }
    }

    /**
     * @brief Log completion with timing
     */
    void logCompletion() {
        auto elapsed = getElapsedMs();
        spdlog::info("[{}] Completed in {} ms", taskTypeName_, elapsed);
        addHistoryEntry("Completed in " + std::to_string(elapsed) + " ms");
    }

    /**
     * @brief Start timing
     */
    void startTimer() { timerStart_ = std::chrono::steady_clock::now(); }

    /**
     * @brief Get elapsed time in milliseconds
     */
    [[nodiscard]] int64_t getElapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   now - timerStart_)
            .count();
    }

    /**
     * @brief Get configuration value with default
     */
    template <typename T>
    T getConfig(const std::string& key, const T& defaultValue) const {
        if (config_.contains(key)) {
            return config_[key].get<T>();
        }
        return defaultValue;
    }

    /**
     * @brief Check if task should continue (not cancelled)
     */
    [[nodiscard]] bool shouldContinue() const { return !isCancelled(); }

    std::string taskTypeName_;
    json config_;
    std::chrono::steady_clock::time_point timerStart_;
};

/**
 * @brief Macro for defining a task class with boilerplate
 */
#define DECLARE_TASK(ClassName, TaskTypeName)                               \
    class ClassName : public TaskBase {                                     \
    public:                                                                 \
        ClassName() : TaskBase(TaskTypeName) { setupParameters(); }         \
        ClassName(const std::string& name, const json& config)              \
            : TaskBase(name, config) {                                      \
            setupParameters();                                              \
        }                                                                   \
        static std::string taskName() { return TaskTypeName; }              \
        static std::string getStaticTaskTypeName() { return TaskTypeName; } \
                                                                            \
    protected:                                                              \
        void executeImpl(const json& params) override;                      \
                                                                            \
    private:                                                                \
        void setupParameters();                                             \
    };

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMMON_TASK_BASE_HPP
