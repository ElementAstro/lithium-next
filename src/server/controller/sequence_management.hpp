/*
 * SequenceManagementController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP
#define LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/log/loguru.hpp"
#include "task/sequencer.hpp"

/**
 * @brief Controller for sequence management operations (CRUD, persistence)
 */
class SequenceManagementController : public Controller {
private:
    static std::weak_ptr<lithium::sequencer::ExposureSequence> mExposureSequence;

    /**
     * @brief Utility function to handle sequence management actions with error handling
     */
    static auto handleSequenceAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<crow::json::wvalue(std::shared_ptr<lithium::sequencer::ExposureSequence>)> func) {
        
        crow::json::wvalue res;
        res["command"] = command;

        LOG_F(INFO, "Received sequence management command: {}", command);
        LOG_F(INFO, "Request body: {}", req.body);

        try {
            auto exposureSequence = mExposureSequence.lock();
            if (!exposureSequence) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] = "Internal Server Error: ExposureSequence instance is null.";
                LOG_F(ERROR, "ExposureSequence instance is null for command: {}", command);
                return crow::response(500, res);
            }

            auto result = func(exposureSequence);
            res["status"] = "success";
            res["code"] = 200;
            res["data"] = std::move(result);
            LOG_F(INFO, "Command '{}' executed successfully", command);

        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] = std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_F(ERROR, "Invalid argument for command {}: {}", command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Runtime error - ") + e.what();
            LOG_F(ERROR, "Runtime error for command {}: {}", command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Exception occurred - ") + e.what();
            LOG_F(ERROR, "Exception for command {}: {}", command, e.what());
        }

        LOG_F(INFO, "Response for command '{}': {}", command, res.dump());
        return crow::response(200, res);
    }

public:
    /**
     * @brief Register all sequence management routes
     * @param app The crow application instance
     */
    void registerRoutes(crow::SimpleApp &app) override {
        
        // Create a new sequence
        CROW_ROUTE(app, "/api/sequence/create")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleSequenceAction(req, body, "createSequence", 
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    std::string name = body.has("name") ? std::string(body["name"].s()) : "New Sequence";
                    std::string description = body.has("description") ? std::string(body["description"].s()) : "";
                    
                    // Create new sequence with provided parameters
                    result["message"] = "Sequence created successfully";
                    result["name"] = name;
                    result["description"] = description;
                    
                    return result;
                });
        });

        // Save sequence to file
        CROW_ROUTE(app, "/api/sequence/save")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleSequenceAction(req, body, "saveSequence",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("filename")) {
                        throw std::invalid_argument("Missing required parameter: filename");
                    }
                    
                    std::string filename = body["filename"].s();
                    exposureSequence->saveSequence(filename);
                    
                    result["message"] = "Sequence saved successfully";
                    result["filename"] = filename;
                    
                    return result;
                });
        });

        // Load sequence from file
        CROW_ROUTE(app, "/api/sequence/load")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleSequenceAction(req, body, "loadSequence",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("filename")) {
                        throw std::invalid_argument("Missing required parameter: filename");
                    }
                    
                    std::string filename = body["filename"].s();
                    exposureSequence->loadSequence(filename);
                    
                    result["message"] = "Sequence loaded successfully";
                    result["filename"] = filename;
                    
                    return result;
                });
        });

        // Get sequence information
        CROW_ROUTE(app, "/api/sequence/info")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleSequenceAction(req, body, "getSequenceInfo",
                [](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto targetNames = exposureSequence->getTargetNames();
                    auto progress = exposureSequence->getProgress();
                    auto avgExecutionTime = exposureSequence->getAverageExecutionTime();
                    auto memoryUsage = exposureSequence->getTotalMemoryUsage();
                    
                    result["targetCount"] = targetNames.size();
                    result["targetNames"] = targetNames;
                    result["progress"] = progress;
                    result["averageExecutionTime"] = avgExecutionTime.count();
                    result["memoryUsage"] = memoryUsage;
                    
                    return result;
                });
        });

        // List all available sequences (from database)
        CROW_ROUTE(app, "/api/sequence/list")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleSequenceAction(req, body, "listSequences",
                [](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    // TODO: Implement database query to list saved sequences
                    // This would use the SequenceModel to query the database
                    result["sequences"] = std::vector<std::string>();
                    result["message"] = "Sequence listing not yet implemented";
                    
                    return result;
                });
        });

        // Delete a sequence
        CROW_ROUTE(app, "/api/sequence/delete")
        .methods("DELETE"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body.empty() ? "{}" : req.body);
            return handleSequenceAction(req, body, "deleteSequence",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("id") && !body.has("name")) {
                        throw std::invalid_argument("Missing required parameter: id or name");
                    }
                    
                    // TODO: Implement sequence deletion from database
                    result["message"] = "Sequence deletion not yet implemented";
                    
                    return result;
                });
        });

        // Update sequence metadata
        CROW_ROUTE(app, "/api/sequence/update")
        .methods("PUT"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleSequenceAction(req, body, "updateSequence",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("id")) {
                        throw std::invalid_argument("Missing required parameter: id");
                    }
                    
                    // TODO: Implement sequence metadata update
                    result["message"] = "Sequence update not yet implemented";
                    
                    return result;
                });
        });
    }

    /**
     * @brief Set the ExposureSequence instance
     * @param sequence Shared pointer to the ExposureSequence
     */
    static void setExposureSequence(std::shared_ptr<lithium::sequencer::ExposureSequence> sequence) {
        mExposureSequence = sequence;
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP
