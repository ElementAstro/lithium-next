/*
 * ModernSequenceController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_HPP
#define LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_HPP

#include "controller.hpp"
#include "sequence_management.hpp"
#include "sequence_execution.hpp"
#include "target_management.hpp"
#include "task_management.hpp"

#include <memory>
#include <string>
#include <vector>
#include "atom/log/loguru.hpp"
#include "task/sequencer.hpp"

/**
 * @brief Modern orchestrating controller that delegates to specialized controllers
 * 
 * This controller replaces the monolithic SequenceController and provides:
 * - Clean separation of concerns
 * - RESTful API design
 * - Comprehensive task support
 * - Modern sequencer features
 */
class ModernSequenceController : public Controller {
private:
    std::unique_ptr<SequenceManagementController> mSequenceManagement;
    std::unique_ptr<SequenceExecutionController> mSequenceExecution;
    std::unique_ptr<TargetManagementController> mTargetManagement;
    std::unique_ptr<TaskManagementController> mTaskManagement;
    
    static std::shared_ptr<lithium::sequencer::ExposureSequence> mExposureSequence;

public:
    /**
     * @brief Constructor - initializes all specialized controllers
     */
    ModernSequenceController() {
        mSequenceManagement = std::make_unique<SequenceManagementController>();
        mSequenceExecution = std::make_unique<SequenceExecutionController>();
        mTargetManagement = std::make_unique<TargetManagementController>();
        mTaskManagement = std::make_unique<TaskManagementController>();
        
        LOG_F(INFO, "ModernSequenceController initialized with specialized controllers");
    }

