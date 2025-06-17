#ifndef LITHIUM_TASK_ADVANCED_SMART_EXPOSURE_TASK_HPP
#define LITHIUM_TASK_ADVANCED_SMART_EXPOSURE_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Smart exposure task for automatic exposure optimization.
 * 
 * This task automatically optimizes exposure time to achieve a target
 * signal-to-noise ratio (SNR) through iterative test exposures.
 */
class SmartExposureTask : public Task {
public:
    SmartExposureTask()
        : Task("SmartExposure",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "SmartExposure"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSmartExposureParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_SMART_EXPOSURE_TASK_HPP
