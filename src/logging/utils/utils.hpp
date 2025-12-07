/**
 * @file utils.hpp
 * @brief Aggregated header for logging utilities module.
 *
 * This is the primary include file for the logging utilities.
 * Include this file to get access to log statistics and export utilities.
 *
 * @date 2024-11-28
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_LOGGING_UTILS_HPP
#define LITHIUM_LOGGING_UTILS_HPP

// ============================================================================
// Logging Utilities
// ============================================================================

#include "log_exporter.hpp"
#include "log_statistics.hpp"

namespace lithium::logging {

/**
 * @brief Utils module version.
 */
inline constexpr const char* UTILS_MODULE_VERSION = "1.0.0";

/**
 * @brief Get utils module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getUtilsModuleVersion() noexcept {
    return UTILS_MODULE_VERSION;
}

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_UTILS_HPP
