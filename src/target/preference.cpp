#include "preference.hpp"
#include "reader.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::target {

// Helper method - caller must hold lock
auto AdvancedRecommendationEngine::getUserId(const std::string& user) -> int {
    auto userIterator = userIndex_.find(user);
    if (userIterator == userIndex_.end()) {
        int newIndex = static_cast<int>(userIndex_.size());
        userIndex_[user] = newIndex;
        spdlog::info("New user added: {} with ID: {}", user, newIndex);
        return newIndex;
    }
    return userIterator->second;
}

// Helper method - caller must hold lock
auto AdvancedRecommendationEngine::getItemId(const std::string& item) -> int {
    auto itemIterator = itemIndex_.find(item);
    if (itemIterator == itemIndex_.end()) {
        int newIndex = static_cast<int>(itemIndex_.size());
        itemIndex_[item] = newIndex;
        spdlog::info("New item added: {} with ID: {}", item, newIndex);
        return newIndex;
    }
    return itemIterator->second;
}

// Helper method - caller must hold lock
auto AdvancedRecommendationEngine::getFeatureId(const std::string& feature)
    -> int {
    auto featureIterator = featureIndex_.find(feature);
    if (featureIterator == featureIndex_.end()) {
        int newIndex = static_cast<int>(featureIndex_.size());
        featureIndex_[feature] = newIndex;
        spdlog::info("New feature added: {} with ID: {}", feature, newIndex);
        return newIndex;
    }
    return featureIterator->second;
}

auto AdvancedRecommendationEngine::calculateTimeFactor(
    const std::chrono::system_clock::time_point& ratingTime) const -> double {
    auto now = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::hours>(now - ratingTime);
    double timeFactor =
        std::exp(-TIME_DECAY_FACTOR * static_cast<double>(duration.count()) /
                 (HOURS_IN_A_DAY * DAYS_IN_A_YEAR));  // Annual decay
    return timeFactor;
}

void AdvancedRecommendationEngine::normalizeRatings() {
    std::lock_guard lock(mtx_);
    spdlog::info("Starting normalization of ratings");
    double mean = 0.0;
    if (!ratings_.empty()) {
        mean = std::accumulate(ratings_.begin(), ratings_.end(), 0.0,
                               [&](double sum, const auto& tup) {
                                   return sum + std::get<2>(tup);
                               }) /
               ratings_.size();
    }
    for (auto& tup : ratings_) {
        std::get<2>(tup) -= mean;
    }
    spdlog::info("Ratings normalization completed");
}

void AdvancedRecommendationEngine::updateMatrixFactorization() {
    std::lock_guard lock(mtx_);
    spdlog::info("Starting matrix factorization update");

    size_t numUsers = userIndex_.size();
    size_t numItems = itemIndex_.size();

    // Initialize user and item factor matrices
    if (userFactors_.rows() != static_cast<int>(numUsers) ||
        userFactors_.cols() != LATENT_FACTORS) {
        userFactors_ = Eigen::MatrixXd::Random(numUsers, LATENT_FACTORS) *
                       RANDOM_INIT_RANGE;
    }
    if (itemFactors_.rows() != static_cast<int>(numItems) ||
        itemFactors_.cols() != LATENT_FACTORS) {
        itemFactors_ = Eigen::MatrixXd::Random(numItems, LATENT_FACTORS) *
                       RANDOM_INIT_RANGE;
    }

    // Perform iterative optimization
    for (int iter = 0; iter < MAX_ITERATIONS; ++iter) {
        for (const auto& [userId, itemId, rating, timestamp] : ratings_) {
            double pred =
                userFactors_.row(userId).dot(itemFactors_.row(itemId));
            double err = rating - pred;
            Eigen::VectorXd userVec = userFactors_.row(userId);
            Eigen::VectorXd itemVec = itemFactors_.row(itemId);

            userFactors_.row(userId) +=
                LEARNING_RATE * (err * itemVec - REGULARIZATION * userVec);
            itemFactors_.row(itemId) +=
                LEARNING_RATE * (err * userVec - REGULARIZATION * itemVec);
        }
    }
    spdlog::info("Matrix factorization update completed");
}

