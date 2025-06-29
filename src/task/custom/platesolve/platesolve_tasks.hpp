#ifndef LITHIUM_TASK_PLATESOLVE_PLATESOLVE_TASKS_HPP
#define LITHIUM_TASK_PLATESOLVE_PLATESOLVE_TASKS_HPP

// Main header file that includes all plate solve task components
#include "platesolve_common.hpp"
#include "platesolve_exposure.hpp"
#include "centering_task.hpp"
#include "mosaic_task.hpp"

// Maintain backward compatibility with old namespace
namespace lithium::task::task {

// Type aliases for backward compatibility
using PlateSolveExposureTask = lithium::task::platesolve::PlateSolveExposureTask;
using CenteringTask = lithium::task::platesolve::CenteringTask;
using MosaicTask = lithium::task::platesolve::MosaicTask;

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_PLATESOLVE_PLATESOLVE_TASKS_HPP
