#ifndef LITHIUM_TASK_GUIDE_CONTROL_TASKS_HPP
#define LITHIUM_TASK_GUIDE_CONTROL_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Start guiding task.
 * Starts autoguiding with guide star selection.
 */
class GuiderStartTask : public Task {
public:
    GuiderStartTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void startGuiding(const json& params);
};

/**
 * @brief Stop guiding task.
 * Stops autoguiding.
 */
class GuiderStopTask : public Task {
public:
    GuiderStopTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void stopGuiding(const json& params);
};

/**
 * @brief Pause guiding task.
 * Temporarily pauses autoguiding.
 */
class GuiderPauseTask : public Task {
public:
    GuiderPauseTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void pauseGuiding(const json& params);
};

/**
 * @brief Resume guiding task.
 * Resumes paused autoguiding.
 */
class GuiderResumeTask : public Task {
public:
    GuiderResumeTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void resumeGuiding(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_CONTROL_TASKS_HPP
