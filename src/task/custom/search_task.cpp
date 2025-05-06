#include "search_task.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::sequencer::task {

TaskCelestialSearch::TaskCelestialSearch()
    : Task("TaskCelestialSearch",
           [this](const json& params) { execute(params); }),
      searchEngine_(std::make_shared<target::SearchEngine>()) {
    // 初始化搜索引擎
    searchEngine_->loadFromNameJson("data/name.json");
    searchEngine_->loadFromCelestialJson("data/celestial.json");
    searchEngine_->initializeRecommendationEngine(
        "data/recommendation_model.json");

    // 定义任务参数
    addParamDefinition("searchType", "string", true, nullptr,
                       "搜索类型: name/type/magnitude/recommendation");
    addParamDefinition("query", "string", false, nullptr, "搜索关键词");
    addParamDefinition("userId", "string", false, nullptr, "用户ID");
    addParamDefinition("minMagnitude", "number", false, nullptr, "最小亮度");
    addParamDefinition("maxMagnitude", "number", false, nullptr, "最大亮度");
    addParamDefinition("objectType", "string", false, nullptr, "天体类型");

    // 设置优先级和超时
    setPriority(8);
    setTimeout(std::chrono::seconds(30));
}

void TaskCelestialSearch::execute(const json& params) {
    const auto start = std::chrono::steady_clock::now();
    LOG_F(INFO, "Starting celestial search task with params: {}",
          params.dump());

    try {
        if (!validateParams(params)) {
            LOG_F(ERROR, "Parameter validation failed for params: {}",
                  params.dump());
            THROW_INVALID_ARGUMENT("Invalid parameters");
        }
        LOG_F(INFO, "Parameters validated successfully");

        std::string searchType = params.at("searchType").get<std::string>();
        LOG_F(INFO, "Processing {} search request", searchType);

        if (searchType == "name") {
            searchByName(params);
        } else if (searchType == "type") {
            searchByType(params);
        } else if (searchType == "magnitude") {
            searchByMagnitude(params);
        } else if (searchType == "recommendation") {
            getRecommendations(params);
        } else {
            LOG_F(ERROR, "Invalid search type: {}", searchType);
            THROW_INVALID_ARGUMENT("Unknown search type");
        }

        if (params.contains("userId") && params.contains("query")) {
            LOG_F(DEBUG, "Updating search history for user {} with query {}",
                  params["userId"].get<std::string>(),
                  params["query"].get<std::string>());
            updateSearchHistory(params["userId"].get<std::string>(),
                                params["query"].get<std::string>());
        }

        const auto end = std::chrono::steady_clock::now();
        const auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        LOG_F(INFO, "Search task completed in {}ms", duration.count());

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in TaskCelestialSearch: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        throw;
    }
}

void TaskCelestialSearch::searchByName(const json& params) {
    const auto start = std::chrono::steady_clock::now();
    const std::string query = params.at("query").get<std::string>();
    LOG_F(INFO, "Starting name-based search for query: {}", query);

    auto results = searchEngine_->searchStarObject(query);
    LOG_F(DEBUG, "Exact match results count: {}", results.size());

    if (results.empty()) {
        LOG_F(INFO, "No exact matches found, trying fuzzy search");
        results = searchEngine_->fuzzySearchStarObject(query,
                                                       DEFAULT_FUZZY_TOLERANCE);
        LOG_F(DEBUG, "Fuzzy search results count: {}", results.size());
    }

    auto suggestions = searchEngine_->autoCompleteStarObject(query);
    LOG_F(DEBUG, "Generated {} autocomplete suggestions", suggestions.size());

    auto rankedResults = searchEngine_->getRankedResults(results);
    LOG_F(INFO, "Ranked {} results", rankedResults.size());

    const auto end = std::chrono::steady_clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    LOG_F(INFO, "Name search completed in {}ms", duration.count());

    addHistoryEntry("Found " + std::to_string(results.size()) +
                    " matches for query: " + query);
}

void TaskCelestialSearch::searchByType(const json& params) {
    LOG_F(INFO, "Performing type-based search");

    std::string objectType = params.at("objectType").get<std::string>();

    auto results = searchEngine_->filterSearch(
        objectType,  // type
        "",          // morphology (empty for type-only search)
        -std::numeric_limits<double>::infinity(),  // no minimum magnitude
        std::numeric_limits<double>::infinity()    // no maximum magnitude
    );

    json searchResults;
    searchResults["matches"] = json::array();

    for (const auto& star : results) {
        searchResults["matches"].push_back(star.to_json());
    }

    addHistoryEntry("Found " + std::to_string(results.size()) +
                    " objects of type: " + objectType);
}

void TaskCelestialSearch::searchByMagnitude(const json& params) {
    LOG_F(INFO, "Performing magnitude-based search");

    double minMag = params.at("minMagnitude").get<double>();
    double maxMag = params.at("maxMagnitude").get<double>();

    auto results = searchEngine_->filterSearch(
        "",  // type (empty for magnitude-only search)
        "",  // morphology
        minMag, maxMag);

    json searchResults;
    searchResults["matches"] = json::array();

    for (const auto& star : results) {
        searchResults["matches"].push_back(star.to_json());
    }

    addHistoryEntry("Found " + std::to_string(results.size()) +
                    " objects with magnitude between " +
                    std::to_string(minMag) + " and " + std::to_string(maxMag));
}

void TaskCelestialSearch::getRecommendations(const json& params) {
    LOG_F(INFO, "Getting object recommendations");

    std::string userId = params.at("userId").get<std::string>();

    // 获取混合推荐结果
    auto recommendations =
        searchEngine_->getHybridRecommendations(userId, DEFAULT_TOP_N,
                                                0.3,  // 内容权重
                                                0.7   // 协同过滤权重
        );

    json recommendationResults;
    recommendationResults["recommendations"] = json::array();

    for (const auto& [itemId, score] : recommendations) {
        auto results = searchEngine_->searchStarObject(itemId);
        if (!results.empty()) {
            json item;
            item["object"] = results[0].to_json();
            item["score"] = score;
            recommendationResults["recommendations"].push_back(item);
        }
    }

    addHistoryEntry("Generated " + std::to_string(recommendations.size()) +
                    " recommendations for user: " + userId);
}

void TaskCelestialSearch::updateSearchHistory(const std::string& user,
                                              const std::string& query) {
    LOG_F(INFO, "Updating search history for user: {}", user);

    try {
        searchEngine_->addUserRating(user, query, 0.5);
        LOG_F(DEBUG, "Added user rating for query: {}", query);

        static int searchCount = 0;
        if (++searchCount % 100 == 0) {
            LOG_F(INFO, "Training recommendation engine after {} searches",
                  searchCount);
            searchEngine_->trainRecommendationEngine();
            searchEngine_->saveRecommendationModel(
                "data/recommendation_model.json");
            LOG_F(INFO, "Recommendation model updated and saved");
        }

    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error updating search history: {} (User: {})", e.what(),
              user);
    }
}

}  // namespace lithium::sequencer::task
