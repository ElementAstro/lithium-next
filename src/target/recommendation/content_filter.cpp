#include "content_filter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::target::recommendation {

// Constructor implementations
ContentFilteringEngine::ContentFilteringEngine()
    : ContentFilteringEngine(Config{}) {}

ContentFilteringEngine::ContentFilteringEngine(const Config& config)
    : config_(config) {
    spdlog::info("ContentFilteringEngine initialized");
}

// Helper methods
auto ContentFilteringEngine::getUserId(const std::string& user) -> int {
    auto it = userIndex_.find(user);
    if (it == userIndex_.end()) {
        int newId = static_cast<int>(userIndex_.size());
        userIndex_[user] = newId;
        spdlog::debug("New user added: {} with ID: {}", user, newId);
        return newId;
    }
    return it->second;
}

auto ContentFilteringEngine::getItemId(const std::string& item) -> int {
    auto it = itemIndex_.find(item);
    if (it == itemIndex_.end()) {
        int newId = static_cast<int>(itemIndex_.size());
        itemIndex_[item] = newId;
        spdlog::debug("New item added: {} with ID: {}", item, newId);
        return newId;
    }
    return it->second;
}

auto ContentFilteringEngine::getFeatureId(const std::string& feature) -> int {
    auto it = featureIndex_.find(feature);
    if (it == featureIndex_.end()) {
        int newId = static_cast<int>(featureIndex_.size());
        featureIndex_[feature] = newId;
        spdlog::debug("New feature added: {} with ID: {}", feature, newId);
        return newId;
    }
    return it->second;
}

auto ContentFilteringEngine::getUserName(int userId) const -> std::string {
    for (const auto& [name, id] : userIndex_) {
        if (id == userId) {
            return name;
        }
    }
    return {};
}

auto ContentFilteringEngine::getItemName(int itemId) const -> std::string {
    for (const auto& [name, id] : itemIndex_) {
        if (id == itemId) {
            return name;
        }
    }
    return {};
}

