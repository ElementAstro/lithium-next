/**
 * @file exception.hpp
 * @brief Task system exception handling
 *
 * This file contains the exception classes for the task system.
 *
 * @date 2025-07-11
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2025 Max Qian
 */

#ifndef LITHIUM_TASK_EXCEPTION_HPP
#define LITHIUM_TASK_EXCEPTION_HPP

#include <exception>
#include <string>
#include <chrono>
#include <vector>

namespace lithium::task {

/**
 * @enum TaskErrorSeverity
 * @brief Defines the severity levels for task errors
 */
enum class TaskErrorSeverity {
    Debug,     ///< Debug level, not critical
    Info,      ///< Informational, not an error
    Warning,   ///< Warning level, operation can continue
    Error,     ///< Error level, operation may fail
    Critical,  ///< Critical level, operation will fail
    Fatal      ///< Fatal level, system may be unstable
};

/**
 * @class TaskException
 * @brief Base class for all task-related exceptions.
 */
class TaskException : public std::exception {
public:
    /**
     * @brief Constructor for TaskException.
     * @param message The error message.
     * @param severity The error severity.
     */
    TaskException(const std::string& message, TaskErrorSeverity severity = TaskErrorSeverity::Error)
        : msg_(message), severity_(severity), timestamp_(std::chrono::system_clock::now()) {}
        
    /**
     * @brief Get the error message.
     * @return The error message.
     */
    const char* what() const noexcept override { return msg_.c_str(); }
    
    /**
     * @brief Get the error severity.
     * @return The error severity.
     */
    TaskErrorSeverity getSeverity() const noexcept { return severity_; }
    
    /**
     * @brief Get the error timestamp.
     * @return The error timestamp.
     */
    std::chrono::system_clock::time_point getTimestamp() const noexcept { return timestamp_; }
    
    /**
     * @brief Convert severity to string.
     * @return String representation of the severity.
     */
    std::string severityToString() const noexcept {
        switch (severity_) {
            case TaskErrorSeverity::Debug: return "DEBUG";
            case TaskErrorSeverity::Info: return "INFO";
            case TaskErrorSeverity::Warning: return "WARNING";
            case TaskErrorSeverity::Error: return "ERROR";
            case TaskErrorSeverity::Critical: return "CRITICAL";
            case TaskErrorSeverity::Fatal: return "FATAL";
            default: return "UNKNOWN";
        }
    }

protected:
    std::string msg_;                                ///< The error message
    TaskErrorSeverity severity_;                     ///< The error severity
    std::chrono::system_clock::time_point timestamp_;  ///< When the error occurred
};

/**
 * @class TaskTimeoutException
 * @brief Exception thrown when a task times out.
 */
class TaskTimeoutException : public TaskException {
public:
    /**
     * @brief Constructor for TaskTimeoutException.
     * @param message The error message.
     * @param taskName The name of the task that timed out.
     * @param timeout The timeout duration.
     */
    TaskTimeoutException(const std::string& message, 
                         const std::string& taskName,
                         std::chrono::seconds timeout)
        : TaskException(message, TaskErrorSeverity::Error), 
          taskName_(taskName),
          timeout_(timeout) {}
          
    /**
     * @brief Get the name of the task that timed out.
     * @return The task name.
     */
    const std::string& getTaskName() const noexcept { return taskName_; }
    
    /**
     * @brief Get the timeout duration.
     * @return The timeout duration.
     */
    std::chrono::seconds getTimeout() const noexcept { return timeout_; }
    
private:
    std::string taskName_;         ///< Name of the task that timed out
    std::chrono::seconds timeout_; ///< The timeout duration
};

/**
 * @class TaskParameterException
 * @brief Exception thrown when a task parameter is invalid.
 */
class TaskParameterException : public TaskException {
public:
    /**
     * @brief Constructor for TaskParameterException.
     * @param message The error message.
     * @param paramName The name of the invalid parameter.
     * @param taskName The name of the task with the invalid parameter.
     */
    TaskParameterException(const std::string& message,
                          const std::string& paramName,
                          const std::string& taskName)
        : TaskException(message, TaskErrorSeverity::Error),
          paramName_(paramName),
          taskName_(taskName) {}
          
