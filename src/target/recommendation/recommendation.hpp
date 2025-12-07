// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file recommendation.hpp
 * @brief Aggregated header for target recommendation module.
 *
 * This is the primary include file for the target recommendation components.
 * Include this file to get access to all recommendation engines,
 * filters, and preference management.
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#pragma once

// ============================================================================
// Recommendation Components
// ============================================================================

#include "collaborative_filter.hpp"
#include "content_filter.hpp"
#include "hybrid_recommender.hpp"
#include "matrix_factorization.hpp"
#include "preference.hpp"
#include "recommendation_engine.hpp"

namespace lithium::target::recommendation {

/**
 * @brief Recommendation module version.
 */
inline constexpr const char* RECOMMENDATION_MODULE_VERSION = "1.0.0";

/**
 * @brief Get recommendation module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getRecommendationModuleVersion() noexcept {
    return RECOMMENDATION_MODULE_VERSION;
}
// Re-export recommendation classes and functions

// Factory function for creating recommendation engines
using recommendation_engine::createRecommendationEngine;
using recommendation_engine::DataException;
using recommendation_engine::ModelException;
using recommendation_engine::RecommendationEngineException;

// Recommendation engine types
using recommendation_engine::IRecommendationEngine;
using CollaborativeEngine = CollaborativeFilteringEngine;
using ContentEngine = ContentFilteringEngine;
using MatrixFactorEngine = MatrixFactorizationEngine;
using HybridEngine = HybridRecommender;

}  // namespace lithium::target::recommendation
