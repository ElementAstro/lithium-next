#ifndef LITHIUM_TASK_CAMERA_ALL_TASKS_HPP
#define LITHIUM_TASK_CAMERA_ALL_TASKS_HPP

/**
 * @file camera_tasks.hpp
 * @brief Comprehensive header including all camera-related tasks
 * 
 * This file provides a single include point for all camera task functionality,
 * organized into logical groups for better maintainability.
 */

// Include all camera task categories
#include "common.hpp"
#include "basic_exposure.hpp"
#include "calibration_tasks.hpp"
#include "sequence_tasks.hpp"
#include "focus_tasks.hpp"
#include "filter_tasks.hpp"
#include "guide_tasks.hpp"
#include "safety_tasks.hpp"
#include "platesolve_tasks.hpp"

namespace lithium::task::task {

/**
 * @brief Namespace alias for camera tasks
 * 
 * Provides a convenient way to access camera-specific functionality
 * while maintaining the flat namespace structure for compatibility.
 */
namespace camera = lithium::task::task;

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_ALL_TASKS_HPP
