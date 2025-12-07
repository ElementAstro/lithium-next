/**
 * @file sinks.hpp
 * @brief Aggregated header for logging sinks module.
 *
 * This is the primary include file for the logging sinks.
 * Include this file to get access to all sink implementations
 * and the sink factory.
 *
 * @date 2024-11-28
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_LOGGING_SINKS_HPP
#define LITHIUM_LOGGING_SINKS_HPP

// ============================================================================
// Logging Sinks
// ============================================================================

#include "ring_buffer_sink.hpp"
#include "sink_factory.hpp"

namespace lithium::logging {

/**
 * @brief Sinks module version.
 */
inline constexpr const char* SINKS_MODULE_VERSION = "1.0.0";

/**
 * @brief Get sinks module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getSinksModuleVersion() noexcept {
    return SINKS_MODULE_VERSION;
}

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_SINKS_HPP
