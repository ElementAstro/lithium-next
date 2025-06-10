#ifndef LITHIUM_TARGET_PERFERENCE_HPP
#define LITHIUM_TARGET_PERFERENCE_HPP

#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <eigen3/Eigen/Dense>

/**
 * @brief Base exception class for recommendation engine errors
 */
class RecommendationEngineException : public std::runtime_error {
public:
    explicit RecommendationEngineException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Exception for data-related errors
 */
class DataException : public RecommendationEngineException {
public:
    explicit DataException(const std::string& message)
        : RecommendationEngineException(message) {}
};

/**
 * @brief Exception for model-related errors
 */
class ModelException : public RecommendationEngineException {
public:
    explicit ModelException(const std::string& message)
        : RecommendationEngineException(message) {}
};

/**
 * @brief Advanced recommendation engine using matrix factorization with
 * temporal dynamics
 *
 * Implements a recommendation system using matrix factorization with temporal
 * dynamics and content-based features to provide personalized recommendations.
 * Supports both explicit ratings and implicit feedback.
 */
class AdvancedRecommendationEngine {
private:
    // Index mappings
    std::unordered_map<std::string, int> userIndex_;
    std::unordered_map<std::string, int> itemIndex_;
    std::unordered_map<std::string, int> featureIndex_;

    // Rating and feature data
    std::vector<
        std::tuple<int, int, double, std::chrono::system_clock::time_point>>
        ratings_;
    std::unordered_map<int, std::unordered_map<int, double>> itemFeatures_;

    // Matrix factorization parameters
    Eigen::MatrixXd userFactors_;
    Eigen::MatrixXd itemFactors_;

    // Model parameters
    static constexpr int LATENT_FACTORS = 20;
    static constexpr double LEARNING_RATE = 0.01;
    static constexpr double REGULARIZATION = 0.02;
    static constexpr int MAX_ITERATIONS = 100;
    static constexpr double TIME_DECAY_FACTOR = 0.1;
    static constexpr double HOURS_IN_A_DAY = 24.0;
    static constexpr double DAYS_IN_A_YEAR = 365.0;
    static constexpr double RANDOM_INIT_RANGE = 0.01;
    static constexpr double CONTENT_BOOST_WEIGHT = 0.2;
    static constexpr double GRAPH_BOOST_WEIGHT = 0.3;
    static constexpr double PPR_ALPHA = 0.85;
    static constexpr int PPR_ITERATIONS = 20;
    static constexpr int ALS_ITERATIONS = 10;

    // Cache system
    struct Cache {
        std::unordered_map<std::string,
                           std::vector<std::pair<std::string, double>>>
            recommendations;
        std::chrono::system_clock::time_point lastUpdate;
        static constexpr auto CACHE_DURATION = std::chrono::hours(24);
    };
    mutable Cache cache_;

    // Performance parameters
    static constexpr int BATCH_SIZE = 1000;
    static constexpr double SIMILARITY_THRESHOLD = 0.1;
    static constexpr int CACHE_MAX_ITEMS = 10000;

    mutable std::mutex mtx_;  // Mutex for thread safety

    // Internal methods
    /**
     * @brief Get or create user ID for a given user
     * @param user User identifier
     * @return Internal user ID
     */
    auto getUserId(const std::string& user) -> int;

    /**
     * @brief Get or create item ID for a given item
     * @param item Item identifier
     * @return Internal item ID
     */
    auto getItemId(const std::string& item) -> int;

    /**
     * @brief Get or create feature ID for a given feature
     * @param feature Feature identifier
     * @return Internal feature ID
     */
    auto getFeatureId(const std::string& feature) -> int;

    /**
     * @brief Calculate time decay factor for a rating
     * @param ratingTime Timestamp of the rating
     * @return Time decay factor
     */
    auto calculateTimeFactor(
        const std::chrono::system_clock::time_point& ratingTime) const
        -> double;

    /**
     * @brief Update matrix factorization model
     */
    void updateMatrixFactorization();

    /**
     * @brief Normalize ratings by subtracting mean
     */
    void normalizeRatings();

