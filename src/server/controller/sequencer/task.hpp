/*
 * TaskManagementController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP
#define LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP

#include "../controller.hpp"

#include <functional>
#include <string>
#include <ctime>
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"

// Import specific camera task types
#include "task/custom/camera/basic_exposure.hpp"
#include "../../task/custom/camera/focus_tasks.hpp"
#include "../../task/custom/camera/filter_tasks.hpp"
#include "../../task/custom/camera/guide_tasks.hpp"
#include "../../task/custom/camera/calibration_tasks.hpp"
#include "../../task/custom/solver_task.hpp"

// Asynchronous task manager
#include "../../task_manager.hpp"

// Task factory for device/script/config/search tasks
#include "../../task/custom/factory.hpp"
#include "task/custom/search_task.hpp"

using json = nlohmann::json;

/**
 * @brief Controller for task management and creation operations
 */
class TaskManagementController : public Controller {
private:
    static std::shared_ptr<lithium::server::TaskManager> task_manager_;

    static auto taskStatusToString(lithium::server::TaskManager::Status status)
        -> std::string {
        using Status = lithium::server::TaskManager::Status;
        switch (status) {
            case Status::Pending:
                return "Pending";
            case Status::Running:
                return "Running";
            case Status::Completed:
                return "Completed";
            case Status::Failed:
                return "Failed";
            case Status::Cancelled:
                return "Cancelled";
        }
        return "Unknown";
    }

    static auto mapToFactoryTaskType(const std::string& taskType)
        -> std::string {
        if (taskType == "DeviceTask" || taskType == "device_task") {
            return "device_task";
        }
        if (taskType == "ScriptTask" || taskType == "script_task") {
            return "script_task";
        }
        if (taskType == "ConfigTask" || taskType == "config_task") {
            return "config_task";
        }
        if (taskType == "SearchTask" || taskType == "search_task" ||
            taskType == "CelestialSearch") {
            return "CelestialSearch";
        }
        return taskType;
    }

    /**
     * @brief Utility function to handle task actions with error handling
     */
    static auto handleTaskAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<crow::json::wvalue()> func) {
        
        crow::json::wvalue res;
        res["command"] = command;

        LOG_INFO( "Received task management command: {}", command);
        LOG_INFO( "Request body: {}", req.body);

        try {
            auto result = func();
            res["status"] = "success";
            res["code"] = 200;
            res["data"] = std::move(result);
            LOG_INFO( "Command '{}' executed successfully", command);

        } catch (const std::invalid_argument& e) {
            res["status"] = "error";
            res["code"] = 400;
            res["error"] = std::string("Bad Request: Invalid argument - ") + e.what();
            LOG_ERROR( "Invalid argument for command {}: {}", command, e.what());
        } catch (const std::runtime_error& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Runtime error - ") + e.what();
            LOG_ERROR( "Runtime error for command {}: {}", command, e.what());
        } catch (const std::exception& e) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] = std::string("Internal Server Error: Exception occurred - ") + e.what();
            LOG_ERROR( "Exception for command {}: {}", command, e.what());
        }

        LOG_INFO( "Response for command '{}': {}", command, res.dump());
        return crow::response(200, res);
    }

