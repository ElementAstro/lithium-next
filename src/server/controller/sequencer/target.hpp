/*
 * TargetController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_TARGET_HPP
#define LITHIUM_SERVER_CONTROLLER_TARGET_HPP

#include "../controller.hpp"
#include "server/utils/response.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/log/spdlog_logger.hpp"
#include "task/core/sequencer.hpp"
#include "task/core/target.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Comprehensive controller for target management operations
 * Consolidates all target-related functionality including:
 * - Basic CRUD operations
 * - Priority and dependency management
 * - Alternative targets and recovery
 * - Status monitoring and retry mechanisms
 */
class TargetController : public Controller {
private:
    static std::weak_ptr<lithium::task::ExposureSequence> mExposureSequence;

    /**
     * @brief Utility function to handle target actions with comprehensive error
     * handling
     */
    static auto handleTargetAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<
            nlohmann::json(std::shared_ptr<lithium::task::ExposureSequence>)>
            func) {
        LOG_INFO("Received target command: {}", command);
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

    /**
     * @brief Helper function to extract target name from request
     */
    static std::string extractTargetName(const crow::request& req,
                                         const crow::json::rvalue& body) {
        if (body.has("name")) {
            return body["name"].s();
        }

        auto url_params = crow::query_string(req.url_params);
        std::string name = url_params.get("name");
        if (!name.empty()) {
            return name;
        }

        throw std::invalid_argument("Missing required parameter: name");
    }

    /**
     * @brief Helper function to create target with parameters
     */
    static std::unique_ptr<lithium::task::Target> createTargetFromJson(
        const std::string& name, const crow::json::rvalue& body) {
        auto target = std::make_unique<lithium::task::Target>(name);

        // Set basic properties
        if (body.has("enabled")) {
            target->setEnabled(body["enabled"].b());
        }

        // Set coordinate and priority parameters
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
        if (body.has("altitude")) {
            targetParams["altitude"] = body["altitude"].d();
        }
        if (body.has("azimuth")) {
            targetParams["azimuth"] = body["azimuth"].d();
        }

        // Set parameters if any were provided
        if (!targetParams.empty()) {
            target->setParams(targetParams);
        }

        return target;
    }

public:
    /**
     * @brief Register all consolidated target management routes
     * @param app The lithium::server::ServerApp instance
     */
    void registerRoutes(lithium::server::ServerApp& app) override {
        // ==================== BASIC CRUD OPERATIONS ====================

        // Add a target to the sequence
        CROW_ROUTE(app, "/api/targets/add")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "addTarget",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("name")) {
                            throw std::invalid_argument(
                                "Missing required parameter: name");
                        }

                        std::string name = body["name"].s();
                        auto target = createTargetFromJson(name, body);

                        exposureSequence->addTarget(std::move(target));

                        result["message"] = "Target added successfully";
                        result["name"] = name;
                        return result;
                    });
            });

        // Remove a target from the sequence
        CROW_ROUTE(app, "/api/targets/remove")
            .methods("DELETE"_method)([](const crow::request& req) {
                auto body =
                    crow::json::load(req.body.empty() ? "{}" : req.body);
                return handleTargetAction(
                    req, body, "removeTarget",
                    [&body, &req](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        std::string name = extractTargetName(req, body);
                        exposureSequence->removeTarget(name);

                        result["message"] = "Target removed successfully";
                        result["name"] = name;
                        return result;
                    });
            });

        // Modify an existing target
        CROW_ROUTE(app, "/api/targets/modify")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "modifyTarget",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("name")) {
                            throw std::invalid_argument(
                                "Missing required parameter: name");
                        }

                        std::string name = body["name"].s();

                        lithium::task::TargetModifier modifier =
                            [&body](lithium::task::Target& target) {
                                if (body.has("enabled")) {
                                    target.setEnabled(body["enabled"].b());
                                }

                                json targetParams;
                                if (body.has("ra")) {
                                    targetParams["ra"] = body["ra"].d();
                                }
                                if (body.has("dec")) {
                                    targetParams["dec"] = body["dec"].d();
                                }
                                if (body.has("priority")) {
                                    targetParams["priority"] =
                                        body["priority"].i();
                                }
                                if (body.has("altitude")) {
                                    targetParams["altitude"] =
                                        body["altitude"].d();
                                }
                                if (body.has("azimuth")) {
                                    targetParams["azimuth"] =
                                        body["azimuth"].d();
                                }

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

        // ==================== TARGET LISTING AND STATUS ====================

        // Get all target names
        CROW_ROUTE(app, "/api/targets/list")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "getTargetNames",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto targetNames = exposureSequence->getTargetNames();
                        result["targets"] = targetNames;
                        result["count"] = targetNames.size();
                        return result;
                    });
            });

        // Get target status
        CROW_ROUTE(app, "/api/targets/status")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "getTargetStatus",
                    [&req](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        std::string name =
                            extractTargetName(req, crow::json::rvalue{});
                        auto status = exposureSequence->getTargetStatus(name);

                        result["name"] = name;
                        result["status"] = static_cast<int>(status);

                        // Convert status enum to string for better readability
                        switch (status) {
                            case lithium::task::TargetStatus::Pending:
                                result["statusText"] = "Pending";
                                break;
                            case lithium::task::TargetStatus::InProgress:
                                result["statusText"] = "InProgress";
                                break;
                            case lithium::task::TargetStatus::Completed:
                                result["statusText"] = "Completed";
                                break;
                            case lithium::task::TargetStatus::Failed:
                                result["statusText"] = "Failed";
                                break;
                            case lithium::task::TargetStatus::Skipped:
                                result["statusText"] = "Skipped";
                                break;
                            default:
                                result["statusText"] = "Unknown";
                                break;
                        }

                        return result;
                    });
            });

        // Get comprehensive target details
        CROW_ROUTE(app, "/api/targets/details")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "getTargetDetails",
                    [&req](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto url_params = crow::query_string(req.url_params);
                        std::string name = url_params.get("name");

                        if (name.empty()) {
                            // Return details for all targets
                            auto targetNames =
                                exposureSequence->getTargetNames();
                            std::vector<nlohmann::json> targets;

                            for (const auto& targetName : targetNames) {
                                nlohmann::json targetInfo;
                                targetInfo["name"] = targetName;

                                auto status = exposureSequence->getTargetStatus(
                                    targetName);
                                targetInfo["status"] = static_cast<int>(status);

                                // Add readiness check
                                targetInfo["isReady"] =
                                    exposureSequence->isTargetReady(targetName);

                                // Add dependencies
                                auto dependencies =
                                    exposureSequence->getTargetDependencies(
                                        targetName);
                                targetInfo["dependencies"] = dependencies;

                                targets.push_back(std::move(targetInfo));
                            }

                            result["targets"] = std::move(targets);
                            result["totalCount"] = targets.size();
                        } else {
                            // Return details for specific target
                            auto status =
                                exposureSequence->getTargetStatus(name);
                            result["name"] = name;
                            result["status"] = static_cast<int>(status);
                            result["isReady"] =
                                exposureSequence->isTargetReady(name);
                            result["dependencies"] =
                                exposureSequence->getTargetDependencies(name);
                        }

                        return result;
                    });
            });

        // ==================== PRIORITY MANAGEMENT ====================

        // Set target priority
        CROW_ROUTE(app, "/api/targets/priority")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "setTargetPriority",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("name") || !body.has("priority")) {
                            throw std::invalid_argument(
                                "Missing required parameters: name, priority");
                        }

                        std::string name = body["name"].s();
                        int priority = body["priority"].i();

                        exposureSequence->setTargetPriority(name, priority);

                        result["message"] = "Target priority set successfully";
                        result["name"] = name;
                        result["priority"] = priority;
                        return result;
                    });
            });

        // ==================== DEPENDENCY MANAGEMENT ====================

        // Add target dependency
        CROW_ROUTE(app, "/api/targets/dependencies/add")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "addTargetDependency",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("name") || !body.has("dependsOn")) {
                            throw std::invalid_argument(
                                "Missing required parameters: name, dependsOn");
                        }

                        std::string name = body["name"].s();
                        std::string dependsOn = body["dependsOn"].s();

                        exposureSequence->addTargetDependency(name, dependsOn);

                        result["message"] =
                            "Target dependency added successfully";
                        result["name"] = name;
                        result["dependsOn"] = dependsOn;
                        return result;
                    });
            });

        // Remove target dependency
        CROW_ROUTE(app, "/api/targets/dependencies/remove")
            .methods("DELETE"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "removeTargetDependency",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("name") || !body.has("dependsOn")) {
                            throw std::invalid_argument(
                                "Missing required parameters: name, dependsOn");
                        }

                        std::string name = body["name"].s();
                        std::string dependsOn = body["dependsOn"].s();

                        exposureSequence->removeTargetDependency(name,
                                                                 dependsOn);

                        result["message"] =
                            "Target dependency removed successfully";
                        result["name"] = name;
                        result["dependsOn"] = dependsOn;
                        return result;
                    });
            });

        // Get target dependencies
        CROW_ROUTE(app, "/api/targets/dependencies")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "getTargetDependencies",
                    [&req](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        std::string name =
                            extractTargetName(req, crow::json::rvalue{});
                        auto dependencies =
                            exposureSequence->getTargetDependencies(name);

                        result["name"] = name;
                        result["dependencies"] = dependencies;
                        result["count"] = dependencies.size();
                        return result;
                    });
            });

        // Check if target is ready
        CROW_ROUTE(app, "/api/targets/ready")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "isTargetReady",
                    [&req](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        std::string name =
                            extractTargetName(req, crow::json::rvalue{});
                        bool isReady = exposureSequence->isTargetReady(name);

                        result["name"] = name;
                        result["isReady"] = isReady;
                        return result;
                    });
            });

        // ==================== ALTERNATIVE TARGETS AND RECOVERY
        // ====================

        // Add alternative target for recovery
        CROW_ROUTE(app, "/api/targets/alternatives/add")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "addAlternativeTarget",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("targetName") ||
                            !body.has("alternativeName")) {
                            throw std::invalid_argument(
                                "Missing required parameters: targetName, "
                                "alternativeName");
                        }

                        std::string targetName = body["targetName"].s();
                        std::string alternativeName =
                            body["alternativeName"].s();

                        auto alternative =
                            createTargetFromJson(alternativeName, body);
                        exposureSequence->addAlternativeTarget(
                            targetName, std::move(alternative));

                        result["message"] =
                            "Alternative target added successfully";
                        result["targetName"] = targetName;
                        result["alternativeName"] = alternativeName;
                        return result;
                    });
            });

        // Get failed targets
        CROW_ROUTE(app, "/api/targets/failed")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleTargetAction(
                    req, body, "getFailedTargets",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto failedTargets =
                            exposureSequence->getFailedTargets();
                        result["failedTargets"] = failedTargets;
                        result["count"] = failedTargets.size();
                        return result;
                    });
            });

        // Retry failed targets
        CROW_ROUTE(app, "/api/targets/retry")
            .methods("POST"_method)([](const crow::request& req) {
                auto body =
                    crow::json::load(req.body.empty() ? "{}" : req.body);
                return handleTargetAction(
                    req, body, "retryFailedTargets",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->retryFailedTargets();
                        result["message"] = "Failed targets retry initiated";
                        return result;
                    });
            });

        // ==================== BATCH OPERATIONS ====================

        // Batch add targets
        CROW_ROUTE(app, "/api/targets/batch/add")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "batchAddTargets",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("targets")) {
                            throw std::invalid_argument(
                                "Missing required parameter: targets array");
                        }

                        auto targets = body["targets"];
                        std::vector<std::string> addedTargets;
                        std::vector<std::string> failedTargets;

                        for (const auto& targetData : targets) {
                            try {
                                if (!targetData.has("name")) {
                                    throw std::invalid_argument(
                                        "Target missing name");
                                }

                                std::string name = targetData["name"].s();
                                auto target =
                                    createTargetFromJson(name, targetData);
                                exposureSequence->addTarget(std::move(target));
                                addedTargets.push_back(name);

                            } catch (const std::exception& e) {
                                failedTargets.push_back(
                                    (targetData.has("name")
                                         ? std::string(targetData["name"].s())
                                         : std::string("unknown")));
                            }
                        }

                        result["message"] = "Batch add targets completed";
                        result["addedTargets"] = addedTargets;
                        result["failedTargets"] = failedTargets;
                        result["successCount"] = addedTargets.size();
                        result["failureCount"] = failedTargets.size();
                        return result;
                    });
            });

        // Batch remove targets
        CROW_ROUTE(app, "/api/targets/batch/remove")
            .methods("DELETE"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleTargetAction(
                    req, body, "batchRemoveTargets",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("names")) {
                            throw std::invalid_argument(
                                "Missing required parameter: names array");
                        }

                        auto names = body["names"];
                        std::vector<std::string> removedTargets;
                        std::vector<std::string> failedTargets;

                        for (const auto& name : names) {
                            try {
                                std::string targetName = name.s();
                                exposureSequence->removeTarget(targetName);
                                removedTargets.push_back(targetName);
                            } catch (const std::exception& e) {
                                failedTargets.push_back(name.s());
                            }
                        }

                        result["message"] = "Batch remove targets completed";
                        result["removedTargets"] = removedTargets;
                        result["failedTargets"] = failedTargets;
                        result["successCount"] = removedTargets.size();
                        result["failureCount"] = failedTargets.size();
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

    /**
     * @brief Get the current ExposureSequence instance
     * @return Shared pointer to the ExposureSequence (may be null)
     */
    static std::shared_ptr<lithium::task::ExposureSequence>
    getExposureSequence() {
        return mExposureSequence.lock();
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_TARGET_HPP
