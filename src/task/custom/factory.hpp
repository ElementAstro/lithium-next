/**
 * @file factory.hpp
 * @brief Task Factory and Registry for custom task management
 *
 * This file contains the task factory and registry system that allows
 * for dynamic registration and creation of custom tasks.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_FACTORY_HPP
#define LITHIUM_TASK_FACTORY_HPP

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../task.hpp"
#include "atom/type/json.hpp"

namespace lithium::sequencer {

using json = nlohmann::json;

/**
 * @class TaskCreator
 * @brief Abstract interface for task creator objects.
 *
 * Provides an interface for creating tasks, retrieving descriptions,
 * required parameters, and parameter schemas for each task type.
 */
class TaskCreator {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~TaskCreator() = default;

    /**
     * @brief Create a new task instance.
     * @param name The name of the task instance.
     * @param config The configuration parameters for the task.
     * @return A unique pointer to the created Task.
     */
    virtual std::unique_ptr<Task> createTask(const std::string& name,
                                             const json& config) = 0;

    /**
     * @brief Get a human-readable description of the task type.
     * @return The description string.
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Get a list of required parameter names for the task.
     * @return Vector of required parameter names.
     */
    virtual std::vector<std::string> getRequiredParameters() const = 0;

    /**
     * @brief Get the JSON schema describing the parameters for the task.
     * @return JSON object representing the parameter schema.
     */
    virtual json getParameterSchema() const = 0;
};

/**
 * @class TypedTaskCreator
 * @brief Template implementation of TaskCreator for specific task types.
 *
 * Allows registration of custom factory functions and metadata for each task
 * type.
 * @tparam TaskType The concrete task type to be created.
 */
template <typename TaskType>
class TypedTaskCreator : public TaskCreator {
    static_assert(std::is_base_of_v<Task, TaskType>,
                  "TaskType must inherit from Task");

public:
    /**
     * @brief Type alias for the factory function signature.
     */
    using FactoryFunction = std::function<std::unique_ptr<TaskType>(
        const std::string&, const json&)>;

    /**
     * @brief Constructor.
     * @param factory The factory function to create TaskType instances.
     * @param description Human-readable description of the task.
     * @param requiredParams List of required parameter names.
     * @param paramSchema JSON schema for the parameters.
     */
    explicit TypedTaskCreator(
        FactoryFunction factory, const std::string& description = "",
        const std::vector<std::string>& requiredParams = {},
        const json& paramSchema = json::object())
        : factory_(std::move(factory)),
          description_(description),
          requiredParameters_(requiredParams),
          parameterSchema_(paramSchema) {}

    /**
     * @brief Create a new task instance.
     * @param name The name of the task instance.
     * @param config The configuration parameters for the task.
     * @return A unique pointer to the created Task.
     */
    std::unique_ptr<Task> createTask(const std::string& name,
                                     const json& config) override {
        return factory_(name, config);
    }

    /**
     * @brief Get a human-readable description of the task type.
     * @return The description string.
     */
    std::string getDescription() const override { return description_; }

    /**
     * @brief Get a list of required parameter names for the task.
     * @return Vector of required parameter names.
     */
    std::vector<std::string> getRequiredParameters() const override {
        return requiredParameters_;
    }

    /**
     * @brief Get the JSON schema describing the parameters for the task.
     * @return JSON object representing the parameter schema.
     */
    json getParameterSchema() const override { return parameterSchema_; }

private:
    FactoryFunction factory_;  ///< Factory function for creating tasks.
    std::string description_;  ///< Human-readable description.
    std::vector<std::string>
        requiredParameters_;  ///< Required parameter names.
    json parameterSchema_;    ///< JSON schema for parameters.
};

/**
 * @struct TaskInfo
 * @brief Metadata information for a registered task type.
 */
struct TaskInfo {
    std::string name;         ///< Task type name/identifier.
    std::string description;  ///< Human-readable description.
    std::string category;     ///< Category for grouping tasks.
    std::vector<std::string>
        requiredParameters;                 ///< List of required parameters.
    json parameterSchema;                   ///< JSON schema for parameters.
    std::string version;                    ///< Version string.
    std::vector<std::string> dependencies;  ///< List of dependent task types.
    bool isEnabled = true;                  ///< Whether the task is enabled.
};

/**
 * @class TaskFactory
 * @brief Singleton factory and registry for creating and managing custom tasks.
 *
 * Provides thread-safe registration, creation, and metadata management for
 * custom tasks.
 */
class TaskFactory {
public:
    /**
     * @brief Get the singleton instance of TaskFactory.
     * @return Reference to the singleton TaskFactory.
     */
    static TaskFactory& getInstance();

    /**
     * @brief Register a new task type.
     * @param taskType The unique identifier for the task type.
     * @param creator The task creator instance.
     * @param info Task metadata information.
     */
    void registerTask(const std::string& taskType,
                      std::unique_ptr<TaskCreator> creator,
                      const TaskInfo& info);

    /**
     * @brief Register a task type using a template function.
     * @tparam TaskType The concrete task type.
     * @param taskType The unique identifier for the task type.
     * @param factory The factory function for creating the task.
     * @param info Task metadata information.
     */
    template <typename TaskType>
    void registerTask(
        const std::string& taskType,
        typename TypedTaskCreator<TaskType>::FactoryFunction factory,
        const TaskInfo& info) {
        auto creator = std::make_unique<TypedTaskCreator<TaskType>>(
            std::move(factory), info.description, info.requiredParameters,
            info.parameterSchema);
        registerTask(taskType, std::move(creator), info);
    }