    /**
     * @brief Get the name of the invalid parameter.
     * @return The parameter name.
     */
    const std::string& getParamName() const noexcept { return paramName_; }
    
    /**
     * @brief Get the name of the task with the invalid parameter.
     * @return The task name.
     */
    const std::string& getTaskName() const noexcept { return taskName_; }
    
private:
    std::string paramName_;  ///< Name of the invalid parameter
    std::string taskName_;   ///< Name of the task with the invalid parameter
};

/**
 * @class TaskDependencyException
 * @brief Exception thrown when a task dependency error occurs.
 */
class TaskDependencyException : public TaskException {
public:
    /**
     * @brief Constructor for TaskDependencyException.
     * @param message The error message.
     * @param taskName The name of the task with the dependency error.
     * @param dependencyNames The names of the dependencies causing the error.
     */
    TaskDependencyException(const std::string& message,
                           const std::string& taskName,
                           const std::vector<std::string>& dependencyNames)
        : TaskException(message, TaskErrorSeverity::Error),
          taskName_(taskName),
          dependencyNames_(dependencyNames) {}
          
    /**
     * @brief Get the name of the task with the dependency error.
     * @return The task name.
     */
    const std::string& getTaskName() const noexcept { return taskName_; }
    
    /**
     * @brief Get the names of the dependencies causing the error.
     * @return The dependency names.
     */
    const std::vector<std::string>& getDependencyNames() const noexcept { return dependencyNames_; }
    
private:
    std::string taskName_;                     ///< Name of the task with the dependency error
    std::vector<std::string> dependencyNames_; ///< Names of the dependencies causing the error
};

/**
 * @class TaskExecutionException
 * @brief Exception thrown when a task execution error occurs.
 */
class TaskExecutionException : public TaskException {
public:
    /**
     * @brief Constructor for TaskExecutionException.
     * @param message The error message.
     * @param taskName The name of the task with the execution error.
     * @param errorDetails Additional error details.
     */
    TaskExecutionException(const std::string& message,
                          const std::string& taskName,
                          const std::string& errorDetails)
        : TaskException(message, TaskErrorSeverity::Error),
          taskName_(taskName),
          errorDetails_(errorDetails) {}
          
    /**
     * @brief Get the name of the task with the execution error.
     * @return The task name.
     */
    const std::string& getTaskName() const noexcept { return taskName_; }
    
    /**
     * @brief Get additional error details.
     * @return The error details.
     */
    const std::string& getErrorDetails() const noexcept { return errorDetails_; }
    
private:
    std::string taskName_;      ///< Name of the task with the execution error
    std::string errorDetails_;  ///< Additional error details
};

}  // namespace lithium::task

// Convenience macros for throwing exceptions
#define THROW_TASK_EXCEPTION(message, severity) \
    throw lithium::task::TaskException((message), (severity))

#define THROW_TASK_TIMEOUT_EXCEPTION(message, taskName, timeout) \
    throw lithium::task::TaskTimeoutException((message), (taskName), (timeout))

#define THROW_TASK_PARAMETER_EXCEPTION(message, paramName, taskName) \
    throw lithium::task::TaskParameterException((message), (paramName), (taskName))

#define THROW_TASK_DEPENDENCY_EXCEPTION(message, taskName, dependencyNames) \
    throw lithium::task::TaskDependencyException((message), (taskName), (dependencyNames))

#define THROW_TASK_EXECUTION_EXCEPTION(message, taskName, errorDetails) \
    throw lithium::task::TaskExecutionException((message), (taskName), (errorDetails))

#endif  // LITHIUM_TASK_EXCEPTION_HPP
