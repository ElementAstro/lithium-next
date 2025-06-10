#ifndef LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 板面解析集成任务 ====================

/**
 * @brief Plate solving exposure task.
 * Takes an exposure and performs plate solving for astrometry.
 */
class PlateSolveExposureTask : public TaskCreator<PlateSolveExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePlateSolveParameters(const json& params);
};

/**
 * @brief Automatic centering task.
 * Centers the target object in the field of view using plate solving.
 */
class CenteringTask : public TaskCreator<CenteringTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCenteringParameters(const json& params);
};

/**
 * @brief Mosaic imaging task.
 * Performs automated mosaic imaging with plate solving and positioning.
 */
class MosaicTask : public TaskCreator<MosaicTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMosaicParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP
