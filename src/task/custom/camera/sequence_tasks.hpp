#ifndef LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 智能曝光和序列任务 ====================

/**
 * @brief Derived class for creating SmartExposure tasks.
 */
class SmartExposureTask : public TaskCreator<SmartExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSmartExposureParameters(const json& params);
};

// ==================== 自动化拍摄序列任务 ====================

/**
 * @brief Deep sky sequence task.
 * Performs automated deep sky imaging sequence.
 */
class DeepSkySequenceTask : public TaskCreator<DeepSkySequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateDeepSkyParameters(const json& params);
};

/**
 * @brief Planetary imaging task.
 * Performs high-speed planetary imaging with lucky imaging.
 */
class PlanetaryImagingTask : public TaskCreator<PlanetaryImagingTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePlanetaryParameters(const json& params);
};

/**
 * @brief Timelapse task.
 * Performs timelapse imaging with specified intervals.
 */
class TimelapseTask : public TaskCreator<TimelapseTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTimelapseParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP
