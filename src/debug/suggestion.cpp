/**
 * @file suggestion.cpp
 * @brief Command suggestion engine implementation
 */

#include "suggestion.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace lithium::debug {

class SuggestionEngine::Implementation {
public:
    Implementation(const std::vector<std::string>& dataset,
                   const SuggestionConfig& config)
        : dataset_(dataset), config_(config) {
        buildIndex();
        stats_.datasetSize = dataset.size();
    }

    std::vector<std::string> suggest(std::string_view input,
                                     MatchType matchType) {
        auto startTime = std::chrono::high_resolution_clock::now();

        try {
            if (input.empty()) {
                throw SuggestionException("Empty input string");
            }

            std::lock_guard<std::mutex> lock(mutex_);
            stats_.totalSuggestionCalls++;

            // Check cache
            std::string cacheKey = makeCacheKey(input, matchType);
            if (auto iter = cache_.find(cacheKey); iter != cache_.end()) {
                stats_.cacheHits++;
                auto endTime = std::chrono::high_resolution_clock::now();
                stats_.totalProcessingTime +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        endTime - startTime);
                return iter->second;
            }

            stats_.cacheMisses++;

            // Process new suggestions
            std::vector<std::string> suggestions =
                processSuggestions(input, matchType);

            // Update cache
            if (cache_.size() >= config_.maxCacheSize) {
                // Remove 20% of the oldest entries when cache is full
                size_t toRemove = config_.maxCacheSize / 5;
                if (toRemove < 1)
                    toRemove = 1;

                std::vector<std::string> keys;
                keys.reserve(cache_.size());
                for (const auto& item : cache_) {
                    keys.push_back(item.first);
                }

                // Sort by access time and remove oldest
                std::sort(keys.begin(), keys.end(),
                          [this](const std::string& a, const std::string& b) {
                              return cacheAccessTime_[a] < cacheAccessTime_[b];
                          });

                for (size_t i = 0; i < toRemove && i < keys.size(); i++) {
                    cache_.erase(keys[i]);
                    cacheAccessTime_.erase(keys[i]);
                }
            }

            // Add to cache
            cache_[cacheKey] = suggestions;
            cacheAccessTime_[cacheKey] = std::chrono::steady_clock::now();
            stats_.cacheSize = cache_.size();

            auto endTime = std::chrono::high_resolution_clock::now();
            stats_.totalProcessingTime +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(endTime -
                                                                     startTime);

            return suggestions;
        } catch (const std::exception& e) {
            auto endTime = std::chrono::high_resolution_clock::now();
            stats_.totalProcessingTime +=
                std::chrono::duration_cast<std::chrono::nanoseconds>(endTime -
                                                                     startTime);
            throw SuggestionException(std::string("Suggestion error: ") +
                                      e.what());
        }
    }

    std::vector<std::string> processSuggestions(std::string_view input,
                                                MatchType matchType) {
        std::string processedInput = config_.caseSensitive
                                         ? std::string(input)
                                         : convertToLowerCase(input);

        struct ScoredItem {
            std::string item;
            float score;
        };
        std::vector<ScoredItem> scoredItems;

        size_t filteredItems = 0;

        // Match and score items
        for (const auto& [lowerItem, originalItem] : index_) {
            bool matches = false;
            float matchScore = 0.0F;

            const std::string& compareItem =
                config_.caseSensitive ? originalItem : lowerItem;

            switch (matchType) {
                case MatchType::Prefix:
                    if (compareItem.starts_with(processedInput)) {
                        matches = true;
                        matchScore =
                            1.0F + (1.0F - static_cast<float>(
                                               processedInput.length()) /
                                               compareItem.length());
                    }
                    break;

                case MatchType::Substring:
                    if (size_t pos = compareItem.find(processedInput);
                        pos != std::string::npos) {
                        matches = true;
                        matchScore =
                            0.9F + (0.1F * (1.0F - static_cast<float>(pos) /
                                                       compareItem.length()));
                    }
                    break;

                case MatchType::Fuzzy: {
                    int distance =
                        calculateEditDistance(processedInput, compareItem);
                    float maxAllowedDistance = config_.maxEditDistance;
                    if (distance <= maxAllowedDistance) {
                        matches = true;
                        matchScore =
                            0.8F * (1.0F - static_cast<float>(distance) /
                                               (maxAllowedDistance + 1));
                    }
                    break;
                }

                case MatchType::Regex:
                    try {
                        std::regex pattern(
                            std::string(processedInput),
                            config_.caseSensitive
                                ? std::regex::ECMAScript
                                : std::regex::ECMAScript | std::regex::icase);
                        if (std::regex_search(compareItem, pattern)) {
                            matches = true;
                            matchScore = 0.7F;
                        }
                    } catch (const std::regex_error&) {
                        // Invalid regex pattern, skip this match type
                    }
                    break;
            }

            if (matches) {
                // Apply additional scoring factors
                float baseScore =
                    calculateAdvancedScore(processedInput, compareItem);
                float editScore =
                    1.0F /
                    (calculateEditDistance(processedInput, compareItem) + 1.0F);

                // Apply weights
                float weight = weights_.contains(originalItem)
                                   ? weights_[originalItem]
                                   : 1.0F;

                // History weight
                float historyWeight = 1.0F;
                if (auto it = historyFrequency_.find(originalItem);
                    it != historyFrequency_.end()) {
                    historyWeight +=
                        static_cast<float>(it->second) *
                        config_.historyWeightFactor /
                        std::max(1.0F, static_cast<float>(totalHistoryItems_));
                }

                float finalScore = (baseScore + matchScore) * editScore *
                                   weight * historyWeight;

                // Apply filters
                bool passFilter = true;
                for (const auto& filter : filters_) {
                    if (!filter(originalItem)) {
                        passFilter = false;
                        filteredItems++;
                        break;
                    }
                }

                if (passFilter) {
                    scoredItems.push_back({originalItem, finalScore});
                }
            }
        }

        // Sort by score and select top results
        std::sort(scoredItems.begin(), scoredItems.end(),
                  [](const ScoredItem& a, const ScoredItem& b) {
                      return a.score > b.score;
                  });

        std::vector<std::string> suggestions;
        suggestions.reserve(std::min(
            scoredItems.size(), static_cast<size_t>(config_.maxSuggestions)));

        for (size_t i = 0;
             i < scoredItems.size() &&
             suggestions.size() < static_cast<size_t>(config_.maxSuggestions);
             i++) {
            suggestions.push_back(scoredItems[i].item);
        }

        stats_.itemsFiltered += filteredItems;

        return suggestions;
    }

    void updateDataset(const std::vector<std::string>& newItems) {
        std::lock_guard<std::mutex> lock(mutex_);
        dataset_.insert(dataset_.end(), newItems.begin(), newItems.end());
        buildIndex();
        stats_.datasetSize = dataset_.size();
    }

    void setDataset(const std::vector<std::string>& newDataset) {
        std::lock_guard<std::mutex> lock(mutex_);
        dataset_ = newDataset;
        buildIndex();
        stats_.datasetSize = dataset_.size();
    }

    void buildIndex() {
        index_.clear();
        for (const auto& item : dataset_) {
            if (config_.caseSensitive) {
                index_.emplace(item, item);
            } else {
                std::string itemLower = convertToLowerCase(item);
                index_.emplace(itemLower, item);
            }
        }
    }

    void setWeight(const std::string& item, float weight) {
        std::lock_guard<std::mutex> lock(mutex_);
        weights_[item] = weight;
    }

    void addFilter(FilterFunction filter) {
        std::lock_guard<std::mutex> lock(mutex_);
        filters_.push_back(std::move(filter));
    }

    void clearFilters() {
        std::lock_guard<std::mutex> lock(mutex_);
        filters_.clear();
    }

    void clearCache() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        cacheAccessTime_.clear();
        stats_.cacheSize = 0;
    }

    void setFuzzyMatchThreshold(float threshold) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.fuzzyMatchThreshold = threshold;
    }

    void setMaxSuggestions(int maxSuggestions) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.maxSuggestions = maxSuggestions;
    }

    void setCaseSensitivity(bool caseSensitive) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (config_.caseSensitive != caseSensitive) {
            config_.caseSensitive = caseSensitive;
            buildIndex();  // Need to rebuild index with new case sensitivity
        }
    }

    void updateFromHistory(const std::vector<std::string>& history) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Reset history counters
        historyFrequency_.clear();
        totalHistoryItems_ = 0;

        // Count frequency of each command in history
        for (const auto& cmd : history) {
            historyFrequency_[cmd]++;
            totalHistoryItems_++;
        }

        // Clear cache as weights have changed
        clearCache();
    }

    std::vector<SuggestionDetail> getSuggestionDetails(std::string_view input,
                                                       MatchType matchType) {
        std::vector<SuggestionDetail> details;
        std::string processedInput = config_.caseSensitive
                                         ? std::string(input)
                                         : convertToLowerCase(input);

        std::lock_guard<std::mutex> lock(mutex_);

        // Process all items in the index
        for (const auto& [lowerItem, originalItem] : index_) {
            bool matches = false;
            float matchScore = 0.0F;
            std::string matchTypeStr = "None";

            const std::string& compareItem =
                config_.caseSensitive ? originalItem : lowerItem;

            // Check for matches based on match type
            switch (matchType) {
                case MatchType::Prefix:
                    if (compareItem.starts_with(processedInput)) {
                        matches = true;
                        matchScore = 1.0F;
                        matchTypeStr = "Prefix";
                    }
                    break;

                case MatchType::Substring:
                    if (compareItem.find(processedInput) != std::string::npos) {
                        matches = true;
                        matchScore = 0.8F;
                        matchTypeStr = "Substring";
                    }
                    break;

                case MatchType::Fuzzy: {
                    int distance =
                        calculateEditDistance(processedInput, compareItem);
                    float maxAllowedDistance = config_.maxEditDistance;
                    if (distance <= maxAllowedDistance) {
                        matches = true;
                        matchScore =
                            0.6F * (1.0F - static_cast<float>(distance) /
                                               (maxAllowedDistance + 1));
                        matchTypeStr = "Fuzzy";
                    }
                    break;
                }

                case MatchType::Regex:
                    try {
                        std::regex pattern(
                            std::string(processedInput),
                            config_.caseSensitive
                                ? std::regex::ECMAScript
                                : std::regex::ECMAScript | std::regex::icase);
                        if (std::regex_search(compareItem, pattern)) {
                            matches = true;
                            matchScore = 0.7F;
                            matchTypeStr = "Regex";
                        }
                    } catch (const std::regex_error&) {
                        // Invalid regex pattern, skip
                    }
                    break;
            }

            if (matches) {
                float editDistance = static_cast<float>(
                    calculateEditDistance(processedInput, compareItem));

                float baseScore =
                    calculateAdvancedScore(processedInput, compareItem);
                float editScore = 1.0F / (editDistance + 1.0F);

                // Apply weights
                float weight = weights_.contains(originalItem)
                                   ? weights_[originalItem]
                                   : 1.0F;

                // History weight
                float historyWeight = 1.0F;
                if (auto it = historyFrequency_.find(originalItem);
                    it != historyFrequency_.end()) {
                    historyWeight +=
                        static_cast<float>(it->second) *
                        config_.historyWeightFactor /
                        std::max(1.0F, static_cast<float>(totalHistoryItems_));
                }

                float confidence = (baseScore + matchScore) * editScore *
                                   weight * historyWeight;

                // Apply filters
                bool passFilter = true;
                for (const auto& filter : filters_) {
                    if (!filter(originalItem)) {
                        passFilter = false;
                        break;
                    }
                }

                if (passFilter) {
                    details.push_back({.suggestion = originalItem,
                                       .confidence = confidence,
                                       .editDistance = editDistance,
                                       .matchType = matchTypeStr});
                }
            }
        }

        // Sort by confidence (highest first)
        std::sort(details.begin(), details.end(), std::greater<>());

        // Limit to max suggestions
        if (details.size() > static_cast<size_t>(config_.maxSuggestions)) {
            details.resize(config_.maxSuggestions);
        }

        return details;
    }

    SuggestionStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    std::string getStatisticsText() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ostringstream oss;
        oss << "SuggestionEngine Statistics:\n";
        oss << "- Dataset size: " << stats_.datasetSize << " items\n";
        oss << "- Cache size: " << stats_.cacheSize << "/"
            << config_.maxCacheSize << " entries\n";
        oss << "- Total calls: " << stats_.totalSuggestionCalls << "\n";
        oss << "- Cache hits: " << stats_.cacheHits << "\n";
        oss << "- Cache misses: " << stats_.cacheMisses << "\n";

        double hitRate = stats_.totalSuggestionCalls > 0
                             ? static_cast<double>(stats_.cacheHits) * 100.0 /
                                   stats_.totalSuggestionCalls
                             : 0.0;
        oss << "- Cache hit rate: " << hitRate << "%\n";

        double avgTime =
            stats_.totalSuggestionCalls > 0
                ? stats_.totalProcessingTime.count() /
                      static_cast<double>(stats_.totalSuggestionCalls) / 1000.0
                : 0.0;
        oss << "- Average processing time: " << avgTime << " μs\n";

        oss << "- Items filtered: " << stats_.itemsFiltered << "\n";

        return oss.str();
    }

    void resetStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = SuggestionStats{};
        stats_.datasetSize = dataset_.size();
        stats_.cacheSize = cache_.size();
    }

    void updateConfig(const SuggestionConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        bool rebuildIndexNeeded = config_.caseSensitive != config.caseSensitive;

        config_ = config;

        if (rebuildIndexNeeded) {
            buildIndex();
        }

        // Clear cache if configuration changes might affect results
        clearCache();
    }

    SuggestionConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

