/*
 * TaskManagementController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP
#define LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP

#include "controller.hpp"

#include <functional>
#include <string>
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// Import specific camera task types
#include "../../task/custom/camera/basic_exposure.hpp"
#include "../../task/custom/camera/focus_tasks.hpp"
#include "../../task/custom/camera/filter_tasks.hpp"
#include "../../task/custom/camera/guide_tasks.hpp"
#include "../../task/custom/camera/calibration_tasks.hpp"

using json = nlohmann::json;

/**
 * @brief Controller for task management and creation operations
 */
class TaskManagementController : public Controller {
private:
    /**
     * @brief Utility function to handle task actions with error handling
     */
    static auto handleTaskAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<crow::json::wvalue()> func) {
        
        crow::json::wvalue res;
        res["command"] = command;

        LOG_F(INFO, "Received task management command: {}", command);
        LOG_F(INFO, "Request body: {}", req.body);

        try {
            auto result = func();
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
     * @brief Register all task management routes
     * @param app The crow application instance
     */
    void registerRoutes(crow::SimpleApp &app) override {
        
        // ===== CAMERA TASKS =====
        
        // Create generic camera task
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
                    
                    std::unique_ptr<lithium::sequencer::Task> task;
                    
                    // Create task based on type
                    if (taskType == "TakeExposureTask") {
                        task = lithium::sequencer::task::TakeExposureTask::createEnhancedTask();
                        lithium::sequencer::task::TakeExposureTask::execute(params);
                    } else if (taskType == "TakeManyExposureTask") {
                        task = lithium::sequencer::task::TakeManyExposureTask::createEnhancedTask();
                        lithium::sequencer::task::TakeManyExposureTask::execute(params);
                    } else if (taskType == "SubframeExposureTask") {
                        task = lithium::sequencer::task::SubframeExposureTask::createEnhancedTask();
                        lithium::sequencer::task::SubframeExposureTask::execute(params);
                    } else if (taskType == "CameraSettingsTask") {
                        task = lithium::sequencer::task::CameraSettingsTask::createEnhancedTask();
                        lithium::sequencer::task::CameraSettingsTask::execute(params);
                    } else if (taskType == "CameraPreviewTask") {
                        task = lithium::sequencer::task::CameraPreviewTask::createEnhancedTask();
                        lithium::sequencer::task::CameraPreviewTask::execute(params);
                    } else if (taskType == "AutoFocusTask") {
                        task = lithium::sequencer::task::AutoFocusTask::createEnhancedTask();
                        lithium::sequencer::task::AutoFocusTask::execute(params);
                    } else if (taskType == "FilterSequenceTask") {
                        task = lithium::sequencer::task::FilterSequenceTask::createEnhancedTask();
                        lithium::sequencer::task::FilterSequenceTask::execute(params);
                    } else if (taskType == "RGBSequenceTask") {
                        task = lithium::sequencer::task::RGBSequenceTask::createEnhancedTask();
                        lithium::sequencer::task::RGBSequenceTask::execute(params);
                    } else if (taskType == "GuidedExposureTask") {
                        task = lithium::sequencer::task::GuidedExposureTask::createEnhancedTask();
                        lithium::sequencer::task::GuidedExposureTask::execute(params);
                    } else if (taskType == "AutoCalibrationTask") {
                        task = lithium::sequencer::task::AutoCalibrationTask::createEnhancedTask();
                        lithium::sequencer::task::AutoCalibrationTask::execute(params);
                    } else {
                        throw std::invalid_argument("Unsupported camera task type: " + taskType);
                    }
                    
                    if (!task) {
                        throw std::runtime_error("Failed to create camera task of type: " + taskType);
                    }
                    
                    result["message"] = "Camera task created and executed successfully";
                    result["taskType"] = taskType;
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create exposure task
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
                    
                    // Create and execute the actual exposure task
                    auto task = lithium::sequencer::task::TakeExposureTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create exposure task");
                    }
                    
                    // Execute the task with parameters
                    json params;
                    params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    lithium::sequencer::task::TakeExposureTask::execute(params);
                    
                    result["message"] = "Exposure task created and executed successfully";
                    result["taskType"] = "TakeExposureTask";
                    result["taskId"] = task->getUUID();
                    result["exposureTime"] = body["exposure"].d();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create multiple exposures task
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
                    
                    // Create and execute the actual multiple exposures task
                    auto task = lithium::sequencer::task::TakeManyExposureTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create multiple exposures task");
                    }
                    
                    // Execute the task with parameters
                    json params;
                    params["exposure"] = body["exposure"].d();
                    params["count"] = body["count"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("delay")) params["delay"] = body["delay"].d();
                    
                    lithium::sequencer::task::TakeManyExposureTask::execute(params);
                    
                    result["message"] = "Multiple exposures task created and executed successfully";
                    result["taskType"] = "TakeManyExposureTask";
                    result["taskId"] = task->getUUID();
                    result["exposureTime"] = body["exposure"].d();
                    result["count"] = body["count"].i();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create subframe exposure task
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
                    
                    auto task = lithium::sequencer::task::SubframeExposureTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create subframe exposure task");
                    }
                    
                    json params;
                    params["exposure"] = body["exposure"].d();
                    params["x"] = body["x"].i();
                    params["y"] = body["y"].i();
                    params["width"] = body["width"].i();
                    params["height"] = body["height"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    lithium::sequencer::task::SubframeExposureTask::execute(params);
                    
                    result["message"] = "Subframe exposure task created and executed successfully";
                    result["taskType"] = "SubframeExposureTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create camera settings task
        CROW_ROUTE(app, "/api/tasks/camera/settings")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createCameraSettingsTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto task = lithium::sequencer::task::CameraSettingsTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create camera settings task");
                    }
                    
                    json params;
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("gain")) params["gain"] = body["gain"].d();
                    if (body.has("offset")) params["offset"] = body["offset"].i();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("temperature")) params["temperature"] = body["temperature"].d();
                    if (body.has("cooler")) params["cooler"] = body["cooler"].b();
                    
                    lithium::sequencer::task::CameraSettingsTask::execute(params);
                    
                    result["message"] = "Camera settings task created and executed successfully";
                    result["taskType"] = "CameraSettingsTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create camera preview task
        CROW_ROUTE(app, "/api/tasks/camera/preview")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createCameraPreviewTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto task = lithium::sequencer::task::CameraPreviewTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create camera preview task");
                    }
                    
                    json params;
                    if (body.has("exposure")) params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    lithium::sequencer::task::CameraPreviewTask::execute(params);
                    
                    result["message"] = "Camera preview task created and executed successfully";
                    result["taskType"] = "CameraPreviewTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create auto focus task
        CROW_ROUTE(app, "/api/tasks/camera/autofocus")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createAutoFocusTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto task = lithium::sequencer::task::AutoFocusTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create auto focus task");
                    }
                    
                    json params;
                    if (body.has("exposure")) params["exposure"] = body["exposure"].d();
                    if (body.has("binning")) params["binning"] = body["binning"].i();
                    if (body.has("step_size")) params["step_size"] = body["step_size"].i();
                    if (body.has("max_steps")) params["max_steps"] = body["max_steps"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("focuser")) params["focuser"] = body["focuser"].s();
                    
                    lithium::sequencer::task::AutoFocusTask::execute(params);
                    
                    result["message"] = "Auto focus task created and executed successfully";
                    result["taskType"] = "AutoFocusTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create filter sequence task
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
                    
                    auto task = lithium::sequencer::task::FilterSequenceTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create filter sequence task");
                    }
                    
                    json params;
                    params["filters"] = body["filters"];
                    params["exposure"] = body["exposure"].d();
                    if (body.has("count")) params["count"] = body["count"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("filter_wheel")) params["filter_wheel"] = body["filter_wheel"].s();
                    
                    lithium::sequencer::task::FilterSequenceTask::execute(params);
                    
                    result["message"] = "Filter sequence task created and executed successfully";
                    result["taskType"] = "FilterSequenceTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create RGB sequence task
        CROW_ROUTE(app, "/api/tasks/camera/rgb-sequence")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createRGBSequenceTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto task = lithium::sequencer::task::RGBSequenceTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create RGB sequence task");
                    }
                    
                    json params;
                    if (body.has("r_exposure")) params["r_exposure"] = body["r_exposure"].d();
                    if (body.has("g_exposure")) params["g_exposure"] = body["g_exposure"].d();
                    if (body.has("b_exposure")) params["b_exposure"] = body["b_exposure"].d();
                    if (body.has("count")) params["count"] = body["count"].i();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("filter_wheel")) params["filter_wheel"] = body["filter_wheel"].s();
                    
                    lithium::sequencer::task::RGBSequenceTask::execute(params);
                    
                    result["message"] = "RGB sequence task created and executed successfully";
                    result["taskType"] = "RGBSequenceTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create guided exposure task
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
                    
                    auto task = lithium::sequencer::task::GuidedExposureTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create guided exposure task");
                    }
                    
                    json params;
                    params["exposure"] = body["exposure"].d();
                    if (body.has("guide_exposure")) params["guide_exposure"] = body["guide_exposure"].d();
                    if (body.has("settle_time")) params["settle_time"] = body["settle_time"].d();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    if (body.has("guide_camera")) params["guide_camera"] = body["guide_camera"].s();
                    
                    lithium::sequencer::task::GuidedExposureTask::execute(params);
                    
                    result["message"] = "Guided exposure task created and executed successfully";
                    result["taskType"] = "GuidedExposureTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
                    return result;
                });
        });

        // Create auto calibration task
        CROW_ROUTE(app, "/api/tasks/camera/auto-calibration")
        .methods("POST"_method)
        ([](const crow::request& req) {
            auto body = crow::json::load(req.body);
            return handleTaskAction(req, body, "createAutoCalibrationTask",
                [&body]() -> crow::json::wvalue {
                    crow::json::wvalue result;
                    
                    auto task = lithium::sequencer::task::AutoCalibrationTask::createEnhancedTask();
                    if (!task) {
                        throw std::runtime_error("Failed to create auto calibration task");
                    }
                    
                    json params;
                    if (body.has("dark_count")) params["dark_count"] = body["dark_count"].i();
                    if (body.has("bias_count")) params["bias_count"] = body["bias_count"].i();
                    if (body.has("flat_count")) params["flat_count"] = body["flat_count"].i();
                    if (body.has("dark_exposure")) params["dark_exposure"] = body["dark_exposure"].d();
                    if (body.has("camera")) params["camera"] = body["camera"].s();
                    
                    lithium::sequencer::task::AutoCalibrationTask::execute(params);
                    
                    result["message"] = "Auto calibration task created and executed successfully";
                    result["taskType"] = "AutoCalibrationTask";
                    result["taskId"] = task->getUUID();
                    result["status"] = "executed";
                    
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
                    
                    // TODO: Implement task status lookup from task manager
                    result["taskId"] = taskId;
                    result["message"] = "Task status lookup - implementation needed";
                    result["status"] = "placeholder - implementation needed";
                    
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
                    
                    // TODO: Implement active task listing from task manager
                    result["message"] = "Active tasks listing - implementation needed";
                    result["tasks"] = std::vector<std::string>{};
                    result["status"] = "placeholder - implementation needed";
                    
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
                    
                    // TODO: Implement task cancellation
                    result["taskId"] = taskId;
                    result["message"] = "Task cancellation - implementation needed";
                    result["status"] = "placeholder - implementation needed";
                    
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
                    
                    // TODO: Create device task instance and execute
                    result["message"] = "Device task created successfully";
                    result["taskType"] = "DeviceTask";
                    result["operation"] = operation;
                    result["deviceName"] = deviceName;
                    result["status"] = "placeholder - implementation needed";
                    
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
                    
                    if (!body.has("script")) {
                        throw std::invalid_argument("Missing required parameter: script");
                    }
                    
                    std::string script = body["script"].s();
                    
                    // TODO: Create and execute script task
                    result["message"] = "Script task created successfully";
                    result["taskType"] = "ScriptTask";
                    result["script"] = script;
                    result["status"] = "placeholder - implementation needed";
                    
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
                    
                    std::vector<std::string> cameraTaskTypes = {
                        "TakeExposureTask", "TakeManyExposureTask", "SubframeExposureTask",
                        "CameraSettingsTask", "CameraPreviewTask", "AutoFocusTask", 
                        "FocusSeriesTask", "FilterSequenceTask", "RGBSequenceTask",
                        "GuidedExposureTask", "DitherSequenceTask", "AutoCalibrationTask",
                        "ThermalCycleTask", "FlatFieldSequenceTask"
                    };
                    
                    std::vector<std::string> deviceTaskTypes = {
                        "DeviceTask", "ConnectDevice", "ScanDevices", "InitializeDevice"
                    };
                    
                    std::vector<std::string> otherTaskTypes = {
                        "ScriptTask", "ConfigTask", "SearchTask"
                    };
                    
                    result["camera"] = cameraTaskTypes;
                    result["device"] = deviceTaskTypes;
                    result["other"] = otherTaskTypes;
                    
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
                    
                    // TODO: Implement task parameter schema retrieval
                    result["taskType"] = taskType;
                    result["message"] = "Task schema retrieval - placeholder implementation";
                    result["status"] = "placeholder - implementation needed";
                    
                    return result;
                });
        });
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_TASK_MANAGEMENT_HPP
