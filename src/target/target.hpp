// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file target.hpp
 * @brief Main aggregated header for Lithium Target library.
 *
 * This is the primary include file for the target library.
 * Include this file to get access to all target management functionality.
 *
 * @par Module Architecture:
 * - model/         : Core data models for celestial targets
 * - index/         : Indexing and caching mechanisms
 * - repository/    : Data access and persistence layer
 * - search/        : Search engine and query mechanisms
 * - recommendation/: Preference management and recommendations
 * - observability/ : Metrics, monitoring, and logging
 * - io/            : Input/Output operations and file handling
 * - service/       : High-level service layer
 * - online/        : Online celestial database queries
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#pragma once

// ============================================================================
// Model Module
// ============================================================================
// Core data models for celestial targets
//
// Components:
// - CelestialObject: Base celestial object representation
// - CelestialObjectModel: Database ORM model
// - SearchFilter: Search filter definitions
// - SearchResult: Search result structures

#include "model/model.hpp"

// ============================================================================
// Index Module
// ============================================================================
// Indexing and caching mechanisms
//
// Components:
// - TrieIndex: Prefix tree for autocomplete
// - SpatialIndex: R-tree for coordinate queries
// - FuzzyMatcher: BK-tree for fuzzy matching

#include "index/index.hpp"

// ============================================================================
// Repository Module
// ============================================================================
// Data access and persistence layer
//
// Components:
// - ICelestialRepository: Repository interface
// - SqliteRepository: SQLite storage
// - MemoryRepository: In-memory storage
// - CachedRepository: LRU caching decorator

#include "repository/repository.hpp"

// ============================================================================
// Search Module
// ============================================================================
// Search engine and query mechanisms
//
// Components:
// - SearchEngine: Main search engine
// - CoordinateSearcher: Coordinate-based search
// - FilterEvaluator: Filter evaluation
// - Engine: Database-integrated search engine

#include "search/search.hpp"

// ============================================================================
// Recommendation Module
// ============================================================================
// Preference management and recommendations
//
// Components:
// - RecommendationEngine: Main recommendation engine
// - CollaborativeFilter: Collaborative filtering
// - ContentFilter: Content-based filtering
// - HybridRecommender: Hybrid recommendation
// - Preference: User preference management

#include "recommendation/recommendation.hpp"

// ============================================================================
// Observability Module
// ============================================================================
// Metrics, monitoring, and visibility calculations
//
// Components:
// - VisibilityCalculator: Target visibility calculations
// - TimeWindowFilter: Time-based filtering

#include "observability/observability.hpp"

// ============================================================================
// IO Module
// ============================================================================
// Input/Output operations and file handling
//
// Components:
// - CsvHandler: CSV import/export
// - JsonHandler: JSON import/export
// - DictReader/DictWriter: CSV dictionary operations

#include "io/io.hpp"

// ============================================================================
// Service Module
// ============================================================================
// High-level service layer
//
// Components:
// - CelestialService: Main service facade

#include "service/service.hpp"

namespace lithium::target {

/**
 * @brief Lithium Target library version.
 */
inline constexpr const char* TARGET_VERSION = "1.0.0";

/**
 * @brief Get target library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getTargetVersion() noexcept {
    return TARGET_VERSION;
}

// ============================================================================
// Convenience Namespace Aggregation
// ============================================================================

// Re-export all submodule namespaces for convenient access
using namespace lithium::target::model;
using namespace lithium::target::index;
using namespace lithium::target::repository;
using namespace lithium::target::search;
using namespace lithium::target::recommendation;
using namespace lithium::target::observability;
using namespace lithium::target::io;
using namespace lithium::target::service;

}  // namespace lithium::target
