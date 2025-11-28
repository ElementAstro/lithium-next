/**
 * @file components.hpp
 * @brief Aggregated header for all configuration components.
 *
 * Include this file to get access to all configuration components.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_CONFIG_COMPONENTS_HPP
#define LITHIUM_CONFIG_COMPONENTS_HPP

// ============================================================================
// Configuration Components
// ============================================================================

#include "cache.hpp"
#include "serializer.hpp"
#include "validator.hpp"
#include "watcher.hpp"

namespace lithium::config {

/**
 * @brief Components module version.
 */
inline constexpr const char* COMPONENTS_VERSION = "1.0.0";

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_COMPONENTS_HPP
