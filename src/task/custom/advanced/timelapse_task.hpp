#ifndef LITHIUM_TASK_ADVANCED_TIMELAPSE_TASK_HPP
#define LITHIUM_TASK_ADVANCED_TIMELAPSE_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Timelapse task.
 * 
 * Performs timelapse imaging with specified intervals and automatic
 * exposure adjustments for different scenarios.
 */
class TimelapseTask : public Task {
public:
    TimelapseTask()
        : Task("Timelapse",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "Timelapse"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTimelapseParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_TIMELAPSE_TASK_HPP
