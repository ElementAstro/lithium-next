#include "collaborative_filter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::target::recommendation {

// Constructor implementations
CollaborativeFilteringEngine::CollaborativeFilteringEngine()
    : CollaborativeFilteringEngine(Config{}) {}

CollaborativeFilteringEngine::CollaborativeFilteringEngine(const Config& config)
    : config_(config) {
    spdlog::info("CollaborativeFilteringEngine initialized with {} metric",
                 config_.metric == SimilarityMetric::COSINE ? "COSINE"
                                                             : "PEARSON");
}

// Helper methods
auto CollaborativeFilteringEngine::getUserId(const std::string& user) -> int {
    auto it = userIndex_.find(user);
    if (it == userIndex_.end()) {
        int newId = static_cast<int>(userIndex_.size());
        userIndex_[user] = newId;
        spdlog::debug("New user added: {} with ID: {}", user, newId);
        return newId;
    }
    return it->second;
}

auto CollaborativeFilteringEngine::getItemId(const std::string& item) -> int {
    auto it = itemIndex_.find(item);
    if (it == itemIndex_.end()) {
        int newId = static_cast<int>(itemIndex_.size());
        itemIndex_[item] = newId;
        spdlog::debug("New item added: {} with ID: {}", item, newId);
        return newId;
    }
    return it->second;
}

auto CollaborativeFilteringEngine::getUserName(int userId) const
    -> std::string {
    for (const auto& [name, id] : userIndex_) {
        if (id == userId) {
            return name;
        }
    }
    return {};
}

auto CollaborativeFilteringEngine::getItemName(int itemId) const
    -> std::string {
    for (const auto& [name, id] : itemIndex_) {
        if (id == itemId) {
            return name;
        }
    }
    return {};
}

