#include "hybrid_recommender.hpp"

#include "collaborative_filter.hpp"
#include "content_filter.hpp"
#include "matrix_factorization.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lithium::target::recommendation {

// Constructor implementations
HybridRecommender::HybridRecommender()
    : HybridRecommender(Config{}) {}

HybridRecommender::HybridRecommender(
    std::unique_ptr<IRecommendationEngine> collaborative,
    std::unique_ptr<IRecommendationEngine> content,
    std::unique_ptr<IRecommendationEngine> matrixFactorization,
    const Config& config)
    : config_(config),
      collaborative_(std::move(collaborative)),
      content_(std::move(content)),
      matrixFactorization_(std::move(matrixFactorization)) {
    if (config_.normalizeWeights) {
        normalizeWeights();
    }
    spdlog::info(
        "HybridRecommender initialized with {} engines (CF: {:.2f}, "
        "Content: {:.2f}, MF: {:.2f})",
        getAvailableEngineCount(), config_.collaborativeWeight,
        config_.contentWeight, config_.matrixFactorizationWeight);
}

HybridRecommender::HybridRecommender(const Config& config)
    : HybridRecommender(
        std::make_unique<CollaborativeFilteringEngine>(),
        std::make_unique<ContentFilteringEngine>(),
        std::make_unique<MatrixFactorizationEngine>(), config) {}

// Helper methods
void HybridRecommender::normalizeWeights() {
    double total = config_.collaborativeWeight + config_.contentWeight +
                  config_.matrixFactorizationWeight;
    if (total > 0.0) {
        config_.collaborativeWeight /= total;
        config_.contentWeight /= total;
        config_.matrixFactorizationWeight /= total;
    }
}

auto HybridRecommender::getAvailableEngineCount() const -> int {
    int count = 0;
    if (collaborative_) count++;
    if (content_) count++;
    if (matrixFactorization_) count++;
    return count;
}

auto HybridRecommender::aggregateRecommendations(const std::string& userId,
                                                int topN) const
    -> std::vector<std::pair<std::string, double>> {
    std::unordered_map<std::string, double> aggregatedScores;
    std::unordered_map<std::string, int> scoreCount;

    // Collaborative filtering recommendations
    if (collaborative_) {
        auto recommendations = collaborative_->recommend(userId, topN);
        for (const auto& [itemId, score] : recommendations) {
            aggregatedScores[itemId] +=
                score * config_.collaborativeWeight;
            scoreCount[itemId]++;
        }
    }

    // Content-based filtering recommendations
    if (content_) {
        auto recommendations = content_->recommend(userId, topN);
        for (const auto& [itemId, score] : recommendations) {
            aggregatedScores[itemId] += score * config_.contentWeight;
            scoreCount[itemId]++;
        }
    }

    // Matrix factorization recommendations
    if (matrixFactorization_) {
        auto recommendations =
            matrixFactorization_->recommend(userId, topN);
        for (const auto& [itemId, score] : recommendations) {
            aggregatedScores[itemId] +=
                score * config_.matrixFactorizationWeight;
            scoreCount[itemId]++;
        }
    }

    // Average scores by number of engines that recommended the item
    for (auto& [_, score] : aggregatedScores) {
        auto count = scoreCount[_];
        if (count > 0) {
            score /= count;
        }
    }

    // Convert to sorted vector
    std::vector<std::pair<std::string, double>> result(
        aggregatedScores.begin(), aggregatedScores.end());
    std::sort(result.begin(), result.end(),
             [](const auto& a, const auto& b) {
                 return a.second > b.second;
             });

    if (result.size() > static_cast<size_t>(topN)) {
        result.resize(topN);
    }

    return result;
}

// Public API implementations
void HybridRecommender::addRating(const std::string& userId,
                                 const std::string& itemId, double rating) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addRating(userId, itemId, rating);
    }
    if (content_) {
        content_->addRating(userId, itemId, rating);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addRating(userId, itemId, rating);
    }
}

