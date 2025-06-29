#ifndef LITHIUM_TASK_GUIDE_ALGORITHM_TASKS_HPP
#define LITHIUM_TASK_GUIDE_ALGORITHM_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Set guide algorithm parameter task.
 * Sets parameters for RA/Dec guiding algorithms.
 */
class SetAlgoParamTask : public Task {
public:
    SetAlgoParamTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setAlgorithmParameter(const json& params);
};

/**
 * @brief Get guide algorithm parameter task.
 * Gets current algorithm parameter values.
 */
class GetAlgoParamTask : public Task {
public:
    GetAlgoParamTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getAlgorithmParameter(const json& params);
};

/**
 * @brief Set Dec guide mode task.
 * Sets declination guiding mode (Off/Auto/North/South).
 */
class SetDecGuideModeTask : public Task {
public:
    SetDecGuideModeTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setDecGuideMode(const json& params);
};

/**
 * @brief Get Dec guide mode task.
 * Gets current declination guiding mode.
 */
class GetDecGuideModeTask : public Task {
public:
    GetDecGuideModeTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getDecGuideMode(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_ALGORITHM_TASKS_HPP