void AdvancedRecommendationEngine::addRating(const std::string& user,
                                             const std::string& item,
                                             double rating) {
    if (rating < 0.0 || rating > 5.0) {
        throw DataException("Rating must be between 0 and 5");
    }
    if (user.empty() || item.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }
    std::lock_guard lock(mtx_);
    int userId = getUserId(user);
    int itemId = getItemId(item);
    ratings_.emplace_back(userId, itemId, rating,
                          std::chrono::system_clock::now());
}

void AdvancedRecommendationEngine::addImplicitFeedback(
    const std::string& user, const std::string& item) {
    if (user.empty() || item.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }
    std::lock_guard lock(mtx_);
    int userId = getUserId(user);
    int itemId = getItemId(item);
    ratings_.emplace_back(userId, itemId, 1.0,
                          std::chrono::system_clock::now());
}

void AdvancedRecommendationEngine::addRatings(
    const std::vector<std::tuple<std::string, std::string, double>>& ratings) {
    std::vector<std::tuple<std::string, std::string, double>> batch;
    batch.reserve(BATCH_SIZE);

    for (const auto& rating : ratings) {
        batch.push_back(rating);
        if (batch.size() >= BATCH_SIZE) {
            processBatch(batch);
            batch.clear();
            batch.reserve(BATCH_SIZE);
        }
    }

    if (!batch.empty()) {
        processBatch(batch);
    }
}

void AdvancedRecommendationEngine::processBatch(
    const std::vector<std::tuple<std::string, std::string, double>>& batch) {
    // Note: Caller must NOT hold lock - this method acquires it
    std::lock_guard lock(mtx_);

    for (const auto& [user, item, rating] : batch) {
        try {
            if (rating < 0.0 || rating > 5.0) {
                spdlog::warn("Invalid rating value {} for user {} and item {}",
                             rating, user, item);
                continue;
            }

            int userId = getUserId(user);
            int itemId = getItemId(item);
            ratings_.emplace_back(userId, itemId, rating,
                                  std::chrono::system_clock::now());
        } catch (const std::exception& e) {
            spdlog::warn("Failed to process rating: {}", e.what());
        }
    }
}

void AdvancedRecommendationEngine::clearExpiredCache() {
    auto now = std::chrono::system_clock::now();
    if (now - cache_.lastUpdate > Cache::CACHE_DURATION) {
        cache_.recommendations.clear();
        cache_.lastUpdate = now;
    }

    // Limit cache size for memory efficiency
    if (cache_.recommendations.size() > CACHE_MAX_ITEMS) {
        cache_.recommendations.clear();
    }
}

auto AdvancedRecommendationEngine::calculateItemSimilarity(int item1, int item2)
    -> double {
    if (!itemFeatures_.contains(item1) || !itemFeatures_.contains(item2)) {
        return 0.0;
    }

    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    // Calculate cosine similarity between feature vectors
    for (const auto& [featureId, value1] : itemFeatures_[item1]) {
        if (itemFeatures_[item2].contains(featureId)) {
            dotProduct += value1 * itemFeatures_[item2][featureId];
        }
        norm1 += value1 * value1;
    }

    for (const auto& [_, value2] : itemFeatures_[item2]) {
        norm2 += value2 * value2;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2) + 1e-8);
}

auto AdvancedRecommendationEngine::hybridScore(const std::string& user,
                                               const std::string& item)
    -> double {
    int userId = getUserId(user);
    int itemId = getItemId(item);

    // Matrix factorization score
    double mfScore = userFactors_.row(userId).dot(itemFactors_.row(itemId));

    // Content-based score
    double contentScore = 0.0;
    for (const auto& [otherItemId, _] : itemFeatures_) {
        if (otherItemId != itemId) {
            double similarity = calculateItemSimilarity(itemId, otherItemId);
            if (similarity > SIMILARITY_THRESHOLD) {
                contentScore += similarity;
            }
        }
    }

    // Combine scores with weighting
    return 0.7 * mfScore + 0.3 * contentScore;
}

auto AdvancedRecommendationEngine::getCachedRecommendations(
    const std::string& user)
    -> std::optional<std::vector<std::pair<std::string, double>>> {
    clearExpiredCache();
    auto it = cache_.recommendations.find(user);
    if (it != cache_.recommendations.end()) {
        return it->second;
    }
    return std::nullopt;
}

void AdvancedRecommendationEngine::optimize() {
    std::lock_guard lock(mtx_);

    // Remove invalid ratings
    ratings_.erase(std::remove_if(ratings_.begin(), ratings_.end(),
                                  [](const auto& rating) {
                                      return std::get<2>(rating) < 0 ||
                                             std::get<2>(rating) > 5;
                                  }),
                   ratings_.end());

    // Update model
    updateMatrixFactorization();

    // Reset cache
    cache_.recommendations.clear();
    spdlog::info("Model optimization completed");
}

void AdvancedRecommendationEngine::clear() {
    std::lock_guard lock(mtx_);
    ratings_.clear();
    itemFeatures_.clear();
    userIndex_.clear();
    itemIndex_.clear();
    featureIndex_.clear();
    cache_.recommendations.clear();
    userFactors_.resize(0, 0);
    itemFactors_.resize(0, 0);

    spdlog::info("All data cleared from recommendation engine");
}

auto AdvancedRecommendationEngine::getStats() -> std::string {
    std::lock_guard lock(mtx_);
    std::stringstream ss;
    ss << "Users: " << userIndex_.size() << "\n"
       << "Items: " << itemIndex_.size() << "\n"
       << "Features: " << featureIndex_.size() << "\n"
       << "Ratings: " << ratings_.size() << "\n"
       << "Cache entries: " << cache_.recommendations.size();
    return ss.str();
}

void AdvancedRecommendationEngine::addItem(
    const std::string& item, const std::vector<std::string>& features) {
    std::lock_guard lock(mtx_);
    int itemId = getItemId(item);
    for (const auto& feature : features) {
        int featureId = getFeatureId(feature);
        itemFeatures_[itemId][featureId] = 1.0;  // Binary feature presence
    }
}

void AdvancedRecommendationEngine::addItems(
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        items) {
    for (const auto& [item, features] : items) {
        addItem(item, features);
    }
}

void AdvancedRecommendationEngine::addItemFeature(const std::string& item,
                                                  const std::string& feature,
                                                  double value) {
    std::lock_guard lock(mtx_);
    if (value < 0.0 || value > 1.0) {
        throw DataException("Feature value must be between 0 and 1");
    }
    int itemId = getItemId(item);
    int featureId = getFeatureId(feature);
    itemFeatures_[itemId][featureId] = value;
}

void AdvancedRecommendationEngine::train() {
    spdlog::info("Starting model training");
    normalizeRatings();
    updateMatrixFactorization();
    spdlog::info("Model training completed");
}