    /**
     * @brief Clear expired cache entries
     */
    void clearExpiredCache();

    /**
     * @brief Calculate similarity between two items
     * @param item1 First item ID
     * @param item2 Second item ID
     * @return Similarity score between 0 and 1
     */
    auto calculateItemSimilarity(int item1, int item2) -> double;

    /**
     * @brief Get cached recommendations for a user if available
     * @param user User identifier
     * @return Optional vector of recommended items and scores
     */
    auto getCachedRecommendations(const std::string& user)
        -> std::optional<std::vector<std::pair<std::string, double>>>;

    /**
     * @brief Process batch of ratings
     * @param batch Vector of user-item-rating tuples
     */
    void processBatch(
        const std::vector<std::tuple<std::string, std::string, double>>& batch);

    /**
     * @brief Calculate hybrid recommendation score
     * @param user User identifier
     * @param item Item identifier
     * @return Hybrid recommendation score
     */
    auto hybridScore(const std::string& user, const std::string& item)
        -> double;

public:
    AdvancedRecommendationEngine() = default;
    ~AdvancedRecommendationEngine() = default;

    /**
     * @brief Add single rating
     * @param user User identifier
     * @param item Item identifier
     * @param rating Rating value (0-5)
     * @throws DataException if rating is out of valid range
     */
    void addRating(const std::string& user, const std::string& item,
                   double rating);

    /**
     * @brief Add implicit feedback
     * @param user User identifier
     * @param item Item identifier
     */
    void addImplicitFeedback(const std::string& user, const std::string& item);

    /**
     * @brief Add item with features
     * @param item Item identifier
     * @param features Vector of feature identifiers
     */
    void addItem(const std::string& item,
                 const std::vector<std::string>& features);

    /**
     * @brief Add feature to an item with value
     * @param item Item identifier
     * @param feature Feature identifier
     * @param value Feature value (0-1)
     * @throws DataException if value is out of valid range
     */
    void addItemFeature(const std::string& item, const std::string& feature,
                        double value);

    /**
     * @brief Train the recommendation model
     */
    void train();

    /**
     * @brief Get recommended items for a user
     * @param user User identifier
     * @param topN Number of recommendations to return
     * @return Vector of recommended items with scores
     */
    auto recommendItems(const std::string& user, int topN = 5)
        -> std::vector<std::pair<std::string, double>>;

    /**
     * @brief Predict rating for user-item pair
     * @param user User identifier
     * @param item Item identifier
     * @return Predicted rating
     */
    auto predictRating(const std::string& user, const std::string& item)
        -> double;

    /**
     * @brief Save model to file
     * @param filename Path to output file
     * @throws ModelException if file cannot be opened
     */
    void saveModel(const std::string& filename);

    /**
     * @brief Load model from file
     * @param filename Path to input file
     * @throws ModelException if file cannot be opened or is invalid
     */
    void loadModel(const std::string& filename);

    /**
     * @brief Add multiple ratings at once
     * @param ratings Vector of user-item-rating tuples
     */
    void addRatings(
        const std::vector<std::tuple<std::string, std::string, double>>&
            ratings);

    /**
     * @brief Add multiple items with features at once
     * @param items Vector of item-features pairs
     */
    void addItems(
        const std::vector<std::pair<std::string, std::vector<std::string>>>&
            items);

    /**
     * @brief Simplified recommend method (alias for recommendItems)
     * @param user User identifier
     * @param topN Number of recommendations to return
     * @return Vector of recommended items with scores
     */
    auto recommend(const std::string& user, int topN = 5)
        -> std::vector<std::pair<std::string, double>>;

    /**
     * @brief Simplified predict method (alias for predictRating)
     * @param user User identifier
     * @param item Item identifier
     * @return Predicted rating
     */
    auto predict(const std::string& user, const std::string& item) -> double;

    /**
     * @brief Optimize model performance
     */
    void optimize();

    /**
     * @brief Clear all data
     */
    void clear();

    /**
     * @brief Get statistics about the model
     * @return String with model statistics
     */
    auto getStats() -> std::string;
};

#endif  // LITHIUM_TARGET_PERFERENCE_HPP
