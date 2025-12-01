// ===========================================================================
// Lithium-Target Index Module Header
// ===========================================================================
// This project is licensed under the terms of the GPL3 license.
//
// Module: Target Index
// Description: Public interface for target index subsystem
//
// Components:
//   - TrieIndex: Prefix tree for efficient autocomplete queries
//   - SpatialIndex: R-tree for celestial coordinate spatial queries
//   - FuzzyMatcher: BK-tree for fuzzy string matching with edit distance
// ===========================================================================

#pragma once

#include "spdlog/spdlog.h"

#include "fuzzy_matcher.hpp"
#include "spatial_index.hpp"
#include "trie_index.hpp"

// Aggregate namespace for index module
namespace lithium::target::index {

/**
 * @brief Initialize all index components with logging
 *
 * Called during system startup to initialize index module.
 */
inline void initializeIndexModule() {
    spdlog::info("Initializing Target Index module");
    // Individual components initialize themselves on first use
    // via singleton pattern or explicit construction
}

// Re-export index classes for convenience
using TrieIndex = lithium::target::index::TrieIndex;
using SpatialIndex = lithium::target::index::SpatialIndex;
using FuzzyMatcher = lithium::target::index::FuzzyMatcher;

}  // namespace lithium::target::index
