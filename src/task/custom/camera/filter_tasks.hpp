#ifndef LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

// ==================== 滤镜轮集成任务 ====================

/**
 * @brief Filter sequence task.
 * Performs multi-filter sequence imaging.
 */
class FilterSequenceTask : public Task {
public:
using Task::Task;
    FilterSequenceTask()
        : Task("FilterSequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFilterSequenceParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief RGB sequence task.
 * Performs RGB color imaging sequence.
 */
class RGBSequenceTask : public Task {
public:
    RGBSequenceTask()
        : Task("RGBSequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateRGBParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Narrowband sequence task.
 * Performs narrowband filter imaging sequence (Ha, OIII, SII, etc.).
 */
class NarrowbandSequenceTask : public Task {
public:
    NarrowbandSequenceTask()
        : Task("NarrowbandSequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateNarrowbandParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