    /**
     * @brief Register all routes by delegating to specialized controllers
     * @param app The crow application instance
     */
    void registerRoutes(crow::SimpleApp &app) override {
        LOG_F(INFO, "Registering routes for ModernSequenceController");
        
        // Ensure we have the required instances
        if (!mExposureSequence) {
            LOG_F(WARNING, "ExposureSequence instance not set, creating default instance");
            mExposureSequence = std::make_shared<lithium::sequencer::ExposureSequence>();
        }
        
        // Set the shared instances on all controllers
        SequenceManagementController::setExposureSequence(mExposureSequence);
        SequenceExecutionController::setExposureSequence(mExposureSequence);
        TargetManagementController::setExposureSequence(mExposureSequence);
        
        // Register routes from all specialized controllers
        mSequenceManagement->registerRoutes(app);
        mSequenceExecution->registerRoutes(app);
        mTargetManagement->registerRoutes(app);
        mTaskManagement->registerRoutes(app);
        
        // Add a general health check endpoint
        CROW_ROUTE(app, "/api/sequence/health")
        .methods("GET"_method)
        ([](const crow::request& req) {
            crow::json::wvalue response;
            response["status"] = "healthy";
            response["message"] = "ModernSequenceController is operational";
            response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            
            response["controllers"] = std::vector<std::string>{
                "SequenceManagementController",
                "SequenceExecutionController", 
                "TargetManagementController",
                "TaskManagementController"
            };
            
            response["features"] = std::vector<std::string>{
                "RESTful API design",
                "Comprehensive task support",
                "Modern sequencer features",
                "Separated concerns",
                "Enhanced error handling"
            };
            
            return crow::response(200, response);
        });
        
        // Add a general API documentation endpoint
        CROW_ROUTE(app, "/api/sequence/docs")
        .methods("GET"_method)
        ([](const crow::request& req) {
            crow::json::wvalue response;
            response["title"] = "Modern Sequence Controller API";
            response["version"] = "2.0.0";
            response["description"] = "Modern sequencer API with split controllers";
            
            crow::json::wvalue endpoints;
            
            // Sequence Management endpoints
            endpoints["sequence_management"] = std::vector<std::string>{
                "POST /api/sequence/create - Create new sequence",
                "POST /api/sequence/save - Save sequence to file",
                "POST /api/sequence/load - Load sequence from file",
                "GET /api/sequence/info - Get sequence information",
                "GET /api/sequence/list - List all sequences",
                "DELETE /api/sequence/delete - Delete sequence",
                "PUT /api/sequence/update - Update sequence"
            };
            
            // Sequence Execution endpoints
            endpoints["sequence_execution"] = std::vector<std::string>{
                "POST /api/sequence/execute - Execute all targets",
                "POST /api/sequence/stop - Stop execution",
                "POST /api/sequence/pause - Pause execution",
                "POST /api/sequence/resume - Resume execution",
                "GET /api/sequence/progress - Get execution progress",
                "GET /api/sequence/stats - Get execution statistics",
                "PUT /api/sequence/scheduling-strategy - Set scheduling strategy",
                "PUT /api/sequence/recovery-strategy - Set recovery strategy",
                "PUT /api/sequence/max-concurrent - Set max concurrent targets",
                "PUT /api/sequence/timeout - Set global timeout"
            };
            
            // Target Management endpoints
            endpoints["target_management"] = std::vector<std::string>{
                "POST /api/targets/add - Add target",
                "DELETE /api/targets/remove - Remove target", 
                "PUT /api/targets/modify - Modify target",
                "GET /api/targets/list - List all targets",
                "GET /api/targets/status - Get target status",
                "POST /api/targets/alternative - Add alternative target",
                "GET /api/targets/failed - Get failed targets",
                "POST /api/targets/retry - Retry failed targets",
                "GET /api/targets/details - Get target details"
            };
            
            // Task Management endpoints
            endpoints["task_management"] = std::vector<std::string>{
                "POST /api/tasks/camera/exposure - Create exposure task",
                "POST /api/tasks/camera/exposures - Create multiple exposures task",
                "POST /api/tasks/camera/subframe - Create subframe task",
                "POST /api/tasks/camera/settings - Create camera settings task",
                "POST /api/tasks/camera/preview - Create camera preview task",
                "POST /api/tasks/camera/filter - Create filter task",
                "POST /api/tasks/camera/focus - Create focus task",
                "POST /api/tasks/camera/guide - Create guide task",
                "POST /api/tasks/camera/platesolve - Create platesolve task",
                "POST /api/tasks/camera/calibration - Create calibration task",
                "POST /api/tasks/device - Create device task",
                "POST /api/tasks/device/connect - Connect device",
                "POST /api/tasks/device/scan - Scan devices",
                "POST /api/tasks/script - Create script task",
                "POST /api/tasks/config - Create config task",
                "POST /api/tasks/search - Create search task",
                "GET /api/tasks/types - Get available task types",
                "GET /api/tasks/schema - Get task parameter schema"
            };
            
            response["endpoints"] = std::move(endpoints);
            
            return crow::response(200, response);
        });
        
        LOG_F(INFO, "All routes registered successfully for ModernSequenceController");
    }

    /**
     * @brief Set the ExposureSequence instance
     * @param sequence Shared pointer to the ExposureSequence
     */
    static void setExposureSequence(std::shared_ptr<lithium::sequencer::ExposureSequence> sequence) {
        mExposureSequence = sequence;
        LOG_F(INFO, "ExposureSequence instance set on ModernSequenceController");
    }

    /**
     * @brief Get the current ExposureSequence instance
     * @return Shared pointer to the ExposureSequence
     */
    static std::shared_ptr<lithium::sequencer::ExposureSequence> getExposureSequence() {
        return mExposureSequence;
    }

    /**
     * @brief Initialize the controller with default instances if needed
     */
    void initialize() {
        if (!mExposureSequence) {
            mExposureSequence = std::make_shared<lithium::sequencer::ExposureSequence>();
            LOG_F(INFO, "Created default ExposureSequence instance");
        }
        
        LOG_F(INFO, "ModernSequenceController initialized successfully");
    }
};

// Static member definitions
std::shared_ptr<lithium::sequencer::ExposureSequence> ModernSequenceController::mExposureSequence;

#endif // LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_HPP
