#include "matrix_factorization.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::target::recommendation {

// Constructor implementations
MatrixFactorizationEngine::MatrixFactorizationEngine()
    : MatrixFactorizationEngine(Config{}) {}

MatrixFactorizationEngine::MatrixFactorizationEngine(const Config& config)
    : config_(config) {
    spdlog::info("MatrixFactorizationEngine initialized with {} latent factors",
                 config_.latentFactors);
}

// Helper methods
auto MatrixFactorizationEngine::getUserId(const std::string& user) -> int {
    auto it = userIndex_.find(user);
    if (it == userIndex_.end()) {
        int newId = static_cast<int>(userIndex_.size());
        userIndex_[user] = newId;
        spdlog::debug("New user added: {} with ID: {}", user, newId);
        return newId;
    }
    return it->second;
}

auto MatrixFactorizationEngine::getItemId(const std::string& item) -> int {
    auto it = itemIndex_.find(item);
    if (it == itemIndex_.end()) {
        int newId = static_cast<int>(itemIndex_.size());
        itemIndex_[item] = newId;
        spdlog::debug("New item added: {} with ID: {}", item, newId);
        return newId;
    }
    return it->second;
}

auto MatrixFactorizationEngine::getFeatureId(const std::string& feature)
    -> int {
    auto it = featureIndex_.find(feature);
    if (it == featureIndex_.end()) {
        int newId = static_cast<int>(featureIndex_.size());
        featureIndex_[feature] = newId;
        spdlog::debug("New feature added: {} with ID: {}", feature, newId);
        return newId;
    }
    return it->second;
}

auto MatrixFactorizationEngine::getUserName(int userId) const -> std::string {
    for (const auto& [name, id] : userIndex_) {
        if (id == userId) {
            return name;
        }
    }
    return {};
}

auto MatrixFactorizationEngine::getItemName(int itemId) const -> std::string {
    for (const auto& [name, id] : itemIndex_) {
        if (id == itemId) {
            return name;
        }
    }
    return {};
}

auto MatrixFactorizationEngine::calculateTimeFactor(
    const std::chrono::system_clock::time_point& ratingTime) const -> double {
    auto now = std::chrono::system_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::hours>(now - ratingTime);
    double timeFactor = std::exp(
        -config_.timeDecayFactor * static_cast<double>(duration.count()) /
        (HOURS_IN_A_DAY * DAYS_IN_A_YEAR));
    return timeFactor;
}

void MatrixFactorizationEngine::normalizeRatings() {
    double mean = 0.0;
    if (!ratings_.empty()) {
        mean =
            std::accumulate(ratings_.begin(), ratings_.end(), 0.0,
                           [](double sum, const auto& rating) {
                               return sum + rating.value;
                           }) /
            ratings_.size();
    }

    for (auto& rating : ratings_) {
        rating.value -= mean;
    }
    spdlog::info("Ratings normalization completed (mean: {})", mean);
}

void MatrixFactorizationEngine::updateMatrixFactorizationSGD() {
    std::lock_guard lock(mtx_);
    spdlog::info("Starting SGD matrix factorization update");

    size_t numUsers = userIndex_.size();
    size_t numItems = itemIndex_.size();

    // Initialize matrices
    if (userFactors_.rows() != static_cast<int>(numUsers) ||
        userFactors_.cols() != config_.latentFactors) {
        userFactors_ =
            Eigen::MatrixXd::Random(numUsers, config_.latentFactors) *
            RANDOM_INIT_RANGE;
    }
    if (itemFactors_.rows() != static_cast<int>(numItems) ||
        itemFactors_.cols() != config_.latentFactors) {
        itemFactors_ =
            Eigen::MatrixXd::Random(numItems, config_.latentFactors) *
            RANDOM_INIT_RANGE;
    }

    // SGD optimization
    for (int iter = 0; iter < config_.maxIterations; ++iter) {
        for (const auto& rating : ratings_) {
            double pred =
                userFactors_.row(rating.userId).dot(itemFactors_.row(rating.itemId));
            double err = rating.value - pred;

            Eigen::VectorXd userVec = userFactors_.row(rating.userId);
            Eigen::VectorXd itemVec = itemFactors_.row(rating.itemId);

            userFactors_.row(rating.userId) +=
                config_.learningRate *
                (err * itemVec - config_.regularization * userVec);
            itemFactors_.row(rating.itemId) +=
                config_.learningRate *
                (err * userVec - config_.regularization * itemVec);
        }
    }

    spdlog::info("SGD matrix factorization completed");
}

