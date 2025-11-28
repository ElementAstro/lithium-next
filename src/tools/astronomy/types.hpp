/**
 * @file types.hpp
 * @brief Aggregated header for all astronomical types.
 *
 * This is the main include file for the astronomy types module.
 * Include this file to get access to all astronomical data types.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TOOLS_ASTRONOMY_TYPES_HPP
#define LITHIUM_TOOLS_ASTRONOMY_TYPES_HPP

// Core constants and utilities
#include "constants.hpp"

// Coordinate systems
#include "coordinates.hpp"

// Observation constraints
#include "constraints.hpp"

// Meridian flip handling
#include "meridian.hpp"

// Exposure planning
#include "exposure.hpp"

// Target configuration
#include "target_config.hpp"

namespace lithium::tools::astronomy {

/**
 * @brief Astronomy types module version.
 */
inline constexpr const char* ASTRONOMY_TYPES_VERSION = "1.0.0";

}  // namespace lithium::tools::astronomy

#endif  // LITHIUM_TOOLS_ASTRONOMY_TYPES_HPP
