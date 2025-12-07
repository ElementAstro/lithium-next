/**
 * @file types.hpp
 * @brief Aggregated header for core task types.
 *
 * Include this file to get access to all core task types.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_CORE_TYPES_HPP
#define LITHIUM_TASK_CORE_TYPES_HPP

// ============================================================================
// Core Task Types
// ============================================================================

#include "exception.hpp"
#include "factory.hpp"
#include "generator.hpp"
#include "registration.hpp"
#include "sequencer.hpp"
#include "target.hpp"
#include "task.hpp"

namespace lithium::task {

/**
 * @brief Core module version.
 */
inline constexpr const char* CORE_VERSION = "1.0.0";

}  // namespace lithium::task

#endif  // LITHIUM_TASK_CORE_TYPES_HPP