void MatrixFactorizationEngine::updateMatrixFactorizationALS() {
    std::lock_guard lock(mtx_);
    spdlog::info("Starting ALS matrix factorization update");

    size_t numUsers = userIndex_.size();
    size_t numItems = itemIndex_.size();

    // Initialize matrices
    if (userFactors_.rows() != static_cast<int>(numUsers) ||
        userFactors_.cols() != config_.latentFactors) {
        userFactors_ =
            Eigen::MatrixXd::Random(numUsers, config_.latentFactors) *
            RANDOM_INIT_RANGE;
    }
    if (itemFactors_.rows() != static_cast<int>(numItems) ||
        itemFactors_.cols() != config_.latentFactors) {
        itemFactors_ =
            Eigen::MatrixXd::Random(numItems, config_.latentFactors) *
            RANDOM_INIT_RANGE;
    }

    // Build rating matrix
    Eigen::MatrixXd ratingMatrix =
        Eigen::MatrixXd::Zero(numUsers, numItems);
    for (const auto& rating : ratings_) {
        ratingMatrix(rating.userId, rating.itemId) = rating.value;
    }

    // ALS iterations
    const int ALSIterations = 10;
    for (int iter = 0; iter < ALSIterations; ++iter) {
        // Update user factors
        for (size_t u = 0; u < numUsers; ++u) {
            Eigen::MatrixXd YtY =
                itemFactors_.transpose() * itemFactors_;
            Eigen::VectorXd Yt_r = itemFactors_.transpose() *
                                   ratingMatrix.row(u).transpose();
            Eigen::MatrixXd A = YtY + config_.regularization *
                               Eigen::MatrixXd::Identity(
                                   config_.latentFactors,
                                   config_.latentFactors);
            userFactors_.row(u) = A.inverse() * Yt_r;
        }

        // Update item factors
        for (size_t i = 0; i < numItems; ++i) {
            Eigen::MatrixXd XtX =
                userFactors_.transpose() * userFactors_;
            Eigen::VectorXd Xt_r = userFactors_.transpose() *
                                   ratingMatrix.col(i);
            Eigen::MatrixXd A = XtX + config_.regularization *
                               Eigen::MatrixXd::Identity(
                                   config_.latentFactors,
                                   config_.latentFactors);
            itemFactors_.row(i) = A.inverse() * Xt_r;
        }
    }

    spdlog::info("ALS matrix factorization completed");
}

void MatrixFactorizationEngine::clearExpiredCache() const {
    auto now = std::chrono::system_clock::now();
    if (now - cache_.lastUpdate > Cache::CACHE_DURATION) {
        cache_.recommendations.clear();
        cache_.lastUpdate = now;
    }

    if (cache_.recommendations.size() > CACHE_MAX_ITEMS) {
        cache_.recommendations.clear();
    }
}

