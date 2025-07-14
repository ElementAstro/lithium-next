#ifndef LITHIUM_TASK_GUIDE_ADVANCED_TASKS_HPP
#define LITHIUM_TASK_GUIDE_ADVANCED_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Get search region task.
 * Gets the current search region radius.
 */
class GetSearchRegionTask : public Task {
public:
    GetSearchRegionTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getSearchRegion(const json& params);
};

/**
 * @brief Flip calibration task.
 * Flips the calibration data (useful for meridian flips).
 */
class FlipCalibrationTask : public Task {
public:
    FlipCalibrationTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void flipCalibration(const json& params);
};

/**
 * @brief Get calibration data task.
 * Gets detailed calibration information.
 */
class GetCalibrationDataTask : public Task {
public:
    GetCalibrationDataTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getCalibrationData(const json& params);
};

/**
 * @brief Get algorithm parameter names task.
 * Gets all available parameter names for a given axis.
 */
class GetAlgoParamNamesTask : public Task {
public:
    GetAlgoParamNamesTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getAlgoParamNames(const json& params);
};

/**
 * @brief Comprehensive guide stats task.
 * Gets comprehensive guiding statistics and performance metrics.
 */
class GuideStatsTask : public Task {
public:
    GuideStatsTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getGuideStats(const json& params);
};

/**
 * @brief Emergency stop task.
 * Emergency stop all guiding operations.
 */
class EmergencyStopTask : public Task {
public:
    EmergencyStopTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void emergencyStop(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_ADVANCED_TASKS_HPP