auto CollaborativeFilteringEngine::cosineSimilarity(
    const std::unordered_map<int, double>& vec1,
    const std::unordered_map<int, double>& vec2) -> double {
    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (const auto& [id, val] : vec1) {
        norm1 += val * val;
        if (vec2.contains(id)) {
            dotProduct += val * vec2.at(id);
        }
    }

    for (const auto& [_, val] : vec2) {
        norm2 += val * val;
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

auto CollaborativeFilteringEngine::pearsonCorrelation(
    const std::unordered_map<int, double>& vec1,
    const std::unordered_map<int, double>& vec2) -> double {
    // Find common items
    std::vector<std::pair<double, double>> commonRatings;
    for (const auto& [id, val1] : vec1) {
        if (vec2.contains(id)) {
            commonRatings.emplace_back(val1, vec2.at(id));
        }
    }

    if (commonRatings.size() < 2) {
        return 0.0;
    }

    // Calculate means
    double mean1 = std::accumulate(
        commonRatings.begin(), commonRatings.end(), 0.0,
        [](double sum, const auto& pair) { return sum + pair.first; });
    mean1 /= commonRatings.size();

    double mean2 = std::accumulate(
        commonRatings.begin(), commonRatings.end(), 0.0,
        [](double sum, const auto& pair) { return sum + pair.second; });
    mean2 /= commonRatings.size();

    // Calculate correlation
    double numerator = 0.0;
    double denom1 = 0.0;
    double denom2 = 0.0;

    for (const auto& [val1, val2] : commonRatings) {
        double diff1 = val1 - mean1;
        double diff2 = val2 - mean2;
        numerator += diff1 * diff2;
        denom1 += diff1 * diff1;
        denom2 += diff2 * diff2;
    }

    if (denom1 == 0.0 || denom2 == 0.0) {
        return 0.0;
    }

    return numerator / (std::sqrt(denom1) * std::sqrt(denom2));
}

auto CollaborativeFilteringEngine::calculateUserSimilarity(int user1, int user2)
    const -> double {
    if (!userRatings_.contains(user1) || !userRatings_.contains(user2)) {
        return 0.0;
    }

    const auto& ratings1 = userRatings_.at(user1);
    const auto& ratings2 = userRatings_.at(user2);

    if (config_.metric == SimilarityMetric::COSINE) {
        return cosineSimilarity(ratings1, ratings2);
    } else {
        return pearsonCorrelation(ratings1, ratings2);
    }
}

auto CollaborativeFilteringEngine::calculateItemSimilarity(int item1, int item2)
    const -> double {
    if (!itemRatings_.contains(item1) || !itemRatings_.contains(item2)) {
        return 0.0;
    }

    const auto& ratings1 = itemRatings_.at(item1);
    const auto& ratings2 = itemRatings_.at(item2);

    if (config_.metric == SimilarityMetric::COSINE) {
        return cosineSimilarity(ratings1, ratings2);
    } else {
        return pearsonCorrelation(ratings1, ratings2);
    }
}

auto CollaborativeFilteringEngine::getNearestUserNeighbors(int userId, int k)
    const -> std::vector<std::pair<int, double>> {
    std::vector<std::pair<int, double>> neighbors;

    for (const auto& [otherId, _] : userRatings_) {
        if (otherId == userId) continue;

        double similarity = calculateUserSimilarity(userId, otherId);
        if (similarity > config_.similarityThreshold) {
            neighbors.emplace_back(otherId, similarity);
        }
    }

    std::sort(neighbors.begin(), neighbors.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });

    if (neighbors.size() > static_cast<size_t>(config_.maxNeighbors)) {
        neighbors.resize(config_.maxNeighbors);
    }

    return neighbors;
}

auto CollaborativeFilteringEngine::getNearestItemNeighbors(int itemId, int k)
    const -> std::vector<std::pair<int, double>> {
    std::vector<std::pair<int, double>> neighbors;

    for (const auto& [otherId, _] : itemRatings_) {
        if (otherId == itemId) continue;

        double similarity = calculateItemSimilarity(itemId, otherId);
        if (similarity > config_.similarityThreshold) {
            neighbors.emplace_back(otherId, similarity);
        }
    }

    std::sort(neighbors.begin(), neighbors.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });

    if (neighbors.size() > static_cast<size_t>(config_.maxNeighbors)) {
        neighbors.resize(config_.maxNeighbors);
    }

    return neighbors;
}

void CollaborativeFilteringEngine::clearSimilarityCache() const {
    userSimilarityCache_.clear();
    itemSimilarityCache_.clear();
}

// Public API implementations
void CollaborativeFilteringEngine::addRating(const std::string& userId,
                                            const std::string& itemId,
                                            double rating) {
    if (rating < 0.0 || rating > 5.0) {
        throw DataException("Rating must be between 0 and 5");
    }
    if (userId.empty() || itemId.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }

    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);

    userRatings_[uid][iid] = rating;
    itemRatings_[iid][uid] = rating;

    clearSimilarityCache();
}

void CollaborativeFilteringEngine::addImplicitFeedback(
    const std::string& userId, const std::string& itemId) {
    if (userId.empty() || itemId.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }

    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);

    userRatings_[uid][iid] = 1.0;
    itemRatings_[iid][uid] = 1.0;

    clearSimilarityCache();
}

void CollaborativeFilteringEngine::addRatings(
    const std::vector<std::tuple<std::string, std::string, double>>&
        ratings) {
    for (const auto& [userId, itemId, rating] : ratings) {
        if (rating >= 0.0 && rating <= 5.0) {
            addRating(userId, itemId, rating);
        }
    }
}

void CollaborativeFilteringEngine::addItem(
    const std::string& itemId, const std::vector<std::string>& features) {
    // Pure collaborative filtering doesn't use item features
    std::lock_guard lock(mtx_);
    getItemId(itemId);  // Just ensure the item exists
}

void CollaborativeFilteringEngine::addItemFeature(const std::string& itemId,
                                                 const std::string& featureId,
                                                 double value) {
    // Pure collaborative filtering doesn't use item features
    std::lock_guard lock(mtx_);
    getItemId(itemId);  // Just ensure the item exists
}

