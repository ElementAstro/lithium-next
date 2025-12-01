// ===========================================================================
// Lithium-Target Recommendation Module Header
// ===========================================================================
// This project is licensed under the terms of the GPL3 license.
//
// Module: Target Recommendation
// Description: Public interface for target recommendation subsystem
// ===========================================================================

#pragma once

// Include all recommendation-related headers
#include "collaborative_filter.hpp"
#include "content_filter.hpp"
#include "hybrid_recommender.hpp"
#include "matrix_factorization.hpp"
#include "preference.hpp"
#include "recommendation_engine.hpp"

// Aggregate namespace for recommendation module
namespace lithium::target::recommendation {
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
