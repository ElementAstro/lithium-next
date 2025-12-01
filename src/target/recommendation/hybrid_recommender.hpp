#ifndef LITHIUM_TARGET_RECOMMENDATION_HYBRID_RECOMMENDER_HPP
#define LITHIUM_TARGET_RECOMMENDATION_HYBRID_RECOMMENDER_HPP

#include "recommendation_engine.hpp"

#include <memory>
#include <mutex>

namespace lithium::target::recommendation {

/**
 * @brief Hybrid Recommendation Engine
 *
 * Combines multiple recommendation algorithms (collaborative filtering,
 * content-based filtering, matrix factorization) using weighted aggregation
 * to provide superior recommendations by leveraging the strengths of each
 * approach.
 *
 * Features:
 * - Configurable weights for different algorithms
 * - Dynamic algorithm composition
 * - Consensus-based recommendations
 */
class HybridRecommender : public IRecommendationEngine {
public:
    /**
     * @brief Configuration for hybrid recommender
     */
    struct Config {
        double collaborativeWeight = 0.5;  ///< Weight for collaborative
                                           ///< filtering
        double contentWeight = 0.3;        ///< Weight for content-based
        double matrixFactorizationWeight = 0.2;  ///< Weight for matrix
                                                  ///< factorization
        bool normalizeWeights = true;             ///< Auto-normalize weights
    };

    /**
     * @brief Construct with default configuration and engines
     */
    HybridRecommender();

    /**
     * @brief Construct with explicit engines
     * @param collaborative Collaborative filtering engine (or nullptr)
     * @param content Content-based filtering engine (or nullptr)
     * @param matrixFactorization Matrix factorization engine (or nullptr)
     * @param config Configuration parameters
     */
    HybridRecommender(
        std::unique_ptr<IRecommendationEngine> collaborative,
        std::unique_ptr<IRecommendationEngine> content,
        std::unique_ptr<IRecommendationEngine> matrixFactorization,
        const Config& config);

    /**
     * @brief Construct with default engines and custom config
     * @param config Configuration parameters
     */
    explicit HybridRecommender(const Config& config);

    ~HybridRecommender() override = default;

    // Rating Management
    void addRating(const std::string& userId, const std::string& itemId,
                  double rating) override;

    void addImplicitFeedback(const std::string& userId,
                            const std::string& itemId) override;

    void addRatings(
        const std::vector<std::tuple<std::string, std::string, double>>&
            ratings) override;

    // Item Features
    void addItem(const std::string& itemId,
                const std::vector<std::string>& features) override;

    void addItemFeature(const std::string& itemId,
                       const std::string& featureId, double value) override;

    void addItems(
        const std::vector<std::pair<std::string, std::vector<std::string>>>&
            items) override;

    // Training and Prediction
    void train() override;

    [[nodiscard]] auto recommend(const std::string& userId, int topN = 10)
        -> std::vector<std::pair<std::string, double>> override;

    [[nodiscard]] auto predictRating(const std::string& userId,
                                    const std::string& itemId)
        -> double override;

    // Model Persistence
    void saveModel(const std::string& path) override;

    void loadModel(const std::string& path) override;

    // Statistics and Management
    [[nodiscard]] auto getStats() const -> std::string override;

    void clear() override;

    void optimize() override;

    /**
     * @brief Set weights for different algorithms
     * @param collaborativeWeight Weight for collaborative filtering
     * @param contentWeight Weight for content-based filtering
     * @param matrixFactorizationWeight Weight for matrix factorization
     */
    void setWeights(double collaborativeWeight, double contentWeight,
                   double matrixFactorizationWeight);

    /**
     * @brief Get current configuration
     * @return Reference to configuration struct
     */
    [[nodiscard]] auto getConfig() const -> const Config& {
        return config_;
    }

private:
    // Configuration
    Config config_;

    // Recommendation engines
    std::unique_ptr<IRecommendationEngine> collaborative_;
    std::unique_ptr<IRecommendationEngine> content_;
    std::unique_ptr<IRecommendationEngine> matrixFactorization_;

    mutable std::mutex mtx_;

    /**
     * @brief Normalize weights to sum to 1
     */
    void normalizeWeights();

    /**
     * @brief Aggregate recommendations from multiple engines
     * @param userId User identifier
     * @param topN Number of recommendations
     * @return Aggregated recommendations
     */
    [[nodiscard]] auto aggregateRecommendations(const std::string& userId,
                                               int topN) const
        -> std::vector<std::pair<std::string, double>>;

    /**
     * @brief Get available engines count
     * @return Number of non-null engines
     */
    [[nodiscard]] auto getAvailableEngineCount() const -> int;
};

}  // namespace lithium::target::recommendation

#endif  // LITHIUM_TARGET_RECOMMENDATION_HYBRID_RECOMMENDER_HPP
