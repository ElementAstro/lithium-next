/**
 * @file task_registry.hpp
 * @brief Task Registry for automatic registration of custom tasks
 *
 * This file contains the task registry system that automatically registers
 * all custom tasks with the factory system.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_REGISTRY_HPP
#define LITHIUM_TASK_REGISTRY_HPP

#include "factory.hpp"
#include "config_task.hpp"
#include "device_task.hpp"
#include "script_task.hpp"

namespace lithium::sequencer {

/**
 * @brief Task Registry class for registering all custom tasks
 */
class TaskRegistry {
public:
    /**
     * @brief Register all available custom tasks
     */
    static void registerAllTasks();

    /**
     * @brief Register core system tasks
     */
    static void registerCoreTasks();

    /**
     * @brief Register camera-related tasks
     */
    static void registerCameraTasks();

    /**
     * @brief Register device management tasks
     */
    static void registerDeviceTasks();

    /**
     * @brief Register script and automation tasks
     */
    static void registerScriptTasks();

    /**
     * @brief Register configuration management tasks
     */
    static void registerConfigTasks();

    /**
     * @brief Register custom user tasks from directory
     */
    static size_t registerTasksFromDirectory(const std::string& directory);

    /**
     * @brief Initialize the task system
     */
    static void initialize();

    /**
     * @brief Shutdown the task system
     */
    static void shutdown();

    /**
     * @brief Get task registration status
     */
    static bool isInitialized();

private:
    static bool initialized_;
};

/**
 * @brief Helper macros for task registration
 */
#define LITHIUM_REGISTER_TASK(TaskClass, TaskType, Category, Description) \
    do { \
        TaskInfo info; \
        info.name = TaskType; \
        info.description = Description; \
        info.category = Category; \
        info.version = "1.0.0"; \
        info.isEnabled = true; \
        TaskFactory::getInstance().registerTask<TaskClass>( \
            TaskType, \
            [](const std::string& name, const json& config) -> std::unique_ptr<TaskClass> { \
                return std::make_unique<TaskClass>(name, config); \
            }, \
            info \
        ); \
    } while(0)

#define LITHIUM_REGISTER_TASK_WITH_DEPS(TaskClass, TaskType, Category, Description, Dependencies) \
    do { \
        TaskInfo info; \
        info.name = TaskType; \
        info.description = Description; \
        info.category = Category; \
        info.version = "1.0.0"; \
        info.dependencies = Dependencies; \
        info.isEnabled = true; \
        TaskFactory::getInstance().registerTask<TaskClass>( \
            TaskType, \
            [](const std::string& name, const json& config) -> std::unique_ptr<TaskClass> { \
                return std::make_unique<TaskClass>(name, config); \
            }, \
            info \
        ); \
    } while(0)

} // namespace lithium::sequencer

#endif // LITHIUM_TASK_REGISTRY_HPP
