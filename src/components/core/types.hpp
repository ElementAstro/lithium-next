/**
 * @file types.hpp
 * @brief Aggregated header for core component types.
 *
 * Include this file to get access to all core component types.
 * This is a lightweight alternative to core.hpp that includes
 * only the essential type definitions.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_CORE_TYPES_HPP
#define LITHIUM_COMPONENTS_CORE_TYPES_HPP

// ============================================================================
// Core Component Types
// ============================================================================

#include "module.hpp"
#include "version.hpp"

namespace lithium {

/**
 * @brief Core types module version.
 */
inline constexpr const char* CORE_TYPES_VERSION = "1.0.0";

}  // namespace lithium

#endif  // LITHIUM_COMPONENTS_CORE_TYPES_HPP
