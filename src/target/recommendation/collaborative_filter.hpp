#ifndef LITHIUM_TARGET_RECOMMENDATION_COLLABORATIVE_FILTER_HPP
#define LITHIUM_TARGET_RECOMMENDATION_COLLABORATIVE_FILTER_HPP

#include "recommendation_engine.hpp"

#include <chrono>
#include <mutex>
#include <unordered_map>

namespace lithium::target::recommendation {

/**
 * @brief Collaborative Filtering based recommendation engine
 *
 * Implements IRecommendationEngine using collaborative filtering techniques:
 * - User-User Collaborative Filtering: Finds similar users and recommends items
 *   they liked
 * - Item-Item Collaborative Filtering: Finds similar items and recommends them
 *   based on user preferences
 *
 * Uses multiple similarity metrics:
 * - Cosine similarity
 * - Pearson correlation
 */
class CollaborativeFilteringEngine : public IRecommendationEngine {
public:
    /**
     * @brief Similarity metric type
     */
    enum class SimilarityMetric {
        COSINE,   ///< Cosine similarity
        PEARSON,  ///< Pearson correlation
    };

    /**
     * @brief Configuration for collaborative filtering
     */
    struct Config {
        SimilarityMetric metric = SimilarityMetric::COSINE;
        int minCommonRatings = 2;  ///< Minimum common ratings to consider
        double similarityThreshold = 0.1;
        int maxNeighbors = 50;  ///< Maximum neighbors to consider
    };

    /**
     * @brief Construct with default configuration
     */
    CollaborativeFilteringEngine();

    /**
     * @brief Construct with custom configuration
     * @param config Configuration parameters
     */
    explicit CollaborativeFilteringEngine(const Config& config);

    ~CollaborativeFilteringEngine() override = default;

    // Rating Management
    void addRating(const std::string& userId, const std::string& itemId,
                   double rating) override;

    void addImplicitFeedback(const std::string& userId,
                             const std::string& itemId) override;

    void addRatings(
        const std::vector<std::tuple<std::string, std::string, double>>&
            ratings) override;

    // Item Features (not used in pure collaborative filtering)
    void addItem(const std::string& itemId,
                 const std::vector<std::string>& features) override;

    void addItemFeature(const std::string& itemId, const std::string& featureId,
                        double value) override;

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

private:
    // Configuration
    Config config_;

    // Index mappings
    std::unordered_map<std::string, int> userIndex_;
    std::unordered_map<std::string, int> itemIndex_;

    // Rating data (userId -> itemId -> rating)
    std::unordered_map<int, std::unordered_map<int, double>> userRatings_;

    // Item data (itemId -> userId -> rating)
    std::unordered_map<int, std::unordered_map<int, double>> itemRatings_;

    // Similarity caches
    mutable std::unordered_map<int, std::unordered_map<int, double>>
        userSimilarityCache_;
    mutable std::unordered_map<int, std::unordered_map<int, double>>
        itemSimilarityCache_;

    mutable std::mutex mtx_;

    // Internal methods

    /**
     * @brief Get or create user ID
     * @param user User identifier
     * @return Internal user ID (caller must hold lock)
     */
    auto getUserId(const std::string& user) -> int;

    /**
     * @brief Get or create item ID
     * @param item Item identifier
     * @return Internal item ID (caller must hold lock)
     */
    auto getItemId(const std::string& item) -> int;

    /**
     * @brief Reverse lookup user name from ID
     * @param userId Internal user ID
     * @return User name or empty string if not found
     */
    [[nodiscard]] auto getUserName(int userId) const -> std::string;

    /**
     * @brief Reverse lookup item name from ID
     * @param itemId Internal item ID
     * @return Item name or empty string if not found
     */
    [[nodiscard]] auto getItemName(int itemId) const -> std::string;

    /**
     * @brief Calculate similarity between two users
     * @param user1 First user ID
     * @param user2 Second user ID
     * @return Similarity score between -1 and 1 (PEARSON) or 0 and 1 (COSINE)
     */
    [[nodiscard]] auto calculateUserSimilarity(int user1, int user2) const
        -> double;

    /**
     * @brief Calculate similarity between two items
     * @param item1 First item ID
     * @param item2 Second item ID
     * @return Similarity score between -1 and 1 (PEARSON) or 0 and 1 (COSINE)
     */
    [[nodiscard]] auto calculateItemSimilarity(int item1, int item2) const
        -> double;

    /**
     * @brief Calculate cosine similarity between two vectors
     * @param vec1 First rating vector
     * @param vec2 Second rating vector
     * @return Cosine similarity score
     */
    [[nodiscard]] static auto cosineSimilarity(
        const std::unordered_map<int, double>& vec1,
        const std::unordered_map<int, double>& vec2) -> double;

    /**
     * @brief Calculate Pearson correlation between two vectors
     * @param vec1 First rating vector
     * @param vec2 Second rating vector
     * @return Pearson correlation coefficient
     */
    [[nodiscard]] static auto pearsonCorrelation(
        const std::unordered_map<int, double>& vec1,
        const std::unordered_map<int, double>& vec2) -> double;

    /**
     * @brief Get k most similar users to a given user
     * @param userId User ID
     * @param k Number of neighbors
     * @return Vector of (similarUserId, similarity) pairs
     */
    [[nodiscard]] auto getNearestUserNeighbors(int userId, int k) const
        -> std::vector<std::pair<int, double>>;

    /**
     * @brief Get k most similar items to a given item
     * @param itemId Item ID
     * @param k Number of neighbors
     * @return Vector of (similarItemId, similarity) pairs
     */
    [[nodiscard]] auto getNearestItemNeighbors(int itemId, int k) const
        -> std::vector<std::pair<int, double>>;

    /**
     * @brief Clear expired similarity caches
     */
    void clearSimilarityCache() const;
};

}  // namespace lithium::target::recommendation

#endif  // LITHIUM_TARGET_RECOMMENDATION_COLLABORATIVE_FILTER_HPP
