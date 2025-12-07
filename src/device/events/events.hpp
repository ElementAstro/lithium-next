/*
 * events.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Aggregated header for device events module

**************************************************/

#ifndef LITHIUM_DEVICE_EVENTS_HPP
#define LITHIUM_DEVICE_EVENTS_HPP

// Device event bus
#include "device_event_bus.hpp"

namespace lithium::device {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Device events module version.
 */
inline constexpr const char* DEVICE_EVENTS_VERSION = "1.0.0";

/**
 * @brief Get device events module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDeviceEventsVersion() noexcept {
    return DEVICE_EVENTS_VERSION;
}

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_EVENTS_HPP
