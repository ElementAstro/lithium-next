/**
 * @file suggestion.hpp
 * @brief Command suggestion engine
 *
 * This file contains the definition and implementation of a command suggestion
 * engine with advanced matching and configuration options.
 */

#ifndef LITHIUM_DEBUG_SUGGESTION_HPP
#define LITHIUM_DEBUG_SUGGESTION_HPP

#include <chrono>
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
 * @brief Configuration options for the suggestion engine
 */
struct SuggestionConfig {
    int maxSuggestions = 5;            ///< Maximum suggestions to return
    float fuzzyMatchThreshold = 0.5F;  ///< Threshold for fuzzy matching
    size_t maxCacheSize = 1000;        ///< Maximum cache size
    float historyWeightFactor = 1.5F;  ///< Weight factor for history
    bool caseSensitive = false;        ///< Case sensitivity toggle
    bool useTransposition = true;      ///< Use transposition in edit distance
    int maxEditDistance = 3;           ///< Maximum edit distance for matches
};

/**
 * @brief Suggestion detail information structure
 */
struct SuggestionDetail {
    std::string suggestion;  ///< Suggested text
    float confidence;        ///< Confidence score (0.0-1.0)
    float editDistance;      ///< Edit distance value
    std::string matchType;   ///< Match type description

    // Comparison operators for sorting
    bool operator<(const SuggestionDetail& other) const {
        return confidence < other.confidence;
    }

    bool operator>(const SuggestionDetail& other) const {
        return confidence > other.confidence;
    }
};

/**
 * @brief Statistics for the suggestion engine
 */
struct SuggestionStats {
    size_t totalSuggestionCalls = 0;  ///< Total calls to suggest()
    size_t cacheHits = 0;             ///< Cache hit count
    size_t cacheMisses = 0;           ///< Cache miss count
    std::chrono::nanoseconds totalProcessingTime{0};  ///< Total processing time
    size_t itemsFiltered = 0;                         ///< Items filtered out
    size_t datasetSize = 0;                           ///< Current dataset size
    size_t cacheSize = 0;                             ///< Current cache size
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
        Prefix,     ///< Match by prefix
        Substring,  ///< Match by substring
        Fuzzy,      ///< Match using fuzzy algorithm
        Regex       ///< Match using regular expressions
    };

    /**
     * @brief Constructor for SuggestionEngine.
     * @param dataset The initial dataset for suggestions.
     * @param config Configuration options for the engine.
     */
    explicit SuggestionEngine(const std::vector<std::string>& dataset,
                              const SuggestionConfig& config = {});

    /**
     * @brief Constructor with maxSuggestions parameter (for backward
     * compatibility)
     * @param dataset The initial dataset for suggestions.
     * @param maxSuggestions The maximum number of suggestions to return.
     */
    explicit SuggestionEngine(const std::vector<std::string>& dataset,
                              int maxSuggestions);

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
     * @param matchType The type of match to perform.
     * @return A vector of suggested strings.
     */
    [[nodiscard]] auto suggest(std::string_view input,
                               MatchType matchType = MatchType::Prefix)
        -> std::vector<std::string>;

    /**
     * @brief Update the dataset with new items.
     * @param newItems The new items to add to the dataset.
     */
    void updateDataset(const std::vector<std::string>& newItems);

    /**
     * @brief Replace the entire dataset with new items.
     * @param newDataset The new dataset.
     */
    void setDataset(const std::vector<std::string>& newDataset);

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
     * @brief Clear all filter functions.
     */
    void clearFilters();

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
     * @brief Set the maximum number of suggestions to return
     * @param maxSuggestions The maximum number of suggestions
     */
    void setMaxSuggestions(int maxSuggestions);

    /**
     * @brief Set case sensitivity for matching
     * @param caseSensitive True to enable case sensitive matching
     */
    void setCaseSensitivity(bool caseSensitive);

    /**
     * @brief Optimize suggestions based on user history
     * @param history List of user history commands
     */
    void updateFromHistory(const std::vector<std::string>& history);

    /**
     * @brief Get detailed information about suggestions
     * @param input Input string
     * @param matchType The type of match to perform
     * @return List of suggestion details
     */
    [[nodiscard]] auto getSuggestionDetails(
        std::string_view input, MatchType matchType = MatchType::Prefix)
        -> std::vector<SuggestionDetail>;

    /**
     * @brief Get statistics about the suggestion engine
     * @return Statistics structure
     */
    [[nodiscard]] SuggestionStats getStats() const;

    /**
     * @brief Get a textual representation of statistics
     * @return String containing statistics information
     */
    [[nodiscard]] std::string getStatisticsText() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStats();

    /**
     * @brief Update engine configuration
     * @param config New configuration options
     */
    void updateConfig(const SuggestionConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    [[nodiscard]] SuggestionConfig getConfig() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> impl_;  ///< Pimpl for encapsulation
};

}  // namespace lithium::debug

#endif  // LITHIUM_DEBUG_SUGGESTION_HPP