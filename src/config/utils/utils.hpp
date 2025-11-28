/**
 * @file utils.hpp
 * @brief Aggregated header for configuration utilities.
 *
 * Include this file to get access to all configuration utilities.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_CONFIG_UTILS_HPP
#define LITHIUM_CONFIG_UTILS_HPP

// ============================================================================
// Configuration Utilities
// ============================================================================

#include "json5.hpp"
#include "macro.hpp"

namespace lithium::config {

/**
 * @brief Utils module version.
 */
inline constexpr const char* UTILS_VERSION = "1.0.0";

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_UTILS_HPP
