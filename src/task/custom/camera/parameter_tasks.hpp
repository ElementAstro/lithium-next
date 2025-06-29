#ifndef LITHIUM_TASK_CAMERA_PARAMETER_TASKS_HPP
#define LITHIUM_TASK_CAMERA_PARAMETER_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Camera gain control task.
 * Manages camera gain settings for sensitivity adjustment.
 */
class GainControlTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateGainParameters(const json& params);
    static void handleParameterError(Task& task, const std::exception& e);
};

/**
 * @brief Camera offset control task.
 * Manages camera offset/pedestal settings.
 */
class OffsetControlTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateOffsetParameters(const json& params);
};

/**
 * @brief Camera ISO control task.
 * Manages ISO settings for DSLR-type cameras.
 */
class ISOControlTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateISOParameters(const json& params);
};

/**
 * @brief Auto parameter optimization task.
 * Automatically optimizes gain, offset, and other parameters.
 */
class AutoParameterTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoParameters(const json& params);
};

/**
 * @brief Parameter profile management task.
 * Saves and loads parameter profiles for different imaging scenarios.
 */
class ParameterProfileTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateProfileParameters(const json& params);
};

/**
 * @brief Parameter status query task.
 * Retrieves current parameter values and camera status.
 */
class ParameterStatusTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_PARAMETER_TASKS_HPP
