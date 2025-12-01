#ifndef LITHIUM_TARGET_RECOMMENDATION_CONTENT_FILTER_HPP
#define LITHIUM_TARGET_RECOMMENDATION_CONTENT_FILTER_HPP

#include "recommendation_engine.hpp"

#include <chrono>
#include <mutex>
#include <unordered_map>

namespace lithium::target::recommendation {

/**
 * @brief Content-Based Filtering recommendation engine
 *
 * Implements IRecommendationEngine using content-based filtering.
 * Recommends items similar to those the user has liked in the past,
 * based on item features/attributes.
 *
 * Features:
 * - Item feature vectors
 * - User preference profiles built from rated items
 * - Cosine similarity for item matching
 */
class ContentFilteringEngine : public IRecommendationEngine {
public:
    /**
     * @brief Configuration for content-based filtering
     */
    struct Config {
        double featureSimilarityThreshold = 0.1;
        int maxRecommendedItems = 100;
        bool useUserProfile = true;  ///< Build user preference profiles
    };

    /**
     * @brief Construct with default configuration
     */
    ContentFilteringEngine();

    /**
     * @brief Construct with custom configuration
     * @param config Configuration parameters
     */
    explicit ContentFilteringEngine(const Config& config);

    ~ContentFilteringEngine() override = default;

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

    // Rating data (userId -> itemId -> rating)
    std::unordered_map<int, std::unordered_map<int, double>> userRatings_;

    // Item features (itemId -> featureId -> value)
    std::unordered_map<int, std::unordered_map<int, double>> itemFeatures_;

    // User preference profiles (userId -> featureId -> preference)
    std::unordered_map<int, std::unordered_map<int, double>> userProfiles_;

    // Rated items tracking (userId -> itemId set)
    std::unordered_map<int, std::unordered_map<int, bool>> userRatedItems_;

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
     * @brief Get or create feature ID
     * @param feature Feature identifier
     * @return Internal feature ID (caller must hold lock)
     */
    auto getFeatureId(const std::string& feature) -> int;

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
     * @brief Calculate similarity between two feature vectors
     * @param features1 First feature vector
     * @param features2 Second feature vector
     * @return Cosine similarity score
     */
    [[nodiscard]] static auto calculateFeatureSimilarity(
        const std::unordered_map<int, double>& features1,
        const std::unordered_map<int, double>& features2) -> double;

    /**
     * @brief Build user preference profile from rated items
     * @param userId User ID
     * (caller must hold lock)
     */
    void buildUserProfile(int userId);

    /**
     * @brief Calculate similarity between user profile and item
     * @param userId User ID
     * @param itemId Item ID
     * @return Similarity score
     * (caller must hold lock)
     */
    [[nodiscard]] auto calculateProfileItemSimilarity(int userId, int itemId)
        const -> double;
};

}  // namespace lithium::target::recommendation

#endif  // LITHIUM_TARGET_RECOMMENDATION_CONTENT_FILTER_HPP
