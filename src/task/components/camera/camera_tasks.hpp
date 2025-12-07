/**
 * @file camera_tasks.hpp
 * @brief Unified header for camera-related tasks
 *
 * This file provides a single include point for camera task functionality.
 * Camera tasks are those directly related to camera operation and imaging.
 *
 * Module structure:
 * - common/       : Camera-specific base class and validation
 * - exposure/     : Single/multiple exposure tasks
 * - settings/     : Camera settings and preview
 * - calibration/  : Dark, bias, flat frame acquisition
 * - imaging/      : Deep sky, planetary, timelapse, mosaic
 *
 * Note: The following are in separate modules under src/task/components/:
 * - focuser/      : Focus tasks
 * - filterwheel/  : Filter tasks
 * - guider/       : Guiding tasks
 * - astrometry/   : Plate solving
 * - observatory/  : Safety tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMPONENTS_CAMERA_TASKS_HPP
#define LITHIUM_TASK_COMPONENTS_CAMERA_TASKS_HPP

// Common components
#include "common/camera_task_base.hpp"

// Camera-specific task modules
#include "calibration/calibration_tasks.hpp"
#include "exposure/exposure_tasks.hpp"
#include "guiding/guiding_tasks.hpp"
#include "imaging/imaging_tasks.hpp"
#include "settings/settings_tasks.hpp"

namespace lithium::task {

/**
 * @brief Convenience namespace alias for camera tasks
 */
namespace camera_tasks = lithium::task::camera;

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMPONENTS_CAMERA_TASKS_HPP
