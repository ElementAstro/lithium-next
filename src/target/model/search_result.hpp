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

#ifndef LITHIUM_TARGET_MODEL_SEARCH_RESULT_HPP
#define LITHIUM_TARGET_MODEL_SEARCH_RESULT_HPP

#include <compare>
#include <memory>
#include <optional>
#include <string>

#include "atom/type/expected.hpp"
#include "atom/type/json_fwd.hpp"

#include "celestial_object.hpp"

namespace lithium::target::model {

/**
 * @brief Type of match for a search result
 *
 * Indicates how the result matched the search query.
 */
enum class MatchType {
    /// Exact match on primary identifier
    Exact = 0,

    /// Fuzzy match using edit distance
    Fuzzy = 1,

    /// Match on an alias or alternative name
    Alias = 2,

    /// Match based on celestial coordinates
    Coordinate = 3,

    /// Match based on filter criteria
    Filter = 4
};

/**
 * @brief Converts MatchType to string
 *
 * @param matchType Type to convert
 * @return String representation
 */
[[nodiscard]] inline auto matchTypeToString(MatchType matchType) -> std::string {
    switch (matchType) {
        case MatchType::Exact:
            return "Exact";
        case MatchType::Fuzzy:
            return "Fuzzy";
        case MatchType::Alias:
            return "Alias";
        case MatchType::Coordinate:
            return "Coordinate";
        case MatchType::Filter:
            return "Filter";
        default:
            return "Unknown";
    }
}

/**
 * @brief Scored search result with relevance and quality metrics
 *
 * Represents a single search result with scoring information to support
 * ranking and sorting of multiple results.
 */
struct ScoredSearchResult {
    /// The matched celestial object
    std::shared_ptr<CelestialObject> object;

    /// Type of match that produced this result
    MatchType matchType = MatchType::Filter;

    /// Relevance score (0.0 to 1.0, higher is better)
    double relevanceScore = 0.0;

    /// Edit distance for fuzzy matches (0 = exact match)
    int editDistance = 0;

    /// Distance from search coordinates (in degrees, if applicable)
    double coordinateDistance = 0.0;

    /// Whether the object has complete data
    bool isComplete = true;

    /// Custom metadata string (e.g., for search context)
    std::string metadata;

    /**
     * @brief Default constructor
     */
    ScoredSearchResult() = default;

    /**
     * @brief Construct with celestial object
     *
     * @param obj Celestial object
     * @param type Match type
     */
    ScoredSearchResult(std::shared_ptr<CelestialObject> obj, MatchType type)
        : object(std::move(obj)), matchType(type) {}

    /**
     * @brief Check if result is valid
     *
     * @return true if object pointer is not null and scores are valid
     */
    [[nodiscard]] auto isValid() const -> bool {
        return object != nullptr && relevanceScore >= 0.0 &&
               relevanceScore <= 1.0 && editDistance >= 0 &&
               coordinateDistance >= 0.0;
    }

    /**
     * @brief Three-way comparison for sorting results
     *
     * Sorts by relevance score (descending), then by match type (exact first),
     * then by edit distance (ascending).
     *
     * @param other Other result to compare with
     * @return Comparison result
     */
    [[nodiscard]] auto operator<=>(const ScoredSearchResult& other) const {
        // First, compare by relevance score (higher is better)
        if (relevanceScore != other.relevanceScore) {
            return other.relevanceScore <=> relevanceScore;  // Reverse order
        }

        // Then by match type (exact is best)
        if (matchType != other.matchType) {
            return static_cast<int>(matchType) <=>
                   static_cast<int>(other.matchType);
        }

        // Finally by edit distance (lower is better)
        return editDistance <=> other.editDistance;
    }

    /**
     * @brief Equality comparison
     *
     * @param other Other result to compare with
     * @return true if objects are the same
     */
    [[nodiscard]] bool operator==(const ScoredSearchResult& other) const {
        if (object == nullptr && other.object == nullptr) {
            return true;
        }
        if (object == nullptr || other.object == nullptr) {
            return false;
        }
        return object->ID == other.object->ID;
    }

    /**
     * @brief Get summary of the result
     *
     * @return String with key information
     */
    [[nodiscard]] auto getSummary() const -> std::string;

    /**
     * @brief Serialize to JSON
     *
     * @return JSON representation
     */
    [[nodiscard]] auto toJson() const
        -> atom::type::Expected<nlohmann::json, std::string>;

    /**
     * @brief Deserialize from JSON
     *
     * @param j JSON object
     * @return ScoredSearchResult or error
     */
    static auto fromJson(const nlohmann::json& j)
        -> atom::type::Expected<ScoredSearchResult, std::string>;

    /**
     * @brief Calculate relevance score based on match type and edit distance
     *
     * @param type Match type
     * @param distance Edit distance (0 for exact matches)
     * @param maxDistance Maximum expected distance for normalization
     * @return Calculated relevance score (0.0 to 1.0)
     */
    static [[nodiscard]] auto calculateScore(MatchType type, int distance,
                                            int maxDistance = 10) -> double {
        double typeScore = 0.0;
        switch (type) {
            case MatchType::Exact:
                typeScore = 1.0;
                break;
            case MatchType::Alias:
                typeScore = 0.9;
                break;
            case MatchType::Coordinate:
                typeScore = 0.8;
                break;
            case MatchType::Fuzzy:
                typeScore = 0.7 - (static_cast<double>(distance) / maxDistance) *
                                     0.2;
                break;
            case MatchType::Filter:
                typeScore = 0.5;
                break;
        }
        return std::max(0.0, std::min(1.0, typeScore));
    }
};

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_SEARCH_RESULT_HPP