void CollaborativeFilteringEngine::addItems(
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        items) {
    for (const auto& [itemId, features] : items) {
        addItem(itemId, features);
    }
}

void CollaborativeFilteringEngine::train() {
    // Collaborative filtering doesn't require explicit training
    // Similarities are computed on-demand
    spdlog::info("CollaborativeFilteringEngine training (clearing caches)");
    clearSimilarityCache();
}

auto CollaborativeFilteringEngine::recommend(const std::string& userId,
                                            int topN)
    -> std::vector<std::pair<std::string, double>> {
    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);

    if (!userRatings_.contains(uid)) {
        return {};
    }

    std::unordered_map<int, double> scores;

    // Get similar users
    auto neighbors = getNearestUserNeighbors(uid, config_.maxNeighbors);

    // Aggregate ratings from similar users
    for (const auto& [neighborId, similarity] : neighbors) {
        if (!userRatings_.contains(neighborId)) continue;

        for (const auto& [itemId, rating] : userRatings_.at(neighborId)) {
            // Don't recommend items the user already rated
            if (userRatings_[uid].contains(itemId)) {
                continue;
            }

            if (!scores.contains(itemId)) {
                scores[itemId] = 0.0;
            }
            scores[itemId] += rating * similarity;
        }
    }

    // Sort and select top N
    std::vector<std::pair<int, double>> scoredItems(scores.begin(),
                                                     scores.end());
    std::sort(scoredItems.begin(), scoredItems.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });

    std::vector<std::pair<std::string, double>> recommendations;
    recommendations.reserve(topN);

    for (const auto& [itemId, score] : scoredItems) {
        auto itemName = getItemName(itemId);
        if (!itemName.empty()) {
            recommendations.emplace_back(itemName, score);
        }
        if (recommendations.size() >= static_cast<size_t>(topN)) {
            break;
        }
    }

    return recommendations;
}

auto CollaborativeFilteringEngine::predictRating(const std::string& userId,
                                                const std::string& itemId)
    -> double {
    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);

    if (!userRatings_.contains(uid) || !userRatings_[uid].contains(iid)) {
        // Predict using similar users
        auto neighbors = getNearestUserNeighbors(uid, config_.maxNeighbors);

        double weightedSum = 0.0;
        double similaritySum = 0.0;

        for (const auto& [neighborId, similarity] : neighbors) {
            if (userRatings_.contains(neighborId) &&
                userRatings_[neighborId].contains(iid)) {
                weightedSum +=
                    userRatings_[neighborId][iid] * similarity;
                similaritySum += similarity;
            }
        }

        if (similaritySum == 0.0) {
            return 2.5;  // Default middle rating
        }

        return weightedSum / similaritySum;
    }

    return userRatings_[uid][iid];
}

void CollaborativeFilteringEngine::saveModel(const std::string& path) {
    std::lock_guard lock(mtx_);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for saving: " + path);
    }

    // Save indices
    size_t userIndexSize = userIndex_.size();
    file.write(reinterpret_cast<const char*>(&userIndexSize), sizeof(size_t));
    for (const auto& [user, uid] : userIndex_) {
        size_t length = user.size();
        file.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        file.write(user.data(), static_cast<std::streamsize>(length));
        file.write(reinterpret_cast<const char*>(&uid), sizeof(int));
    }

    size_t itemIndexSize = itemIndex_.size();
    file.write(reinterpret_cast<const char*>(&itemIndexSize), sizeof(size_t));
    for (const auto& [item, iid] : itemIndex_) {
        size_t length = item.size();
        file.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        file.write(item.data(), static_cast<std::streamsize>(length));
        file.write(reinterpret_cast<const char*>(&iid), sizeof(int));
    }

    // Save ratings
    size_t ratingsSize = userRatings_.size();
    file.write(reinterpret_cast<const char*>(&ratingsSize), sizeof(size_t));
    for (const auto& [uid, itemRatings] : userRatings_) {
        file.write(reinterpret_cast<const char*>(&uid), sizeof(int));
        size_t itemCount = itemRatings.size();
        file.write(reinterpret_cast<const char*>(&itemCount), sizeof(size_t));
        for (const auto& [iid, rating] : itemRatings) {
            file.write(reinterpret_cast<const char*>(&iid), sizeof(int));
            file.write(reinterpret_cast<const char*>(&rating), sizeof(double));
        }
    }

    file.close();
    spdlog::info("Model saved successfully to {}", path);
}

