#ifndef LITHIUM_TASK_CORE_REGISTRATION_HPP
#define LITHIUM_TASK_CORE_REGISTRATION_HPP

#include "factory.hpp"
#include "task.hpp"

namespace lithium::task {

/**
 * @brief Registers all built-in tasks with the TaskFactory
 *
 * This function registers all camera, device, config, script, and search
 * tasks with the global TaskFactory instance. Should be called once at
 * application startup.
 *
 * Registered task categories:
 * - Camera: TakeExposure, TakeManyExposure, SubframeExposure, CameraSettings,
 * CameraPreview
 * - Sequence: SmartExposure, DeepSkySequence, PlanetaryImaging, Timelapse
 * - Calibration: AutoCalibration, ThermalCycle
 * - Focus: AutoFocus, FocusSeries
 * - Filter: FilterSequence, RGBSequence, NarrowbandSequence
 * - Guide: AutoGuiding, GuidedExposure, DitherSequence
 * - Safety: WeatherMonitor, CloudDetection, SafetyShutdown
 * - Platesolve: PlateSolveExposure, Centering, Mosaic
 * - Device: DeviceConnect
 * - Config: LoadConfig
 * - Script: RunScript
 * - Search: TargetSearch
 */
void registerBuiltInTasks();

/**
 * @brief Template wrapper for creating task instances
 *
 * Creates a task instance using the standard Task constructor pattern.
 * The task is created with the name and config parameters.
 *
 * @tparam TaskType The concrete task type (must inherit from Task)
 * @param name The name for the task instance
 * @param config Initial configuration parameters
 * @return Unique pointer to the created task instance
 */
template <typename TaskType>
std::unique_ptr<TaskType> createTaskWrapper(const std::string& name,
                                            const json& config) {
    auto task = std::make_unique<TaskType>(name, config);
    task->setTaskType(TaskType::taskName());
    return task;
}

/**
 * @brief Get a list of all registered task type names
 * @return Vector of registered task type identifiers
 */
std::vector<std::string> getRegisteredTaskTypes();

/**
 * @brief Check if a specific task type is registered
 * @param taskType Task type identifier
 * @return True if registered
 */
bool isTaskTypeRegistered(const std::string& taskType);

}  // namespace lithium::task

#endif  // LITHIUM_TASK_CORE_REGISTRATION_HPP
