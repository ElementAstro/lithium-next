/*
 * SearchController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP
#define LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP

#include <crow/json.h>
#include "../utils/response.hpp"
#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "target/engine.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class SearchController : public Controller {
private:
    static std::weak_ptr<lithium::target::SearchEngine> mSearchEngine;

    // Utility function to handle all search engine actions
    static auto handleSearchEngineAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<
            crow::response(std::shared_ptr<lithium::target::SearchEngine>)>
            func) -> crow::response {
        try {
            auto searchEngine = mSearchEngine.lock();
            if (!searchEngine) {
                LOG_ERROR(
                    "SearchEngine instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "SearchEngine instance is null.");
            }
            return func(searchEngine);
        } catch (const std::invalid_argument& e) {
            LOG_ERROR(
                "Invalid argument while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            LOG_ERROR(
                "Runtime error while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Create a weak pointer to the SearchEngine
        GET_OR_CREATE_WEAK_PTR(mSearchEngine, lithium::target::SearchEngine,
                               Constants::SEARCH_ENGINE);
        // Define the routes
        CROW_ROUTE(app, "/search_engine/addStarObject")
            .methods("POST"_method)(&SearchController::addStarObject, this);
        CROW_ROUTE(app, "/search_engine/searchStarObject")
            .methods("POST"_method)(&SearchController::searchStarObject, this);
        CROW_ROUTE(app, "/search_engine/fuzzySearchStarObject")
            .methods("POST"_method)(&SearchController::fuzzySearchStarObject,
                                    this);
        CROW_ROUTE(app, "/search_engine/autoCompleteStarObject")
            .methods("POST"_method)(&SearchController::autoCompleteStarObject,
                                    this);
        CROW_ROUTE(app, "/search_engine/filterSearch")
            .methods("POST"_method)(&SearchController::filterSearch, this);
        CROW_ROUTE(app, "/search_engine/loadFromNameJson")
            .methods("POST"_method)(&SearchController::loadFromNameJson, this);
        CROW_ROUTE(app, "/search_engine/loadFromCelestialJson")
            .methods("POST"_method)(&SearchController::loadFromCelestialJson,
                                    this);
        CROW_ROUTE(app, "/search_engine/initializeRecommendationEngine")
            .methods("POST"_method)(
                &SearchController::initializeRecommendationEngine, this);
        CROW_ROUTE(app, "/search_engine/addUserRating")
            .methods("POST"_method)(&SearchController::addUserRating, this);
        CROW_ROUTE(app, "/search_engine/recommendItems")
            .methods("POST"_method)(&SearchController::recommendItems, this);
        CROW_ROUTE(app, "/search_engine/saveRecommendationModel")
            .methods("POST"_method)(&SearchController::saveRecommendationModel,
                                    this);
        CROW_ROUTE(app, "/search_engine/loadRecommendationModel")
            .methods("POST"_method)(&SearchController::loadRecommendationModel,
                                    this);
        CROW_ROUTE(app, "/search_engine/trainRecommendationEngine")
            .methods("POST"_method)(
                &SearchController::trainRecommendationEngine, this);
        CROW_ROUTE(app, "/search_engine/loadFromCSV")
            .methods("POST"_method)(&SearchController::loadFromCSV, this);
        CROW_ROUTE(app, "/search_engine/getHybridRecommendations")
            .methods("POST"_method)(&SearchController::getHybridRecommendations,
                                    this);
        CROW_ROUTE(app, "/search_engine/exportToCSV")
            .methods("POST"_method)(&SearchController::exportToCSV, this);
        CROW_ROUTE(app, "/search_engine/clearCache")
            .methods("POST"_method)(&SearchController::clearCache, this);
        CROW_ROUTE(app, "/search_engine/setCacheSize")
            .methods("POST"_method)(&SearchController::setCacheSize, this);
        CROW_ROUTE(app, "/search_engine/getCacheStats")
            .methods("GET"_method)(&SearchController::getCacheStats, this);
    }

    // Endpoint to add a star object
    void addStarObject(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "addStarObject",
                [&](auto searchEngine) -> crow::response {
                    std::vector<std::string> aliases;
                    for (const auto& alias : body["aliases"]) {
                        aliases.push_back(alias.get<std::string>());
                    }
                    lithium::target::StarObject starObject(
                        body["name"].get<std::string>(), aliases,
                        body["clickCount"].get<int>());
                    searchEngine->addStarObject(starObject);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    auto to_json(const lithium::target::CelestialObject& celestialObject) {
        nlohmann::json obj;
        obj["ID"] = celestialObject.ID;
        obj["Identifier"] = celestialObject.Identifier;
        obj["MIdentifier"] = celestialObject.MIdentifier;
        obj["ExtensionName"] = celestialObject.ExtensionName;
        obj["Component"] = celestialObject.Component;
        obj["ClassName"] = celestialObject.ClassName;
        obj["AmateurRank"] = celestialObject.AmateurRank;
        obj["ChineseName"] = celestialObject.ChineseName;
        obj["Type"] = celestialObject.Type;
        obj["DuplicateType"] = celestialObject.DuplicateType;
        obj["Morphology"] = celestialObject.Morphology;
        obj["ConstellationZh"] = celestialObject.ConstellationZh;
        obj["ConstellationEn"] = celestialObject.ConstellationEn;
        obj["RAJ2000"] = celestialObject.RAJ2000;
        obj["RADJ2000"] = celestialObject.RADJ2000;
        obj["DecJ2000"] = celestialObject.DecJ2000;
        obj["DecDJ2000"] = celestialObject.DecDJ2000;
        obj["VisualMagnitudeV"] = celestialObject.VisualMagnitudeV;
        obj["PhotographicMagnitudeB"] = celestialObject.PhotographicMagnitudeB;
        obj["BMinusV"] = celestialObject.BMinusV;
        obj["SurfaceBrightness"] = celestialObject.SurfaceBrightness;
        obj["MajorAxis"] = celestialObject.MajorAxis;
        obj["MinorAxis"] = celestialObject.MinorAxis;
        obj["PositionAngle"] = celestialObject.PositionAngle;
        obj["DetailedDescription"] = celestialObject.DetailedDescription;
        obj["BriefDescription"] = celestialObject.BriefDescription;

        return obj;
    }

    auto to_json(const lithium::target::StarObject& starObject) {
        nlohmann::json obj;
        obj["name"] = starObject.getName();
        obj["aliases"] = starObject.getAliases();
        obj["clickCount"] = starObject.getClickCount();
        obj["celestialObject"] = to_json(starObject.getCelestialObject());
        return obj;
    }

    auto to_json(const std::vector<lithium::target::StarObject>& starObjects) {
        nlohmann::json arr = nlohmann::json::array();
        for (size_t i = 0; i < starObjects.size(); i++) {
            arr.push_back(to_json(starObjects[i]));
        }
        return arr;
    }

    // Endpoint to search for a star object
    void searchStarObject(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "searchStarObject",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->searchStarObject(
                        body["query"].get<std::string>());
                    nlohmann::json response;
                    response["results"] = to_json(results);
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to perform a fuzzy search for a star object
    void fuzzySearchStarObject(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "fuzzySearchStarObject",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->fuzzySearchStarObject(
                        body["query"].get<std::string>(),
                        body["tolerance"].get<int>());
                    nlohmann::json response;
                    response["results"] = to_json(results);
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to auto-complete a star object name
    void autoCompleteStarObject(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "autoCompleteStarObject",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->autoCompleteStarObject(
                        body["prefix"].get<std::string>());
                    nlohmann::json response = nlohmann::json::array();
                    for (size_t i = 0; i < results.size(); i++) {
                        response.push_back(results[i]);
                    }
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to perform a filtered search
    void filterSearch(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "filterSearch",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->filterSearch(
                        body["type"].get<std::string>(),
                        body["morphology"].get<std::string>(),
                        body["minMagnitude"].get<double>(),
                        body["maxMagnitude"].get<double>());
                    nlohmann::json response;
                    response["results"] = to_json(results);
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to load star objects from a name JSON file
    void loadFromNameJson(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "loadFromNameJson",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->loadFromNameJson(
                        body["filename"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to load celestial objects from a JSON file
    void loadFromCelestialJson(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "loadFromCelestialJson",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->loadFromCelestialJson(
                        body["filename"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to initialize the recommendation engine
    void initializeRecommendationEngine(const crow::request& req,
                                        crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "initializeRecommendationEngine",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->initializeRecommendationEngine(
                        body["modelFilename"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to add a user rating
    void addUserRating(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "addUserRating",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->addUserRating(body["user"].get<std::string>(),
                                                body["item"].get<std::string>(),
                                                body["rating"].get<double>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    auto to_json(const std::vector<std::pair<std::string, double>>& results) {
        nlohmann::json arr = nlohmann::json::array();
        for (size_t i = 0; i < results.size(); i++) {
            nlohmann::json obj;
            obj["item"] = results[i].first;
            obj["rating"] = results[i].second;
            arr.push_back(obj);
        }
        return arr;
    }

    // Endpoint to recommend items for a user
    void recommendItems(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "recommendItems",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->recommendItems(
                        body["user"].get<std::string>(),
                        body["topN"].get<int>());
                    nlohmann::json response;
                    response["results"] = to_json(results);
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to save the recommendation model
    void saveRecommendationModel(const crow::request& req,
                                 crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "saveRecommendationModel",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->saveRecommendationModel(
                        body["filename"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to load the recommendation model
    void loadRecommendationModel(const crow::request& req,
                                 crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "loadRecommendationModel",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->loadRecommendationModel(
                        body["filename"].get<std::string>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to train the recommendation engine
    void trainRecommendationEngine(const crow::request& req,
                                   crow::response& res) {
        res = handleSearchEngineAction(
            req, nlohmann::json{}, "trainRecommendationEngine",
            [&](auto searchEngine) -> crow::response {
                searchEngine->trainRecommendationEngine();
                return ResponseBuilder::success(nlohmann::json{});
            });
    }

    // Endpoint to load data from a CSV file
    void loadFromCSV(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "loadFromCSV",
                [&](auto searchEngine) -> crow::response {
                    std::vector<std::string> requiredFields;
                    for (const auto& field : body["requiredFields"]) {
                        requiredFields.push_back(field.get<std::string>());
                    }
                    lithium::target::Dialect dialect;
                    if (body.contains("dialect")) {
                        const auto& d = body["dialect"];
                        if (d.contains("delimiter") &&
                            d["delimiter"].is_string()) {
                            std::string s = d["delimiter"].get<std::string>();
                            if (!s.empty())
                                dialect.delimiter = s[0];
                        }
                        if (d.contains("quotechar") &&
                            d["quotechar"].is_string()) {
                            std::string s = d["quotechar"].get<std::string>();
                            if (!s.empty())
                                dialect.quotechar = s[0];
                        }
                        if (d.contains("doublequote")) {
                            dialect.doublequote = d["doublequote"].get<bool>();
                        }
                        if (d.contains("skip_initial_space")) {
                            dialect.skip_initial_space =
                                d["skip_initial_space"].get<bool>();
                        }
                        if (d.contains("lineterminator") &&
                            d["lineterminator"].is_string()) {
                            dialect.lineterminator =
                                d["lineterminator"].get<std::string>();
                        }
                        if (d.contains("quoting") && d["quoting"].is_number()) {
                            dialect.quoting =
                                static_cast<lithium::target::Quoting>(
                                    d["quoting"].get<int>());
                        }
                    }
                    searchEngine->loadFromCSV(
                        body["filename"].get<std::string>(), requiredFields,
                        dialect);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get hybrid recommendations
    void getHybridRecommendations(const crow::request& req,
                                  crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "getHybridRecommendations",
                [&](auto searchEngine) -> crow::response {
                    auto results = searchEngine->getHybridRecommendations(
                        body["user"].get<std::string>(),
                        body["topN"].get<int>(),
                        body["contentWeight"].get<double>(),
                        body["collaborativeWeight"].get<double>());
                    nlohmann::json response;
                    response["results"] = to_json(results);
                    return ResponseBuilder::success(response);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to export data to a CSV file
    void exportToCSV(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "exportToCSV",
                [&](auto searchEngine) -> crow::response {
                    std::vector<std::string> fields;
                    for (const auto& field : body["fields"]) {
                        fields.push_back(field.get<std::string>());
                    }
                    lithium::target::Dialect dialect;
                    if (body.contains("dialect")) {
                        const auto& d = body["dialect"];
                        if (d.contains("delimiter") &&
                            d["delimiter"].is_string()) {
                            std::string s = d["delimiter"].get<std::string>();
                            if (!s.empty())
                                dialect.delimiter = s[0];
                        }
                        if (d.contains("quotechar") &&
                            d["quotechar"].is_string()) {
                            std::string s = d["quotechar"].get<std::string>();
                            if (!s.empty())
                                dialect.quotechar = s[0];
                        }
                        if (d.contains("doublequote")) {
                            dialect.doublequote = d["doublequote"].get<bool>();
                        }
                        if (d.contains("skip_initial_space")) {
                            dialect.skip_initial_space =
                                d["skip_initial_space"].get<bool>();
                        }
                        if (d.contains("lineterminator") &&
                            d["lineterminator"].is_string()) {
                            dialect.lineterminator =
                                d["lineterminator"].get<std::string>();
                        }
                        if (d.contains("quoting") && d["quoting"].is_number()) {
                            dialect.quoting =
                                static_cast<lithium::target::Quoting>(
                                    d["quoting"].get<int>());
                        }
                    }
                    searchEngine->exportToCSV(
                        body["filename"].get<std::string>(), fields, dialect);
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to clear the cache
    void clearCache(const crow::request& req, crow::response& res) {
        res = handleSearchEngineAction(
            req, nlohmann::json{}, "clearCache",
            [&](auto searchEngine) -> crow::response {
                searchEngine->clearCache();
                return ResponseBuilder::success(nlohmann::json{});
            });
    }

    // Endpoint to set the cache size
    void setCacheSize(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleSearchEngineAction(
                req, body, "setCacheSize",
                [&](auto searchEngine) -> crow::response {
                    searchEngine->setCacheSize(body["size"].get<size_t>());
                    return ResponseBuilder::success(nlohmann::json{});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Endpoint to get cache statistics
    void getCacheStats(const crow::request& req, crow::response& res) {
        res = handleSearchEngineAction(
            req, nlohmann::json{}, "getCacheStats",
            [&](auto searchEngine) -> crow::response {
                auto stats = searchEngine->getCacheStats();
                nlohmann::json response;
                response["stats"] = stats;
                return ResponseBuilder::success(response);
            });
    }
};

inline std::weak_ptr<lithium::target::SearchEngine>
    SearchController::mSearchEngine;

}  // namespace lithium::server::controller

#endif  // LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP
