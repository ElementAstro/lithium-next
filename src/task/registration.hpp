#ifndef LITHIUM_TASK_REGISTRATION_HPP
#define LITHIUM_TASK_REGISTRATION_HPP

#include "custom/factory.hpp"

namespace lithium::sequencer {

/**
 * @brief Registers all built-in tasks with the TaskFactory
 */
void registerBuiltInTasks();

/**
 * @brief Creates a wrapper function to adapt static task creators to the
 * factory pattern
 */
template <typename TaskType>
std::unique_ptr<TaskType> createTaskWrapper(const std::string& name,
                                            const json& config) {
    // Create a wrapper task that delegates to the static execute method
    return std::make_unique<TaskType>(name, [config](const json& params) {
        // Merge config into params for execution
        json mergedParams = config;
        for (const auto& [key, value] : params.items()) {
            mergedParams[key] = value;
        }
        TaskType::execute(mergedParams);
    });
}

}  // namespace lithium::sequencer

#endif  // LITHIUM_TASK_REGISTRATION_HPP
