/*
 * SequenceController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_ASYNC_EXPOSURE_SEQUENCE_CONTROLLER_HPP
#define LITHIUM_ASYNC_EXPOSURE_SEQUENCE_CONTROLLER_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "task/sequencer.hpp"

class SequenceController : public Controller {
private:
    static std::weak_ptr<lithium::sequencer::ExposureSequence>
        mExposureSequence;

    // Utility function to handle all exposure sequence actions
    static auto handleExposureSequenceAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<
            bool(std::shared_ptr<lithium::sequencer::ExposureSequence>)>
            func) {
        crow::json::wvalue res;
        res["command"] = command;

        LOG_F(INFO, "Received command: {}", command);
        LOG_F(INFO, "Request body: {}", req.body);

        try {
            auto exposureSequence = mExposureSequence.lock();
            if (!exposureSequence) {
                res["status"] = "error";
                res["code"] = 500;
                res["error"] =
                    "Internal Server Error: ExposureSequence instance is null.";
                LOG_F(
                    ERROR,
                    "ExposureSequence instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return crow::response(500, res);
            } else {
                LOG_F(INFO,
                      "ExposureSequence instance acquired successfully for "
                      "command: {}",
                      command);
                bool success = func(exposureSequence);
                if (success) {
                    res["status"] = "success";
                    res["code"] = 200;
                    LOG_F(INFO, "Command '{}' executed successfully", command);
                } else {
                    res["status"] = "error";
                    res["code"] = 404;
                    res["error"] = "Not Found: The specified operation failed.";
                    LOG_F(WARNING, "Command '{}' failed to execute", command);
                }
            }
        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] =
                std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_F(ERROR,
                  "Invalid argument while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Runtime error - ") +
                e.what();
            LOG_F(ERROR,
                  "Runtime error while executing command: {}. Exception: {}",
                  command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                std::string("Internal Server Error: Exception occurred - ") +
                e.what();
            LOG_F(
                ERROR,
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
        }

        LOG_F(INFO, "Response for command '{}': {}", command, res.dump());
        return crow::response(200, res);
    }

public:
    void registerRoutes(crow::SimpleApp& app) override {
        // Create a weak pointer to the ExposureSequence
        GET_OR_CREATE_WEAK_PTR(mExposureSequence,
                               lithium::sequencer::ExposureSequence,
                               Constants::EXPOSURE_SEQUENCE);
        // Define the routes
        CROW_ROUTE(app, "/exposure_sequence/addTarget")
            .methods("POST"_method)(&SequenceController::addTarget, this);
        CROW_ROUTE(app, "/exposure_sequence/removeTarget")
            .methods("POST"_method)(&SequenceController::removeTarget, this);
        CROW_ROUTE(app, "/exposure_sequence/modifyTarget")
            .methods("POST"_method)(&SequenceController::modifyTarget, this);
        CROW_ROUTE(app, "/exposure_sequence/executeAll")
            .methods("POST"_method)(&SequenceController::executeAll, this);
        CROW_ROUTE(app, "/exposure_sequence/stop")
            .methods("POST"_method)(&SequenceController::stop, this);
        CROW_ROUTE(app, "/exposure_sequence/pause")
            .methods("POST"_method)(&SequenceController::pause, this);
        CROW_ROUTE(app, "/exposure_sequence/resume")
            .methods("POST"_method)(&SequenceController::resume, this);
        CROW_ROUTE(app, "/exposure_sequence/saveSequence")
            .methods("POST"_method)(&SequenceController::saveSequence, this);
        CROW_ROUTE(app, "/exposure_sequence/loadSequence")
            .methods("POST"_method)(&SequenceController::loadSequence, this);
        CROW_ROUTE(app, "/exposure_sequence/getTargetNames")
            .methods("GET"_method)(&SequenceController::getTargetNames, this);
        CROW_ROUTE(app, "/exposure_sequence/getTargetStatus")
            .methods("POST"_method)(&SequenceController::getTargetStatus, this);
        CROW_ROUTE(app, "/exposure_sequence/getProgress")
            .methods("GET"_method)(&SequenceController::getProgress, this);
        CROW_ROUTE(app, "/exposure_sequence/setSchedulingStrategy")
            .methods("POST"_method)(&SequenceController::setSchedulingStrategy,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/setRecoveryStrategy")
            .methods("POST"_method)(&SequenceController::setRecoveryStrategy,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/addAlternativeTarget")
            .methods("POST"_method)(&SequenceController::addAlternativeTarget,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/setMaxConcurrentTargets")
            .methods("POST"_method)(
                &SequenceController::setMaxConcurrentTargets, this);
        CROW_ROUTE(app, "/exposure_sequence/setGlobalTimeout")
            .methods("POST"_method)(&SequenceController::setGlobalTimeout,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/getFailedTargets")
            .methods("GET"_method)(&SequenceController::getFailedTargets, this);
        CROW_ROUTE(app, "/exposure_sequence/retryFailedTargets")
            .methods("POST"_method)(&SequenceController::retryFailedTargets,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/getExecutionStats")
            .methods("GET"_method)(&SequenceController::getExecutionStats,
                                   this);
        CROW_ROUTE(app, "/exposure_sequence/setTargetTaskParams")
            .methods("POST"_method)(&SequenceController::setTargetTaskParams,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/getTargetTaskParams")
            .methods("POST"_method)(&SequenceController::getTargetTaskParams,
                                    this);
        CROW_ROUTE(app, "/exposure_sequence/setTargetParams")
            .methods("POST"_method)(&SequenceController::setTargetParams, this);
        CROW_ROUTE(app, "/exposure_sequence/getTargetParams")
            .methods("POST"_method)(&SequenceController::getTargetParams, this);
    }

    // Endpoint to add a target
    void addTarget(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "addTarget", [&](auto exposureSequence) {
                auto target = std::make_unique<lithium::sequencer::Target>(
                    body["name"].s());
                exposureSequence->addTarget(std::move(target));
                return true;
            });
    }

    // Endpoint to remove a target
    void removeTarget(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "removeTarget", [&](auto exposureSequence) {
                exposureSequence->removeTarget(body["name"].s());
                return true;
            });
    }

    // Endpoint to modify a target
    void modifyTarget(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "modifyTarget", [&](auto exposureSequence) {
                exposureSequence->modifyTarget(body["name"].s(),
                                               [&](auto& target) {
                                                   // Modify the target based on
                                                   // the provided parameters
                                                   // This is a placeholder for
                                                   // actual modification logic
                                               });
                return true;
            });
    }

    // Endpoint to execute all targets
    void executeAll(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(req, crow::json::rvalue{},
                                           "executeAll",
                                           [&](auto exposureSequence) {
                                               exposureSequence->executeAll();
                                               return true;
                                           });
    }

    // Endpoint to stop the sequence
    void stop(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(req, crow::json::rvalue{}, "stop",
                                           [&](auto exposureSequence) {
                                               exposureSequence->stop();
                                               return true;
                                           });
    }

    // Endpoint to pause the sequence
    void pause(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(req, crow::json::rvalue{}, "pause",
                                           [&](auto exposureSequence) {
                                               exposureSequence->pause();
                                               return true;
                                           });
    }

    // Endpoint to resume the sequence
    void resume(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(req, crow::json::rvalue{}, "resume",
                                           [&](auto exposureSequence) {
                                               exposureSequence->resume();
                                               return true;
                                           });
    }

    // Endpoint to save the sequence
    void saveSequence(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "saveSequence", [&](auto exposureSequence) {
                exposureSequence->saveSequence(body["filename"].s());
                return true;
            });
    }

    // Endpoint to load the sequence
    void loadSequence(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "loadSequence", [&](auto exposureSequence) {
                exposureSequence->loadSequence(body["filename"].s());
                return true;
            });
    }

    // Endpoint to get target names
    void getTargetNames(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(
            req, crow::json::rvalue{}, "getTargetNames",
            [&](auto exposureSequence) {
                auto names = exposureSequence->getTargetNames();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["targetNames"] = names;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to get target status
    void getTargetStatus(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "getTargetStatus", [&](auto exposureSequence) {
                auto status =
                    exposureSequence->getTargetStatus(body["name"].s());
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["targetStatus"] = static_cast<int>(status);
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to get progress
    void getProgress(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(
            req, crow::json::rvalue{}, "getProgress",
            [&](auto exposureSequence) {
                auto progress = exposureSequence->getProgress();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["progress"] = progress;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to set scheduling strategy
    void setSchedulingStrategy(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setSchedulingStrategy", [&](auto exposureSequence) {
                auto strategy = static_cast<
                    lithium::sequencer::ExposureSequence::SchedulingStrategy>(
                    body["strategy"].i());
                exposureSequence->setSchedulingStrategy(strategy);
                return true;
            });
    }

    // Endpoint to set recovery strategy
    void setRecoveryStrategy(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setRecoveryStrategy", [&](auto exposureSequence) {
                auto strategy = static_cast<
                    lithium::sequencer::ExposureSequence::RecoveryStrategy>(
                    body["strategy"].i());
                exposureSequence->setRecoveryStrategy(strategy);
                return true;
            });
    }

    // Endpoint to add an alternative target
    void addAlternativeTarget(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "addAlternativeTarget", [&](auto exposureSequence) {
                auto alternative = std::make_unique<lithium::sequencer::Target>(
                    body["alternativeName"].s());
                exposureSequence->addAlternativeTarget(body["targetName"].s(),
                                                       std::move(alternative));
                return true;
            });
    }

    // Endpoint to set max concurrent targets
    void setMaxConcurrentTargets(const crow::request& req,
                                 crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setMaxConcurrentTargets", [&](auto exposureSequence) {
                exposureSequence->setMaxConcurrentTargets(body["max"].u());
                return true;
            });
    }

    // Endpoint to set global timeout
    void setGlobalTimeout(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setGlobalTimeout", [&](auto exposureSequence) {
                exposureSequence->setGlobalTimeout(
                    std::chrono::seconds(body["timeout"].i()));
                return true;
            });
    }

    // Endpoint to get failed targets
    void getFailedTargets(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(
            req, crow::json::rvalue{}, "getFailedTargets",
            [&](auto exposureSequence) {
                auto failedTargets = exposureSequence->getFailedTargets();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["failedTargets"] = failedTargets;
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to retry failed targets
    void retryFailedTargets(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(
            req, crow::json::rvalue{}, "retryFailedTargets",
            [&](auto exposureSequence) {
                exposureSequence->retryFailedTargets();
                return true;
            });
    }

    // Endpoint to get execution stats
    void getExecutionStats(const crow::request& req, crow::response& res) {
        res = handleExposureSequenceAction(
            req, crow::json::rvalue{}, "getExecutionStats",
            [&](auto exposureSequence) {
                auto stats = exposureSequence->getExecutionStats();
                crow::json::wvalue response;
                response["status"] = "success";
                response["code"] = 200;
                response["stats"] = stats.dump();
                res.write(response.dump());
                return true;
            });
    }

    // Endpoint to set target task parameters
    void setTargetTaskParams(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setTargetTaskParams", [&](auto exposureSequence) {
                exposureSequence->setTargetTaskParams(body["targetName"].s(),
                                                      body["taskUUID"].s(),
                                                      body["params"]);
                return true;
            });
    }

    // Endpoint to get target task parameters
    void getTargetTaskParams(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "getTargetTaskParams", [&](auto exposureSequence) {
                auto params = exposureSequence->getTargetTaskParams(
                    body["targetName"].s(), body["taskUUID"].s());
                if (params) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["params"] = params.value().dump();
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }

    // Endpoint to set target parameters
    void setTargetParams(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "setTargetParams", [&](auto exposureSequence) {
                exposureSequence->setTargetParams(body["targetName"].s(),
                                                  body["params"]);
                return true;
            });
    }

    // Endpoint to get target parameters
    void getTargetParams(const crow::request& req, crow::response& res) {
        auto body = crow::json::load(req.body);
        res = handleExposureSequenceAction(
            req, body, "getTargetParams", [&](auto exposureSequence) {
                auto params =
                    exposureSequence->getTargetParams(body["targetName"].s());
                if (params) {
                    crow::json::wvalue response;
                    response["status"] = "success";
                    response["code"] = 200;
                    response["params"] = params.value().dump();
                    res.write(response.dump());
                    return true;
                }
                return false;
            });
    }
};

inline std::weak_ptr<lithium::sequencer::ExposureSequence>
    SequenceController::mExposureSequence;

#endif  // LITHIUM_ASYNC_EXPOSURE_SEQUENCE_CONTROLLER_HPP
