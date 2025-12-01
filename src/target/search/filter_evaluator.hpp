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

#ifndef LITHIUM_TARGET_SEARCH_FILTER_EVALUATOR_HPP
#define LITHIUM_TARGET_SEARCH_FILTER_EVALUATOR_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

#include "celestial_model.hpp"
#include "celestial_repository.hpp"

namespace lithium::target::search {

/**
 * @brief Complex filter criteria evaluation engine
 *
 * Evaluates celestial objects against comprehensive filter criteria.
 * Supports:
 * - Name pattern matching (wildcards %, _)
 * - Type and morphology classification
 * - Magnitude constraints (visual, photographic, surface brightness)
 * - Size constraints (major/minor axis)
 * - Coordinate constraints (RA/Dec box)
 * - Observability constraints (visibility from location)
 * - Pagination and sorting
 *
 * Optimizes query evaluation by short-circuiting on failed constraints.
 */
class FilterEvaluator {
public:
    /**
     * @brief Construct filter evaluator
     */
    FilterEvaluator() = default;

    /**
     * @brief Destructor
     */
    ~FilterEvaluator() = default;

    // Non-copyable
    FilterEvaluator(const FilterEvaluator&) = delete;
    FilterEvaluator& operator=(const FilterEvaluator&) = delete;

    // Movable
    FilterEvaluator(FilterEvaluator&&) noexcept = default;
    FilterEvaluator& operator=(FilterEvaluator&&) noexcept = default;

    /**
     * @brief Evaluate if object matches filter criteria
     *
     * Applies all filter constraints to object. Returns on first
     * failed constraint for efficiency.
     *
     * @param obj Celestial object to evaluate
     * @param filter Filter criteria
     * @return true if object matches all active filter criteria
     */
    [[nodiscard]] static auto matches(
        const CelestialObjectModel& obj,
        const CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Filter collection of results
     *
     * Applies filter to vector of results, removing non-matching items.
     *
     * @param results Results to filter
     * @param filter Filter criteria
     * @return Filtered results matching all criteria
     */
    [[nodiscard]] static auto filterResults(
        const std::vector<CelestialObjectModel>& results,
        const CelestialSearchFilter& filter)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Validate filter consistency
     *
     * Checks filter for logical inconsistencies (e.g., min > max).
     *
     * @param filter Filter to validate
     * @return Error message if invalid, empty string if valid
     */
    [[nodiscard]] static auto validateFilter(
        const CelestialSearchFilter& filter) -> std::string;

    /**
     * @brief Explain why object doesn't match filter
     *
     * Useful for debugging why certain objects are filtered out.
     *
     * @param obj Object to evaluate
     * @param filter Filter criteria
     * @return String describing why object was rejected (empty if matches)
     */
    [[nodiscard]] static auto explainMismatch(
        const CelestialObjectModel& obj,
        const CelestialSearchFilter& filter) -> std::string;

    /**
     * @brief Sort results by filter's orderBy field
     *
     * Applies sorting specified in filter.orderBy field,
     * with direction from filter.ascending.
     *
     * @param results Results to sort
     * @param filter Filter with sort specification
     * @return Sorted results
     */
    [[nodiscard]] static auto sortResults(
        std::vector<CelestialObjectModel> results,
        const CelestialSearchFilter& filter)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Apply pagination to results
     *
     * Skips offset results and returns limit results.
     *
     * @param results Results to paginate
     * @param offset Number of results to skip
     * @param limit Maximum results to return
     * @return Paginated results
     */
    [[nodiscard]] static auto paginate(
        const std::vector<CelestialObjectModel>& results,
        int offset,
        int limit) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Get statistics about filter constraints
     *
     * Returns information about which constraints are active,
     * useful for query optimization.
     *
     * @param filter Filter to analyze
     * @return Statistics string
     */
    [[nodiscard]] static auto getFilterStats(
        const CelestialSearchFilter& filter) -> std::string;

private:
    /**
     * @brief Check name pattern constraint
     */
    [[nodiscard]] static auto matchesNamePattern(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check type constraint
     */
    [[nodiscard]] static auto matchesType(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check morphology constraint
     */
    [[nodiscard]] static auto matchesMorphology(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check constellation constraint
     */
    [[nodiscard]] static auto matchesConstellation(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check magnitude constraints
     */
    [[nodiscard]] static auto matchesMagnitude(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check surface brightness constraints
     */
    [[nodiscard]] static auto matchesSurfaceBrightness(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check size constraints
     */
    [[nodiscard]] static auto matchesSize(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check coordinate constraints
     */
    [[nodiscard]] static auto matchesCoordinates(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check visibility constraint
     */
    [[nodiscard]] static auto isVisible(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Check completeness constraint
     */
    [[nodiscard]] static auto meetsCompletenessRequirement(
        const model::CelestialObject& obj,
        const model::CelestialSearchFilter& filter) -> bool;

    /**
     * @brief Perform SQL LIKE pattern matching
     */
    [[nodiscard]] static auto likeMatch(
        const std::string& str,
        const std::string& pattern) -> bool;
};

}  // namespace lithium::target::search

#endif  // LITHIUM_TARGET_SEARCH_FILTER_EVALUATOR_HPP