void CollaborativeFilteringEngine::loadModel(const std::string& path) {
    std::lock_guard lock(mtx_);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for loading: " + path);
    }

    // Load indices
    size_t userIndexSize;
    file.read(reinterpret_cast<char*>(&userIndexSize), sizeof(size_t));
    for (size_t i = 0; i < userIndexSize; ++i) {
        size_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(size_t));
        std::string user(length, '\0');
        file.read(user.data(), static_cast<std::streamsize>(length));
        int uid;
        file.read(reinterpret_cast<char*>(&uid), sizeof(int));
        userIndex_[user] = uid;
    }

    size_t itemIndexSize;
    file.read(reinterpret_cast<char*>(&itemIndexSize), sizeof(size_t));
    for (size_t i = 0; i < itemIndexSize; ++i) {
        size_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(size_t));
        std::string item(length, '\0');
        file.read(item.data(), static_cast<std::streamsize>(length));
        int iid;
        file.read(reinterpret_cast<char*>(&iid), sizeof(int));
        itemIndex_[item] = iid;
    }

    // Load ratings
    size_t ratingsSize;
    file.read(reinterpret_cast<char*>(&ratingsSize), sizeof(size_t));
    for (size_t i = 0; i < ratingsSize; ++i) {
        int uid;
        file.read(reinterpret_cast<char*>(&uid), sizeof(int));
        size_t itemCount;
        file.read(reinterpret_cast<char*>(&itemCount), sizeof(size_t));
        for (size_t j = 0; j < itemCount; ++j) {
            int iid;
            double rating;
            file.read(reinterpret_cast<char*>(&iid), sizeof(int));
            file.read(reinterpret_cast<char*>(&rating), sizeof(double));
            userRatings_[uid][iid] = rating;
            itemRatings_[iid][uid] = rating;
        }
    }

    file.close();
    spdlog::info("Model loaded successfully from {}", path);
}

auto CollaborativeFilteringEngine::getStats() const -> std::string {
    std::lock_guard lock(mtx_);
    std::stringstream ss;
    size_t totalRatings = 0;
    for (const auto& [_, ratings] : userRatings_) {
        totalRatings += ratings.size();
    }

    ss << "Collaborative Filtering Engine Statistics:\n"
       << "  Users: " << userIndex_.size() << "\n"
       << "  Items: " << itemIndex_.size() << "\n"
       << "  Total Ratings: " << totalRatings << "\n"
       << "  Similarity Metric: "
       << (config_.metric == SimilarityMetric::COSINE ? "COSINE" : "PEARSON")
       << "\n"
       << "  Min Common Ratings: " << config_.minCommonRatings;
    return ss.str();
}

void CollaborativeFilteringEngine::clear() {
    std::lock_guard lock(mtx_);
    userRatings_.clear();
    itemRatings_.clear();
    userIndex_.clear();
    itemIndex_.clear();
    clearSimilarityCache();
    spdlog::info("All data cleared from CollaborativeFilteringEngine");
}

void CollaborativeFilteringEngine::optimize() {
    std::lock_guard lock(mtx_);
    clearSimilarityCache();
    spdlog::info("CollaborativeFilteringEngine optimization completed");
}

}  // namespace lithium::target::recommendation
