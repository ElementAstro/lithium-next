#ifndef LITHIUM_TASK_GUIDE_VARIABLE_DELAY_TASKS_HPP
#define LITHIUM_TASK_GUIDE_VARIABLE_DELAY_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Get variable delay settings task.
 * Gets current variable delay configuration.
 */
class GetVariableDelaySettingsTask : public Task {
public:
    GetVariableDelaySettingsTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getVariableDelaySettings(const json& params);
};

/**
 * @brief Set variable delay settings task.
 * Sets variable delay configuration parameters.
 */
class SetVariableDelaySettingsTask : public Task {
public:
    SetVariableDelaySettingsTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setVariableDelaySettings(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_VARIABLE_DELAY_TASKS_HPP
