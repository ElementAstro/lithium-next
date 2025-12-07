// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file model.hpp
 * @brief Aggregated header for target model module.
 *
 * This is the primary include file for the target model components.
 * Include this file to get access to all celestial object models,
 * search filters, and database models.
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_MODEL_HPP
#define LITHIUM_TARGET_MODEL_HPP

// ============================================================================
// Model Components
// ============================================================================

#include "celestial_model.hpp"
#include "celestial_object.hpp"
#include "database_models.hpp"
#include "search_filter.hpp"
#include "search_result.hpp"
#include "star_object.hpp"

namespace lithium::target::model {

/**
 * @brief Model module version.
 */
inline constexpr const char* MODEL_MODULE_VERSION = "1.0.0";

/**
 * @brief Get model module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getModelModuleVersion() noexcept {
    return MODEL_MODULE_VERSION;
}
// Re-export model classes and structures for convenience

// Celestial object classes
using CelestialObjectClass = CelestialObject;
using StarObjectClass = StarObject;
using CoordinatesType = CelestialCoordinates;

// Search and filter types
using SearchFilterType = CelestialSearchFilter;
using SearchResultType = ScoredSearchResult;
using MatchTypeEnum = MatchType;

// Database model types
using ObjectModelType = CelestialObjectModel;
using RatingModelType = UserRatingModel;
using HistoryModelType = SearchHistoryModel;
using StatsType = CelestialObjectStatistics;

}  // namespace lithium::target::model

#endif  // LITHIUM_TARGET_MODEL_HPP
