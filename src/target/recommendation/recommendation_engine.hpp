#ifndef LITHIUM_TARGET_RECOMMENDATION_RECOMMENDATION_ENGINE_HPP
#define LITHIUM_TARGET_RECOMMENDATION_RECOMMENDATION_ENGINE_HPP

#include <memory>
#include <string>
#include <vector>

namespace lithium::target::recommendation {

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
 * @brief Interface for recommendation engines
 *
 * Defines the contract for all recommendation engine implementations,
 * supporting both explicit ratings and implicit feedback with various
 * recommendation strategies (matrix factorization, collaborative filtering,
 * content-based, hybrid).
 */
class IRecommendationEngine {
public:
    virtual ~IRecommendationEngine() = default;

    // Rating Management

    /**
     * @brief Add a single explicit rating
     * @param userId User identifier
     * @param itemId Item identifier
     * @param rating Rating value (typically 0-5)
     * @throws DataException if rating is out of valid range
     */
    virtual void addRating(const std::string& userId,
                          const std::string& itemId, double rating) = 0;

    /**
     * @brief Add implicit feedback (e.g., view, click, purchase)
     * @param userId User identifier
     * @param itemId Item identifier
     */
    virtual void addImplicitFeedback(const std::string& userId,
                                      const std::string& itemId) = 0;

    /**
     * @brief Add multiple ratings at once
     * @param ratings Vector of (userId, itemId, rating) tuples
     */
    virtual void addRatings(
        const std::vector<std::tuple<std::string, std::string, double>>&
            ratings) = 0;

    // Item Features

    /**
     * @brief Add item with features
     * @param itemId Item identifier
     * @param features Vector of feature identifiers
     */
    virtual void addItem(const std::string& itemId,
                        const std::vector<std::string>& features) = 0;

    /**
     * @brief Add feature to an item with value
     * @param itemId Item identifier
     * @param featureId Feature identifier
     * @param value Feature value (typically 0-1)
     * @throws DataException if value is out of valid range
     */
    virtual void addItemFeature(const std::string& itemId,
                               const std::string& featureId, double value) = 0;

    /**
     * @brief Add multiple items with features at once
     * @param items Vector of (itemId, features) pairs
     */
    virtual void addItems(const std::vector<std::pair<std::string,
                          std::vector<std::string>>>& items) = 0;

    // Training and Prediction

    /**
     * @brief Train the recommendation model
     */
    virtual void train() = 0;

    /**
     * @brief Get recommended items for a user
     * @param userId User identifier
     * @param topN Number of recommendations to return (default: 10)
     * @return Vector of (itemId, score) pairs sorted by score descending
     */
    [[nodiscard]] virtual auto recommend(const std::string& userId, int topN = 10)
        -> std::vector<std::pair<std::string, double>> = 0;

    /**
     * @brief Predict rating for a user-item pair
     * @param userId User identifier
     * @param itemId Item identifier
     * @return Predicted rating value
     */
    [[nodiscard]] virtual auto predictRating(const std::string& userId,
                                            const std::string& itemId)
        -> double = 0;

    // Model Persistence

    /**
     * @brief Save model to file
     * @param path File path for saving
     * @throws ModelException if file cannot be opened or written
     */
    virtual void saveModel(const std::string& path) = 0;

    /**
     * @brief Load model from file
     * @param path File path for loading
     * @throws ModelException if file cannot be opened or is invalid
     */
    virtual void loadModel(const std::string& path) = 0;

    // Statistics and Management

    /**
     * @brief Get statistics about the model
     * @return String containing model statistics (users, items, ratings, etc.)
     */
    [[nodiscard]] virtual auto getStats() const -> std::string = 0;

    /**
     * @brief Clear all data from the model
     */
    virtual void clear() = 0;

    /**
     * @brief Optimize model performance
     */
    virtual void optimize() = 0;
};

/**
 * @brief Factory function to create recommendation engine instances
 * @param type Engine type ("matrix_factorization", "collaborative", "content",
 *             "hybrid")
 * @return Unique pointer to created engine
 * @throws std::invalid_argument if type is not recognized
 */
[[nodiscard]] auto createRecommendationEngine(
    const std::string& type = "matrix_factorization")
    -> std::unique_ptr<IRecommendationEngine>;

}  // namespace lithium::target::recommendation

#endif  // LITHIUM_TARGET_RECOMMENDATION_RECOMMENDATION_ENGINE_HPP
