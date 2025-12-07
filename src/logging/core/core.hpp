/**
 * @file core.hpp
 * @brief Aggregated header for logging core module.
 *
 * This is the primary include file for the core logging components.
 * Include this file to get access to all core logging functionality
 * including types, logging manager, and logger registry.
 *
 * @date 2024-11-28
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_LOGGING_CORE_HPP
#define LITHIUM_LOGGING_CORE_HPP

// ============================================================================
// Core Logging Components
// ============================================================================

#include "logger_registry.hpp"
#include "logging_manager.hpp"
#include "types.hpp"

namespace lithium::logging {

/**
 * @brief Core module version.
 */
inline constexpr const char* CORE_MODULE_VERSION = "1.0.0";

/**
 * @brief Get core module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getCoreModuleVersion() noexcept {
    return CORE_MODULE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

// Manager types
using LoggingManagerPtr = std::shared_ptr<LoggingManager>;

// Registry types
using LoggerRegistryPtr = std::shared_ptr<LoggerRegistry>;

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_CORE_HPP