auto AdvancedRecommendationEngine::recommendItems(const std::string& user,
                                                  int topN)
    -> std::vector<std::pair<std::string, double>> {
    // Check cache first for performance
    auto cachedRecommendations = getCachedRecommendations(user);
    if (cachedRecommendations.has_value()) {
        auto recommendations = cachedRecommendations.value();
        if (recommendations.size() >= static_cast<size_t>(topN)) {
            return {recommendations.begin(), recommendations.begin() + topN};
        }
    }

    std::lock_guard lock(mtx_);
    int userId = getUserId(user);
    std::unordered_map<int, double> scores;

    // Calculate scores for all items
    Eigen::VectorXd userVec = userFactors_.row(userId);
    for (int itemId = 0; itemId < itemFactors_.rows(); ++itemId) {
        double score = userVec.dot(itemFactors_.row(itemId));
        scores[itemId] = score;
    }

    // Sort by score and select top N
    std::vector<std::pair<int, double>> scoredItems(scores.begin(),
                                                    scores.end());
    std::sort(scoredItems.begin(), scoredItems.end(),
              [](const auto& itemA, const auto& itemB) {
                  return itemA.second > itemB.second;
              });

    std::vector<std::pair<std::string, double>> recommendations;
    recommendations.reserve(topN);

    for (const auto& [itemId, score] : scoredItems) {
        const auto& itemNameIt = std::find_if(
            itemIndex_.begin(), itemIndex_.end(),
            [itemId](const auto& pair) { return pair.second == itemId; });
        if (itemNameIt != itemIndex_.end()) {
            recommendations.emplace_back(itemNameIt->first, score);
        }
        if (recommendations.size() >= static_cast<size_t>(topN)) {
            break;
        }
    }

    // Cache results for future use
    cache_.recommendations[user] = recommendations;
    cache_.lastUpdate = std::chrono::system_clock::now();

    return recommendations;
}

auto AdvancedRecommendationEngine::recommend(const std::string& user, int topN)
    -> std::vector<std::pair<std::string, double>> {
    return recommendItems(user, topN);
}

auto AdvancedRecommendationEngine::predictRating(const std::string& user,
                                                 const std::string& item)
    -> double {
    std::lock_guard lock(mtx_);
    int userId = getUserId(user);
    int itemId = getItemId(item);
    return userFactors_.row(userId).dot(itemFactors_.row(itemId));
}

auto AdvancedRecommendationEngine::predict(const std::string& user,
                                           const std::string& item) -> double {
    return predictRating(user, item);
}

void AdvancedRecommendationEngine::saveModel(const std::string& filename) {
    std::lock_guard lock(mtx_);
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for saving: " + filename);
    }

    // Save user index
    size_t userIndexSize = userIndex_.size();
    file.write(reinterpret_cast<const char*>(&userIndexSize), sizeof(size_t));

    for (const auto& [user, userId] : userIndex_) {
        size_t length = user.size();
        file.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        file.write(user.data(), static_cast<std::streamsize>(length));
        file.write(reinterpret_cast<const char*>(&userId), sizeof(int));
    }

    // Save item index
    size_t itemIndexSize = itemIndex_.size();
    file.write(reinterpret_cast<const char*>(&itemIndexSize), sizeof(size_t));

    for (const auto& [item, itemId] : itemIndex_) {
        size_t length = item.size();
        file.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        file.write(item.data(), static_cast<std::streamsize>(length));
        file.write(reinterpret_cast<const char*>(&itemId), sizeof(int));
    }

    // Save user factor matrix
    Eigen::Index rows = userFactors_.rows();
    Eigen::Index cols = userFactors_.cols();
    file.write(reinterpret_cast<const char*>(&rows), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(&cols), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(userFactors_.data()),
               static_cast<std::streamsize>(sizeof(double) * rows * cols));

    // Save item factor matrix
    rows = itemFactors_.rows();
    cols = itemFactors_.cols();
    file.write(reinterpret_cast<const char*>(&rows), sizeof(int));
    file.write(reinterpret_cast<const char*>(&cols), sizeof(int));
    file.write(reinterpret_cast<const char*>(itemFactors_.data()),
               static_cast<std::streamsize>(sizeof(double) * rows * cols));

    file.close();
    spdlog::info("Model saved successfully to {}", filename);
}

