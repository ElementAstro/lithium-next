/*
 * SequenceManagementController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP
#define LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP

#include "../controller.hpp"
#include "server/utils/response.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/log/spdlog_logger.hpp"
#include "task/core/sequencer.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for sequence management operations (CRUD, persistence)
 */
class SequenceManagementController : public Controller {
private:
    static std::weak_ptr<lithium::task::ExposureSequence> mExposureSequence;

    /**
     * @brief Utility function to handle sequence management actions with error
     * handling
     */
    static auto handleSequenceAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<
            nlohmann::json(std::shared_ptr<lithium::task::ExposureSequence>)>
            func) {
        LOG_INFO("Received sequence management command: {}", command);
        LOG_INFO("Request body: {}", req.body);

        try {
            auto exposureSequence = mExposureSequence.lock();
            if (!exposureSequence) {
                LOG_ERROR("ExposureSequence instance is null for command: {}",
                          command);
                return ResponseBuilder::internalError(
                    "ExposureSequence instance is null");
            }

            auto result = func(exposureSequence);
            LOG_INFO("Command '{}' executed successfully", command);
            return ResponseBuilder::success(result, command);

        } catch (const std::invalid_argument& e) {
            LOG_ERROR("Invalid argument for command {}: {}", command, e.what());
            return ResponseBuilder::badRequest(
                std::string("Invalid argument - ") + e.what());
        } catch (const std::runtime_error& e) {
            LOG_ERROR("Runtime error for command {}: {}", command, e.what());
            return ResponseBuilder::internalError(
                std::string("Runtime error - ") + e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Exception for command {}: {}", command, e.what());
            return ResponseBuilder::internalError(
                std::string("Exception occurred - ") + e.what());
        }
    }

public:
    /**
     * @brief Register all sequence management routes
     * @param app The crow application instance
     */
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Save sequence to file
        CROW_ROUTE(app, "/api/sequence/save")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleSequenceAction(
                    req, body, "saveSequence",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("filename")) {
                            throw std::invalid_argument(
                                "Missing required parameter: filename");
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
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleSequenceAction(
                    req, body, "loadSequence",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("filename")) {
                            throw std::invalid_argument(
                                "Missing required parameter: filename");
                        }

                        std::string filename = body["filename"].s();
                        exposureSequence->loadSequence(filename);

                        result["message"] = "Sequence loaded successfully";
                        result["filename"] = filename;

                        return result;
                    });
            });

        // Save sequence to database
        CROW_ROUTE(app, "/api/sequence/save-db")
            .methods("POST"_method)([](const crow::request& req) {
                auto body =
                    crow::json::load(req.body.empty() ? "{}" : req.body);
                return handleSequenceAction(
                    req, body, "saveToDatabase",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->saveToDatabase();
                        result["message"] =
                            "Sequence saved to database successfully";

                        return result;
                    });
            });

        // Load sequence from database
        CROW_ROUTE(app, "/api/sequence/load-db")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleSequenceAction(
                    req, body, "loadFromDatabase",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("uuid")) {
                            throw std::invalid_argument(
                                "Missing required parameter: uuid");
                        }

                        std::string uuid = body["uuid"].s();
                        exposureSequence->loadFromDatabase(uuid);

                        result["message"] =
                            "Sequence loaded from database successfully";
                        result["uuid"] = uuid;

                        return result;
                    });
            });

        // List all available sequences (from database)
        CROW_ROUTE(app, "/api/sequence/list")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleSequenceAction(
                    req, body, "listSequences",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto sequences = exposureSequence->listSequences();

                        std::vector<nlohmann::json> sequenceList;
                        for (const auto& seq : sequences) {
                            nlohmann::json seqJson;
                            seqJson["uuid"] = seq.uuid;
                            seqJson["name"] = seq.name;
                            seqJson["createdAt"] = seq.createdAt;
                            sequenceList.push_back(std::move(seqJson));
                        }

                        result["sequences"] = std::move(sequenceList);
                        result["count"] = sequences.size();

                        return result;
                    });
            });

        // Delete a sequence from database
        CROW_ROUTE(app, "/api/sequence/delete")
            .methods("DELETE"_method)([](const crow::request& req) {
                auto body =
                    crow::json::load(req.body.empty() ? "{}" : req.body);
                return handleSequenceAction(
                    req, body, "deleteSequence",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("uuid")) {
                            throw std::invalid_argument(
                                "Missing required parameter: uuid");
                        }

                        std::string uuid = body["uuid"].s();
                        exposureSequence->deleteFromDatabase(uuid);

                        result["message"] = "Sequence deleted successfully";
                        result["uuid"] = uuid;

                        return result;
                    });
            });

        // Get sequence information
        CROW_ROUTE(app, "/api/sequence/info")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleSequenceAction(
                    req, body, "getSequenceInfo",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto targetNames = exposureSequence->getTargetNames();
                        auto progress = exposureSequence->getProgress();
                        auto avgExecutionTime =
                            exposureSequence->getAverageExecutionTime();
                        auto memoryUsage =
                            exposureSequence->getTotalMemoryUsage();
                        auto executionStats =
                            exposureSequence->getExecutionStats();
                        auto resourceUsage =
                            exposureSequence->getResourceUsage();
                        auto failedTargets =
                            exposureSequence->getFailedTargets();

                        result["targetCount"] = targetNames.size();
                        result["targetNames"] = targetNames;
                        result["progress"] = progress;
                        result["averageExecutionTime"] =
                            avgExecutionTime.count();
                        result["memoryUsage"] = memoryUsage;
                        result["failedTargets"] = failedTargets;
                        result["executionStats"] = executionStats;
                        result["resourceUsage"] = resourceUsage;

                        return result;
                    });
            });

        // Get target status
        CROW_ROUTE(app, "/api/sequence/target/status")
            .methods("GET"_method)([](const crow::request& req) {
                auto name = req.url_params.get("name");
                if (!name) {
                    return ResponseBuilder::badRequest(
                        "Missing required parameter: name");
                }

                auto body = crow::json::load("{}");
                return handleSequenceAction(
                    req, body, "getTargetStatus",
                    [name](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto status = exposureSequence->getTargetStatus(name);
                        result["targetName"] = name;
                        result["status"] = static_cast<int>(status);

                        return result;
                    });
            });

        // Set target parameters
        CROW_ROUTE(app, "/api/sequence/target/params")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleSequenceAction(
                    req, body, "setTargetParams",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("targetName") || !body.has("params")) {
                            throw std::invalid_argument(
                                "Missing required parameters: targetName, "
                                "params");
                        }

                        std::string targetName = body["targetName"].s();
                        // Convert crow::json to nlohmann::json
                        nlohmann::json params = nlohmann::json::parse(
                            static_cast<std::string>(body["params"]));

                        exposureSequence->setTargetParams(targetName, params);

                        result["message"] =
                            "Target parameters set successfully";
                        result["targetName"] = targetName;

                        return result;
                    });
            });

        // Get target parameters
        CROW_ROUTE(app, "/api/sequence/target/params")
            .methods("GET"_method)([](const crow::request& req) {
                auto name = req.url_params.get("name");
                if (!name) {
                    return ResponseBuilder::badRequest(
                        "Missing required parameter: name");
                }

                auto body = crow::json::load("{}");
                return handleSequenceAction(
                    req, body, "getTargetParams",
                    [name](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto params = exposureSequence->getTargetParams(name);
                        result["targetName"] = name;

                        if (params.has_value()) {
                            result["params"] = *params;
                        } else {
                            result["params"] = nullptr;
                        }

                        return result;
                    });
            });

        // Set target task parameters
        CROW_ROUTE(app, "/api/sequence/target/task/params")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleSequenceAction(
                    req, body, "setTargetTaskParams",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("targetName") || !body.has("taskUUID") ||
                            !body.has("params")) {
                            throw std::invalid_argument(
                                "Missing required parameters: targetName, "
                                "taskUUID, params");
                        }

                        std::string targetName = body["targetName"].s();
                        std::string taskUUID = body["taskUUID"].s();
                        nlohmann::json params = nlohmann::json::parse(
                            static_cast<std::string>(body["params"]));

                        exposureSequence->setTargetTaskParams(targetName,
                                                              taskUUID, params);

                        result["message"] =
                            "Target task parameters set successfully";
                        result["targetName"] = targetName;
                        result["taskUUID"] = taskUUID;

                        return result;
                    });
            });

        // Get target task parameters
        CROW_ROUTE(app, "/api/sequence/target/task/params")
            .methods("GET"_method)([](const crow::request& req) {
                auto targetName = req.url_params.get("targetName");
                auto taskUUID = req.url_params.get("taskUUID");

                if (!targetName || !taskUUID) {
                    return ResponseBuilder::badRequest(
                        "Missing required parameters: targetName, taskUUID");
                }

                auto body = crow::json::load("{}");
                return handleSequenceAction(
                    req, body, "getTargetTaskParams",
                    [targetName,
                     taskUUID](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto params = exposureSequence->getTargetTaskParams(
                            targetName, taskUUID);
                        result["targetName"] = targetName;
                        result["taskUUID"] = taskUUID;

                        if (params.has_value()) {
                            result["params"] = *params;
                        } else {
                            result["params"] = nullptr;
                        }

                        return result;
                    });
            });
    }

    /**
     * @brief Set the ExposureSequence instance
     * @param sequence Shared pointer to the ExposureSequence
     */
    static void setExposureSequence(
        std::shared_ptr<lithium::task::ExposureSequence> sequence) {
        mExposureSequence = sequence;
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SEQUENCE_MANAGEMENT_HPP
