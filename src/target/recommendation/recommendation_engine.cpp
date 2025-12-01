#include "recommendation_engine.hpp"

#include "collaborative_filter.hpp"
#include "content_filter.hpp"
#include "hybrid_recommender.hpp"
#include "matrix_factorization.hpp"

#include <spdlog/spdlog.h>

namespace lithium::target::recommendation {

auto createRecommendationEngine(const std::string& type)
    -> std::unique_ptr<IRecommendationEngine> {
    if (type == "matrix_factorization") {
        spdlog::info("Creating MatrixFactorizationEngine");
        return std::make_unique<MatrixFactorizationEngine>();
    } else if (type == "collaborative") {
        spdlog::info("Creating CollaborativeFilteringEngine");
        return std::make_unique<CollaborativeFilteringEngine>();
    } else if (type == "content") {
        spdlog::info("Creating ContentFilteringEngine");
        return std::make_unique<ContentFilteringEngine>();
    } else if (type == "hybrid") {
        spdlog::info("Creating HybridRecommender");
        return std::make_unique<HybridRecommender>();
    } else {
        spdlog::error("Unknown recommendation engine type: {}", type);
        throw std::invalid_argument("Unknown recommendation engine type: " +
                                   type);
    }
}

}  // namespace lithium::target::recommendation