void HybridRecommender::addImplicitFeedback(const std::string& userId,
                                           const std::string& itemId) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addImplicitFeedback(userId, itemId);
    }
    if (content_) {
        content_->addImplicitFeedback(userId, itemId);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addImplicitFeedback(userId, itemId);
    }
}

void HybridRecommender::addRatings(
    const std::vector<std::tuple<std::string, std::string, double>>&
        ratings) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addRatings(ratings);
    }
    if (content_) {
        content_->addRatings(ratings);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addRatings(ratings);
    }
}

void HybridRecommender::addItem(
    const std::string& itemId, const std::vector<std::string>& features) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addItem(itemId, features);
    }
    if (content_) {
        content_->addItem(itemId, features);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addItem(itemId, features);
    }
}

void HybridRecommender::addItemFeature(const std::string& itemId,
                                      const std::string& featureId,
                                      double value) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addItemFeature(itemId, featureId, value);
    }
    if (content_) {
        content_->addItemFeature(itemId, featureId, value);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addItemFeature(itemId, featureId, value);
    }
}

void HybridRecommender::addItems(
    const std::vector<std::pair<std::string, std::vector<std::string>>>&
        items) {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->addItems(items);
    }
    if (content_) {
        content_->addItems(items);
    }
    if (matrixFactorization_) {
        matrixFactorization_->addItems(items);
    }
}

void HybridRecommender::train() {
    std::lock_guard lock(mtx_);
    spdlog::info("Training HybridRecommender with {} engines",
                getAvailableEngineCount());

    if (collaborative_) {
        collaborative_->train();
    }
    if (content_) {
        content_->train();
    }
    if (matrixFactorization_) {
        matrixFactorization_->train();
    }

    spdlog::info("HybridRecommender training completed");
}

auto HybridRecommender::recommend(const std::string& userId, int topN)
    -> std::vector<std::pair<std::string, double>> {
    std::lock_guard lock(mtx_);
    return aggregateRecommendations(userId, topN);
}

auto HybridRecommender::predictRating(const std::string& userId,
                                     const std::string& itemId) -> double {
    std::lock_guard lock(mtx_);
    double totalPrediction = 0.0;
    int engineCount = 0;

    if (collaborative_) {
        totalPrediction +=
            collaborative_->predictRating(userId, itemId) *
            config_.collaborativeWeight;
        engineCount++;
    }
    if (content_) {
        totalPrediction +=
            content_->predictRating(userId, itemId) * config_.contentWeight;
        engineCount++;
    }
    if (matrixFactorization_) {
        totalPrediction += matrixFactorization_->predictRating(userId, itemId) *
                          config_.matrixFactorizationWeight;
        engineCount++;
    }

    if (engineCount == 0) {
        return 2.5;  // Default middle rating
    }

    return totalPrediction / engineCount;
}

void HybridRecommender::saveModel(const std::string& path) {
    std::lock_guard lock(mtx_);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for saving: " + path);
    }

    // Save configuration
    file.write(reinterpret_cast<const char*>(&config_.collaborativeWeight),
              sizeof(double));
    file.write(reinterpret_cast<const char*>(&config_.contentWeight),
              sizeof(double));
    file.write(
        reinterpret_cast<const char*>(&config_.matrixFactorizationWeight),
        sizeof(double));
    bool normalizeWeights = config_.normalizeWeights;
    file.write(reinterpret_cast<const char*>(&normalizeWeights), sizeof(bool));

    file.close();

    // Save individual engines to separate files
    std::string basePath = path;
    size_t dotPos = basePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        basePath = basePath.substr(0, dotPos);
    }

    if (collaborative_) {
        collaborative_->saveModel(basePath + "_collaborative.bin");
    }
    if (content_) {
        content_->saveModel(basePath + "_content.bin");
    }
    if (matrixFactorization_) {
        matrixFactorization_->saveModel(basePath + "_mf.bin");
    }

    spdlog::info("HybridRecommender model saved to {}", path);
}