auto MatrixFactorizationEngine::calculateItemSimilarity(int item1, int item2)
    const -> double {
    if (!itemFeatures_.contains(item1) || !itemFeatures_.contains(item2)) {
        return 0.0;
    }

    double dotProduct = 0.0;
    double norm1 = 0.0;
    double norm2 = 0.0;

    for (const auto& [featureId, value1] : itemFeatures_.at(item1)) {
        if (itemFeatures_.at(item2).contains(featureId)) {
            dotProduct += value1 * itemFeatures_.at(item2).at(featureId);
        }
        norm1 += value1 * value1;
    }

    for (const auto& [_, value2] : itemFeatures_.at(item2)) {
        norm2 += value2 * value2;
    }

    if (norm1 == 0.0 || norm2 == 0.0) {
        return 0.0;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

auto MatrixFactorizationEngine::hybridScore(int userId, int itemId) const
    -> double {
    double mfScore =
        userFactors_.row(userId).dot(itemFactors_.row(itemId));

    double contentScore = 0.0;
    for (const auto& [otherItemId, _] : itemFeatures_) {
        if (otherItemId != itemId) {
            double similarity = calculateItemSimilarity(itemId, otherItemId);
            if (similarity > SIMILARITY_THRESHOLD) {
                contentScore += similarity;
            }
        }
    }

    return 0.7 * mfScore + 0.3 * contentScore;
}

auto MatrixFactorizationEngine::getCachedRecommendations(
    const std::string& user) const
    -> std::optional<std::vector<std::pair<std::string, double>>> {
    clearExpiredCache();
    auto it = cache_.recommendations.find(user);
    if (it != cache_.recommendations.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MatrixFactorizationEngine::processBatch(
    const std::vector<std::tuple<std::string, std::string, double>>& batch) {
    std::lock_guard lock(mtx_);

    for (const auto& [userId, itemId, value] : batch) {
        try {
            if (value < 0.0 || value > 5.0) {
                spdlog::warn("Invalid rating value {} for user {} and item {}",
                            value, userId, itemId);
                continue;
            }

            int uid = getUserId(userId);
            int iid = getItemId(itemId);
            ratings_.emplace_back(Rating{uid, iid, value,
                                        std::chrono::system_clock::now()});
        } catch (const std::exception& e) {
            spdlog::warn("Failed to process rating: {}", e.what());
        }
    }
}

// Public API implementations
void MatrixFactorizationEngine::addRating(const std::string& userId,
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
    ratings_.emplace_back(Rating{uid, iid, rating,
                                std::chrono::system_clock::now()});
}

void MatrixFactorizationEngine::addImplicitFeedback(const std::string& userId,
                                                   const std::string& itemId) {
    if (userId.empty() || itemId.empty()) {
        throw DataException("User and item identifiers cannot be empty");
    }

    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);
    ratings_.emplace_back(Rating{uid, iid, 1.0,
                                std::chrono::system_clock::now()});
}

void MatrixFactorizationEngine::addRatings(
    const std::vector<std::tuple<std::string, std::string, double>>&
        ratings) {
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

void MatrixFactorizationEngine::addItem(const std::string& itemId,
                                       const std::vector<std::string>&
                                           features) {
    std::lock_guard lock(mtx_);
    int iid = getItemId(itemId);
    for (const auto& feature : features) {
        int fid = getFeatureId(feature);
        itemFeatures_[iid][fid] = 1.0;
    }
}

void MatrixFactorizationEngine::addItemFeature(const std::string& itemId,
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

void MatrixFactorizationEngine::addItems(
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        items) {
    for (const auto& [itemId, features] : items) {
        addItem(itemId, features);
    }
}

void MatrixFactorizationEngine::train() {
    spdlog::info("Starting model training");
    normalizeRatings();

    if (config_.optimizationMethod == "als") {
        updateMatrixFactorizationALS();
    } else {
        updateMatrixFactorizationSGD();
    }

    spdlog::info("Model training completed");
}

auto MatrixFactorizationEngine::recommend(const std::string& userId, int topN)
    -> std::vector<std::pair<std::string, double>> {
    auto cachedRecommendations = getCachedRecommendations(userId);
    if (cachedRecommendations.has_value()) {
        auto recommendations = cachedRecommendations.value();
        if (recommendations.size() >= static_cast<size_t>(topN)) {
            return {recommendations.begin(), recommendations.begin() + topN};
        }
    }

    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    std::unordered_map<int, double> scores;

    Eigen::VectorXd userVec = userFactors_.row(uid);
    for (int iid = 0; iid < itemFactors_.rows(); ++iid) {
        double score = userVec.dot(itemFactors_.row(iid));
        scores[iid] = score;
    }

    std::vector<std::pair<int, double>> scoredItems(scores.begin(),
                                                     scores.end());
    std::sort(scoredItems.begin(), scoredItems.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });

    std::vector<std::pair<std::string, double>> recommendations;
    recommendations.reserve(topN);

    for (const auto& [iid, score] : scoredItems) {
        auto itemName = getItemName(iid);
        if (!itemName.empty()) {
            recommendations.emplace_back(itemName, score);
        }
        if (recommendations.size() >= static_cast<size_t>(topN)) {
            break;
        }
    }

    cache_.recommendations[userId] = recommendations;
    cache_.lastUpdate = std::chrono::system_clock::now();

    return recommendations;
}

auto MatrixFactorizationEngine::predictRating(const std::string& userId,
                                             const std::string& itemId)
    -> double {
    std::lock_guard lock(mtx_);
    int uid = getUserId(userId);
    int iid = getItemId(itemId);
    return userFactors_.row(uid).dot(itemFactors_.row(iid));
}

void MatrixFactorizationEngine::saveModel(const std::string& path) {
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

    // Save matrices
    Eigen::Index rows = userFactors_.rows();
    Eigen::Index cols = userFactors_.cols();
    file.write(reinterpret_cast<const char*>(&rows), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(&cols), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(userFactors_.data()),
              static_cast<std::streamsize>(sizeof(double) * rows * cols));

    rows = itemFactors_.rows();
    cols = itemFactors_.cols();
    file.write(reinterpret_cast<const char*>(&rows), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(&cols), sizeof(Eigen::Index));
    file.write(reinterpret_cast<const char*>(itemFactors_.data()),
              static_cast<std::streamsize>(sizeof(double) * rows * cols));

    file.close();
    spdlog::info("Model saved successfully to {}", path);
}

void MatrixFactorizationEngine::loadModel(const std::string& path) {
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

    // Load matrices
    Eigen::Index rows, cols;
    file.read(reinterpret_cast<char*>(&rows), sizeof(Eigen::Index));
    file.read(reinterpret_cast<char*>(&cols), sizeof(Eigen::Index));
    userFactors_.resize(rows, cols);
    file.read(reinterpret_cast<char*>(userFactors_.data()),
             static_cast<std::streamsize>(sizeof(double) * rows * cols));

    file.read(reinterpret_cast<char*>(&rows), sizeof(Eigen::Index));
    file.read(reinterpret_cast<char*>(&cols), sizeof(Eigen::Index));
    itemFactors_.resize(rows, cols);
    file.read(reinterpret_cast<char*>(itemFactors_.data()),
             static_cast<std::streamsize>(sizeof(double) * rows * cols));

    file.close();
    spdlog::info("Model loaded successfully from {}", path);
}

auto MatrixFactorizationEngine::getStats() const -> std::string {
    std::lock_guard lock(mtx_);
    std::stringstream ss;
    ss << "Matrix Factorization Engine Statistics:\n"
       << "  Users: " << userIndex_.size() << "\n"
       << "  Items: " << itemIndex_.size() << "\n"
       << "  Features: " << featureIndex_.size() << "\n"
       << "  Ratings: " << ratings_.size() << "\n"
       << "  Latent Factors: " << config_.latentFactors << "\n"
       << "  Optimization Method: " << config_.optimizationMethod << "\n"
       << "  Cache Entries: " << cache_.recommendations.size();
    return ss.str();
}

void MatrixFactorizationEngine::clear() {
    std::lock_guard lock(mtx_);
    ratings_.clear();
    itemFeatures_.clear();
    userIndex_.clear();
    itemIndex_.clear();
    featureIndex_.clear();
    cache_.recommendations.clear();
    userFactors_.resize(0, 0);
    itemFactors_.resize(0, 0);
    spdlog::info("All data cleared from MatrixFactorizationEngine");
}

void MatrixFactorizationEngine::optimize() {
    std::lock_guard lock(mtx_);

    ratings_.erase(
        std::remove_if(ratings_.begin(), ratings_.end(),
                      [](const auto& rating) {
                          return rating.value < 0 || rating.value > 5;
                      }),
        ratings_.end());

    updateMatrixFactorizationSGD();
    cache_.recommendations.clear();
    spdlog::info("Model optimization completed");
}

}  // namespace lithium::target::recommendation
