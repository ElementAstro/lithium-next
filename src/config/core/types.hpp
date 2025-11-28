/**
 * @file types.hpp
 * @brief Aggregated header for core configuration types.
 *
 * Include this file to get access to all core configuration types.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_CONFIG_CORE_TYPES_HPP
#define LITHIUM_CONFIG_CORE_TYPES_HPP

// ============================================================================
// Core Configuration Types
// ============================================================================

#include "exception.hpp"
#include "manager.hpp"

namespace lithium::config {

/**
 * @brief Core module version.
 */
inline constexpr const char* CORE_VERSION = "1.0.0";

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_CORE_TYPES_HPP
