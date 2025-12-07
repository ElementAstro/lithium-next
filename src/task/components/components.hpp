/**
 * @file components.hpp
 * @brief Aggregated header for all task components.
 *
 * Include this file to get access to all task components including
 * device-specific tasks, workflow tasks, and script tasks.
 *
 * Module structure:
 * - common/       : Shared task base class, types, and validation
 * - camera/       : Camera exposure, calibration, and imaging tasks
 * - focuser/      : Focus and temperature compensation tasks
 * - filterwheel/  : Filter wheel and sequence tasks
 * - guider/       : Autoguiding and dithering tasks
 * - astrometry/   : Plate solving and centering tasks
 * - observatory/  : Safety, weather, and dome control tasks
 * - workflow/     : High-level workflow tasks
 * - script/       : Script execution tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_COMPONENTS_HPP
#define LITHIUM_TASK_COMPONENTS_HPP

// ============================================================================
// Common Components
// ============================================================================

#include "common/task_base.hpp"
#include "common/types.hpp"
#include "common/validation.hpp"

// ============================================================================
// Device-Specific Task Modules
// ============================================================================

#include "astrometry/astrometry_tasks.hpp"
#include "camera/camera_tasks.hpp"
#include "filterwheel/filterwheel_tasks.hpp"
#include "focuser/focuser_tasks.hpp"
#include "guider/guider_tasks.hpp"
#include "observatory/observatory_tasks.hpp"

// ============================================================================
// Workflow and Script Tasks
// ============================================================================

#include "workflow/workflow_tasks.hpp"

// ============================================================================
// Standalone Task Types
// ============================================================================

#include "config_task.hpp"
#include "device_task.hpp"
#include "script_task.hpp"
#include "search_task.hpp"
#include "solver_task.hpp"
#include "task_wrappers.hpp"

namespace lithium::task {

/**
 * @brief Components module version.
 */
inline constexpr const char* COMPONENTS_VERSION = "1.0.0";

// ============================================================================
// Convenience Namespace Aliases
// ============================================================================

namespace camera_tasks = lithium::task::camera;
namespace focuser_tasks = lithium::task::focuser;
namespace filterwheel_tasks = lithium::task::filterwheel;
namespace guider_tasks = lithium::task::guider;
namespace astrometry_tasks = lithium::task::astrometry;
namespace observatory_tasks = lithium::task::observatory;

}  // namespace lithium::task

#endif  // LITHIUM_TASK_COMPONENTS_HPP
