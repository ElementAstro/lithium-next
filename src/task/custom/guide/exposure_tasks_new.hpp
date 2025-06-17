#ifndef LITHIUM_TASK_GUIDE_EXPOSURE_TASKS_HPP
#define LITHIUM_TASK_GUIDE_EXPOSURE_TASKS_HPP

#include "task/task.hpp"
#include "atom/function/global_ptr.hpp"
#include "client/phd2/client.h"
#include "constant/constant.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Guided exposure task.
 * Performs a single guided exposure with dithering support.
 */
class GuidedExposureTask : public Task {
public:
    GuidedExposureTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performGuidedExposure(const json& params);
};

/**
 * @brief Auto guiding task.
 * Manages continuous guiding throughout imaging session.
 */
class AutoGuidingTask : public Task {
public:
    AutoGuidingTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performAutoGuiding(const json& params);
};

/**
 * @brief Guided sequence task.
 * Performs a sequence of guided exposures with automatic dithering.
 */
class GuidedSequenceTask : public Task {
public:
    GuidedSequenceTask();
    
    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performGuidedSequence(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_EXPOSURE_TASKS_HPP
