// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_MODEL_HPP
#define LITHIUM_TARGET_MODEL_HPP

// Include all model-related headers
#include "celestial_object.hpp"
#include "database_models.hpp"
#include "search_filter.hpp"
#include "search_result.hpp"
#include "star_object.hpp"

// Aggregate namespace for model module
namespace lithium::target::model {
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
