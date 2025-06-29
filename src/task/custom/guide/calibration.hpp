#ifndef LITHIUM_TASK_GUIDE_CALIBRATION_TASKS_HPP
#define LITHIUM_TASK_GUIDE_CALIBRATION_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Guider calibration task.
 * Performs mount calibration for guiding.
 */
class GuiderCalibrateTask : public Task {
public:
    GuiderCalibrateTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void performCalibration(const json& params);
};

/**
 * @brief Clear guider calibration task.
 * Clears existing calibration data.
 */
class GuiderClearCalibrationTask : public Task {
public:
    GuiderClearCalibrationTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void clearCalibration(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_CALIBRATION_TASKS_HPP
