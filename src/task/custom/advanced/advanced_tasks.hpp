#ifndef LITHIUM_TASK_ADVANCED_TASKS_HPP
#define LITHIUM_TASK_ADVANCED_TASKS_HPP

/**
 * @file advanced_tasks.hpp
 * @brief Advanced astrophotography task components
 *
 * This header includes all advanced task implementations for automated
 * astrophotography operations including smart exposure, deep sky sequences,
 * planetary imaging, and timelapse functionality.
 */

#include "smart_exposure_task.hpp"
#include "deep_sky_sequence_task.hpp"
#include "planetary_imaging_task.hpp"
#include "timelapse_task.hpp"
#include "meridian_flip_task.hpp"
#include "intelligent_sequence_task.hpp"
#include "auto_calibration_task.hpp"

namespace lithium::task::advanced {

/**
 * @brief Register all advanced tasks with the task factory
 *
 * This function should be called during application initialization
 * to make all advanced tasks available for execution.
 */
void registerAdvancedTasks();

/**
 * @brief Get list of all available advanced task names
 * @return Vector of task names
 */
std::vector<std::string> getAdvancedTaskNames();

/**
 * @brief Check if a task is an advanced task
 * @param taskName Name of the task to check
 * @return True if the task is an advanced task
 */
bool isAdvancedTask(const std::string& taskName);

}  // namespace lithium::task::advanced

#endif  // LITHIUM_TASK_ADVANCED_TASKS_HPP