private:
    std::string makeCacheKey(std::string_view input,
                             MatchType matchType) const {
        return std::string(input) + ":" +
               std::to_string(static_cast<int>(matchType)) + ":" +
               (config_.caseSensitive ? "1" : "0");
    }

    std::string convertToLowerCase(std::string_view text) const {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    float calculateAdvancedScore(const std::string& input,
                                 const std::string& item) const {
        float score = 0.0F;

        // Exact matches get highest priority
        if (item == input) {
            score += 10.0F;
            return score;  // Early return for exact matches
        }

        // Prefix matches are favored
        if (item.starts_with(input)) {
            score += 5.0F;
        }

        // Early occurrence of input in item is favored
        size_t pos = item.find(input);
        if (pos != std::string::npos) {
            score += 3.0F * (1.0F - static_cast<float>(pos) / item.size());
        }

        // Length similarity is a positive factor
        float lengthRatio =
            static_cast<float>(std::min(item.size(), input.size())) /
            static_cast<float>(std::max(item.size(), input.size()));
        score += 2.0F * lengthRatio;

        return score;
    }

    int calculateEditDistance(const std::string& s1,
                              const std::string& s2) const {
        const size_t len1 = s1.size();
        const size_t len2 = s2.size();

        // Early exit for empty strings
        if (len1 == 0)
            return static_cast<int>(len2);
        if (len2 == 0)
            return static_cast<int>(len1);

        // Use two row vectors instead of full matrix (space optimization)
        std::vector<int> prevRow(len2 + 1);
        std::vector<int> currRow(len2 + 1);

        // Initialize first row
        for (size_t j = 0; j <= len2; ++j) {
            prevRow[j] = static_cast<int>(j);
        }

        // Fill the matrix
        for (size_t i = 1; i <= len1; ++i) {
            currRow[0] = static_cast<int>(i);

            for (size_t j = 1; j <= len2; ++j) {
                int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

                currRow[j] = std::min({
                    prevRow[j] + 1,        // Deletion
                    currRow[j - 1] + 1,    // Insertion
                    prevRow[j - 1] + cost  // Substitution
                });

                // Consider transposition for adjacent characters
                if (config_.useTransposition && i > 1 && j > 1 &&
                    s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                    currRow[j] = std::min(currRow[j], prevRow[j - 2] + 1);
                }
            }

            std::swap(prevRow, currRow);
        }

        return prevRow[len2];
    }

    mutable std::mutex mutex_;  // For thread safety
    std::vector<std::string> dataset_;
    std::unordered_map<std::string, std::string> index_;
    std::unordered_map<std::string, float> weights_;
    std::vector<FilterFunction> filters_;
    std::unordered_map<std::string, std::vector<std::string>> cache_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        cacheAccessTime_;
    std::unordered_map<std::string, int> historyFrequency_;
    SuggestionConfig config_;
    SuggestionStats stats_;
    size_t totalHistoryItems_ = 0;
};

