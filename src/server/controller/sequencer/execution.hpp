/*
 * SequenceExecutionController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP
#define LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP

#include "../controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include "task/sequencer.hpp"

/**
 * @brief Controller for sequence execution control operations
 */
class SequenceExecutionController : public Controller {
private:
    static std::weak_ptr<lithium::task::ExposureSequence>
        mExposureSequence;

    /**
     * @brief Utility function to handle execution actions with error handling
     */
    static auto handleExecutionAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<crow::json::wvalue(
            std::shared_ptr<lithium::task::ExposureSequence>)>
            func) {
        crow::json::wvalue res;
        res["command"] = command;

        spdlog::info("Received execution command: {}", command);
        spdlog::info("Request body: {}", req.body);

        try {
            auto exposureSequence = mExposureSequence.lock();
            if (!exposureSequence) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ExposureSequence instance is null.";
                spdlog::error(
                    "ExposureSequence instance is null for command: {}",
                    command);
                return crow::response(500, res);
            }

            auto result = func(exposureSequence);
            res["status"] = "success";
            res["code"] = 200;
            res["data"] = std::move(result);
            spdlog::info("Command '{}' executed successfully", command);

        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] =
                std::string("Bad Request: Invalid argument - ") + e.what();
            spdlog::error("Invalid argument for command {}: {}", command,
                          e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Runtime error - ") +
                e.what();
            spdlog::error("Runtime error for command {}: {}", command,
                          e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
            spdlog::error("Exception for command {}: {}", command, e.what());
        }

        spdlog::info("Response for command '{}': {}", command, res.dump());
        return crow::response(200, res);
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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                        result["executionStats"] =
                            crow::json::load(executionStats.dump());
                        result["resourceUsage"] =
                            crow::json::load(resourceUsage.dump());

                        return result;
                    });
            });

        // Set scheduling strategy
        CROW_ROUTE(app, "/api/sequence/scheduling-strategy")
            .methods("PUT"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                return handleExecutionAction(
                    req, body, "setSchedulingStrategy",
                    [&body](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [&body](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [&body](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [&body](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [&body](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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
                    [](auto exposureSequence) -> crow::json::wvalue {
                        crow::json::wvalue result;

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

inline std::weak_ptr<lithium::task::ExposureSequence> SequenceExecutionController::mExposureSequence;

#endif  // LITHIUM_SERVER_CONTROLLER_SEQUENCE_EXECUTION_HPP