void AdvancedRecommendationEngine::loadModel(const std::string& filename) {
    std::lock_guard lock(mtx_);
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for loading: " + filename);
    }

    // Load user index
    size_t userIndexSize;
    file.read(reinterpret_cast<char*>(&userIndexSize), sizeof(size_t));
    for (size_t i = 0; i < userIndexSize; ++i) {
        size_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(size_t));
        std::string user(length, '\0');
        file.read(user.data(), static_cast<std::streamsize>(length));
        int userId;
        file.read(reinterpret_cast<char*>(&userId), sizeof(int));
        userIndex_[user] = userId;
    }

    // Load item index
    size_t itemIndexSize;
    file.read(reinterpret_cast<char*>(&itemIndexSize), sizeof(size_t));
    for (size_t i = 0; i < itemIndexSize; ++i) {
        size_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(size_t));
        std::string item(length, '\0');
        file.read(item.data(), static_cast<std::streamsize>(length));
        int itemId;
        file.read(reinterpret_cast<char*>(&itemId), sizeof(int));
        itemIndex_[item] = itemId;
    }

    // Load user factor matrix
    int rows, cols;
    file.read(reinterpret_cast<char*>(&rows), sizeof(int));
    file.read(reinterpret_cast<char*>(&cols), sizeof(int));
    userFactors_.resize(rows, cols);
    file.read(reinterpret_cast<char*>(userFactors_.data()),
              static_cast<std::streamsize>(sizeof(double) * rows * cols));

    // Load item factor matrix
    file.read(reinterpret_cast<char*>(&rows), sizeof(int));
    file.read(reinterpret_cast<char*>(&cols), sizeof(int));
    itemFactors_.resize(rows, cols);
    file.read(reinterpret_cast<char*>(itemFactors_.data()),
              static_cast<std::streamsize>(sizeof(double) * rows * cols));

    file.close();
    spdlog::info("Model loaded successfully from {}", filename);
}

bool AdvancedRecommendationEngine::exportToCSV(const std::string& filename) {
    std::lock_guard lock(mtx_);
    try {
        std::ofstream output(filename);
        if (!output.is_open()) {
            spdlog::error("Failed to open file for export: {}", filename);
            return false;
        }

        std::vector<std::string> fields = {"user", "item", "rating",
                                           "timestamp"};
        DictWriter writer(output, fields);

        for (const auto& [userId, itemId, rating, timestamp] : ratings_) {
            // Find user and item names
            std::string userName;
            std::string itemName;

            for (const auto& [name, id] : userIndex_) {
                if (id == userId) {
                    userName = name;
                    break;
                }
            }

            for (const auto& [name, id] : itemIndex_) {
                if (id == itemId) {
                    itemName = name;
                    break;
                }
            }

            auto duration = timestamp.time_since_epoch();
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(duration)
                    .count();

            std::unordered_map<std::string, std::string> row;
            row["user"] = userName;
            row["item"] = itemName;
            row["rating"] = std::to_string(rating);
            row["timestamp"] = std::to_string(seconds);
            writer.writeRow(row);
        }

        spdlog::info("Successfully exported {} ratings to {}", ratings_.size(),
                     filename);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error exporting to CSV {}: {}", filename, e.what());
        return false;
    }
}

bool AdvancedRecommendationEngine::importFromCSV(const std::string& filename) {
    try {
        std::ifstream input(filename);
        if (!input.is_open()) {
            spdlog::error("Failed to open file for import: {}", filename);
            return false;
        }

        std::vector<std::string> fields = {"user", "item", "rating"};
        DictReader reader(input, fields);

        std::unordered_map<std::string, std::string> row;
        size_t count = 0;
        std::vector<std::tuple<std::string, std::string, double>> batch;
        batch.reserve(BATCH_SIZE);

        while (reader.next(row)) {
            try {
                std::string user = row["user"];
                std::string item = row["item"];
                double rating = std::stod(row["rating"]);

                if (rating >= 0.0 && rating <= 5.0 && !user.empty() &&
                    !item.empty()) {
                    batch.emplace_back(user, item, rating);
                    count++;

                    if (batch.size() >= BATCH_SIZE) {
                        processBatch(batch);
                        batch.clear();
                        batch.reserve(BATCH_SIZE);
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Skipping invalid row: {}", e.what());
            }
        }

        // Process remaining batch
        if (!batch.empty()) {
            processBatch(batch);
        }

        spdlog::info("Successfully imported {} ratings from {}", count,
                     filename);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error importing from CSV {}: {}", filename, e.what());
        return false;
    }
}

}  // namespace lithium::target
