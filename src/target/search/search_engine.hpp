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

#ifndef LITHIUM_TARGET_SEARCH_SEARCH_ENGINE_HPP
#define LITHIUM_TARGET_SEARCH_SEARCH_ENGINE_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

#include "celestial_repository.hpp"
#include "celestial_model.hpp"
#include "index/fuzzy_matcher.hpp"
#include "index/spatial_index.hpp"
#include "index/trie_index.hpp"

namespace lithium::target::search {

/**
 * @brief Search options configuration
 *
 * Controls behavior of search operations including fuzzy matching,
 * alias searching, and result limits.
 */
struct SearchOptions {
    /// Enable fuzzy matching with edit distance tolerance
    bool useFuzzy = true;

    /// Maximum edit distance for fuzzy matches (0-5)
    int fuzzyTolerance = 2;

    /// Search in alias names and alternative identifiers
    bool searchAliases = true;

    /// Maximum number of results to return
    int maxResults = 100;

    /// Optional filter for advanced search
    std::optional<model::CelestialSearchFilter> filter;
};

/**
 * @brief High-performance search engine for celestial objects
 *
 * Integrates multiple indexing strategies (Trie, R-tree, Fuzzy) with
 * a repository layer to provide fast, accurate celestial object searches.
 * Supports multiple search modes: exact, fuzzy, coordinate-based, and
 * advanced filtering.
 *
 * Uses PIMPL pattern for implementation hiding and thread-safe operations.
 */
class SearchEngine {
public:
    /**
     * @brief Construct SearchEngine with repository
     *
     * @param repository Shared pointer to celestial repository
     * @param trieIndex Optional pre-configured trie index
     * @param spatialIndex Optional pre-configured spatial index
     * @param fuzzyMatcher Optional pre-configured fuzzy matcher
     */
    explicit SearchEngine(
        std::shared_ptr<CelestialRepository> repository,
        std::shared_ptr<index::TrieIndex> trieIndex = nullptr,
        std::shared_ptr<index::SpatialIndex> spatialIndex = nullptr,
        std::shared_ptr<index::FuzzyMatcher> fuzzyMatcher = nullptr);

    /**
     * @brief Destructor
     */
    ~SearchEngine();

    // Non-copyable
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;

    // Movable
    SearchEngine(SearchEngine&&) noexcept;
    SearchEngine& operator=(SearchEngine&&) noexcept;

    /**
     * @brief Initialize search engine and build indexes
     *
     * Loads all celestial objects from repository and builds index
     * structures for efficient searching.
     *
     * @return Success or error message
     */
    [[nodiscard]] auto initialize()
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Check if search engine is initialized
     *
     * @return true if indexes are built and ready
     */
    [[nodiscard]] bool isInitialized() const;

    /**
     * @brief Perform general search with multiple strategies
     *
     * Combines exact match, fuzzy match, and alias search results
     * based on SearchOptions configuration.
     *
     * @param query Search query string
     * @param options Search options configuration
     * @return Vector of search results, sorted by relevance
     */
    [[nodiscard]] auto search(
        const std::string& query,
        const SearchOptions& options = {}) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Exact name search
     *
     * Searches for exact matches on primary identifier only.
     *
     * @param query Query string
     * @param limit Maximum results to return
     * @return Vector of matched results
     */
    [[nodiscard]] auto exactSearch(
        const std::string& query,
        int limit = 50) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Fuzzy search with edit distance tolerance
     *
     * Uses BK-tree fuzzy matching to find results similar to query,
     * allowing for typos and misspellings.
     *
     * @param query Query string
     * @param tolerance Maximum edit distance (0-5)
     * @param limit Maximum results to return
     * @return Vector of fuzzy matched results, sorted by distance
     */
    [[nodiscard]] auto fuzzySearch(
        const std::string& query,
        int tolerance = 2,
        int limit = 50) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Search by celestial coordinates
     *
     * Finds objects within specified radius from given RA/Dec coordinates.
     * Uses R-tree spatial index for O(log n) performance.
     *
     * @param ra Right ascension in degrees (0-360)
     * @param dec Declination in degrees (-90 to +90)
     * @param radius Search radius in degrees
     * @param limit Maximum results to return
     * @return Vector of nearby objects, sorted by distance
     */
    [[nodiscard]] auto searchByCoordinates(
        double ra,
        double dec,
        double radius,
        int limit = 50) -> std::vector<CelestialObjectModel>;

    /**
     * @brief Autocomplete suggestions from prefix
     *
     * Returns object names and aliases starting with given prefix.
     * Uses Trie index for O(k) performance where k = result count.
     *
     * @param prefix Prefix string
     * @param limit Maximum suggestions to return
     * @return Vector of completion suggestions
     */
    [[nodiscard]] auto autocomplete(
        const std::string& prefix,
        int limit = 10) -> std::vector<std::string>;

    /**
     * @brief Advanced search with complex filter criteria
     *
     * Applies comprehensive filter to find objects matching
     * multiple constraints (magnitude, size, type, etc.)
     *
     * @param filter Search filter with constraints
     * @return Vector of matching celestial objects
     */
    [[nodiscard]] auto advancedSearch(
        const CelestialSearchFilter& filter) ->
        std::vector<CelestialObjectModel>;

    /**
     * @brief Rebuild all index structures
     *
     * Clears and rebuilds Trie, Spatial, and Fuzzy indexes from
     * current repository data.
     *
     * @return Success or error message
     */
    [[nodiscard]] auto rebuildIndexes()
        -> atom::type::Expected<void, std::string>;

    /**
     * @brief Clear all indexes
     *
     * Removes all indexed data. Call initialize() to rebuild.
     */
    void clearIndexes();

    /**
     * @brief Get search engine statistics
     *
     * Returns information about indexed objects, index sizes,
     * and performance metrics.
     *
     * @return Statistics string
     */
    [[nodiscard]] auto getStats() const -> std::string;

private:
    /// Implementation details hidden via PIMPL pattern
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::search

#endif  // LITHIUM_TARGET_SEARCH_SEARCH_ENGINE_HPP
