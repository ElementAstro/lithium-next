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

#include "time_window_filter.hpp"

#include <algorithm>
#include <chrono>
#include <map>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::target::observability {

using json = nlohmann::json;

// ============================================================================
// Implementation Class
// ============================================================================

class TimeWindowFilter::Impl {
public:
    std::shared_ptr<VisibilityCalculator> calculator;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    TimeWindowFilter::Preset currentPreset{TimeWindowFilter::Preset::Custom};
    AltitudeConstraints constraints;

    explicit Impl(std::shared_ptr<VisibilityCalculator> calc)
        : calculator(std::move(calc)) {
        if (!calculator) {
            throw std::invalid_argument("Visibility calculator cannot be null");
        }
        // Initialize with tonight
        auto now = std::chrono::system_clock::now();
        startTime = now;
        endTime = now + std::chrono::hours(24);
        constraints = AltitudeConstraints(20.0, 85.0);
    }
};

// ============================================================================
// Public Constructors and Destructors
// ============================================================================

TimeWindowFilter::TimeWindowFilter(
    std::shared_ptr<VisibilityCalculator> calculator)
    : pImpl_(std::make_unique<Impl>(calculator)) {
    SPDLOG_INFO("TimeWindowFilter initialized");
}

TimeWindowFilter::~TimeWindowFilter() = default;

// ========================================================================
// Window Configuration
// ========================================================================

void TimeWindowFilter::setPreset(Preset preset,
                                 std::chrono::system_clock::time_point date) {
    auto now = date;
    std::chrono::system_clock::time_point end;

    switch (preset) {
        case Preset::Tonight: {
            // Get astronomical twilight times
            auto [start, twilightEnd] =
                pImpl_->calculator->getAstronomicalTwilightTimes(now);
            pImpl_->startTime = start;
            pImpl_->endTime = twilightEnd;
            break;
        }
        case Preset::ThisWeek:
            pImpl_->startTime = now;
            pImpl_->endTime = now + std::chrono::hours(24 * 7);
            break;
        case Preset::ThisMonth:
            pImpl_->startTime = now;
            pImpl_->endTime = now + std::chrono::hours(24 * 30);
            break;
        case Preset::Custom:
            // Custom window already set via setCustomWindow
            return;
    }

    pImpl_->currentPreset = preset;
    SPDLOG_INFO("Time window preset set to {}", static_cast<int>(preset));
}

