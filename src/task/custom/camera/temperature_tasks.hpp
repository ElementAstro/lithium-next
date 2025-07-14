#ifndef LITHIUM_TASK_CAMERA_TEMPERATURE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_TEMPERATURE_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Camera cooling control task.
 * Manages camera cooling system with temperature monitoring.
 */
class CoolingControlTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCoolingParameters(const json& params);
    static void handleCoolingError(Task& task, const std::exception& e);
};

/**
 * @brief Temperature monitoring task.
 * Continuously monitors camera temperature and cooling performance.
 */
class TemperatureMonitorTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMonitoringParameters(const json& params);
};

/**
 * @brief Temperature stabilization task.
 * Waits for camera temperature to stabilize within specified range.
 */
class TemperatureStabilizationTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateStabilizationParameters(const json& params);
};

/**
 * @brief Cooling power optimization task.
 * Automatically adjusts cooling power for optimal performance and efficiency.
 */
class CoolingOptimizationTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateOptimizationParameters(const json& params);
};

/**
 * @brief Temperature alert task.
 * Monitors temperature and triggers alerts when thresholds are exceeded.
 */
class TemperatureAlertTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAlertParameters(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_TEMPERATURE_TASKS_HPP
