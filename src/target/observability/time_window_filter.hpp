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

#ifndef LITHIUM_TARGET_OBSERVABILITY_TIME_WINDOW_FILTER_HPP
#define LITHIUM_TARGET_OBSERVABILITY_TIME_WINDOW_FILTER_HPP

#include <chrono>
#include <memory>
#include <span>
#include <vector>

#include "target/celestial_model.hpp"
#include "tools/astronomy/constraints.hpp"
#include "visibility_calculator.hpp"

namespace lithium::target::observability {

using AltitudeConstraints = lithium::tools::astronomy::AltitudeConstraints;
using CelestialObjectModel = lithium::target::CelestialObjectModel;

/**
 * @class TimeWindowFilter
 * @brief Filters and optimizes celestial objects for observation within time windows.
 *
 * Provides convenient methods to filter observable objects within predefined
 * time windows (tonight, this week, this month) or custom time ranges.
 * Also includes optimization for efficient observation sequencing.
 *
 * Features:
 * - Preset time window selection (tonight, week, month)
 * - Custom time range support
 * - Observability filtering with constraints
 * - Observation sequence optimization
 * - Thread-safe operations
 */
class TimeWindowFilter {
public:
    /**
     * @enum Preset
     * @brief Predefined time window presets
     */
    enum class Preset {
        Tonight,      ///< From sunset to sunrise
        ThisWeek,     ///< Next 7 days
        ThisMonth,    ///< Next 30 days
        Custom        ///< Custom time range
    };

    /**
     * @brief Constructor with visibility calculator.
     * @param calculator Shared pointer to visibility calculator
     * @throws std::invalid_argument if calculator is null
     */
    explicit TimeWindowFilter(
        std::shared_ptr<VisibilityCalculator> calculator);

    /**
     * @brief Destructor.
     */
    ~TimeWindowFilter();

    // ========================================================================
    // Window Configuration
    // ========================================================================

    /**
     * @brief Set time window using a preset.
     *
     * Automatically calculates the time range based on the selected preset.
     * Default constraints apply (minimum altitude 20 degrees, etc.).
     *
     * @param preset Preset window type
     * @param date Reference date for calculation (defaults to today)
     */
    void setPreset(Preset preset,
                   std::chrono::system_clock::time_point date =
                       std::chrono::system_clock::now());

    /**
     * @brief Set a custom time window.
     *
     * @param start Start of observation window
     * @param end End of observation window
     * @throws std::invalid_argument if start >= end
     */
    void setCustomWindow(std::chrono::system_clock::time_point start,
                         std::chrono::system_clock::time_point end);

    /**
     * @brief Get current time window.
     * @return Tuple of (start_time, end_time)
     */
    [[nodiscard]] std::pair<std::chrono::system_clock::time_point,
                            std::chrono::system_clock::time_point>
    getTimeWindow() const;

    /**
     * @brief Get current preset type.
     * @return Current preset, or Custom if using custom window
     */
    [[nodiscard]] Preset getCurrentPreset() const;

    // ========================================================================
    // Constraint Management
    // ========================================================================

    /**
     * @brief Set observability constraints.
     *
     * @param constraints Altitude and other observability constraints
     */
    void setConstraints(const AltitudeConstraints& constraints);

    /**
     * @brief Get current constraints.
     * @return Current observability constraints
     */
    [[nodiscard]] const AltitudeConstraints& getConstraints() const;

    /**
     * @brief Reset constraints to defaults.
     *
     * Default constraints: min altitude 20°, max 85°, no moon distance requirement
     */
    void resetConstraints();

    // ========================================================================
    // Filtering Operations
    // ========================================================================

