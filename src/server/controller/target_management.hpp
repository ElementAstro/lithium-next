/*
 * TargetManagementController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_TARGET_MANAGEMENT_HPP
#define LITHIUM_SERVER_CONTROLLER_TARGET_MANAGEMENT_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/log/loguru.hpp"
#include "task/sequencer.hpp"
#include "task/target.hpp"

/**
 * @brief Controller for target management operations
 */
class TargetManagementController : public Controller {
private:
    static std::weak_ptr<lithium::sequencer::ExposureSequence> mExposureSequence;

    /**
     * @brief Utility function to handle target actions with error handling
     */
    static auto handleTargetAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<crow::json::wvalue(std::shared_ptr<lithium::sequencer::ExposureSequence>)> func) {
        
        crow::json::wvalue res;
        res["command"] = command;

        LOG_F(INFO, "Received target management command: {}", command);
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
     * @brief Register all target management routes
     * @param app The crow application instance
     */
    void registerRoutes(crow::SimpleApp &app) override {
        
        // Add a target to the sequence
        CROW_ROUTE(app, "/api/targets/add")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTargetAction(req, body, "addTarget",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("name")) {
                        throw std::invalid_argument("Missing required parameter: name");
                    }
                    
                    std::string name = body["name"].s();
                    
                    // Create target with provided parameters
                    auto target = std::make_unique<lithium::sequencer::Target>(name);
                    
                    // Set optional parameters if provided
                    json targetParams;
                    if (body.has("ra")) {
                        targetParams["ra"] = body["ra"].d();
                    }
                    if (body.has("dec")) {
                        targetParams["dec"] = body["dec"].d();
                    }
                    if (body.has("priority")) {
                        targetParams["priority"] = body["priority"].i();
                    }
                    if (body.has("enabled")) {
                        target->setEnabled(body["enabled"].b());
                    }
                    
                    // Set parameters if any were provided
                    if (!targetParams.empty()) {
                        target->setParams(targetParams);
                    }
                    
                    exposureSequence->addTarget(std::move(target));
                    
                    result["message"] = "Target added successfully";
                    result["name"] = name;
                    
                    return result;
                });
        });

        // Remove a target from the sequence
        CROW_ROUTE(app, "/api/targets/remove")
        .methods("DELETE"_method)
        ([&](const crow::request& req) {
            auto body = crow::json::load(req.body.empty() ? "{}" : req.body);
            return handleTargetAction(req, body, "removeTarget",
                [&body, &req](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    std::string name;
                    if (body.has("name")) {
                        name = body["name"].s();
                    } else {
                        // Try to get name from query parameter
                        auto url_params = crow::query_string(req.url_params);
                        name = url_params.get("name");
                        if (name.empty()) {
                            throw std::invalid_argument("Missing required parameter: name");
                        }
                    }
                    
                    exposureSequence->removeTarget(name);
                    
                    result["message"] = "Target removed successfully";
                    result["name"] = name;
                    
                    return result;
                });
        });

        // Modify an existing target
        CROW_ROUTE(app, "/api/targets/modify")
        .methods("PUT"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTargetAction(req, body, "modifyTarget",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("name")) {
                        throw std::invalid_argument("Missing required parameter: name");
                    }
                    
                    std::string name = body["name"].s();
                    
                    // Create a modifier function based on the provided parameters
                    lithium::sequencer::TargetModifier modifier = [&body](lithium::sequencer::Target& target) {
                        json targetParams;
                        if (body.has("ra")) {
                            targetParams["ra"] = body["ra"].d();
                        }
                        if (body.has("dec")) {
                            targetParams["dec"] = body["dec"].d();
                        }
                        if (body.has("priority")) {
                            targetParams["priority"] = body["priority"].i();
                        }
                        if (body.has("enabled")) {
                            target.setEnabled(body["enabled"].b());
                        }
                        
                        // Set parameters if any were provided
                        if (!targetParams.empty()) {
                            target.setParams(targetParams);
                        }
                    };
                    
                    exposureSequence->modifyTarget(name, modifier);
                    
                    result["message"] = "Target modified successfully";
                    result["name"] = name;
                    
                    return result;
                });
        });

        // Get all target names
        CROW_ROUTE(app, "/api/targets/list")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTargetAction(req, body, "getTargetNames",
                [](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto targetNames = exposureSequence->getTargetNames();
                    result["targets"] = targetNames;
                    result["count"] = targetNames.size();
                    
                    return result;
                });
        });

        // Get target status
        CROW_ROUTE(app, "/api/targets/status")
        .methods("GET"_method)
        ([&](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTargetAction(req, body, "getTargetStatus",
                [req](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto url_params = crow::query_string(req.url_params);
                    std::string name = url_params.get("name");
                    
                    if (name.empty()) {
                        throw std::invalid_argument("Missing required parameter: name");
                    }
                    
                    auto status = exposureSequence->getTargetStatus(name);
                    
                    result["name"] = name;
                    result["status"] = static_cast<int>(status);
                    
                    // Convert status enum to string for better readability
                    switch (status) {
                        case lithium::sequencer::TargetStatus::Pending:
                            result["statusText"] = "Pending";
                            break;
                        case lithium::sequencer::TargetStatus::InProgress:
                            result["statusText"] = "InProgress";
                            break;
                        case lithium::sequencer::TargetStatus::Completed:
                            result["statusText"] = "Completed";
                            break;
                        case lithium::sequencer::TargetStatus::Failed:
                            result["statusText"] = "Failed";
                            break;
                        case lithium::sequencer::TargetStatus::Skipped:
                            result["statusText"] = "Skipped";
                            break;
                        default:
                            result["statusText"] = "Unknown";
                            break;
                    }
                    
                    return result;
                });
        });

        // Add alternative target for recovery
        CROW_ROUTE(app, "/api/targets/alternative")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTargetAction(req, body, "addAlternativeTarget",
                [&body](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("targetName") || !body.has("alternativeName")) {
                        throw std::invalid_argument("Missing required parameters: targetName, alternativeName");
                    }
                    
                    std::string targetName = body["targetName"].s();
                    std::string alternativeName = body["alternativeName"].s();
                    
                    // Create alternative target
                    auto alternative = std::make_unique<lithium::sequencer::Target>(alternativeName);
                    
                    // Set alternative target parameters
                    json altParams;
                    if (body.has("ra")) {
                        altParams["ra"] = body["ra"].d();
                    }
                    if (body.has("dec")) {
                        altParams["dec"] = body["dec"].d();
                    }
                    if (body.has("priority")) {
                        altParams["priority"] = body["priority"].i();
                    }
                    
                    // Set parameters if any were provided
                    if (!altParams.empty()) {
                        alternative->setParams(altParams);
                    }
                    
                    exposureSequence->addAlternativeTarget(targetName, std::move(alternative));
                    
                    result["message"] = "Alternative target added successfully";
                    result["targetName"] = targetName;
                    result["alternativeName"] = alternativeName;
                    
                    return result;
                });
        });

        // Get failed targets
        CROW_ROUTE(app, "/api/targets/failed")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTargetAction(req, body, "getFailedTargets",
                [](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto failedTargets = exposureSequence->getFailedTargets();
                    result["failedTargets"] = failedTargets;
                    result["count"] = failedTargets.size();
                    
                    return result;
                });
        });

        // Retry failed targets
        CROW_ROUTE(app, "/api/targets/retry")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body.empty() ? "{}" : req.body);
            return handleTargetAction(req, body, "retryFailedTargets",
                [](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    exposureSequence->retryFailedTargets();
                    
                    result["message"] = "Failed targets retry initiated";
                    
                    return result;
                });
        });

        // Get target details (comprehensive information)
        CROW_ROUTE(app, "/api/targets/details")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTargetAction(req, body, "getTargetDetails",
                [req](auto exposureSequence) -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto url_params = crow::query_string(req.url_params);
                    std::string name = url_params.get("name");
                    
                    if (name.empty()) {
                        // Return details for all targets
                        auto targetNames = exposureSequence->getTargetNames();
                        std::vector<crow::json::wvalue> targets;
                        
                        for (const auto& targetName : targetNames) {
                            crow::json::wvalue targetInfo;
                            targetInfo["name"] = targetName;
                            targetInfo["status"] = static_cast<int>(exposureSequence->getTargetStatus(targetName));
                            targets.push_back(std::move(targetInfo));
                        }
                        
                        result["targets"] = std::move(targets);
                    } else {
                        // Return details for specific target
                        auto status = exposureSequence->getTargetStatus(name);
                        result["name"] = name;
                        result["status"] = static_cast<int>(status);
                    }
                    
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

#endif // LITHIUM_SERVER_CONTROLLER_TARGET_MANAGEMENT_HPP
