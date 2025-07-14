#include "search_task.hpp"
#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"
#include "factory.hpp"

namespace lithium::task::task {

TaskCelestialSearch::TaskCelestialSearch()
    : Task("TaskCelestialSearch",
           [this](const json& params) { execute(params); }),
      searchEngine_(std::make_shared<target::SearchEngine>()) {
    initializeSearchEngine();
    setupParameterDefinitions();
    setPriority(8);
    setTimeout(std::chrono::seconds(30));
}

TaskCelestialSearch::TaskCelestialSearch(const std::string& name,
                                         const json& config)
    : Task(name, [this](const json& params) { execute(params); }),
      searchEngine_(std::make_shared<target::SearchEngine>()) {
    // Apply configuration if provided
    if (config.contains("priority")) {
        setPriority(config["priority"].get<int>());
    } else {
        setPriority(8);
    }

    if (config.contains("timeout")) {
        setTimeout(std::chrono::seconds(config["timeout"].get<int>()));
    } else {
        setTimeout(std::chrono::seconds(30));
    }

    // Configure data paths from config
    std::string nameJsonPath = config.value("nameJsonPath", "data/name.json");
    std::string celestialJsonPath =
        config.value("celestialJsonPath", "data/celestial.json");
    std::string modelPath =
        config.value("modelPath", "data/recommendation_model.json");

    // Initialize search engine with configured paths
    initializeSearchEngine(nameJsonPath, celestialJsonPath, modelPath);
    setupParameterDefinitions();
}

void TaskCelestialSearch::initializeSearchEngine(
    const std::string& nameJsonPath, const std::string& celestialJsonPath,
    const std::string& modelPath) {
    try {
        searchEngine_->loadFromNameJson(nameJsonPath);
        searchEngine_->loadFromCelestialJson(celestialJsonPath);
        searchEngine_->initializeRecommendationEngine(modelPath);
        spdlog::info("Search engine initialized successfully");
    } catch (const std::exception& e) {
        spdlog::warn("Failed to initialize search engine: {}", e.what());
        // Continue without throwing - allow graceful degradation
    }
}

void TaskCelestialSearch::setupParameterDefinitions() {
    // Define task parameters with proper types
    addParamDefinition("searchType", "string", true, nullptr,
                       "Search type: name/type/magnitude/recommendation");
    addParamDefinition("query", "string", false, nullptr,
                       "Search query string");
    addParamDefinition("userId", "string", false, nullptr, "User identifier");
    addParamDefinition("minMagnitude", "number", false, nullptr,
                       "Minimum magnitude");
    addParamDefinition("maxMagnitude", "number", false, nullptr,
                       "Maximum magnitude");
    addParamDefinition("objectType", "string", false, nullptr,
                       "Object type filter");
    addParamDefinition("morphology", "string", false, nullptr,
                       "Morphological classification");
    addParamDefinition("topN", "number", false, json(DEFAULT_TOP_N),
                       "Number of results to return");
    addParamDefinition("fuzzyTolerance", "number", false,
                       json(DEFAULT_FUZZY_TOLERANCE), "Fuzzy search tolerance");
    addParamDefinition("contentWeight", "number", false, json(0.3),
                       "Content-based recommendation weight");
    addParamDefinition("collaborativeWeight", "number", false, json(0.7),
                       "Collaborative filtering weight");
}

void TaskCelestialSearch::execute(const json& params) {
    const auto start = std::chrono::steady_clock::now();
    spdlog::info("Starting celestial search task with params: {}",
                 params.dump());

    try {
        if (!validateParams(params)) {
            spdlog::error("Parameter validation failed for params: {}",
                          params.dump());
            setErrorType(TaskErrorType::InvalidParameter);
            auto errors = getParamErrors();
            std::string errorMsg = "Invalid parameters: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            THROW_INVALID_ARGUMENT(errorMsg);
        }
        spdlog::info("Parameters validated successfully");

        std::string searchType = params.at("searchType").get<std::string>();
        spdlog::info("Processing {} search request", searchType);

        json results;

        if (searchType == "name") {
            results = searchByName(params);
        } else if (searchType == "type") {
            results = searchByType(params);
        } else if (searchType == "magnitude") {
            results = searchByMagnitude(params);
        } else if (searchType == "recommendation") {
            results = getRecommendations(params);
        } else {
            spdlog::error("Invalid search type: {}", searchType);
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT("Unknown search type: " + searchType);
        }

        // Store results in task history
        addHistoryEntry("Search completed with " +
                        std::to_string(results.contains("matches")
                                           ? results["matches"].size()
                                       : results.contains("recommendations")
                                           ? results["recommendations"].size()
                                           : 0) +
                        " results");

        if (params.contains("userId") && params.contains("query")) {
            spdlog::debug("Updating search history for user {} with query {}",
                          params["userId"].get<std::string>(),
                          params["query"].get<std::string>());
            updateSearchHistory(params["userId"].get<std::string>(),
                                params["query"].get<std::string>());
        }

        const auto end = std::chrono::steady_clock::now();
        const auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        spdlog::info("Search task completed in {}ms", duration.count());

    } catch (const std::exception& e) {
        spdlog::error("Error in TaskCelestialSearch: {}", e.what());
        if (getErrorType() == TaskErrorType::None) {
            setErrorType(TaskErrorType::SystemError);
        }
        throw;
    }
}

json TaskCelestialSearch::searchByName(const json& params) {
    const auto start = std::chrono::steady_clock::now();
    const std::string query = params.at("query").get<std::string>();
    int fuzzyTolerance =
        params.value("fuzzyTolerance", DEFAULT_FUZZY_TOLERANCE);

    spdlog::info("Starting name-based search for query: {}", query);

    // First try exact match
    auto results = searchEngine_->searchStarObject(query);
    spdlog::debug("Exact match results count: {}", results.size());

    // If no exact matches, try fuzzy search
    if (results.empty()) {
        spdlog::info("No exact matches found, trying fuzzy search");
        results = searchEngine_->fuzzySearchStarObject(query, fuzzyTolerance);
        spdlog::debug("Fuzzy search results count: {}", results.size());
    }

    // Get autocomplete suggestions
    auto suggestions = searchEngine_->autoCompleteStarObject(query);
    spdlog::debug("Generated {} autocomplete suggestions", suggestions.size());

    // Rank results by popularity
    auto rankedResults = searchEngine_->getRankedResults(results);
    spdlog::info("Ranked {} results", rankedResults.size());

    // Build response JSON
    json response;
    response["matches"] = json::array();
    response["suggestions"] = json::array();

    for (const auto& star : rankedResults) {
        response["matches"].push_back(star.to_json());
    }

    for (const auto& suggestion : suggestions) {
        response["suggestions"].push_back(suggestion);
    }

    const auto end = std::chrono::steady_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    spdlog::info("Name search completed in {}ms", duration.count());

    addHistoryEntry("Found " + std::to_string(results.size()) +
                    " matches for query: " + query);

    return response;
}

json TaskCelestialSearch::searchByType(const json& params) {
    spdlog::info("Performing type-based search");

    std::string objectType = params.at("objectType").get<std::string>();
    std::string morphology = params.value("morphology", std::string(""));

    auto results = searchEngine_->filterSearch(
        objectType, morphology, -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity());

    json response;
    response["matches"] = json::array();

    for (const auto& star : results) {
        response["matches"].push_back(star.to_json());
    }

    std::string historyMsg = "Found " + std::to_string(results.size()) +
                             " objects of type: " + objectType;
    if (!morphology.empty()) {
        historyMsg += " with morphology: " + morphology;
    }
    addHistoryEntry(historyMsg);

    return response;
}

json TaskCelestialSearch::searchByMagnitude(const json& params) {
    spdlog::info("Performing magnitude-based search");

    double minMag = params.at("minMagnitude").get<double>();
    double maxMag = params.at("maxMagnitude").get<double>();

    std::string objectType = params.value("objectType", std::string(""));
    std::string morphology = params.value("morphology", std::string(""));

    auto results =
        searchEngine_->filterSearch(objectType, morphology, minMag, maxMag);

    json response;
    response["matches"] = json::array();

    for (const auto& star : results) {
        response["matches"].push_back(star.to_json());
    }

    addHistoryEntry("Found " + std::to_string(results.size()) +
                    " objects with magnitude between " +
                    std::to_string(minMag) + " and " + std::to_string(maxMag));

    return response;
}

json TaskCelestialSearch::getRecommendations(const json& params) {
    spdlog::info("Getting object recommendations");

    std::string userId = params.at("userId").get<std::string>();
    int topN = params.value("topN", DEFAULT_TOP_N);
    double contentWeight = params.value("contentWeight", 0.3);
    double collaborativeWeight = params.value("collaborativeWeight", 0.7);

    // Get hybrid recommendations
    auto recommendations = searchEngine_->getHybridRecommendations(
        userId, topN, contentWeight, collaborativeWeight);

    json response;
    response["recommendations"] = json::array();

    for (const auto& [itemId, score] : recommendations) {
        auto results = searchEngine_->searchStarObject(itemId);
        if (!results.empty()) {
            json item;
            item["object"] = results[0].to_json();
            item["score"] = score;
            response["recommendations"].push_back(item);
        }
    }

    addHistoryEntry("Generated " + std::to_string(recommendations.size()) +
                    " recommendations for user: " + userId);

    return response;
}

void TaskCelestialSearch::updateSearchHistory(const std::string& user,
                                              const std::string& query) {
    spdlog::info("Updating search history for user: {}", user);

    try {
        // Add implicit rating for the search query
        searchEngine_->addUserRating(user, query, 0.5);
        spdlog::debug("Added user rating for query: {}", query);

        // Periodically retrain the recommendation engine
        static int searchCount = 0;
        if (++searchCount % 100 == 0) {
            spdlog::info("Training recommendation engine after {} searches",
                         searchCount);
            searchEngine_->trainRecommendationEngine();
            searchEngine_->saveRecommendationModel(
                "data/recommendation_model.json");
            spdlog::info("Recommendation model updated and saved");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error updating search history: {} (User: {})", e.what(),
                      user);
        // Don't throw here as this is not critical for search functionality
    }
}

// Task registration
namespace {
// Define task information
TaskInfo searchTaskInfo = {
    .name = "TaskCelestialSearch",
    .description =
        "Performs celestial object searches with various filtering options and "
        "personalized recommendations",
    .category = "Astronomy",
    .requiredParameters = {"searchType"},
    .parameterSchema =
        json{
            {"type", "object"},
            {"properties",
             json{
                 {"searchType",
                  json{{"type", "string"},
                       {"enum", json::array({"name", "type", "magnitude",
                                             "recommendation"})},
                       {"description", "Type of search to perform"}}},
                 {"query",
                  json{{"type", "string"},
                       {"description",
                        "Search query string (required for name search)"}}},
                 {"userId",
                  json{{"type", "string"},
                       {"description",
                        "User identifier (required for recommendations)"}}},
                 {"objectType",
                  json{{"type", "string"},
                       {"description", "Celestial object type filter"}}},
                 {"morphology",
                  json{{"type", "string"},
                       {"description", "Morphological classification filter"}}},
                 {"minMagnitude",
                  json{{"type", "number"},
                       {"description", "Minimum magnitude for filtering"}}},
                 {"maxMagnitude",
                  json{{"type", "number"},
                       {"description", "Maximum magnitude for filtering"}}},
                 {"topN", json{{"type", "integer"},
                               {"minimum", 1},
                               {"maximum", 100},
                               {"default", 10},
                               {"description", "Number of results to return"}}},
                 {"fuzzyTolerance",
                  json{{"type", "integer"},
                       {"minimum", 0},
                       {"maximum", 10},
                       {"default", 2},
                       {"description", "Fuzzy search tolerance level"}}},
                 {"contentWeight",
                  json{{"type", "number"},
                       {"minimum", 0.0},
                       {"maximum", 1.0},
                       {"default", 0.3},
                       {"description",
                        "Weight for content-based recommendations"}}},
                 {"collaborativeWeight",
                  json{{"type", "number"},
                       {"minimum", 0.0},
                       {"maximum", 1.0},
                       {"default", 0.7},
                       {"description",
                        "Weight for collaborative filtering "
                        "recommendations"}}}}},
            {"required", json::array({"searchType"})},
            {"additionalProperties", false}},
    .version = "1.0.0",
    .dependencies = {},
    .isEnabled = true};

// Custom factory function that handles configuration
auto celestialSearchFactory =
    [](const std::string& name,
       const json& config) -> std::unique_ptr<TaskCelestialSearch> {
    return std::make_unique<TaskCelestialSearch>(name, config);
};

// Register the task using the registrar
static TaskRegistrar<TaskCelestialSearch> searchTaskRegistrar(
    "CelestialSearch", searchTaskInfo, celestialSearchFactory);
}  // namespace

}  // namespace lithium::task::task