    /**
     * @brief Filter observable objects within current time window.
     *
     * Returns only objects that are observable within the configured
     * time window and meet all constraints.
     *
     * @param objects Span of celestial objects to filter
     * @return Vector of observable objects
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filter(
        std::span<const CelestialObjectModel> objects);

    /**
     * @brief Filter objects observable at specific times.
     *
     * Returns objects that are observable at any point during the
     * given time range.
     *
     * @param objects Objects to filter
     * @param start Start of time range
     * @param end End of time range
     * @return Observable objects
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filterInRange(
        std::span<const CelestialObjectModel> objects,
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end);

    /**
     * @brief Filter objects observable at a specific time.
     *
     * @param objects Objects to filter
     * @param time Specific observation time
     * @return Observable objects at that time
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filterAtTime(
        std::span<const CelestialObjectModel> objects,
        std::chrono::system_clock::time_point time);

    /**
     * @brief Filter objects observable within a minimum duration.
     *
     * Returns objects that remain observable for at least the
     * specified duration within the time window.
     *
     * @param objects Objects to filter
     * @param minDuration Minimum observable duration
     * @return Objects meeting duration requirement
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filterByMinDuration(
        std::span<const CelestialObjectModel> objects,
        std::chrono::minutes minDuration);

    /**
     * @brief Filter objects by minimum altitude at meridian transit.
     *
     * Useful for ensuring objects are observed at good elevation angles.
     *
     * @param objects Objects to filter
     * @param minAltitude Minimum altitude at transit (degrees)
     * @return Filtered objects
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filterByTransitAltitude(
        std::span<const CelestialObjectModel> objects, double minAltitude);

    /**
     * @brief Filter objects by minimum distance from Moon.
     *
     * Useful for avoiding Moon interference in observations.
     *
     * @param objects Objects to filter
     * @param minDistance Minimum distance from Moon (degrees)
     * @return Filtered objects
     */
    [[nodiscard]] std::vector<CelestialObjectModel> filterByMoonDistance(
        std::span<const CelestialObjectModel> objects, double minDistance);

    // ========================================================================
    // Sequence Optimization
    // ========================================================================

    /**
     * @brief Optimize observation sequence for efficiency.
     *
     * Reorders objects to minimize telescope movement and maximize
     * observing efficiency. Uses greedy nearest-neighbor algorithm.
     *
     * @param objects Objects to observe
     * @param startTime Start of observation session
     * @return Optimized sequence with suggested observation times
     */
    [[nodiscard]] std::vector<
        std::pair<CelestialObjectModel, std::chrono::system_clock::time_point>>
    optimizeSequence(std::span<const CelestialObjectModel> objects,
                     std::chrono::system_clock::time_point startTime);

    /**
     * @brief Get optimal start time for observation.
     *
     * Returns the best time to start observations within the window,
     * considering twilight conditions and object visibility.
     *
     * @return Recommended start time
     */
    [[nodiscard]] std::chrono::system_clock::time_point getOptimalStartTime() const;

    /**
     * @brief Get night length in seconds.
     *
     * For tonight preset, returns duration from end of twilight to start.
     * For other presets, returns window duration.
     *
     * @return Night duration in seconds
     */
    [[nodiscard]] int64_t getNightDurationSeconds() const;

    /**
     * @brief Get total observable duration for an object in current window.
     *
     * @param ra Right Ascension in degrees
     * @param dec Declination in degrees
     * @return Observable duration in seconds within current window
     */
    [[nodiscard]] int64_t getObjectDurationSeconds(double ra, double dec) const;

    // ========================================================================
    // Statistics and Reporting
    // ========================================================================

    /**
     * @brief Get number of observable objects.
     *
     * @param objects Objects to analyze
     * @return Count of observable objects
     */
    [[nodiscard]] size_t countObservable(
        std::span<const CelestialObjectModel> objects) const;

    /**
     * @brief Get statistics for observable objects.
     *
     * Returns JSON object with counts by various categories.
     *
     * @param objects Objects to analyze
     * @return JSON statistics object
     */
    [[nodiscard]] nlohmann::json getStatistics(
        std::span<const CelestialObjectModel> objects) const;

    /**
     * @brief Generate observing plan.
     *
     * Creates a detailed plan of what objects to observe and when.
     *
     * @param objects Available objects
     * @return JSON plan with object sequence and times
     */
    [[nodiscard]] nlohmann::json generateObservingPlan(
        std::span<const CelestialObjectModel> objects);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    // Helper methods
    [[nodiscard]] std::chrono::system_clock::time_point getTonight() const;
    [[nodiscard]] std::chrono::system_clock::time_point getThisWeekEnd() const;
    [[nodiscard]] std::chrono::system_clock::time_point getThisMonthEnd() const;
};

}  // namespace lithium::target::observability

#endif  // LITHIUM_TARGET_OBSERVABILITY_TIME_WINDOW_FILTER_HPP
