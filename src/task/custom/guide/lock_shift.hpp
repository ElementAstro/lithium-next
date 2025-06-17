#ifndef LITHIUM_TASK_GUIDE_LOCK_SHIFT_TASKS_HPP
#define LITHIUM_TASK_GUIDE_LOCK_SHIFT_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Get lock shift enabled status task.
 * Checks if lock shift is currently enabled.
 */
class GetLockShiftEnabledTask : public Task {
public:
    GetLockShiftEnabledTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getLockShiftEnabled(const json& params);
};

/**
 * @brief Set lock shift enabled task.
 * Enables or disables lock shift functionality.
 */
class SetLockShiftEnabledTask : public Task {
public:
    SetLockShiftEnabledTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setLockShiftEnabled(const json& params);
};

/**
 * @brief Get lock shift parameters task.
 * Gets current lock shift configuration parameters.
 */
class GetLockShiftParamsTask : public Task {
public:
    GetLockShiftParamsTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getLockShiftParams(const json& params);
};

/**
 * @brief Set lock shift parameters task.
 * Sets lock shift configuration parameters.
 */
class SetLockShiftParamsTask : public Task {
public:
    SetLockShiftParamsTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setLockShiftParams(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_LOCK_SHIFT_TASKS_HPP