void TimeWindowFilter::setCustomWindow(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {
    if (start >= end) {
        throw std::invalid_argument("Start time must be before end time");
    }
    pImpl_->startTime = start;
    pImpl_->endTime = end;
    pImpl_->currentPreset = Preset::Custom;
    SPDLOG_INFO("Custom time window set");
}

std::pair<std::chrono::system_clock::time_point,
          std::chrono::system_clock::time_point>
TimeWindowFilter::getTimeWindow() const {
    return {pImpl_->startTime, pImpl_->endTime};
}

TimeWindowFilter::Preset TimeWindowFilter::getCurrentPreset() const {
    return pImpl_->currentPreset;
}

// ========================================================================
// Constraint Management
// ========================================================================

void TimeWindowFilter::setConstraints(const AltitudeConstraints& constraints) {
    pImpl_->constraints = constraints;
    SPDLOG_DEBUG("Observability constraints updated");
}

const AltitudeConstraints& TimeWindowFilter::getConstraints() const {
    return pImpl_->constraints;
}

void TimeWindowFilter::resetConstraints() {
    pImpl_->constraints = AltitudeConstraints(20.0, 85.0);
    SPDLOG_INFO("Constraints reset to defaults");
}

// ========================================================================
// Filtering Operations
// ========================================================================

std::vector<CelestialObjectModel> TimeWindowFilter::filter(
    std::span<const CelestialObjectModel> objects) {
    return filterInRange(objects, pImpl_->startTime, pImpl_->endTime);
}

std::vector<CelestialObjectModel> TimeWindowFilter::filterInRange(
    std::span<const CelestialObjectModel> objects,
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) {
    std::vector<CelestialObjectModel> result;
    result.reserve(objects.size());

    auto filtered = pImpl_->calculator->filterObservable(objects, start, end,
                                                         pImpl_->constraints);

    for (const auto& [obj, window] : filtered) {
        result.push_back(obj);
    }

    SPDLOG_INFO("Filtered {} observable objects from {} total", result.size(),
                objects.size());
    return result;
}

std::vector<CelestialObjectModel> TimeWindowFilter::filterAtTime(
    std::span<const CelestialObjectModel> objects,
    std::chrono::system_clock::time_point time) {
    std::vector<CelestialObjectModel> result;
    result.reserve(objects.size());

    for (const auto& obj : objects) {
        try {
            if (pImpl_->calculator->isObservableAt(obj.radJ2000, obj.decDJ2000,
                                                   time, pImpl_->constraints)) {
                result.push_back(obj);
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error checking observability for {}: {}",
                        obj.identifier, e.what());
        }
    }

    return result;
}

std::vector<CelestialObjectModel> TimeWindowFilter::filterByMinDuration(
    std::span<const CelestialObjectModel> objects,
    std::chrono::minutes minDuration) {
    std::vector<CelestialObjectModel> result;
    result.reserve(objects.size());

    auto minSeconds = minDuration.count() * 60;

    for (const auto& obj : objects) {
        try {
            auto window = pImpl_->calculator->calculateWindow(
                obj.radJ2000, obj.decDJ2000, pImpl_->startTime,
                pImpl_->constraints);

            if (!window.neverRises) {
                auto duration = window.totalDurationSeconds();
                if (duration >= minSeconds) {
                    result.push_back(obj);
                }
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error calculating duration for {}: {}", obj.identifier,
                        e.what());
        }
    }

    return result;
}

std::vector<CelestialObjectModel> TimeWindowFilter::filterByTransitAltitude(
    std::span<const CelestialObjectModel> objects, double minAltitude) {
    std::vector<CelestialObjectModel> result;
    result.reserve(objects.size());

    for (const auto& obj : objects) {
        try {
            auto window = pImpl_->calculator->calculateWindow(
                obj.radJ2000, obj.decDJ2000, pImpl_->startTime,
                pImpl_->constraints);

            if (window.maxAltitude >= minAltitude) {
                result.push_back(obj);
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error checking transit altitude for {}: {}",
                        obj.identifier, e.what());
        }
    }

    return result;
}

std::vector<CelestialObjectModel> TimeWindowFilter::filterByMoonDistance(
    std::span<const CelestialObjectModel> objects, double minDistance) {
    std::vector<CelestialObjectModel> result;
    result.reserve(objects.size());

    auto midTime =
        pImpl_->startTime + (pImpl_->endTime - pImpl_->startTime) / 2;

    for (const auto& obj : objects) {
        try {
            double distance = pImpl_->calculator->calculateMoonDistance(
                obj.radJ2000, obj.decDJ2000, midTime);

            if (distance >= minDistance) {
                result.push_back(obj);
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error calculating moon distance for {}: {}",
                        obj.identifier, e.what());
        }
    }

    return result;
}

// ========================================================================
// Sequence Optimization
// ========================================================================

std::vector<
    std::pair<CelestialObjectModel, std::chrono::system_clock::time_point>>
TimeWindowFilter::optimizeSequence(
    std::span<const CelestialObjectModel> objects,
    std::chrono::system_clock::time_point startTime) {
    return pImpl_->calculator->optimizeSequence(objects, startTime);
}

std::chrono::system_clock::time_point TimeWindowFilter::getOptimalStartTime()
    const {
    // For tonight observations, start at end of astronomical twilight
    if (pImpl_->currentPreset == Preset::Tonight) {
        return pImpl_->startTime;
    }

    // For other presets, start at the configured window start
    return pImpl_->startTime;
}

int64_t TimeWindowFilter::getNightDurationSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(pImpl_->endTime -
                                                            pImpl_->startTime)
        .count();
}

int64_t TimeWindowFilter::getObjectDurationSeconds(double ra,
                                                   double dec) const {
    try {
        auto window = pImpl_->calculator->calculateWindow(
            ra, dec, pImpl_->startTime, pImpl_->constraints);

        if (window.neverRises) {
            return 0;
        }

        // Calculate overlap with time window
        auto riseTime = std::max(window.riseTime, pImpl_->startTime);
        auto setTime = std::min(window.setTime, pImpl_->endTime);

        if (riseTime >= setTime) {
            return 0;
        }

        return std::chrono::duration_cast<std::chrono::seconds>(setTime -
                                                                riseTime)
            .count();
    } catch (const std::exception& e) {
        SPDLOG_WARN("Error calculating object duration: {}", e.what());
        return 0;
    }
}

// ========================================================================
// Statistics and Reporting
// ========================================================================

