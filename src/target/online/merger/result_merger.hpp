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

#ifndef LITHIUM_TARGET_ONLINE_MERGER_RESULT_MERGER_HPP
#define LITHIUM_TARGET_ONLINE_MERGER_RESULT_MERGER_HPP

#include <memory>
#include <string>
#include <vector>

#include "../provider/provider_interface.hpp"
#include "../../model/search_result.hpp"

namespace lithium::target::online {

/**
 * @brief Merge strategy enum for combining local and online results
 */
enum class MergeStrategy {
    PreferLocal = 0,      ///< Keep local object, merge missing fields from online
    PreferOnline = 1,     ///< Keep online object, merge missing fields from local
    MostRecent = 2,       ///< Use object with later timestamp
    MostComplete = 3,     ///< Use object with more non-empty fields
    Union = 4             ///< Keep all unique objects
};

/**
 * @brief Configuration for result merging
 */
struct MergeConfig {
    MergeStrategy strategy = MergeStrategy::PreferLocal;

    // Deduplication options
    bool removeDuplicates = true;
    double coordinateMatchRadius = 0.001;  // degrees (~3.6 arcsec)
    bool matchByName = true;
    bool matchByCoordinates = true;

    // Scoring weights
    double localScoreBonus = 0.1;    // Bonus for local results
    double onlineScoreBonus = 0.05;  // Bonus for online results

    // Result limits
    int maxResults = 100;
    double minScore = 0.0;
};

/**
 * @brief Statistics about merge operation
 */
struct MergeStats {
    size_t localCount = 0;
    size_t onlineCount = 0;
    size_t mergedCount = 0;
    size_t duplicatesRemoved = 0;
    size_t conflictsResolved = 0;
};

/**
 * @brief Result merger for combining local and online search results
 *
 * Provides intelligent merging of results from multiple sources with:
 * - Duplicate detection by name and coordinates
 * - Configurable merge strategies
 * - Score-based ranking
 * - Source tracking
 */
class ResultMerger {
public:
    explicit ResultMerger(const MergeConfig& config = {});
    ~ResultMerger();

    // Non-copyable, movable
    ResultMerger(const ResultMerger&) = delete;
    ResultMerger& operator=(const ResultMerger&) = delete;
    ResultMerger(ResultMerger&&) noexcept;
    ResultMerger& operator=(ResultMerger&&) noexcept;

    /**
     * @brief Merge local and online results
     */
    [[nodiscard]] auto merge(
        const std::vector<CelestialObjectModel>& localResults,
        const std::vector<CelestialObjectModel>& onlineResults)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Merge with scored results
     */
    [[nodiscard]] auto mergeScored(
        const std::vector<model::ScoredSearchResult>& localResults,
        const std::vector<CelestialObjectModel>& onlineResults,
        double baseOnlineScore = 0.5)
        -> std::vector<model::ScoredSearchResult>;

    /**
     * @brief Merge results from multiple providers
     */
    [[nodiscard]] auto mergeMultiple(
        const std::vector<OnlineQueryResult>& results)
        -> std::vector<CelestialObjectModel>;

    /**
     * @brief Get statistics from last merge
     */
    [[nodiscard]] auto getLastMergeStats() const -> MergeStats;

    /**
     * @brief Set merge configuration
     */
    void setConfig(const MergeConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] auto getConfig() const -> const MergeConfig&;

    /**
     * @brief Check if two objects are duplicates
     */
    [[nodiscard]] auto isDuplicate(const CelestialObjectModel& a,
                                   const CelestialObjectModel& b) const -> bool;

    /**
     * @brief Merge two objects into one
     */
    [[nodiscard]] auto mergeObjects(const CelestialObjectModel& primary,
                                    const CelestialObjectModel& secondary) const
        -> CelestialObjectModel;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::target::online

#endif  // LITHIUM_TARGET_ONLINE_MERGER_RESULT_MERGER_HPP
