// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LITHIUM_TARGET_OBSERVABILITY_MODULE_HPP
#define LITHIUM_TARGET_OBSERVABILITY_MODULE_HPP

/**
 * @file module.hpp
 * @brief Observability module - comprehensive astronomical observability calculations.
 *
 * This module provides high-precision calculations for determining when and where
 * celestial objects are observable from a specific observer location.
 *
 * Main Components:
 * - VisibilityCalculator: Core calculations for rise/set times and coordinates
 * - TimeWindowFilter: Convenient filtering by time ranges and constraints
 *
 * Usage Example:
 * @code
 * #include "target/observability/module.hpp"
 *
 * using namespace lithium::target::observability;
 *
 * // Create visibility calculator
 * ObserverLocation location(40.1125, -88.2434, 228.0);  // Urbana, IL
 * auto calculator = std::make_shared<VisibilityCalculator>(location);
 * calculator->setTimezone("America/Chicago");
 *
 * // Check current observability
 * double ra = 270.0;  // M31 Andromeda
 * double dec = 41.3;
 * if (calculator->isCurrentlyObservable(ra, dec)) {
 *     auto altAz = calculator->calculateAltAz(ra, dec);
 *     std::cout << "Altitude: " << altAz.altitude << "°\n";
 * }
 *
 * // Calculate tonight's window
 * auto window = calculator->calculateWindow(ra, dec, std::chrono::system_clock::now());
 * std::cout << "Transit altitude: " << window.maxAltitude << "°\n";
 *
 * // Filter objects for tonight
 * auto filter = TimeWindowFilter(calculator);
 * filter.setPreset(TimeWindowFilter::Preset::Tonight);
 * auto observableObjects = filter.filter(allObjects);
 *
 * // Generate observing plan
 * auto plan = filter.generateObservingPlan(observableObjects);
 * @endcode
 *
 * Key Features:
 * - High-precision astronomical calculations (integrates with existing tools)
 * - Rise/set/transit time calculations for any object
 * - Current altitude/azimuth determination
 * - Moon position and distance calculations
 * - Twilight time calculations (civil, nautical, astronomical)
 * - Batch filtering with configurable constraints
 * - Observation sequence optimization
 * - Timezone support
 * - Complete JSON serialization support
 *
 * Coordinate Systems:
 * - Equatorial (RA/Dec): Celestial object positions
 * - Horizontal (Alt/Az): Local observer coordinates
 * - Geographic: Observer location (lat/lon/elevation)
 *
 * Time Handling:
 * - All times use std::chrono::system_clock (UTC)
 * - Timezone conversions supported
 * - Julian Date calculations available
 * - Sidereal time calculations included
 *
 * Constraints:
 * - Altitude limits (minimum and maximum)
 * - Horizon offset for obstructions
 * - Moon distance requirements
 * - Custom observation windows
 */

#include "time_window_filter.hpp"
#include "visibility_calculator.hpp"

namespace lithium::target::observability {

/**
 * @brief Module version information.
 */
inline constexpr const char* OBSERVABILITY_MODULE_VERSION = "1.0.0";

/**
 * @brief Create a visibility calculator with default constraints.
 *
 * Convenience factory function for quick setup.
 *
 * @param latitude Observer latitude in degrees
 * @param longitude Observer longitude in degrees
 * @param elevation Observer elevation in meters
 * @param timezone IANA timezone string
 * @return Configured visibility calculator
 */
inline std::shared_ptr<VisibilityCalculator> createVisibilityCalculator(
    double latitude, double longitude, double elevation,
    const std::string& timezone = "UTC") {
    auto location = lithium::tools::astronomy::ObserverLocation(latitude, longitude, elevation);
    auto calculator = std::make_shared<VisibilityCalculator>(location);
    calculator->setTimezone(timezone);
    return calculator;
}

/**
 * @brief Create a time window filter with default settings.
 *
 * Convenience factory function.
 *
 * @param calculator Visibility calculator to use
 * @return Configured time window filter
 */
inline std::unique_ptr<TimeWindowFilter> createTimeWindowFilter(
    std::shared_ptr<VisibilityCalculator> calculator) {
    return std::make_unique<TimeWindowFilter>(calculator);
}

}  // namespace lithium::target::observability

#endif  // LITHIUM_TARGET_OBSERVABILITY_MODULE_HPP
