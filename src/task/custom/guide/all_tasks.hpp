#ifndef LITHIUM_TASK_GUIDE_ALL_TASKS_HPP
#define LITHIUM_TASK_GUIDE_ALL_TASKS_HPP

/**
 * @file all_tasks.hpp
 * @brief Consolidated header for all guide-related tasks
 * 
 * This header includes all the individual guide task headers for convenience.
 * Include this file to access all guide task functionality.
 */

// Core functionality tasks
#include "core/connection_tasks.hpp"
#include "core/calibration_tasks.hpp"

// Basic operation tasks
#include "basic/control_tasks.hpp"
#include "basic/dither_tasks.hpp"
#include "basic/exposure_tasks.hpp"

// Advanced feature tasks
#include "advanced/algorithm_tasks.hpp"
#include "advanced/star_tasks.hpp"
#include "advanced/camera_tasks.hpp"
#include "advanced/lock_shift_tasks.hpp"
#include "advanced/variable_delay_tasks.hpp"
#include "advanced/advanced_tasks.hpp"

// System and utility tasks
#include "utilities/system_tasks.hpp"
#include "utilities/device_config_tasks.hpp"

// Workflow and automation tasks
#include "workflows.hpp"
#include "auto_config.hpp"
#include "diagnostics.hpp"

namespace lithium::task::guide {

/**
 * @brief Register all guide tasks with the task factory
 * 
 * This function should be called during application initialization
 * to register all guide-related tasks with the task factory system.
 */
void registerAllGuideTasks();

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_ALL_TASKS_HPP