// ==================== SuggestionEngine Implementation ====================

// Constructor implementations
SuggestionEngine::SuggestionEngine(const std::vector<std::string>& dataset,
                                   const SuggestionConfig& config)
    : impl_(std::make_unique<Implementation>(dataset, config)) {}

SuggestionEngine::SuggestionEngine(const std::vector<std::string>& dataset,
                                   int maxSuggestions)
    : impl_(std::make_unique<Implementation>(
          dataset, SuggestionConfig{.maxSuggestions = maxSuggestions})) {}

SuggestionEngine::~SuggestionEngine() = default;

// Move operations
SuggestionEngine::SuggestionEngine(SuggestionEngine&&) noexcept = default;
SuggestionEngine& SuggestionEngine::operator=(SuggestionEngine&&) noexcept =
    default;

// Delegating methods
auto SuggestionEngine::suggest(std::string_view input, MatchType matchType)
    -> std::vector<std::string> {
    return impl_->suggest(input, matchType);
}

void SuggestionEngine::updateDataset(const std::vector<std::string>& newItems) {
    impl_->updateDataset(newItems);
}

void SuggestionEngine::setDataset(const std::vector<std::string>& newDataset) {
    impl_->setDataset(newDataset);
}

void SuggestionEngine::setWeight(const std::string& item, float weight) {
    impl_->setWeight(item, weight);
}