size_t TimeWindowFilter::countObservable(
    std::span<const CelestialObjectModel> objects) const {
    size_t count = 0;
    for (const auto& obj : objects) {
        try {
            if (pImpl_->calculator->isObservableAt(obj.radJ2000, obj.decDJ2000,
                                                   pImpl_->startTime,
                                                   pImpl_->constraints)) {
                count++;
            }
        } catch (const std::exception& e) {
            SPDLOG_WARN("Error counting observable object {}: {}",
                        obj.identifier, e.what());
        }
    }
    return count;
}

json TimeWindowFilter::getStatistics(
    std::span<const CelestialObjectModel> objects) const {
    json stats = json::object();

    stats["total_objects"] = objects.size();
    stats["observable_now"] = countObservable(objects);
    stats["night_duration_hours"] = getNightDurationSeconds() / 3600.0;
    stats["window_type"] = static_cast<int>(pImpl_->currentPreset);
    stats["constraints"] = {{"min_altitude", pImpl_->constraints.minAltitude},
                            {"max_altitude", pImpl_->constraints.maxAltitude}};

    // Category counts
    std::map<std::string, size_t> typeCounts;
    size_t observableCount = 0;

    for (const auto& obj : objects) {
        typeCounts[obj.type]++;

        try {
            if (pImpl_->calculator->isObservableAt(obj.radJ2000, obj.decDJ2000,
                                                   pImpl_->startTime,
                                                   pImpl_->constraints)) {
                observableCount++;
            }
        } catch (const std::exception& e) {
            // Skip on error
        }
    }

    json types = json::object();
    for (const auto& [type, count] : typeCounts) {
        types[type] = count;
    }
    stats["objects_by_type"] = types;

    return stats;
}

json TimeWindowFilter::generateObservingPlan(
    std::span<const CelestialObjectModel> objects) {
    json plan = json::object();

    // Get time window info
    plan["start_time"] =
        std::chrono::system_clock::to_time_t(pImpl_->startTime);
    plan["end_time"] = std::chrono::system_clock::to_time_t(pImpl_->endTime);
    plan["night_duration_hours"] = getNightDurationSeconds() / 3600.0;

    // Filter observable objects
    auto observableObjects = filter(objects);
    plan["observable_objects"] = observableObjects.size();

    // Optimize sequence
    auto sequence = optimizeSequence(observableObjects, pImpl_->startTime);
    json sequenceJson = json::array();

    for (size_t i = 0; i < sequence.size(); ++i) {
        const auto& [obj, time] = sequence[i];
        sequenceJson.push_back(
            {{"index", i},
             {"name", obj.identifier},
             {"ra", obj.radJ2000},
             {"dec", obj.decDJ2000},
             {"magnitude", obj.visualMagnitudeV},
             {"suggested_time", std::chrono::system_clock::to_time_t(time)},
             {"type", obj.type}});
    }

    plan["observation_sequence"] = sequenceJson;

    // Add moon info
    auto midTime =
        pImpl_->startTime + (pImpl_->endTime - pImpl_->startTime) / 2;
    auto [moonRa, moonDec, moonPhase] =
        pImpl_->calculator->getMoonInfo(midTime);
    plan["moon"] = {
        {"ra", moonRa},
        {"dec", moonDec},
        {"phase", moonPhase},
        {"above_horizon", pImpl_->calculator->isMoonAboveHorizon(midTime)}};

    // Add sun info
    auto sunTimes = pImpl_->calculator->getSunTimes(pImpl_->startTime);
    plan["sun"] = {
        {"sunset", std::chrono::system_clock::to_time_t(std::get<0>(sunTimes))},
        {"twilight_end",
         std::chrono::system_clock::to_time_t(std::get<1>(sunTimes))},
        {"twilight_start",
         std::chrono::system_clock::to_time_t(std::get<2>(sunTimes))},
        {"sunrise",
         std::chrono::system_clock::to_time_t(std::get<3>(sunTimes))}};

    SPDLOG_INFO("Generated observing plan for {} objects",
                observableObjects.size());
    return plan;
}

// ========================================================================
// Private Helper Methods
// ========================================================================

std::chrono::system_clock::time_point TimeWindowFilter::getTonight() const {
    return std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point TimeWindowFilter::getThisWeekEnd() const {
    return std::chrono::system_clock::now() + std::chrono::hours(24 * 7);
}

std::chrono::system_clock::time_point TimeWindowFilter::getThisMonthEnd()
    const {
    return std::chrono::system_clock::now() + std::chrono::hours(24 * 30);
}

}  // namespace lithium::target::observability