void HybridRecommender::loadModel(const std::string& path) {
    std::lock_guard lock(mtx_);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw ModelException("Failed to open file for loading: " + path);
    }

    // Load configuration
    file.read(reinterpret_cast<char*>(&config_.collaborativeWeight),
             sizeof(double));
    file.read(reinterpret_cast<char*>(&config_.contentWeight),
             sizeof(double));
    file.read(
        reinterpret_cast<char*>(&config_.matrixFactorizationWeight),
        sizeof(double));
    file.read(reinterpret_cast<char*>(&config_.normalizeWeights),
             sizeof(bool));

    file.close();

    // Load individual engines from separate files
    std::string basePath = path;
    size_t dotPos = basePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        basePath = basePath.substr(0, dotPos);
    }

    try {
        if (collaborative_) {
            collaborative_->loadModel(basePath + "_collaborative.bin");
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load collaborative filter model: {}", e.what());
    }

    try {
        if (content_) {
            content_->loadModel(basePath + "_content.bin");
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load content filter model: {}", e.what());
    }

    try {
        if (matrixFactorization_) {
            matrixFactorization_->loadModel(basePath + "_mf.bin");
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to load matrix factorization model: {}",
                    e.what());
    }

    spdlog::info("HybridRecommender model loaded from {}", path);
}

auto HybridRecommender::getStats() const -> std::string {
    std::lock_guard lock(mtx_);
    std::stringstream ss;
    ss << "Hybrid Recommender Statistics:\n"
       << "  Active Engines: " << getAvailableEngineCount() << "\n"
       << "  Collaborative Weight: " << config_.collaborativeWeight << "\n"
       << "  Content Weight: " << config_.contentWeight << "\n"
       << "  Matrix Factorization Weight: "
       << config_.matrixFactorizationWeight << "\n";

    if (collaborative_) {
        ss << "  Collaborative Filtering:\n";
        auto stats = collaborative_->getStats();
        for (const auto& line : {stats}) {
            for (char c : line) {
                ss << c;
                if (c == '\n') ss << "    ";
            }
        }
        ss << "\n";
    }

    if (content_) {
        ss << "  Content-Based Filtering:\n";
        auto stats = content_->getStats();
        for (const auto& line : {stats}) {
            for (char c : line) {
                ss << c;
                if (c == '\n') ss << "    ";
            }
        }
        ss << "\n";
    }

    if (matrixFactorization_) {
        ss << "  Matrix Factorization:\n";
        auto stats = matrixFactorization_->getStats();
        for (const auto& line : {stats}) {
            for (char c : line) {
                ss << c;
                if (c == '\n') ss << "    ";
            }
        }
    }

    return ss.str();
}

void HybridRecommender::clear() {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->clear();
    }
    if (content_) {
        content_->clear();
    }
    if (matrixFactorization_) {
        matrixFactorization_->clear();
    }
    spdlog::info("All data cleared from HybridRecommender");
}

void HybridRecommender::optimize() {
    std::lock_guard lock(mtx_);
    if (collaborative_) {
        collaborative_->optimize();
    }
    if (content_) {
        content_->optimize();
    }
    if (matrixFactorization_) {
        matrixFactorization_->optimize();
    }
    spdlog::info("HybridRecommender optimization completed");
}

void HybridRecommender::setWeights(double collaborativeWeight,
                                  double contentWeight,
                                  double matrixFactorizationWeight) {
    std::lock_guard lock(mtx_);
    config_.collaborativeWeight = collaborativeWeight;
    config_.contentWeight = contentWeight;
    config_.matrixFactorizationWeight = matrixFactorizationWeight;

    if (config_.normalizeWeights) {
        normalizeWeights();
    }

    spdlog::info("HybridRecommender weights updated (CF: {:.2f}, Content: "
                "{:.2f}, MF: {:.2f})",
                config_.collaborativeWeight, config_.contentWeight,
                config_.matrixFactorizationWeight);
}

}  // namespace lithium::target::recommendation
