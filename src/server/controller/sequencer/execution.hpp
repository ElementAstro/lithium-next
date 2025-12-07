/*
 * SequenceExecutionController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP
#define LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP

#include "../controller.hpp"
#include "server/utils/response.hpp"

#include <functional>
#include <memory>
#include <string>
#include "task/core/sequencer.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for sequence execution control operations
 */
class SequenceExecutionController : public Controller {
private:
    static std::weak_ptr<lithium::task::ExposureSequence> mExposureSequence;

    /**
     * @brief Utility function to handle execution actions with error handling
     */
    static auto handleExecutionAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<
            nlohmann::json(std::shared_ptr<lithium::task::ExposureSequence>)>
            func) {
        spdlog::info("Received execution command: {}", command);
        spdlog::info("Request body: {}", req.body);

        try {
            auto exposureSequence = mExposureSequence.lock();
            if (!exposureSequence) {
                spdlog::error(
                    "ExposureSequence instance is null for command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "ExposureSequence instance is null");
            }

            auto result = func(exposureSequence);
            spdlog::info("Command '{}' executed successfully", command);
            return ResponseBuilder::success(result, command);

        } catch (const std::invalid_argument& e) {
            spdlog::error("Invalid argument for command {}: {}", command,
                          e.what());
            return ResponseBuilder::badRequest(
                std::string("Invalid argument - ") + e.what());
        } catch (const std::runtime_error& e) {
            spdlog::error("Runtime error for command {}: {}", command,
                          e.what());
            return ResponseBuilder::internalError(
                std::string("Runtime error - ") + e.what());
        } catch (const std::exception& e) {
            spdlog::error("Exception for command {}: {}", command, e.what());
            return ResponseBuilder::internalError(
                std::string("Exception occurred - ") + e.what());
        }
    }

public:
    /**
     * @brief Register all sequence execution routes
     * @param app The crow application instance
     */
    void registerRoutes(lithium::server::ServerApp& app) override {
        // Execute all targets in sequence
        CROW_ROUTE(app, "/api/sequence/execute")
            .methods("POST"_method)([](const crow::request& req) {
                auto body =
                    crow::json::load(req.body.empty() ? "{}" : req.body);
                return handleExecutionAction(
                    req, body, "executeAll",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->executeAll();
                        result["message"] = "Sequence execution started";

                        return result;
                    });
            });

        // Stop sequence execution
        CROW_ROUTE(app, "/api/sequence/stop")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "stop",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->stop();
                        result["message"] = "Sequence execution stopped";

                        return result;
                    });
            });

        // Pause sequence execution
        CROW_ROUTE(app, "/api/sequence/pause")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "pause",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->pause();
                        result["message"] = "Sequence execution paused";

                        return result;
                    });
            });

        // Resume sequence execution
        CROW_ROUTE(app, "/api/sequence/resume")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "resume",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->resume();
                        result["message"] = "Sequence execution resumed";

                        return result;
                    });
            });

        // Get execution progress
        CROW_ROUTE(app, "/api/sequence/progress")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "getProgress",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto progress = exposureSequence->getProgress();
                        result["progress"] = progress;
                        result["percentage"] = progress;

                        return result;
                    });
            });

        // Get execution statistics
        CROW_ROUTE(app, "/api/sequence/stats")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "getExecutionStats",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        auto avgTime =
                            exposureSequence->getAverageExecutionTime();
                        auto memoryUsage =
                            exposureSequence->getTotalMemoryUsage();
                        auto progress = exposureSequence->getProgress();
                        auto executionStats =
                            exposureSequence->getExecutionStats();
                        auto resourceUsage =
                            exposureSequence->getResourceUsage();

                        result["averageExecutionTime"] = avgTime.count();
                        result["memoryUsage"] = memoryUsage;
                        result["progress"] = progress;
                        result["executionStats"] = executionStats;
                        result["resourceUsage"] = resourceUsage;

                        return result;
                    });
            });

        // Set scheduling strategy
        CROW_ROUTE(app, "/api/sequence/scheduling-strategy")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setSchedulingStrategy",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("strategy")) {
                            throw std::invalid_argument(
                                "Missing required parameter: strategy");
                        }

                        std::string strategyStr = body["strategy"].s();
                        lithium::task::ExposureSequence::SchedulingStrategy
                            strategy;

                        if (strategyStr == "FIFO") {
                            strategy = lithium::task::ExposureSequence::
                                SchedulingStrategy::FIFO;
                        } else if (strategyStr == "Priority") {
                            strategy = lithium::task::ExposureSequence::
                                SchedulingStrategy::Priority;
                        } else if (strategyStr == "Dependencies") {
                            strategy = lithium::task::ExposureSequence::
                                SchedulingStrategy::Dependencies;
                        } else {
                            throw std::invalid_argument(
                                "Invalid scheduling strategy: " + strategyStr);
                        }

                        exposureSequence->setSchedulingStrategy(strategy);
                        result["message"] =
                            "Scheduling strategy set successfully";
                        result["strategy"] = strategyStr;

                        return result;
                    });
            });

        // Set recovery strategy
        CROW_ROUTE(app, "/api/sequence/recovery-strategy")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setRecoveryStrategy",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("strategy")) {
                            throw std::invalid_argument(
                                "Missing required parameter: strategy");
                        }

                        std::string strategyStr = body["strategy"].s();
                        lithium::task::ExposureSequence::RecoveryStrategy
                            strategy;

                        if (strategyStr == "Stop") {
                            strategy = lithium::task::ExposureSequence::
                                RecoveryStrategy::Stop;
                        } else if (strategyStr == "Skip") {
                            strategy = lithium::task::ExposureSequence::
                                RecoveryStrategy::Skip;
                        } else if (strategyStr == "Retry") {
                            strategy = lithium::task::ExposureSequence::
                                RecoveryStrategy::Retry;
                        } else if (strategyStr == "Alternative") {
                            strategy = lithium::task::ExposureSequence::
                                RecoveryStrategy::Alternative;
                        } else {
                            throw std::invalid_argument(
                                "Invalid recovery strategy: " + strategyStr);
                        }

                        exposureSequence->setRecoveryStrategy(strategy);
                        result["message"] =
                            "Recovery strategy set successfully";
                        result["strategy"] = strategyStr;

                        return result;
                    });
            });

        // Set maximum concurrent targets
        CROW_ROUTE(app, "/api/sequence/max-concurrent")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setMaxConcurrentTargets",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("maxConcurrent")) {
                            throw std::invalid_argument(
                                "Missing required parameter: maxConcurrent");
                        }

                        int maxConcurrent = body["maxConcurrent"].i();
                        if (maxConcurrent < 0) {
                            throw std::invalid_argument(
                                "maxConcurrent must be >= 0");
                        }

                        exposureSequence->setMaxConcurrentTargets(
                            maxConcurrent);
                        result["message"] =
                            "Maximum concurrent targets set successfully";
                        result["maxConcurrent"] = maxConcurrent;

                        return result;
                    });
            });

        // Set global timeout
        CROW_ROUTE(app, "/api/sequence/timeout")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setGlobalTimeout",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("timeout")) {
                            throw std::invalid_argument(
                                "Missing required parameter: timeout");
                        }

                        int timeoutSeconds = body["timeout"].i();
                        if (timeoutSeconds < 0) {
                            throw std::invalid_argument("timeout must be >= 0");
                        }

                        auto timeout = std::chrono::seconds(timeoutSeconds);
                        exposureSequence->setGlobalTimeout(timeout);
                        result["message"] = "Global timeout set successfully";
                        result["timeout"] = timeoutSeconds;

                        return result;
                    });
            });

        // Set target timeout
        CROW_ROUTE(app, "/api/sequence/target/timeout")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setTargetTimeout",
                    [&body](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        if (!body.has("targetName") || !body.has("timeout")) {
                            throw std::invalid_argument(
                                "Missing required parameters: targetName, "
                                "timeout");
                        }

                        std::string targetName = body["targetName"].s();
                        int timeoutSeconds = body["timeout"].i();
                        if (timeoutSeconds < 0) {
                            throw std::invalid_argument("timeout must be >= 0");
                        }

                        auto timeout = std::chrono::seconds(timeoutSeconds);
                        exposureSequence->setTargetTimeout(targetName, timeout);
                        result["message"] = "Target timeout set successfully";
                        result["targetName"] = targetName;
                        result["timeout"] = timeoutSeconds;

                        return result;
                    });
            });

        // Retry failed targets
        CROW_ROUTE(app, "/api/sequence/retry-failed")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "retryFailedTargets",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->retryFailedTargets();
                        result["message"] = "Failed targets retry initiated";

                        return result;
                    });
            });

        // Skip failed targets
        CROW_ROUTE(app, "/api/sequence/skip-failed")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
                    req, body, "skipFailedTargets",
                    [](auto exposureSequence) -> nlohmann::json {
                        nlohmann::json result;

                        exposureSequence->skipFailedTargets();
                        result["message"] = "Failed targets skipped";

                        return result;
                    });
            });

        // Get failed targets
        CROW_ROUTE(app, "/api/sequence/failed-targets")
            .methods("GET"_method)([](const crow::request& req) {
                auto body = crow::json::load("{}");
                return handleExecutionAction(
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

inline std::weak_ptr<lithium::task::ExposureSequence>
    SequenceExecutionController::mExposureSequence;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP
