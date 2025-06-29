#ifndef LITHIUM_TASK_ADVANCED_DEEP_SKY_SEQUENCE_TASK_HPP
#define LITHIUM_TASK_ADVANCED_DEEP_SKY_SEQUENCE_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Deep sky sequence task.
 *
 * Performs automated deep sky imaging sequence with multiple filters,
 * dithering support, and progress tracking.
 */
class DeepSkySequenceTask : public Task {
public:
    DeepSkySequenceTask()
        : Task("DeepSkySequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "DeepSkySequence"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateDeepSkyParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_DEEP_SKY_SEQUENCE_TASK_HPP
