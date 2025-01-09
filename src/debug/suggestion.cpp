/**
 * @file suggestion.cpp
 * @brief Command suggestion engine
 */

#include "suggestion.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <unordered_map>

namespace lithium::debug {

class SuggestionEngine::Implementation {
public:
    Implementation(const std::vector<std::string>& dataset, int maxSuggestions)
        : dataset_(dataset), maxSuggestions_(maxSuggestions) {
        buildIndex();
    }

    std::vector<std::string> suggest(std::string_view input, MatchType matchType);
    void updateDataset(const std::vector<std::string>& newItems);
    void setWeight(const std::string& item, float weight);
    void addFilter(FilterFunction filter);
    void clearCache();

private:
    void buildIndex();
    bool matches(const std::string& input, const std::string& item,
                MatchType matchType);
    int calculateScore(const std::string& input, const std::string& item);
    int calculateEditDistance(const std::string& s1, const std::string& s2);

    std::unordered_map<std::string, std::string> index_;
    std::vector<std::string> dataset_;
    int maxSuggestions_;
    std::mutex mutex_;
    std::unordered_map<std::string, float> weights_;
    std::vector<FilterFunction> filters_;
    std::unordered_map<std::string, std::vector<std::string>> cache_;
    static constexpr size_t MAX_CACHE_SIZE = 1000;
};

// 构造函数和析构函数
SuggestionEngine::SuggestionEngine(const std::vector<std::string>& dataset,
                                 int maxSuggestions)
    : impl_(std::make_unique<Implementation>(dataset, maxSuggestions)) {}

SuggestionEngine::~SuggestionEngine() = default;

// 移动构造和赋值
SuggestionEngine::SuggestionEngine(SuggestionEngine&&) noexcept = default;
SuggestionEngine& SuggestionEngine::operator=(SuggestionEngine&&) noexcept = default;

// 委托函数
auto SuggestionEngine::suggest(std::string_view input, MatchType matchType)
    -> std::vector<std::string> {
    return impl_->suggest(input, matchType);
}

void SuggestionEngine::updateDataset(const std::vector<std::string>& newItems) {
    impl_->updateDataset(newItems);
}

void SuggestionEngine::setWeight(const std::string& item, float weight) {
    impl_->setWeight(item, weight);
}

void SuggestionEngine::addFilter(FilterFunction filter) {
    impl_->addFilter(std::move(filter));
}

void SuggestionEngine::clearCache() {
    impl_->clearCache();
}

// Implementation 类方法实现
auto SuggestionEngine::Implementation::suggest(std::string_view input, MatchType matchType)
    -> std::vector<std::string> {
    try {
        if (input.empty()) {
            throw SuggestionException("Empty input string");
        }

        // 检查缓存
        std::string cacheKey =
            std::string(input) + std::to_string(static_cast<int>(matchType));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (auto iter = cache_.find(cacheKey); iter != cache_.end()) {
                return iter->second;
            }
        }

        std::vector<std::string> suggestions;
        std::string inputLower(input.size(), '\0');
        std::transform(input.begin(), input.end(), inputLower.begin(),
                       ::tolower);

        struct alignas(64) ScoredItem {
            std::string item;
            float score;
        };
        std::vector<ScoredItem> scoredItems;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [lowerItem, originalItem] : index_) {
                if (matches(inputLower, lowerItem, matchType)) {
                    float baseScore = calculateScore(inputLower, lowerItem);

                    // 应用编辑距离
                    float editScore =
                        1.0F /
                        (calculateEditDistance(inputLower, lowerItem) + 1.0F);

                    // 应用权重
                    float weight = weights_.contains(originalItem)
                                       ? weights_[originalItem]
                                       : 1.0F;

                    float finalScore = baseScore * editScore * weight;

                    // 应用过滤器
                    bool passFilter = true;
                    for (const auto& filter : filters_) {
                        if (!filter(originalItem)) {
                            passFilter = false;
                            break;
                        }
                    }

                    if (passFilter) {
                        scoredItems.push_back({originalItem, finalScore});
                    }
                }
            }
        }

        // 排序并选择最佳结果
        std::sort(scoredItems.begin(), scoredItems.end(),
                  [](const ScoredItem& itemA, const ScoredItem& itemB) {
                      return itemA.score > itemB.score;
                  });

        suggestions.reserve(
            std::min(scoredItems.size(), static_cast<size_t>(maxSuggestions_)));
        for (size_t index = 0;
             index < scoredItems.size() &&
             suggestions.size() < static_cast<size_t>(maxSuggestions_);
             ++index) {
            suggestions.push_back(scoredItems[index].item);
        }

        // 更新缓存
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (cache_.size() >= MAX_CACHE_SIZE) {
                cache_.clear();
            }
            cache_[cacheKey] = suggestions;
        }

        return suggestions;
    } catch (const std::exception& e) {
        throw SuggestionException(std::string("Suggestion error: ") + e.what());
    }
}

