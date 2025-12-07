/**
 * @file exception.hpp
 * @brief Task system exception types
 *
 * This file defines all exception types used throughout the task system.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_CORE_EXCEPTION_HPP
#define LITHIUM_TASK_CORE_EXCEPTION_HPP

#include <exception>
#include <string>

namespace lithium::task {

/**
 * @brief Base exception class for task system errors
 */
class TaskException : public std::exception {
public:
    explicit TaskException(const std::string& message) : message_(message) {}

    const char* what() const noexcept override { return message_.c_str(); }

protected:
    std::string message_;
};

/**
 * @brief Exception thrown when task execution fails
 */
class TaskExecutionException : public TaskException {
public:
    explicit TaskExecutionException(const std::string& message)
        : TaskException("Task execution failed: " + message) {}
};

/**
 * @brief Exception thrown when task parameters are invalid
 */
class TaskParameterException : public TaskException {
public:
    explicit TaskParameterException(const std::string& message)
        : TaskException("Invalid task parameter: " + message) {}
};

/**
 * @brief Exception thrown when a task is not found
 */
class TaskNotFoundException : public TaskException {
public:
    explicit TaskNotFoundException(const std::string& taskType)
        : TaskException("Task type not found: " + taskType) {}
};

/**
 * @brief Exception thrown when task registration fails
 */
class TaskRegistrationException : public TaskException {
public:
    explicit TaskRegistrationException(const std::string& message)
        : TaskException("Task registration failed: " + message) {}
};

/**
 * @brief Exception thrown when task timeout occurs
 */
class TaskTimeoutException : public TaskException {
public:
    explicit TaskTimeoutException(const std::string& taskName)
        : TaskException("Task timed out: " + taskName) {}
};

/**
 * @brief Exception thrown when task is cancelled
 */
class TaskCancelledException : public TaskException {
public:
    explicit TaskCancelledException(const std::string& taskName)
        : TaskException("Task was cancelled: " + taskName) {}
};

/**
 * @brief Exception thrown when task dependency fails
 */
class TaskDependencyException : public TaskException {
public:
    explicit TaskDependencyException(const std::string& message)
        : TaskException("Task dependency error: " + message) {}
};

/**
 * @brief Exception thrown when sequence operation fails
 */
class SequenceException : public TaskException {
public:
    explicit SequenceException(const std::string& message)
        : TaskException("Sequence error: " + message) {}
};

/**
 * @brief Exception thrown when target operation fails
 */
class TargetException : public TaskException {
public:
    explicit TargetException(const std::string& message)
        : TaskException("Target error: " + message) {}
};

/**
 * @brief Exception thrown when task generator fails
 */
class TaskGeneratorException : public TaskException {
public:
    /**
     * @brief Error codes for TaskGeneratorException
     */
    enum class ErrorCode {
        UNDEFINED_MACRO,         ///< Undefined macro error
        INVALID_MACRO_ARGS,      ///< Invalid macro arguments error
        MACRO_EVALUATION_ERROR,  ///< Macro evaluation error
        JSON_PROCESSING_ERROR,   ///< JSON processing error
        INVALID_MACRO_TYPE,      ///< Invalid macro type error
        CACHE_ERROR,             ///< Cache error
        TEMPLATE_NOT_FOUND,      ///< Template not found error
        TASK_GENERATION_ERROR,   ///< Task generation error
        FILE_IO_ERROR,           ///< File I/O error
        VALIDATION_ERROR         ///< Validation error
    };

    TaskGeneratorException(ErrorCode code, const std::string& message)
        : TaskException(message), code_(code) {}

    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_CORE_EXCEPTION_HPP
