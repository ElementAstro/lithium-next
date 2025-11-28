/*
 * SearchController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP
#define LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP

#include <crow/json.h>
#include "controller.hpp"
#include "../utils/response.hpp"

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
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::target::SearchEngine>)>
            func) {
        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto searchEngine = mSearchEngine.lock();
            if (!searchEngine) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: SearchEngine instance is null.";
                LOG_ERROR(
                      "SearchEngine instance is null. Unable to proceed with "
                      "command: {}",
                      command);
                return crow::response(500, res);
            } else {
                bool success = func(searchEngine);
                if (success) {
                    res["status"] = "success";
                    res["code"] = 200;
                } else {
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] = "Not Found: The specified operation failed.";
                }
            }
        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] =
                std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_ERROR(
                  "Invalid argument while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Runtime error - ") +
                e.what();
            LOG_ERROR(
                  "Runtime error while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
            LOG_ERROR(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
        }

        return crow::response(200, res);
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
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "addStarObject", [&](auto searchEngine) {
                std::vector<std::string> aliases;
                for (const auto& alias : body["aliases"]) {
                    aliases.push_back(alias.s());
                }
                lithium::target::StarObject starObject(
                    body["name"].s(), aliases, body["clickCount"].i());
                searchEngine->addStarObject(starObject);
                return true;
            });
    }

    auto to_json(const lithium::target::CelestialObject& celestialObject) {
        crow::json::wvalue obj;
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
        crow::json::wvalue obj;
        obj["name"] = starObject.getName();
        obj["aliases"] = starObject.getAliases();
        obj["clickCount"] = starObject.getClickCount();
        obj["celestialObject"] = to_json(starObject.getCelestialObject());
        return obj;
    }

    auto to_json(const std::vector<lithium::target::StarObject>& starObjects) {
        crow::json::wvalue arr;
        for (size_t i = 0; i < starObjects.size(); i++) {
            arr[i] = to_json(starObjects[i]);
        }
        return arr;
    }

    // Endpoint to search for a star object
    void searchStarObject(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "searchStarObject", [&](auto searchEngine) {
                auto results =
                    searchEngine->searchStarObject(body["query"].s());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = to_json(results);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to perform a fuzzy search for a star object
    void fuzzySearchStarObject(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "fuzzySearchStarObject", [&](auto searchEngine) {
                auto results = searchEngine->fuzzySearchStarObject(
                    body["query"].s(), body["tolerance"].i());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = to_json(results);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to auto-complete a star object name
    void autoCompleteStarObject(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "autoCompleteStarObject", [&](auto searchEngine) {
                auto results =
                    searchEngine->autoCompleteStarObject(body["prefix"].s());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                for (size_t i = 0; i < results.size(); i++) {
                    response[i] = results[i];
                }
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to perform a filtered search
    void filterSearch(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "filterSearch", [&](auto searchEngine) {
                auto results = searchEngine->filterSearch(
                    body["type"].s(), body["morphology"].s(),
                    body["minMagnitude"].d(), body["maxMagnitude"].d());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = to_json(results);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to load star objects from a name JSON file
    void loadFromNameJson(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "loadFromNameJson", [&](auto searchEngine) {
                searchEngine->loadFromNameJson(body["filename"].s());
                return true;
            });
    }

    // Endpoint to load celestial objects from a JSON file
    void loadFromCelestialJson(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "loadFromCelestialJson", [&](auto searchEngine) {
                searchEngine->loadFromCelestialJson(body["filename"].s());
                return true;
            });
    }

    // Endpoint to initialize the recommendation engine
    void initializeRecommendationEngine(const crow::request& req,
                                        crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "initializeRecommendationEngine",
            [&](auto searchEngine) {
                searchEngine->initializeRecommendationEngine(
                    body["modelFilename"].s());
                return true;
            });
    }

    // Endpoint to add a user rating
    void addUserRating(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "addUserRating", [&](auto searchEngine) {
                searchEngine->addUserRating(body["user"].s(), body["item"].s(),
                                            body["rating"].d());
                return true;
            });
    }

    auto to_json(const std::vector<std::pair<std::string, double>>& results) {
        crow::json::wvalue arr;
        for (size_t i = 0; i < results.size(); i++) {
            arr[i]["item"] = results[i].first;
            arr[i]["rating"] = results[i].second;
        }
        return arr;
    }

    // Endpoint to recommend items for a user
    void recommendItems(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "recommendItems", [&](auto searchEngine) {
                auto results = searchEngine->recommendItems(body["user"].s(),
                                                            body["topN"].i());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = to_json(results);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to save the recommendation model
    void saveRecommendationModel(const crow::request& req,
                                 crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "saveRecommendationModel", [&](auto searchEngine) {
                searchEngine->saveRecommendationModel(body["filename"].s());
                return true;
            });
    }

    // Endpoint to load the recommendation model
    void loadRecommendationModel(const crow::request& req,
                                 crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "loadRecommendationModel", [&](auto searchEngine) {
                searchEngine->loadRecommendationModel(body["filename"].s());
                return true;
            });
    }

    // Endpoint to train the recommendation engine
    void trainRecommendationEngine(const crow::request& req,
                                   crow::response& res) {
        res = handleSearchEngineAction(
            req, crow::json::rvalue{}, "trainRecommendationEngine",
            [&](auto searchEngine) {
                searchEngine->trainRecommendationEngine();
                return true;
            });
    }

    // Endpoint to load data from a CSV file
    void loadFromCSV(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "loadFromCSV", [&](auto searchEngine) {
                std::vector<std::string> requiredFields;
                for (const auto& field : body["requiredFields"]) {
                    requiredFields.push_back(field.s());
                }
                lithium::target::Dialect dialect;
                if (body.has("dialect")) {
                    const auto& d = body["dialect"];
                    if (d.has("delimiter") &&
                        d["delimiter"].t() == crow::json::type::String) {
                        std::string s = d["delimiter"].s();
                        if (!s.empty()) dialect.delimiter = s[0];
                    }
                    if (d.has("quotechar") &&
                        d["quotechar"].t() == crow::json::type::String) {
                        std::string s = d["quotechar"].s();
                        if (!s.empty()) dialect.quotechar = s[0];
                    }
                    if (d.has("doublequote")) {
                        dialect.doublequote = d["doublequote"].b();
                    }
                    if (d.has("skip_initial_space")) {
                        dialect.skip_initial_space = d["skip_initial_space"].b();
                    }
                    if (d.has("lineterminator") &&
                        d["lineterminator"].t() == crow::json::type::String) {
                        dialect.lineterminator = d["lineterminator"].s();
                    }
                    if (d.has("quoting") &&
                        d["quoting"].t() == crow::json::type::Number) {
                        dialect.quoting = static_cast<lithium::target::Quoting>(
                            d["quoting"].i());
                    }
                }
                searchEngine->loadFromCSV(body["filename"].s(), requiredFields,
                                          dialect);
                return true;
            });
    }

    // Endpoint to get hybrid recommendations
    void getHybridRecommendations(const crow::request& req,
                                  crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "getHybridRecommendations", [&](auto searchEngine) {
                auto results = searchEngine->getHybridRecommendations(
                    body["user"].s(), body["topN"].i(),
                    body["contentWeight"].d(), body["collaborativeWeight"].d());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["results"] = to_json(results);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to export data to a CSV file
    void exportToCSV(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "exportToCSV", [&](auto searchEngine) {
                std::vector<std::string> fields;
                for (const auto& field : body["fields"]) {
                    fields.push_back(field.s());
                }
                lithium::target::Dialect dialect;
                if (body.has("dialect")) {
                    const auto& d = body["dialect"];
                    if (d.has("delimiter") &&
                        d["delimiter"].t() == crow::json::type::String) {
                        std::string s = d["delimiter"].s();
                        if (!s.empty()) dialect.delimiter = s[0];
                    }
                    if (d.has("quotechar") &&
                        d["quotechar"].t() == crow::json::type::String) {
                        std::string s = d["quotechar"].s();
                        if (!s.empty()) dialect.quotechar = s[0];
                    }
                    if (d.has("doublequote")) {
                        dialect.doublequote = d["doublequote"].b();
                    }
                    if (d.has("skip_initial_space")) {
                        dialect.skip_initial_space = d["skip_initial_space"].b();
                    }
                    if (d.has("lineterminator") &&
                        d["lineterminator"].t() == crow::json::type::String) {
                        dialect.lineterminator = d["lineterminator"].s();
                    }
                    if (d.has("quoting") &&
                        d["quoting"].t() == crow::json::type::Number) {
                        dialect.quoting = static_cast<lithium::target::Quoting>(
                            d["quoting"].i());
                    }
                }
                searchEngine->exportToCSV(body["filename"].s(), fields,
                                          dialect);
                return true;
            });
    }

    // Endpoint to clear the cache
    void clearCache(const crow::request& req, crow::response& res) {
        res = handleSearchEngineAction(req, crow::json::rvalue{}, "clearCache",
                                       [&](auto searchEngine) {
                                           searchEngine->clearCache();
                                           return true;
                                       });
    }

    // Endpoint to set the cache size
    void setCacheSize(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleSearchEngineAction(
            req, body, "setCacheSize", [&](auto searchEngine) {
                searchEngine->setCacheSize(body["size"].u());
                return true;
            });
    }

    // Endpoint to get cache statistics
    void getCacheStats(const crow::request& req, crow::response& res) {
        res = handleSearchEngineAction(
            req, crow::json::rvalue{}, "getCacheStats", [&](auto searchEngine) {
                auto stats = searchEngine->getCacheStats();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["stats"] = stats;
                res.write(response.dump());
                return true;
            });
    }
};

inline std::weak_ptr<lithium::target::SearchEngine> SearchController::mSearchEngine;

}  // namespace lithium::server::controller

#endif  // LITHIUM_ASYNC_SEARCH_ENGINE_CONTROLLER_HPP
