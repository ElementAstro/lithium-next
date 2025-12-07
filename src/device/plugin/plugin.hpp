/*
 * plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Aggregated header for device plugin module

**************************************************/

#ifndef LITHIUM_DEVICE_PLUGIN_HPP
#define LITHIUM_DEVICE_PLUGIN_HPP

// Device plugin interface
#include "device_plugin_interface.hpp"

// Device plugin loader
#include "device_plugin_loader.hpp"

namespace lithium::device {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Device plugin module version.
 */
inline constexpr const char* DEVICE_PLUGIN_VERSION = "1.0.0";

/**
 * @brief Get device plugin module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDevicePluginVersion() noexcept {
    return DEVICE_PLUGIN_VERSION;
}

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_PLUGIN_HPP