void SuggestionEngine::Implementation::updateDataset(const std::vector<std::string>& newItems) {
    std::lock_guard<std::mutex> lock(mutex_);
    dataset_.insert(dataset_.end(), newItems.begin(), newItems.end());
    buildIndex();
}

void SuggestionEngine::Implementation::buildIndex() {
    index_.clear();
    for (const auto& item : dataset_) {
        std::string itemLower(item.size(), '\0');
        std::transform(item.begin(), item.end(), itemLower.begin(), ::tolower);
        index_.emplace(itemLower, item);
    }
}

bool SuggestionEngine::Implementation::matches(const std::string& input,
                               const std::string& item,
                               MatchType matchType) {
    switch (matchType) {
        case MatchType::Prefix:
            return item.starts_with(input);
        case MatchType::Substring:
            return item.find(input) != std::string::npos;
    }
    return false;
}

int SuggestionEngine::Implementation::calculateScore(const std::string& input,
                                      const std::string& item) {
    int score = 0;
    size_t inputPos = 0;
    for (char character : item) {
        if (inputPos < input.size() && character == input[inputPos]) {
            score += 2;
            inputPos++;
        } else {
            score -= 1;
        }
    }
    return score;
}

int SuggestionEngine::Implementation::calculateEditDistance(const std::string& str1,
                                             const std::string& str2) {
    const size_t LENGTH1 = str1.length();
    const size_t LENGTH2 = str2.length();
    std::vector<std::vector<int>> dp(LENGTH1 + 1,
                                     std::vector<int>(LENGTH2 + 1));

    for (size_t index1 = 0; index1 <= LENGTH1; index1++) {
        dp[index1][0] = static_cast<int>(index1);
    }
    for (size_t index2 = 0; index2 <= LENGTH2; index2++) {
        dp[0][index2] = static_cast<int>(index2);
    }

    for (size_t index1 = 1; index1 <= LENGTH1; index1++) {
        for (size_t index2 = 1; index2 <= LENGTH2; index2++) {
            if (str1[index1 - 1] == str2[index2 - 1]) {
                dp[index1][index2] = dp[index1 - 1][index2 - 1];
            } else {
                dp[index1][index2] =
                    1 + std::min({dp[index1 - 1][index2],        // 删除
                                  dp[index1][index2 - 1],        // 插入
                                  dp[index1 - 1][index2 - 1]});  // 替换
            }
        }
    }
    return dp[LENGTH1][LENGTH2];
}

void SuggestionEngine::Implementation::setWeight(const std::string& item, float weight) {
    std::lock_guard<std::mutex> lock(mutex_);
    weights_[item] = weight;
}

void SuggestionEngine::Implementation::addFilter(FilterFunction filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    filters_.push_back(std::move(filter));
}

void SuggestionEngine::Implementation::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}

}  // namespace lithium::debug