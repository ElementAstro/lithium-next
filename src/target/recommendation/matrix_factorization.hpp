#ifndef LITHIUM_TARGET_RECOMMENDATION_MATRIX_FACTORIZATION_HPP
#define LITHIUM_TARGET_RECOMMENDATION_MATRIX_FACTORIZATION_HPP

#include "recommendation_engine.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <eigen3/Eigen/Dense>

namespace lithium::target::recommendation {

/**
 * @brief Matrix Factorization based recommendation engine
 *
 * Implements the IRecommendationEngine interface using matrix factorization
 * with temporal dynamics. Supports both Stochastic Gradient Descent (SGD) and
 * Alternating Least Squares (ALS) optimization methods.
 *
 * Features:
 * - User and item latent factors
 * - Time decay for temporal dynamics
 * - Content-based features integration
 * - Caching for performance optimization
 * - Thread-safe operations
 */
class MatrixFactorizationEngine : public IRecommendationEngine {
public:
    /**
     * @brief Configuration structure for matrix factorization
     */
    struct Config {
        int latentFactors = 20;           ///< Number of latent factors
        double learningRate = 0.01;       ///< Learning rate for SGD
        double regularization = 0.02;     ///< Regularization parameter
        int maxIterations = 100;          ///< Maximum training iterations
        double timeDecayFactor = 0.1;     ///< Temporal decay factor
        std::string optimizationMethod =
            "sgd";  ///< "sgd" or "als"
    };

    /**
     * @brief Construct with default configuration
     */
    MatrixFactorizationEngine();

    /**
     * @brief Construct with custom configuration
     * @param config Configuration parameters
     */
    explicit MatrixFactorizationEngine(const Config& config);

    ~MatrixFactorizationEngine() override = default;

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

private:
    // Configuration
    Config config_;

    // Index mappings
    std::unordered_map<std::string, int> userIndex_;
    std::unordered_map<std::string, int> itemIndex_;
    std::unordered_map<std::string, int> featureIndex_;

    // Rating and feature data
    struct Rating {
        int userId;
        int itemId;
        double value;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<Rating> ratings_;

    // Item features (itemId -> featureId -> value)
    std::unordered_map<int, std::unordered_map<int, double>> itemFeatures_;

    // Matrix factorization parameters
    Eigen::MatrixXd userFactors_;
    Eigen::MatrixXd itemFactors_;

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
    static constexpr double HOURS_IN_A_DAY = 24.0;
    static constexpr double DAYS_IN_A_YEAR = 365.0;
    static constexpr double RANDOM_INIT_RANGE = 0.01;

    mutable std::mutex mtx_;  ///< Mutex for thread safety

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
     * @brief Get or create feature ID
     * @param feature Feature identifier
     * @return Internal feature ID (caller must hold lock)
     */
    auto getFeatureId(const std::string& feature) -> int;

    /**
     * @brief Calculate time decay factor for a rating
     * @param ratingTime Timestamp of the rating
     * @return Time decay factor
     */
    [[nodiscard]] auto calculateTimeFactor(
        const std::chrono::system_clock::time_point& ratingTime) const
        -> double;

    /**
     * @brief Update matrix factorization model using SGD
     */
    void updateMatrixFactorizationSGD();

    /**
     * @brief Update matrix factorization model using ALS
     */
    void updateMatrixFactorizationALS();

    /**
     * @brief Normalize ratings by subtracting mean
     */
    void normalizeRatings();

    /**
     * @brief Clear expired cache entries
     */
    void clearExpiredCache() const;

    /**
     * @brief Calculate similarity between two items
     * @param item1 First item ID
     * @param item2 Second item ID
     * @return Similarity score between 0 and 1
     */
    [[nodiscard]] auto calculateItemSimilarity(int item1, int item2) const
        -> double;

    /**
     * @brief Get cached recommendations for a user if available
     * @param user User identifier
     * @return Optional vector of recommended items and scores
     */
    [[nodiscard]] auto getCachedRecommendations(const std::string& user) const
        -> std::optional<std::vector<std::pair<std::string, double>>>;

    /**
     * @brief Process batch of ratings
     * @param batch Vector of user-item-rating tuples
     * (caller must hold lock)
     */
    void processBatch(
        const std::vector<std::tuple<std::string, std::string, double>>&
            batch);

    /**
     * @brief Calculate hybrid recommendation score
     * @param userId User ID
     * @param itemId Item ID
     * @return Hybrid score combining matrix factorization and content-based
     * (caller must hold lock)
     */
    [[nodiscard]] auto hybridScore(int userId, int itemId) const -> double;

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
};

}  // namespace lithium::target::recommendation

#endif  // LITHIUM_TARGET_RECOMMENDATION_MATRIX_FACTORIZATION_HPP
