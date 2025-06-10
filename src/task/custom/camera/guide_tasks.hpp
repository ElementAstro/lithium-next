#ifndef LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 导星和抖动任务 ====================

/**
 * @brief Guided exposure task.
 * Performs guided exposure with autoguiding integration.
 */
class GuidedExposureTask : public TaskCreator<GuidedExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateGuidingParameters(const json& params);
};

/**
 * @brief Dithering sequence task.
 * Performs dithering sequence for improved image quality.
 */
class DitherSequenceTask : public TaskCreator<DitherSequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateDitheringParameters(const json& params);
};

/**
 * @brief Automatic guiding setup task.
 * Sets up and calibrates autoguiding system.
 */
class AutoGuidingTask : public TaskCreator<AutoGuidingTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoGuidingParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP
