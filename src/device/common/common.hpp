/*
 * common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Aggregated header for device common module

**************************************************/

#ifndef LITHIUM_DEVICE_COMMON_HPP
#define LITHIUM_DEVICE_COMMON_HPP

// Error codes and error structures
#include "device_error.hpp"

// Exception hierarchy
#include "device_exceptions.hpp"

// Result types using std::expected
#include "device_result.hpp"

namespace lithium::device {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Device common module version.
 */
inline constexpr const char* DEVICE_COMMON_VERSION = "1.0.0";

/**
 * @brief Get device common module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDeviceCommonVersion() noexcept {
    return DEVICE_COMMON_VERSION;
}

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_COMMON_HPP
