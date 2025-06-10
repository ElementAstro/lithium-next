#ifndef LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 滤镜轮集成任务 ====================

/**
 * @brief Filter sequence task.
 * Performs multi-filter sequence imaging.
 */
class FilterSequenceTask : public TaskCreator<FilterSequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFilterSequenceParameters(const json& params);
};

/**
 * @brief RGB sequence task.
 * Performs RGB color imaging sequence.
 */
class RGBSequenceTask : public TaskCreator<RGBSequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateRGBParameters(const json& params);
};

/**
 * @brief Narrowband sequence task.
 * Performs narrowband filter imaging sequence (Ha, OIII, SII, etc.).
 */
class NarrowbandSequenceTask : public TaskCreator<NarrowbandSequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateNarrowbandParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
