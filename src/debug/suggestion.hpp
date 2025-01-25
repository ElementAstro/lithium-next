/**
 * @file suggestion.hpp
 * @brief Command suggestion engine
 *
 * This file contains the definition and implementation of a command suggestion
 * engine.
 */

#ifndef LITHIUM_DEBUG_SUGGESTION_HPP
#define LITHIUM_DEBUG_SUGGESTION_HPP

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lithium::debug {

/**
 * @brief Exception class for SuggestionEngine errors.
 */
class SuggestionException : public std::runtime_error {
public:
    /**
     * @brief Constructor for SuggestionException.
     * @param message The error message.
     */
    explicit SuggestionException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief 建议详细信息结构体
 */
struct SuggestionDetail {
    std::string suggestion;  ///< 建议的文本
    float confidence;        ///< 置信度分数
    float editDistance;      ///< 编辑距离
    std::string matchType;   ///< 匹配类型描述
};

/**
 * @brief SuggestionEngine class for generating command suggestions.
 */
class SuggestionEngine {
public:
    /**
     * @brief Enumeration for match types.
     */
    enum class MatchType {
        Prefix,    ///< Match by prefix.
        Substring  ///< Match by substring.
    };

    /**
     * @brief Constructor for SuggestionEngine.
     * @param dataset The initial dataset for suggestions.
     * @param maxSuggestions The maximum number of suggestions to return.
     */
    explicit SuggestionEngine(const std::vector<std::string>& dataset,
                              int maxSuggestions = 5);

    /**
     * @brief Destructor for SuggestionEngine.
     */
    ~SuggestionEngine();

    // Disable copy
    SuggestionEngine(const SuggestionEngine&) = delete;
    SuggestionEngine& operator=(const SuggestionEngine&) = delete;

    // Enable move
    SuggestionEngine(SuggestionEngine&&) noexcept;
    SuggestionEngine& operator=(SuggestionEngine&&) noexcept;

    /**
     * @brief Generate suggestions based on the input.
     * @param input The input string to match against.
     * @param matchType The type of match to perform (prefix or substring).
     * @return A vector of suggested strings.
     */
    auto suggest(std::string_view input,
                 MatchType matchType = MatchType::Prefix)
        -> std::vector<std::string>;

    /**
     * @brief Update the dataset with new items.
     * @param newItems The new items to add to the dataset.
     */
    void updateDataset(const std::vector<std::string>& newItems);

    /**
     * @brief Type definition for filter functions.
     */
    using FilterFunction = std::function<bool(const std::string&)>;

    /**
     * @brief Set the weight for a specific item.
     * @param item The item to set the weight for.
     * @param weight The weight to set.
     */
    void setWeight(const std::string& item, float weight);

    /**
     * @brief Add a filter function to the engine.
     * @param filter The filter function to add.
     */
    void addFilter(FilterFunction filter);

    /**
     * @brief Clear the suggestion cache.
     */
    void clearCache();

    /**
     * @brief Set the fuzzy match threshold
     * @param threshold The threshold range [0.0, 1.0]
     * @throw SuggestionException if the threshold is out of range
     */
    void setFuzzyMatchThreshold(float threshold);

    /**
     * @brief Optimize suggestions based on user history
     * @param history List of user history commands
     */
    void updateFromHistory(const std::vector<std::string>& history);

    /**
     * @brief Get detailed information about suggestions
     * @param input Input string
     * @return List of suggestion details
     */
    [[nodiscard]] auto getSuggestionDetails(std::string_view input)
        -> std::vector<SuggestionDetail>;

private:
    class Implementation;
    std::unique_ptr<Implementation> impl_;  ///< Pimpl for encapsulation
};

}  // namespace lithium::debug

#endif  // LITHIUM_DEBUG_SUGGESTION_HPP
