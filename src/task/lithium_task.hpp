/**
 * @file lithium_task.hpp
 * @brief Unified header for the Lithium Task System
 *
 * This header provides a single include point for all task system
 * functionality. Include this header to access the complete task management,
 * execution, and sequencing capabilities.
 *
 * @example
 * ```cpp
 * #include "task/lithium_task.hpp"
 *
 * int main() {
 *     // Initialize task system
 *     lithium::task::initializeTaskSystem();
 *
 *     // Create and execute a simple task
 *     auto task = lithium::task::createTask("TakeExposure", "my_exposure", {
 *         {"exposure", 30.0},
 *         {"binning", 1}
 *     });
 *     task->execute({});
 *
 *     // Create a sequence
 *     auto sequence = lithium::task::createSequence();
 *     sequence->addTarget(lithium::task::createTarget("M31", tasksJson));
 *     sequence->executeAll();
 *
 *     return 0;
 * }
 * ```
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_HPP
#define LITHIUM_TASK_HPP

// Core components
#include "generator.hpp"
#include "sequencer.hpp"
#include "target.hpp"
#include "task.hpp"

// Factory and registration
#include "custom/factory.hpp"
#include "registration.hpp"

// Utilities
#include "imagepath.hpp"
#include "integration_utils.hpp"

// API types and utilities
#include "api_adapter.hpp"

namespace lithium::task {

/**
 * @brief Initialize the task system
 *
 * This function must be called before using any task system functionality.
 * It registers all built-in tasks with the TaskFactory.
 *
 * @return true if initialization was successful
 */
inline bool initializeTaskSystem() {
    try {
        registerBuiltInTasks();
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * @brief Create a task instance
 *
 * @param taskType The type of task to create (e.g., "TakeExposure")
 * @param name Instance name for the task
 * @param config Initial configuration parameters
 * @return Unique pointer to the created task, or nullptr on failure
 */
inline std::unique_ptr<Task> createTask(const std::string& taskType,
                                        const std::string& name,
                                        const json& config = json::object()) {
    return TaskFactory::getInstance().createTask(taskType, name, config);
}

/**
 * @brief Create a target with tasks loaded from JSON
 *
 * @param name Name for the target
 * @param tasksJson JSON array containing task definitions
 * @param cooldown Optional cooldown between tasks
 * @param maxRetries Maximum retries for failed tasks
 * @return Unique pointer to the created target
 */
inline std::unique_ptr<Target> createTarget(
    const std::string& name, const json& tasksJson = json::array(),
    std::chrono::seconds cooldown = std::chrono::seconds{0},
    int maxRetries = 0) {
    auto target = std::make_unique<Target>(name, cooldown, maxRetries);
    if (!tasksJson.empty()) {
        target->loadTasksFromJson(tasksJson);
    }
    return target;
}

/**
 * @brief Create an exposure sequence
 *
 * @return Unique pointer to the created sequence
 */
inline std::unique_ptr<ExposureSequence> createSequence() {
    return std::make_unique<ExposureSequence>();
}

/**
 * @brief Get the TaskFactory singleton instance
 *
 * @return Reference to the TaskFactory
 */
inline TaskFactory& getFactory() { return TaskFactory::getInstance(); }

/**
 * @brief Check if a task type is available
 *
 * @param taskType The task type to check
 * @return true if the task type is registered
 */
inline bool isTaskAvailable(const std::string& taskType) {
    return TaskFactory::getInstance().isTaskRegistered(taskType);
}

/**
 * @brief Get all available task types grouped by category
 *
 * @return Map of category to task info list
 */
inline auto getTasksByCategory() {
    return TaskFactory::getInstance().getTasksByCategory();
}

/**
 * @brief Validate task parameters before execution
 *
 * @param taskType The task type
 * @param params Parameters to validate
 * @return true if parameters are valid
 */
inline bool validateTaskParams(const std::string& taskType,
                               const json& params) {
    return TaskFactory::getInstance().validateTaskParameters(taskType, params);
}

/**
 * @brief Quick execution helper - create and run a task
 *
 * @param taskType The type of task to execute
 * @param params Execution parameters
 * @return true if execution completed successfully
 */
inline bool executeTask(const std::string& taskType, const json& params) {
    try {
        auto task = createTask(taskType, "quick_exec_" + taskType, params);
        if (!task)
            return false;
        task->execute(params);
        return task->getStatus() == TaskStatus::Completed;
    } catch (...) {
        return false;
    }
}

}  // namespace lithium::task

#endif  // LITHIUM_TASK_HPP
