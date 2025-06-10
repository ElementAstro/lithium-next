#ifndef LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 安全和监控任务 ====================

/**
 * @brief Weather monitoring task.
 * Monitors weather conditions and performs safety imaging.
 */
class WeatherMonitorTask : public TaskCreator<WeatherMonitorTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateWeatherParameters(const json& params);
};

/**
 * @brief Cloud detection task.
 * Performs cloud detection using all-sky camera.
 */
class CloudDetectionTask : public TaskCreator<CloudDetectionTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCloudDetectionParameters(const json& params);
    static void validateCloudParameters(const json& params);
};

/**
 * @brief Safety shutdown task.
 * Performs safe shutdown of imaging equipment.
 */
class SafetyShutdownTask : public TaskCreator<SafetyShutdownTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSafetyParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP
