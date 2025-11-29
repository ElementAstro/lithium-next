/**
 * @file all_tasks.hpp
 * @brief Unified header for all task modules
 *
 * This file provides a single include point for all task functionality,
 * organized by device/category for better maintainability.
 *
 * Module structure:
 * - common/       : Shared task base class, types, and validation
 * - camera/       : Camera exposure, calibration, and imaging tasks
 * - focuser/      : Focus and temperature compensation tasks
 * - filterwheel/  : Filter wheel and sequence tasks
 * - guider/       : Autoguiding and dithering tasks
 * - astrometry/   : Plate solving and centering tasks
 * - observatory/  : Safety, weather, and dome control tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_ALL_TASKS_HPP
#define LITHIUM_TASK_ALL_TASKS_HPP

// Common components
#include "common/task_base.hpp"
#include "common/types.hpp"
#include "common/validation.hpp"

// Device-specific task modules
#include "astrometry/astrometry_tasks.hpp"
#include "camera/camera_tasks.hpp"
#include "filterwheel/filterwheel_tasks.hpp"
#include "focuser/focuser_tasks.hpp"
#include "guider/guider_tasks.hpp"
#include "observatory/observatory_tasks.hpp"

namespace lithium::task {

/**
 * @brief Namespace aliases for convenient access
 */
namespace camera_tasks = lithium::task::camera;
namespace focuser_tasks = lithium::task::focuser;
namespace filterwheel_tasks = lithium::task::filterwheel;
namespace guider_tasks = lithium::task::guider;
namespace astrometry_tasks = lithium::task::astrometry;
namespace observatory_tasks = lithium::task::observatory;

}  // namespace lithium::task

#endif  // LITHIUM_TASK_ALL_TASKS_HPP
