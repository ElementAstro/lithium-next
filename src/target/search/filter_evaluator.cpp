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

#include "filter_evaluator.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <spdlog/spdlog.h>

namespace lithium::target::search {

auto FilterEvaluator::matches(const CelestialObjectModel& obj,
                              const CelestialSearchFilter& filter)
    -> bool {
    // Short-circuit on failed constraints for efficiency

    if (!filter.namePattern.empty()) {
        if (!matchesNamePattern(obj, filter)) {
            return false;
        }
    }

    if (!filter.type.empty()) {
        if (obj.type != filter.type) {
            return false;
        }
    }

    if (!filter.morphology.empty()) {
        if (obj.morphology != filter.morphology) {
            return false;
        }
    }

    if (!filter.constellation.empty()) {
        if (obj.constellationEn != filter.constellation) {
            return false;
        }
    }

    if (!matchesMagnitude(obj, filter)) {
        return false;
    }

    if (!matchesSize(obj, filter)) {
        return false;
    }

    if (!matchesCoordinates(obj, filter)) {
        return false;
    }

    return true;
}

auto FilterEvaluator::filterResults(
    const std::vector<CelestialObjectModel>& results,
    const CelestialSearchFilter& filter)
    -> std::vector<CelestialObjectModel> {
    std::vector<CelestialObjectModel> filtered;

    for (const auto& result : results) {
        if (matches(result, filter)) {
            filtered.push_back(result);
        }
    }

    return filtered;
}

auto FilterEvaluator::validateFilter(
    const CelestialSearchFilter& filter) -> std::string {
    if (filter.minMagnitude > filter.maxMagnitude) {
        return "Min magnitude cannot be greater than max magnitude";
    }

    if (filter.minRA > filter.maxRA) {
        return "Min RA cannot be greater than max RA";
    }

    if (filter.minDec > filter.maxDec) {
        return "Min declination cannot be greater than max declination";
    }

    return "";
}

auto FilterEvaluator::explainMismatch(
    const CelestialObjectModel& obj,
    const CelestialSearchFilter& filter) -> std::string {
    // Check each constraint and report first failure

    if (!filter.namePattern.empty()) {
        if (!matchesNamePattern(obj, filter)) {
            return std::format("Name '{}' does not match pattern '{}'",
                             obj.identifier, filter.namePattern);
        }
    }

    if (!filter.type.empty()) {
        if (obj.type != filter.type) {
            return std::format("Type '{}' does not match '{}'", obj.type,
                             filter.type);
        }
    }

    if (!matchesMagnitude(obj, filter)) {
        return std::format(
            "Magnitude {:.2f} not in range [{:.2f}, {:.2f}]",
            obj.visualMagnitudeV, filter.minMagnitude, filter.maxMagnitude);
    }

    if (!matchesSize(obj, filter)) {
        return std::format(
            "Major axis {:.2f} outside range", obj.visualMagnitudeV);
    }

    if (!matchesCoordinates(obj, filter)) {
        return std::format(
            "Coordinates (RA={:.2f}, Dec={:.2f}) outside box "
            "[RA: {:.2f}-{:.2f}, Dec: {:.2f}-{:.2f}]",
            obj.radJ2000, obj.decDJ2000, filter.minRA, filter.maxRA,
            filter.minDec, filter.maxDec);
    }

    return "";  // Matches all constraints
}

auto FilterEvaluator::sortResults(
    std::vector<CelestialObjectModel> results,
    const CelestialSearchFilter& filter)
    -> std::vector<CelestialObjectModel> {
    std::string orderBy = filter.orderBy;
    bool ascending = filter.ascending;

    std::sort(results.begin(), results.end(),
             [&orderBy, ascending](const auto& a, const auto& b) {
                 int cmp = 0;

                 if (orderBy == "identifier") {
                     cmp = a.identifier.compare(b.identifier);
                 } else if (orderBy == "magnitude") {
                     if (a.visualMagnitudeV < b.visualMagnitudeV) {
                         cmp = -1;
                     } else if (a.visualMagnitudeV > b.visualMagnitudeV) {
                         cmp = 1;
                     }
                 } else if (orderBy == "ra") {
                     if (a.radJ2000 < b.radJ2000) {
                         cmp = -1;
                     } else if (a.radJ2000 > b.radJ2000) {
                         cmp = 1;
                     }
                 } else if (orderBy == "dec") {
                     if (a.decDJ2000 < b.decDJ2000) {
                         cmp = -1;
                     } else if (a.decDJ2000 > b.decDJ2000) {
                         cmp = 1;
                     }
                 } else {
                     // Default to identifier
                     cmp = a.identifier.compare(b.identifier);
                 }

                 return ascending ? (cmp < 0) : (cmp > 0);
             });

    return results;
}

auto FilterEvaluator::paginate(
    const std::vector<CelestialObjectModel>& results,
    int offset,
    int limit) -> std::vector<CelestialObjectModel> {
    if (offset < 0 || limit <= 0) {
        return {};
    }

    if (offset >= static_cast<int>(results.size())) {
        return {};
    }

    auto end = std::min(static_cast<int>(results.size()), offset + limit);
    return std::vector<CelestialObjectModel>(results.begin() + offset,
                                             results.begin() + end);
}

auto FilterEvaluator::getFilterStats(
    const CelestialSearchFilter& filter) -> std::string {
    int activeConstraints = 0;

    if (!filter.namePattern.empty()) {
        activeConstraints++;
    }
    if (!filter.type.empty()) {
        activeConstraints++;
    }
    if (!filter.morphology.empty()) {
        activeConstraints++;
    }
    if (!filter.constellation.empty()) {
        activeConstraints++;
    }
    if (filter.minMagnitude > -30.0 || filter.maxMagnitude < 30.0) {
        activeConstraints++;
    }
    if (filter.minRA > 0.0 || filter.maxRA < 360.0) {
        activeConstraints++;
    }
    if (filter.minDec > -90.0 || filter.maxDec < 90.0) {
        activeConstraints++;
    }

    return std::format("Filter Statistics:\n"
                      "  Active Constraints: {}\n"
                      "  Limit: {}\n"
                      "  Offset: {}\n"
                      "  Sort By: {} ({})\n",
                      activeConstraints, filter.limit, filter.offset,
                      filter.orderBy,
                      filter.ascending ? "ascending" : "descending");
}

auto FilterEvaluator::matchesNamePattern(const CelestialObjectModel& obj,
                                         const CelestialSearchFilter& filter)
    -> bool {
    const std::string& pattern = filter.namePattern;

    return likeMatch(obj.identifier, pattern) ||
           likeMatch(obj.mIdentifier, pattern) ||
           likeMatch(obj.chineseName, pattern) ||
           likeMatch(obj.extensionName, pattern);
}

auto FilterEvaluator::matchesMagnitude(const CelestialObjectModel& obj,
                                       const CelestialSearchFilter& filter)
    -> bool {
    return obj.visualMagnitudeV >= filter.minMagnitude &&
           obj.visualMagnitudeV <= filter.maxMagnitude;
}

auto FilterEvaluator::matchesSize(const CelestialObjectModel& obj,
                                  const CelestialSearchFilter& filter)
    -> bool {
    // For now, just check magnitude as size data may not be available
    return true;
}

auto FilterEvaluator::matchesCoordinates(const CelestialObjectModel& obj,
                                         const CelestialSearchFilter& filter)
    -> bool {
    return obj.radJ2000 >= filter.minRA && obj.radJ2000 <= filter.maxRA &&
           obj.decDJ2000 >= filter.minDec && obj.decDJ2000 <= filter.maxDec;
}

auto FilterEvaluator::likeMatch(const std::string& str,
                                const std::string& pattern) -> bool {
    // Simple SQL LIKE pattern matching
    // % = any characters, _ = single character

    if (pattern.empty()) {
        return true;
    }

    size_t sIdx = 0;
    size_t pIdx = 0;

    while (pIdx < pattern.length()) {
        if (pattern[pIdx] == '%') {
            // Match zero or more characters
            if (pIdx + 1 >= pattern.length()) {
                return true;  // % at end matches rest
            }

            // Find next non-% character
            pIdx++;
            while (pIdx < pattern.length() && pattern[pIdx] == '%') {
                pIdx++;
            }

            if (pIdx >= pattern.length()) {
                return true;
            }

            // Find pattern[pIdx] in remaining string
            while (sIdx < str.length()) {
                if (str[sIdx] == pattern[pIdx]) {
                    break;
                }
                sIdx++;
            }

            if (sIdx >= str.length()) {
                return false;
            }
        } else if (pattern[pIdx] == '_') {
            // Match single character
            if (sIdx >= str.length()) {
                return false;
            }
            sIdx++;
            pIdx++;
        } else {
            // Exact match
            if (sIdx >= str.length() || str[sIdx] != pattern[pIdx]) {
                return false;
            }
            sIdx++;
            pIdx++;
        }
    }

    return sIdx == str.length();
}

}  // namespace lithium::target::search