auto ContentFilteringEngine::calculateFeatureSimilarity(
    const std::unordered_map<int, double>& features1,
    const std::unordered_map<int, double>& features2) -> double {
    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (const auto& [id, val] : features1) {
        norm1 += val * val;
        if (features2.contains(id)) {
            dotProduct += val * features2.at(id);
        }
    }

    for (const auto& [_, val] : features2) {
        norm2 += val * val;
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

void ContentFilteringEngine::buildUserProfile(int userId) {
    if (!userRatings_.contains(userId)) {
        return;
    }

    // Clear existing profile
    userProfiles_[userId].clear();

    // Aggregate features from highly rated items
    for (const auto& [itemId, rating] : userRatings_[userId]) {
        if (itemFeatures_.contains(itemId)) {
            // Weight by rating (normalize)
            double weight = (rating - 2.5) / 2.5;  // Center around 2.5
            for (const auto& [featureId, value] : itemFeatures_[itemId]) {
                if (!userProfiles_[userId].contains(featureId)) {
                    userProfiles_[userId][featureId] = 0.0;
                }
                userProfiles_[userId][featureId] += value * weight;
            }
        }
    }

    // Normalize profile
    double norm = 0.0;
    for (const auto& [_, val] : userProfiles_[userId]) {
        norm += val * val;
    }

    if (norm > 0.0) {
        norm = std::sqrt(norm);
        for (auto& [_, val] : userProfiles_[userId]) {
            val /= norm;
        }
    }
}

auto ContentFilteringEngine::calculateProfileItemSimilarity(int userId,
                                                          int itemId) const
    -> double {
    if (!userProfiles_.contains(userId) || !itemFeatures_.contains(itemId)) {
        return 0.0;
    }

    return calculateFeatureSimilarity(userProfiles_.at(userId),
                                     itemFeatures_.at(itemId));
}

// Public API implementations
void ContentFilteringEngine::addRating(const std::string& userId,
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
    userRatedItems_[uid][iid] = true;

    if (config_.useUserProfile) {
        buildUserProfile(uid);
    }
}

void ContentFilteringEngine::addImplicitFeedback(const std::string& userId,
                                               const std::string& itemId) {
    if (userId.empty() || itemId.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }

    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);

    userRatings_[uid][iid] = 1.0;
    userRatedItems_[uid][iid] = true;

    if (config_.useUserProfile) {
        buildUserProfile(uid);
    }
}

void ContentFilteringEngine::addRatings(
    const std::vector<std::tuple<std::string, std::string, double>>&
        ratings) {
    for (const auto& [userId, itemId, rating] : ratings) {
        if (rating >= 0.0 && rating <= 5.0) {
            addRating(userId, itemId, rating);
        }
    }
}

void ContentFilteringEngine::addItem(const std::string& itemId,
                                    const std::vector<std::string>&
                                        features) {
    std::lock_guard lock(mtx_);
    int iid = getItemId(itemId);
    for (const auto& feature : features) {
        int fid = getFeatureId(feature);
        itemFeatures_[iid][fid] = 1.0;
    }
}

void ContentFilteringEngine::addItemFeature(const std::string& itemId,
                                           const std::string& featureId,
                                           double value) {
    if (value < 0.0 || value > 1.0) {
        throw DataException("Feature value must be between 0 and 1");
    }

    std::lock_guard lock(mtx_);
    int iid = getItemId(itemId);
    int fid = getFeatureId(featureId);
    itemFeatures_[iid][fid] = value;
}

void ContentFilteringEngine::addItems(
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        items) {
    for (const auto& [itemId, features] : items) {
        addItem(itemId, features);
    }
}

void ContentFilteringEngine::train() {
    std::lock_guard lock(mtx_);
    spdlog::info("Training content-based filtering model");

    // Build user profiles from all ratings
    if (config_.useUserProfile) {
        for (const auto& [userId, _] : userRatings_) {
            buildUserProfile(userId);
        }
    }

    spdlog::info("Content-based filtering training completed");
}

auto ContentFilteringEngine::recommend(const std::string& userId, int topN)
    -> std::vector<std::pair<std::string, double>> {
    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);

    if (!userRatings_.contains(uid)) {
        return {};
    }

    // If using profiles, ensure user profile exists
    if (config_.useUserProfile && !userProfiles_.contains(uid)) {
        buildUserProfile(uid);
    }

    std::unordered_map<int, double> scores;

    // Score all items not yet rated by the user
    for (const auto& [itemId, _] : itemFeatures_) {
        // Skip already rated items
        if (userRatedItems_[uid].contains(itemId)) {
            continue;
        }

        double score = 0.0;

        if (config_.useUserProfile && userProfiles_.contains(uid)) {
            // Use user profile similarity
            score = calculateProfileItemSimilarity(uid, itemId);
        } else {
            // Find similar items to those the user liked
            for (const auto& [ratedItemId, rating] : userRatings_[uid]) {
                if (itemFeatures_.contains(ratedItemId)) {
                    double similarity = calculateFeatureSimilarity(
                        itemFeatures_[ratedItemId], itemFeatures_[itemId]);
                    if (similarity > config_.featureSimilarityThreshold) {
                        score += similarity * (rating / 5.0);
                    }
                }
            }
        }

        if (score > 0.0) {
            scores[itemId] = score;
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

auto ContentFilteringEngine::predictRating(const std::string& userId,
                                          const std::string& itemId)
    -> double {
    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);

    // If user has already rated this item
    if (userRatings_.contains(uid) && userRatings_[uid].contains(iid)) {
        return userRatings_[uid][iid];
    }

    // Predict based on similar items
    double weightedSum = 0.0;
    double similaritySum = 0.0;

    for (const auto& [ratedItemId, rating] : userRatings_[uid]) {
        if (itemFeatures_.contains(ratedItemId) &&
            itemFeatures_.contains(iid)) {
            double similarity = calculateFeatureSimilarity(
                itemFeatures_[ratedItemId], itemFeatures_[iid]);
            if (similarity > config_.featureSimilarityThreshold) {
                weightedSum += rating * similarity;
                similaritySum += similarity;
            }
        }
    }

    if (similaritySum == 0.0) {
        return 2.5;  // Default middle rating
    }

    return weightedSum / similaritySum;
}

void ContentFilteringEngine::saveModel(const std::string& path) {
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

    size_t featureIndexSize = featureIndex_.size();
    file.write(reinterpret_cast<const char*>(&featureIndexSize),
              sizeof(size_t));
    for (const auto& [feature, fid] : featureIndex_) {
        size_t length = feature.size();
        file.write(reinterpret_cast<const char*>(&length), sizeof(size_t));
        file.write(feature.data(), static_cast<std::streamsize>(length));
        file.write(reinterpret_cast<const char*>(&fid), sizeof(int));
    }

    // Save item features
    size_t itemFeaturesSize = itemFeatures_.size();
    file.write(reinterpret_cast<const char*>(&itemFeaturesSize),
              sizeof(size_t));
    for (const auto& [iid, features] : itemFeatures_) {
        file.write(reinterpret_cast<const char*>(&iid), sizeof(int));
        size_t featureCount = features.size();
        file.write(reinterpret_cast<const char*>(&featureCount),
                  sizeof(size_t));
        for (const auto& [fid, value] : features) {
            file.write(reinterpret_cast<const char*>(&fid), sizeof(int));
            file.write(reinterpret_cast<const char*>(&value), sizeof(double));
        }
    }

    // Save user profiles
    size_t userProfilesSize = userProfiles_.size();
    file.write(reinterpret_cast<const char*>(&userProfilesSize),
              sizeof(size_t));
    for (const auto& [uid, profile] : userProfiles_) {
        file.write(reinterpret_cast<const char*>(&uid), sizeof(int));
        size_t featureCount = profile.size();
        file.write(reinterpret_cast<const char*>(&featureCount),
                  sizeof(size_t));
        for (const auto& [fid, value] : profile) {
            file.write(reinterpret_cast<const char*>(&fid), sizeof(int));
            file.write(reinterpret_cast<const char*>(&value), sizeof(double));
        }
    }

    file.close();
    spdlog::info("Model saved successfully to {}", path);
}

void ContentFilteringEngine::loadModel(const std::string& path) {
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

    size_t featureIndexSize;
    file.read(reinterpret_cast<char*>(&featureIndexSize), sizeof(size_t));
    for (size_t i = 0; i < featureIndexSize; ++i) {
        size_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(size_t));
        std::string feature(length, '\0');
        file.read(feature.data(), static_cast<std::streamsize>(length));
        int fid;
        file.read(reinterpret_cast<char*>(&fid), sizeof(int));
        featureIndex_[feature] = fid;
    }

    // Load item features
    size_t itemFeaturesSize;
    file.read(reinterpret_cast<char*>(&itemFeaturesSize), sizeof(size_t));
    for (size_t i = 0; i < itemFeaturesSize; ++i) {
        int iid;
        file.read(reinterpret_cast<char*>(&iid), sizeof(int));
        size_t featureCount;
        file.read(reinterpret_cast<char*>(&featureCount), sizeof(size_t));
        for (size_t j = 0; j < featureCount; ++j) {
            int fid;
            double value;
            file.read(reinterpret_cast<char*>(&fid), sizeof(int));
            file.read(reinterpret_cast<char*>(&value), sizeof(double));
            itemFeatures_[iid][fid] = value;
        }
    }

    // Load user profiles
    size_t userProfilesSize;
    file.read(reinterpret_cast<char*>(&userProfilesSize), sizeof(size_t));
    for (size_t i = 0; i < userProfilesSize; ++i) {
        int uid;
        file.read(reinterpret_cast<char*>(&uid), sizeof(int));
        size_t featureCount;
        file.read(reinterpret_cast<char*>(&featureCount), sizeof(size_t));
        for (size_t j = 0; j < featureCount; ++j) {
            int fid;
            double value;
            file.read(reinterpret_cast<char*>(&fid), sizeof(int));
            file.read(reinterpret_cast<char*>(&value), sizeof(double));
            userProfiles_[uid][fid] = value;
        }
    }

    file.close();
    spdlog::info("Model loaded successfully from {}", path);
}

auto ContentFilteringEngine::getStats() const -> std::string {
    std::lock_guard lock(mtx_);
    std::stringstream ss;
    size_t totalRatings = 0;
    for (const auto& [_, ratings] : userRatings_) {
        totalRatings += ratings.size();
    }

    ss << "Content-Based Filtering Engine Statistics:\n"
       << "  Users: " << userIndex_.size() << "\n"
       << "  Items: " << itemIndex_.size() << "\n"
       << "  Features: " << featureIndex_.size() << "\n"
       << "  Total Ratings: " << totalRatings << "\n"
       << "  User Profiles: " << userProfiles_.size();
    return ss.str();
}

void ContentFilteringEngine::clear() {
    std::lock_guard lock(mtx_);
    userRatings_.clear();
    itemFeatures_.clear();
    userProfiles_.clear();
    userRatedItems_.clear();
    userIndex_.clear();
    itemIndex_.clear();
    featureIndex_.clear();
    spdlog::info("All data cleared from ContentFilteringEngine");
}

void ContentFilteringEngine::optimize() {
    std::lock_guard lock(mtx_);

    // Rebuild user profiles
    if (config_.useUserProfile) {
        for (const auto& [userId, _] : userRatings_) {
            buildUserProfile(userId);
        }
    }

    spdlog::info("ContentFilteringEngine optimization completed");
}

}  // namespace lithium::target::recommendation