    /**
     * @brief Unregister a task type.
     * @param taskType The task type to unregister.
     */
    void unregisterTask(const std::string& taskType);

    /**
     * @brief Create a task instance.
     * @param taskType The type of task to create.
     * @param name The name for the task instance.
     * @param config Configuration parameters for the task.
     * @return Unique pointer to the created task, or nullptr if creation
     * failed.
     */
    std::unique_ptr<Task> createTask(const std::string& taskType,
                                     const std::string& name,
                                     const json& config = json::object());

    /**
     * @brief Check if a task type is registered.
     * @param taskType The task type identifier.
     * @return True if the task type is registered, false otherwise.
     */
    bool isTaskRegistered(const std::string& taskType) const;

    /**
     * @brief Get list of all registered task types.
     * @return Vector of registered task type identifiers.
     */
    std::vector<std::string> getRegisteredTaskTypes() const;

    /**
     * @brief Get task information for a specific task type.
     * @param taskType The task type identifier.
     * @return Optional TaskInfo if found.
     */
    std::optional<TaskInfo> getTaskInfo(const std::string& taskType) const;

    /**
     * @brief Get all task information grouped by category.
     * @return Map from category to vector of TaskInfo.
     */
    std::unordered_map<std::string, std::vector<TaskInfo>> getTasksByCategory()
        const;

    /**
     * @brief Validate task parameters against the registered schema.
     * @param taskType The task type identifier.
     * @param params The parameters to validate.
     * @return True if parameters are valid, false otherwise.
     */
    bool validateTaskParameters(const std::string& taskType,
                                const json& params) const;

    /**
     * @brief Clear all registered tasks and metadata.
     */
    void clear();

    /**
     * @brief Enable or disable a task type.
     * @param taskType The task type identifier.
     * @param enabled True to enable, false to disable.
     */
    void setTaskEnabled(const std::string& taskType, bool enabled);

    /**
     * @brief Check dependencies for a given task type.
     * @param taskType The task type identifier.
     * @return Vector of missing or unresolved dependencies.
     */
    std::vector<std::string> checkDependencies(
        const std::string& taskType) const;

    /**
     * @brief Get the dependency graph for all registered tasks.
     * @return Map from task type to its dependencies.
     */
    std::unordered_map<std::string, std::vector<std::string>>
    getDependencyGraph() const;

private:
    /**
     * @brief Private constructor for singleton pattern.
     */
    TaskFactory() = default;

    /**
     * @brief Private destructor.
     */
    ~TaskFactory() = default;

    // Non-copyable and non-movable
    TaskFactory(const TaskFactory&) = delete;
    TaskFactory& operator=(const TaskFactory&) = delete;

    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access.
    std::unordered_map<std::string, std::unique_ptr<TaskCreator>>
        creators_;  ///< Registered task creators.
    std::unordered_map<std::string, TaskInfo>
        taskInfos_;  ///< Registered task metadata.
};

/**
 * @def REGISTER_TASK
 * @brief Helper macro for easy task registration.
 *
 * Registers a task type with a default factory function.
 * @param TaskType The concrete task class.
 * @param taskTypeString The unique string identifier for the task type.
 * @param info The TaskInfo metadata.
 */
#define REGISTER_TASK(TaskType, taskTypeString, info)             \
    do {                                                          \
        TaskFactory::getInstance().registerTask<TaskType>(        \
            taskTypeString,                                       \
            [](const std::string& name,                           \
               const json& config) -> std::unique_ptr<TaskType> { \
                return std::make_unique<TaskType>(name, config);  \
            },                                                    \
            info);                                                \
    } while (0)

/**
 * @def REGISTER_TASK_WITH_FACTORY
 * @brief Helper macro for registering tasks with a custom factory function.
 *
 * @param TaskType The concrete task class.
 * @param taskTypeString The unique string identifier for the task type.
 * @param factory The custom factory function.
 * @param info The TaskInfo metadata.
 */
#define REGISTER_TASK_WITH_FACTORY(TaskType, taskTypeString, factory, info) \
    do {                                                                    \
        TaskFactory::getInstance().registerTask<TaskType>(taskTypeString,   \
                                                          factory, info);   \
    } while (0)

/**
 * @class TaskRegistrar
 * @brief Automatic task registration helper class.
 *
 * Registers a task type at static initialization time.
 * @tparam TaskType The concrete task type.
 */
template <typename TaskType>
class TaskRegistrar {
public:
    /**
     * @brief Constructor for automatic registration.
     * @param taskType The unique string identifier for the task type.
     * @param info The TaskInfo metadata.
     * @param factory Optional custom factory function.
     */
    TaskRegistrar(const std::string& taskType, const TaskInfo& info,
                  typename TypedTaskCreator<TaskType>::FactoryFunction factory =
                      nullptr) {
        if (!factory) {
            factory = [](const std::string& name,
                         const json& config) -> std::unique_ptr<TaskType> {
                return std::make_unique<TaskType>(name, config);
            };
        }
        TaskFactory::getInstance().registerTask<TaskType>(
            taskType, std::move(factory), info);
    }
};

/**
 * @def AUTO_REGISTER_TASK
 * @brief Macro for automatic static registration of a task type.
 *
 * Registers the task type at static initialization time.
 * @param TaskType The concrete task class.
 * @param taskTypeString The unique string identifier for the task type.
 * @param info The TaskInfo metadata.
 */
#define AUTO_REGISTER_TASK(TaskType, taskTypeString, info) \
    static TaskRegistrar<TaskType> _registrar_##TaskType(taskTypeString, info)

}  // namespace lithium::sequencer

#endif  // LITHIUM_TASK_FACTORY_HPP