void SuggestionEngine::addFilter(FilterFunction filter) {
    impl_->addFilter(std::move(filter));
}

void SuggestionEngine::clearFilters() { impl_->clearFilters(); }

void SuggestionEngine::clearCache() { impl_->clearCache(); }

void SuggestionEngine::setFuzzyMatchThreshold(float threshold) {
    if (threshold < 0.0F || threshold > 1.0F) {
        throw SuggestionException(
            "Fuzzy match threshold must be between 0.0 and 1.0");
    }
    impl_->setFuzzyMatchThreshold(threshold);
}

void SuggestionEngine::setMaxSuggestions(int maxSuggestions) {
    if (maxSuggestions <= 0) {
        throw SuggestionException("Max suggestions must be greater than 0");
    }
    impl_->setMaxSuggestions(maxSuggestions);
}

void SuggestionEngine::setCaseSensitivity(bool caseSensitive) {
    impl_->setCaseSensitivity(caseSensitive);
}

void SuggestionEngine::updateFromHistory(
    const std::vector<std::string>& history) {
    impl_->updateFromHistory(history);
}

auto SuggestionEngine::getSuggestionDetails(std::string_view input,
                                            MatchType matchType)
    -> std::vector<SuggestionDetail> {
    return impl_->getSuggestionDetails(input, matchType);
}

SuggestionStats SuggestionEngine::getStats() const { return impl_->getStats(); }

std::string SuggestionEngine::getStatisticsText() const {
    return impl_->getStatisticsText();
}

void SuggestionEngine::resetStats() { impl_->resetStats(); }

void SuggestionEngine::updateConfig(const SuggestionConfig& config) {
    impl_->updateConfig(config);
}

SuggestionConfig SuggestionEngine::getConfig() const {
    return impl_->getConfig();
}

}  // namespace lithium::debug
