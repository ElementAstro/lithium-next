#ifndef LITHIUM_TASK_CAMERA_TELESCOPE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_TELESCOPE_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

/**
 * @brief Telescope goto and imaging task.
 * Slews telescope to target and performs imaging sequence.
 */
class TelescopeGotoImagingTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTelescopeParameters(const json& params);
    static void handleTelescopeError(Task& task, const std::exception& e);
};

/**
 * @brief Telescope tracking control task.
 * Manages telescope tracking during exposures.
 */
class TrackingControlTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTrackingParameters(const json& params);
};

/**
 * @brief Meridian flip task.
 * Handles meridian flip and imaging resumption.
 */
class MeridianFlipTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMeridianFlipParameters(const json& params);
};

/**
 * @brief Telescope park task.
 * Parks telescope safely after imaging session.
 */
class TelescopeParkTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

/**
 * @brief Pointing model task.
 * Builds pointing model for improved accuracy.
 */
class PointingModelTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePointingModelParameters(const json& params);
};

/**
 * @brief Slew speed optimization task.
 * Optimizes telescope slew speeds for different operations.
 */
class SlewSpeedOptimizationTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_TELESCOPE_TASKS_HPP
