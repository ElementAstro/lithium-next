/**
 * @file config.hpp
 * @brief Main aggregated header for Lithium Config library.
 *
 * This is the primary include file for the config library.
 * Include this file to get access to all configuration functionality.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_CONFIG_HPP
#define LITHIUM_CONFIG_HPP

// ============================================================================
// Core Module
// ============================================================================
// Configuration manager and exception types

#include "core/types.hpp"

// ============================================================================
// Components Module
// ============================================================================
// Cache, Validator, Serializer, Watcher components

#include "components/components.hpp"

// ============================================================================
// Utils Module
// ============================================================================
// JSON5 parser and helper macros

#include "utils/utils.hpp"

namespace lithium::config {

/**
 * @brief Lithium Config library version.
 */
inline constexpr const char* CONFIG_VERSION = "1.0.0";

/**
 * @brief Get config library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getConfigVersion() noexcept {
    return CONFIG_VERSION;
}

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_HPP
