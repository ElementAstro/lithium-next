#ifndef LITHIUM_TASK_CELESTIAL_SEARCH_HPP
#define LITHIUM_TASK_CELESTIAL_SEARCH_HPP

#include <memory>
#include <string>
#include "target/engine.hpp"
#include "task/task.hpp"

namespace lithium::task::task {

using json = nlohmann::json;

/**
 * @brief Task for performing celestial object searches
 *
 * This task provides various search capabilities including name-based search,
 * type filtering, magnitude filtering, and personalized recommendations.
 */
class TaskCelestialSearch : public Task {
public:
    /**
     * @brief Default constructor - initializes the search engine and parameters
     */
    TaskCelestialSearch();

    /**
     * @brief Constructor with name and configuration (for factory pattern)
     * @param name Task instance name
     * @param config Configuration parameters
     */
    TaskCelestialSearch(const std::string& name, const json& config);

    /**
     * @brief Execute the search task with given parameters
     * @param params JSON parameters containing search criteria
     */
    void execute(const json& params) override;

    const json& getLastResults() const { return lastResults_; }

private:
    /**
     * @brief Initialize the search engine with data files
     * @param nameJsonPath Path to name data JSON file
     * @param celestialJsonPath Path to celestial data JSON file
     * @param modelPath Path to recommendation model file
     */
    void initializeSearchEngine(
        const std::string& nameJsonPath = "data/name.json",
        const std::string& celestialJsonPath = "data/celestial.json",
        const std::string& modelPath = "data/recommendation_model.json");

    /**
     * @brief Setup parameter definitions for the task
     */
    void setupParameterDefinitions();

    /**
     * @brief Perform name-based search (exact and fuzzy matching)
     * @param params Search parameters including query string
     * @return JSON response with matches and suggestions
     */
    json searchByName(const json& params);

    /**
     * @brief Perform type-based search filtering
     * @param params Search parameters including object type
     * @return JSON response with matching objects
     */
    json searchByType(const json& params);

    /**
     * @brief Perform magnitude-based search filtering
     * @param params Search parameters including magnitude range
     * @return JSON response with matching objects
     */
    json searchByMagnitude(const json& params);

    /**
     * @brief Get personalized recommendations for a user
     * @param params Search parameters including user ID
     * @return JSON response with recommendations
     */
    json getRecommendations(const json& params);

    /**
     * @brief Update user search history for recommendation training
     * @param user User identifier
     * @param query Search query string
     */
    void updateSearchHistory(const std::string& user, const std::string& query);

    std::shared_ptr<target::SearchEngine>
        searchEngine_;    ///< Search engine instance
    json lastResults_{};  ///< Last search results snapshot

    static constexpr int DEFAULT_FUZZY_TOLERANCE =
        2;  ///< Default fuzzy search tolerance
    static constexpr int DEFAULT_TOP_N = 10;  ///< Default number of results
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CELESTIAL_SEARCH_HPP