public:
    static void setTaskManager(
        std::shared_ptr<lithium::server::TaskManager> manager) {
        task_manager_ = std::move(manager);
    }

    /**
     * @brief Register all task management routes
     * @param app The crow application instance
     */
    void registerRoutes(lithium::server::ServerApp &app) override {
        
        // ===== CAMERA TASKS =====
        
        // Create generic camera task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createCameraTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("taskType")) {
                        throw std::invalid_argument("Missing required parameter: taskType");
                    }
                    
                    std::string taskType = body["taskType"].s();
                    json params;
                    
                    // Extract parameters from body (excluding taskType)
                    // Convert crow::json to nlohmann::json for easier parameter handling
                    if (body.has("exposure")) params["exposure"] = body["exposure"].d();
                    if (body.has("count")) params["count"] = body["count"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("x")) params["x"] = body["x"].i();
                    if (body.has("y")) params["y"] = body["y"].i();
                    if (body.has("width")) params["width"] = body["width"].i();
                    if (body.has("height")) params["height"] = body["height"].i();
                    if (body.has("temperature")) params["temperature"] = body["temperature"].d();
                    if (body.has("cooler")) params["cooler"] = body["cooler"].b();
                    if (body.has("delay")) params["delay"] = body["delay"].d();
                    if (body.has("step_size")) params["step_size"] = body["step_size"].i();
                    if (body.has("max_steps")) params["max_steps"] = body["max_steps"].i();
                    if (body.has("focuser")) params["focuser"] = body["focuser"].s();
                    if (body.has("filter_wheel")) params["filter_wheel"] = body["filter_wheel"].s();
                    if (body.has("guide_camera")) params["guide_camera"] = body["guide_camera"].s();
                    if (body.has("guide_exposure")) params["guide_exposure"] = body["guide_exposure"].d();
                    if (body.has("settle_time")) params["settle_time"] = body["settle_time"].d();
                    if (body.has("dark_count")) params["dark_count"] = body["dark_count"].i();
                    if (body.has("bias_count")) params["bias_count"] = body["bias_count"].i();
                    if (body.has("flat_count")) params["flat_count"] = body["flat_count"].i();
                    if (body.has("dark_exposure")) params["dark_exposure"] = body["dark_exposure"].d();
                    if (body.has("r_exposure")) params["r_exposure"] = body["r_exposure"].d();
                    if (body.has("g_exposure")) params["g_exposure"] = body["g_exposure"].d();
                    if (body.has("b_exposure")) params["b_exposure"] = body["b_exposure"].d();
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    // Dispatch to asynchronous task manager
                    std::string id = task_manager_->submitTask(
                        taskType, params,
                        [taskType](const auto& info) {
                            auto& factory = lithium::task::TaskFactory::getInstance();
                            std::string factoryType = taskType;
                            
                            // Try to adapt task type to factory name if needed
                            if (!factory.isTaskRegistered(factoryType)) {
                                // Try removing "Task" suffix (e.g. TakeExposureTask -> TakeExposure)
                                if (factoryType.size() > 4 && 
                                    factoryType.rfind("Task") == factoryType.size() - 4) {
                                    std::string stripped = factoryType.substr(0, factoryType.size() - 4);
                                    if (factory.isTaskRegistered(stripped)) {
                                        factoryType = stripped;
                                    }
                                }
                            }

                            if (factory.isTaskRegistered(factoryType)) {
                                auto task = factory.createTask(factoryType, info->id, info->params);
                                if (task) {
                                    task->execute(info->params);
                                } else {
                                    throw std::runtime_error("Failed to create task instance: " + factoryType);
                                }
                            } else {
                                throw std::invalid_argument(
                                    "Unsupported or unregistered camera task type: " +
                                    taskType);
                            }
                        });

                    result["message"] =
                        "Camera task submitted for asynchronous execution";
                    result["taskType"] = taskType;
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create exposure task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/exposure")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createExposureTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    // Validate required parameters
                    if (!body.has("exposure")) {
                        throw std::invalid_argument("Missing required parameter: exposure");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "TakeExposure", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("TakeExposure", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Exposure task submitted for asynchronous execution";
                    result["taskType"] = "TakeExposure";
                    result["taskId"] = id;
                    result["exposureTime"] = body["exposure"].d();
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create multiple exposures task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/exposures")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createMultipleExposuresTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    // Validate required parameters
                    if (!body.has("exposure") || !body.has("count")) {
                        throw std::invalid_argument("Missing required parameters: exposure, count");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["exposure"] = body["exposure"].d();
                    params["count"] = body["count"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("delay")) params["delay"] = body["delay"].d();
                    
                    std::string id = task_manager_->submitTask(
                        "TakeManyExposure", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("TakeManyExposure", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Multiple exposures task submitted for asynchronous execution";
                    result["taskType"] = "TakeManyExposure";
                    result["taskId"] = id;
                    result["exposureTime"] = body["exposure"].d();
                    result["count"] = body["count"].i();
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create subframe exposure task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/subframe")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createSubframeExposureTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    // Validate required parameters
                    if (!body.has("exposure") || !body.has("x") || !body.has("y") || 
                        !body.has("width") || !body.has("height")) {
                        throw std::invalid_argument("Missing required parameters: exposure, x, y, width, height");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["exposure"] = body["exposure"].d();
                    params["x"] = body["x"].i();
                    params["y"] = body["y"].i();
                    params["width"] = body["width"].i();
                    params["height"] = body["height"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "SubframeExposure", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("SubframeExposure", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Subframe exposure task submitted for asynchronous execution";
                    result["taskType"] = "SubframeExposure";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create camera settings task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/settings")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createCameraSettingsTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("temperature")) params["temperature"] = body["temperature"].d();
                    if (body.has("cooler")) params["cooler"] = body["cooler"].b();
                    
                    std::string id = task_manager_->submitTask(
                        "CameraSettings", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("CameraSettings", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Camera settings task submitted for asynchronous execution";
                    result["taskType"] = "CameraSettings";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create camera preview task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/preview")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createCameraPreviewTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    if (body.has("exposure")) params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "CameraPreview", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("CameraPreview", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Camera preview task submitted for asynchronous execution";
                    result["taskType"] = "CameraPreview";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create auto focus task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/autofocus")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createAutoFocusTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    if (body.has("exposure")) params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("step_size")) params["step_size"] = body["step_size"].i();
                    if (body.has("max_steps")) params["max_steps"] = body["max_steps"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("focuser")) params["focuser"] = body["focuser"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "AutoFocus", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("AutoFocus", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Auto focus task submitted for asynchronous execution";
                    result["taskType"] = "AutoFocus";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create filter sequence task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/filter-sequence")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createFilterSequenceTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("filters") || !body.has("exposure")) {
                        throw std::invalid_argument("Missing required parameters: filters, exposure");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["filters"] = body["filters"];
                    params["exposure"] = body["exposure"].d();
                    if (body.has("count")) params["count"] = body["count"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("filter_wheel")) params["filter_wheel"] = body["filter_wheel"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "FilterSequence", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("FilterSequence", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Filter sequence task submitted for asynchronous execution";
                    result["taskType"] = "FilterSequence";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create RGB sequence task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/rgb-sequence")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createRGBSequenceTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    if (body.has("r_exposure")) params["r_exposure"] = body["r_exposure"].d();
                    if (body.has("g_exposure")) params["g_exposure"] = body["g_exposure"].d();
                    if (body.has("b_exposure")) params["b_exposure"] = body["b_exposure"].d();
                    if (body.has("count")) params["count"] = body["count"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("filter_wheel")) params["filter_wheel"] = body["filter_wheel"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "RGBSequence", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("RGBSequence", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "RGB sequence task submitted for asynchronous execution";
                    result["taskType"] = "RGBSequence";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create guided exposure task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/guided-exposure")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createGuidedExposureTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("exposure")) {
                        throw std::invalid_argument("Missing required parameter: exposure");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["exposure"] = body["exposure"].d();
                    if (body.has("guide_exposure")) params["guide_exposure"] = body["guide_exposure"].d();
                    if (body.has("settle_time")) params["settle_time"] = body["settle_time"].d();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("guide_camera")) params["guide_camera"] = body["guide_camera"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "GuidedExposure", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("GuidedExposure", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Guided exposure task submitted for asynchronous execution";
                    result["taskType"] = "GuidedExposure";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create auto calibration task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/camera/auto-calibration")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createAutoCalibrationTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    if (body.has("dark_count")) params["dark_count"] = body["dark_count"].i();
                    if (body.has("bias_count")) params["bias_count"] = body["bias_count"].i();
                    if (body.has("flat_count")) params["flat_count"] = body["flat_count"].i();
                    if (body.has("dark_exposure")) params["dark_exposure"] = body["dark_exposure"].d();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    std::string id = task_manager_->submitTask(
                        "AutoCalibration", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("AutoCalibration", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Auto calibration task submitted for asynchronous execution";
                    result["taskType"] = "AutoCalibration";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // Create solver task (asynchronous)
        CROW_ROUTE(app, "/api/tasks/solver/solve")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createSolverTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("filePath")) {
                        throw std::invalid_argument("Missing required parameter: filePath");
                    }
                    
                    if (!task_manager_) {
                        throw std::runtime_error("TaskManager is not initialized");
                    }

                    json params;
                    params["filePath"] = body["filePath"].s();
                    if (body.has("ra")) params["ra"] = body["ra"].d();
                    if (body.has("dec")) params["dec"] = body["dec"].d();
                    if (body.has("scale")) params["scale"] = body["scale"].d();
                    if (body.has("radius")) params["radius"] = body["radius"].d();
                    
                    // Ensure SolverTask is registered (idempotent check)
                    auto& factory = lithium::task::TaskFactory::getInstance();
                    if (!factory.isTaskRegistered("SolverTask")) {
                        lithium::task::TaskInfo info;
                        info.name = "SolverTask";
                        info.description = "Plate solve an image";
                        info.category = "Astrometry";
                        info.requiredParameters = {"filePath"};
                        factory.registerTask<lithium::task::SolverTask>(
                            "SolverTask",
                            [](const std::string& name, const json& config) {
                                return std::make_unique<lithium::task::SolverTask>(name, config);
                            },
                            info
                        );
                    }

                    std::string id = task_manager_->submitTask(
                        "SolverTask", params,
                        [](const auto& info) {
                            auto task = lithium::task::TaskFactory::getInstance().createTask("SolverTask", info->id, info->params);
                            task->execute(info->params);
                        });

                    result["message"] = "Solver task submitted for asynchronous execution";
                    result["taskType"] = "SolverTask";
                    result["taskId"] = id;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // ===== TASK STATUS AND MONITORING =====
        
        // Get task status
        CROW_ROUTE(app, "/api/tasks/status/<string>")
        .methods("GET"_method)
        ([](const crow::request& req, const std::string& taskId) {
            auto body = crow::json::load("{}");
            return handleTaskAction(req, body, "getTaskStatus",
                [&taskId]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    auto info = task_manager_->getTask(taskId);
                    if (!info) {
                        result["taskId"] = taskId;
                        result["exists"] = false;
                        result["taskStatus"] = "NotFound";
                        return result;
                    }

                    result["taskId"] = taskId;
                    result["exists"] = true;
                    result["taskType"] = info->type;
                    result["taskStatus"] =
                        taskStatusToString(info->status);
                    result["cancelRequested"] =
                        info->cancelRequested.load();
                    if (!info->error.empty()) {
                        result["error"] = info->error;
                    }
                    if (!info->result.is_null()) {
                        result["result"] = crow::json::load(
                            info->result.dump());
                    }
                    
                    return result;
                });
        });

        // List active tasks
        CROW_ROUTE(app, "/api/tasks/active")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTaskAction(req, body, "getActiveTasks",
                []() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    auto active = task_manager_->listActiveTasks();
                    std::vector<crow::json::wvalue> taskList;
                    taskList.reserve(active.size());

                    for (const auto& t : active) {
                        crow::json::wvalue item;
                        item["taskId"] = t->id;
                        item["taskType"] = t->type;
                        item["taskStatus"] =
                            taskStatusToString(t->status);
                        item["cancelRequested"] =
                            t->cancelRequested.load();
                        taskList.push_back(std::move(item));
                    }

                    result["tasks"] = std::move(taskList);
                    result["count"] = active.size();
                    
                    return result;
                });
        });

        // Cancel task
        CROW_ROUTE(app, "/api/tasks/cancel/<string>")
        .methods("DELETE"_method)
        ([](const crow::request& req, const std::string& taskId) {
            auto body = crow::json::load("{}");
            return handleTaskAction(req, body, "cancelTask",
                [&taskId]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    bool ok = task_manager_->cancelTask(taskId);
                    result["taskId"] = taskId;
                    result["requested"] = ok;
                    result["message"] =
                        ok ? "Cancellation requested"
                           : "Task not found";
                    
                    return result;
                });
        });

        // ===== DEVICE TASKS =====
        
        // Create generic device task
        CROW_ROUTE(app, "/api/tasks/device")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createDeviceTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!body.has("operation") || !body.has("deviceName")) {
                        throw std::invalid_argument("Missing required parameters: operation, deviceName");
                    }
                    
                    std::string operation = body["operation"].s();
                    std::string deviceName = body["deviceName"].s();
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    json params;
                    params["operation"] = operation;
                    params["deviceName"] = deviceName;
                    if (body.has("deviceType"))
                        params["deviceType"] = body["deviceType"].s();
                    if (body.has("timeout"))
                        params["timeout"] = body["timeout"].i();
                    if (body.has("retryCount"))
                        params["retryCount"] = body["retryCount"].i();
                    if (body.has("port"))
                        params["port"] = body["port"].s();
                    if (body.has("config")) {
                        // Expect JSON string/object; convert via dump/parse
                        nlohmann::json cfg = nlohmann::json::parse(
                            static_cast<std::string>(body["config"]));
                        params["config"] = std::move(cfg);
                    }

                    std::string id = task_manager_->submitTask(
                        "DeviceTask", params,
                        [](const auto& info) {
                            using lithium::task::TaskFactory;
                            auto& factory = TaskFactory::getInstance();
                            auto task = factory.createTask(
                                "device_task", info->id,
                                nlohmann::json::object());
                            if (!task) {
                                throw std::runtime_error(
                                    "Failed to create device_task");
                            }
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Device task submitted for asynchronous execution";
                    result["taskType"] = "DeviceTask";
                    result["taskId"] = id;
                    result["operation"] = operation;
                    result["deviceName"] = deviceName;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // ===== SCRIPT TASKS =====
        
        // Create script task
        CROW_ROUTE(app, "/api/tasks/script")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createScriptTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    // Support either explicit scriptName/scriptContent or a
                    // single 'script' field treated as content
                    std::string scriptName;
                    if (body.has("scriptName")) {
                        scriptName = body["scriptName"].s();
                    } else {
                        // Generate a simple name if not provided
                        scriptName = "script_" +
                                     std::to_string(
                                         static_cast<long long>(
                                             std::time(nullptr)));
                    }

                    json params;
                    params["scriptName"] = scriptName;

                    if (body.has("script")) {
                        params["scriptContent"] = body["script"].s();
                    }
                    if (body.has("scriptContent")) {
                        params["scriptContent"] =
                            body["scriptContent"].s();
                    }
                    if (body.has("allowUnsafe")) {
                        params["allowUnsafe"] = body["allowUnsafe"].b();
                    }
                    if (body.has("timeout")) {
                        params["timeout"] = body["timeout"].i();
                    }
                    if (body.has("retryCount")) {
                        params["retryCount"] = body["retryCount"].i();
                    }

                    std::string id = task_manager_->submitTask(
                        "ScriptTask", params,
                        [](const auto& info) {
                            using lithium::task::TaskFactory;
                            auto& factory = TaskFactory::getInstance();
                            auto task = factory.createTask(
                                "script_task", info->id,
                                nlohmann::json::object());
                            if (!task) {
                                throw std::runtime_error(
                                    "Failed to create script_task");
                            }
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Script task submitted for asynchronous execution";
                    result["taskType"] = "ScriptTask";
                    result["taskId"] = id;
                    result["scriptName"] = scriptName;
                    result["status"] = "queued";
                    
                    return result;
                });
        });

        // ===== CONFIG TASKS =====
        
        // Create configuration management task (ConfigTask)
        CROW_ROUTE(app, "/api/tasks/config")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body.empty() ? "{}" : req.body);
            return handleTaskAction(req, body, "createConfigTask",
                [&req]() -> crow::json::wvalue {
                    crow::json::wvalue result;

                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    nlohmann::json params;
                    try {
                        params = nlohmann::json::parse(
                            req.body.empty() ? "{}" : req.body);
                    } catch (const std::exception& e) {
                        throw std::invalid_argument(
                            std::string("Invalid JSON body: ") + e.what());
                    }

                    std::string id = task_manager_->submitTask(
                        "ConfigTask", params,
                        [](const auto& info) {
                            using lithium::task::TaskFactory;
                            auto& factory = TaskFactory::getInstance();
                            auto task = factory.createTask(
                                "config_task", info->id,
                                nlohmann::json::object());
                            if (!task) {
                                throw std::runtime_error(
                                    "Failed to create config_task");
                            }
                            task->execute(info->params);
                        });

                    result["message"] =
                        "Config task submitted for asynchronous execution";
                    result["taskType"] = "ConfigTask";
                    result["taskId"] = id;
                    result["status"] = "queued";

                    return result;
                });
        });

        // ===== SEARCH TASKS =====
        
        // Create celestial search task (SearchTask)
        CROW_ROUTE(app, "/api/tasks/search")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body.empty() ? "{}" : req.body);
            return handleTaskAction(req, body, "createSearchTask",
                [&req]() -> crow::json::wvalue {
                    crow::json::wvalue result;

                    if (!task_manager_) {
                        throw std::runtime_error(
                            "TaskManager is not initialized");
                    }

                    nlohmann::json params;
                    try {
                        params = nlohmann::json::parse(
                            req.body.empty() ? "{}" : req.body);
                    } catch (const std::exception& e) {
                        throw std::invalid_argument(
                            std::string("Invalid JSON body: ") + e.what());
                    }

                    std::string id = task_manager_->submitTask(
                        "SearchTask", params,
                        [](const auto& info) {
                            using lithium::task::TaskFactory;
                            auto& factory = TaskFactory::getInstance();
                            auto taskBase = factory.createTask(
                                "CelestialSearch", info->id,
                                nlohmann::json::object());
                            if (!taskBase) {
                                throw std::runtime_error(
                                    "Failed to create CelestialSearch task");
                            }

                            using lithium::task::task::TaskCelestialSearch;
                            auto* searchTask =
                                dynamic_cast<TaskCelestialSearch*>(
                                    taskBase.get());

                            taskBase->execute(info->params);

                            if (searchTask) {
                                info->result = searchTask->getLastResults();
                            }
                        });

                    result["message"] =
                        "Search task submitted for asynchronous execution";
                    result["taskType"] = "SearchTask";
                    result["taskId"] = id;
                    result["status"] = "queued";

                    return result;
                });
        });

        // ===== TASK INFORMATION =====
        
        // Get available task types
        CROW_ROUTE(app, "/api/tasks/types")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTaskAction(req, body, "getTaskTypes",
                []() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    using lithium::task::TaskFactory;
                    auto& factory = TaskFactory::getInstance();
                    auto tasksByCategory = factory.getTasksByCategory();

                    std::vector<std::string> cameraTaskTypes;
                    std::vector<std::string> deviceTaskTypes;
                    std::vector<std::string> otherTaskTypes;
                    std::vector<std::string> allTaskTypes;

                    crow::json::wvalue categoriesJson;

                    for (const auto& [category, tasks] : tasksByCategory) {
                        std::vector<std::string> namesInCategory;
                        namesInCategory.reserve(tasks.size());

                        for (const auto& info : tasks) {
                            namesInCategory.push_back(info.name);
                            allTaskTypes.push_back(info.name);

                            // Map TaskInfo.category to legacy groups
                            if (category == "camera") {
                                cameraTaskTypes.push_back(info.name);
                            } else if (category == "hardware") {
                                deviceTaskTypes.push_back(info.name);
                            } else {
                                otherTaskTypes.push_back(info.name);
                            }
                        }

                        categoriesJson[category] = namesInCategory;
                    }

                    result["camera"] = cameraTaskTypes;
                    result["device"] = deviceTaskTypes;
                    result["other"] = otherTaskTypes;
                    result["categories"] = std::move(categoriesJson);
                    result["all"] = allTaskTypes;

                    return result;
                });
        });

        // Get task parameters schema
        CROW_ROUTE(app, "/api/tasks/schema")
        .methods("GET"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load("{}");
            return handleTaskAction(req, body, "getTaskSchema",
                [&req]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto url_params = crow::query_string(req.url_params);
                    std::string taskType = url_params.get("type");
                    
                    if (taskType.empty()) {
                        throw std::invalid_argument("Missing required parameter: type");
                    }

                    // Map external name to factory-registered type
                    std::string factoryType = mapToFactoryTaskType(taskType);

                    using lithium::task::TaskFactory;
                    auto& factory = TaskFactory::getInstance();
                    auto infoOpt = factory.getTaskInfo(factoryType);
                    if (!infoOpt.has_value()) {
                        throw std::invalid_argument(
                            "Unknown task type: " + taskType);
                    }

                    const auto& info = infoOpt.value();
                    result["taskType"] = taskType;
                    result["factoryType"] = factoryType;
                    result["name"] = info.name;
                    result["description"] = info.description;
                    result["category"] = info.category;
                    result["version"] = info.version;
                    result["requiredParameters"] = info.requiredParameters;
                    result["parameterSchema"] = crow::json::load(
                        info.parameterSchema.dump());
                    
                    return result;
                });
        });
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP
