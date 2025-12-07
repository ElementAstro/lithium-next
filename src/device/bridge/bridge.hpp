/*
 * bridge.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Aggregated header for device bridge module

**************************************************/

#ifndef LITHIUM_DEVICE_BRIDGE_HPP
#define LITHIUM_DEVICE_BRIDGE_HPP

// Device component adapter
#include "device_component_adapter.hpp"

// Device component bridge
#include "device_component_bridge.hpp"

namespace lithium::device {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Device bridge module version.
 */
inline constexpr const char* DEVICE_BRIDGE_VERSION = "1.0.0";

/**
 * @brief Get device bridge module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDeviceBridgeVersion() noexcept {
    return DEVICE_BRIDGE_VERSION;
}

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_BRIDGE_HPP
