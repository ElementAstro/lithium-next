/*
 * ModernSequenceController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_CONTROLLER_HPP
#define LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_CONTROLLER_HPP

#include "controller.hpp"
#include "sequence_management.hpp"
#include "sequence_execution.hpp"
#include "target_management.hpp"
#include "task_management.hpp"

#include <memory>
#include "atom/log/loguru.hpp"
#include "task/sequencer.hpp"

/**
 * @brief Modern orchestrating controller that manages all sequence-related operations
 * 
 * This controller acts as a facade that coordinates between specialized controllers:
 * - SequenceManagementController: CRUD operations and persistence
 * - SequenceExecutionController: Execution control and monitoring
 * - TargetManagementController: Target operations and status management
 * - TaskManagementController: Task creation and management
 */
class ModernSequenceController : public Controller {
private:
    std::unique_ptr<SequenceManagementController> sequenceManagement;
    std::unique_ptr<SequenceExecutionController> sequenceExecution;
    std::unique_ptr<TargetManagementController> targetManagement;
    std::unique_ptr<TaskManagementController> taskManagement;
    
    std::shared_ptr<lithium::sequencer::ExposureSequence> exposureSequence;

public:
    /**
     * @brief Constructor - initializes all specialized controllers
     */
    ModernSequenceController() {
        LOG_F(INFO, "Initializing ModernSequenceController");
        
        // Initialize specialized controllers
        sequenceManagement = std::make_unique<SequenceManagementController>();
        sequenceExecution = std::make_unique<SequenceExecutionController>();
        targetManagement = std::make_unique<TargetManagementController>();
        taskManagement = std::make_unique<TaskManagementController>();
        
        // Initialize ExposureSequence
        try {
            exposureSequence = std::make_shared<lithium::sequencer::ExposureSequence>();
            
            // Set the ExposureSequence instance for all controllers
            SequenceManagementController::setExposureSequence(exposureSequence);
            SequenceExecutionController::setExposureSequence(exposureSequence);
            TargetManagementController::setExposureSequence(exposureSequence);
            
            LOG_F(INFO, "ExposureSequence initialized and shared with all controllers");
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize ExposureSequence: {}", e.what());
            // Continue without ExposureSequence - controllers will handle null checks
        }
    }

    /**
     * @brief Register all routes from specialized controllers
     * @param app The crow application instance
     */
    void registerRoutes(crow::SimpleApp &app) override {
        LOG_F(INFO, "Registering routes for ModernSequenceController");
        
        // Register routes from all specialized controllers
        sequenceManagement->registerRoutes(app);
        sequenceExecution->registerRoutes(app);
        targetManagement->registerRoutes(app);
        taskManagement->registerRoutes(app);
        
        // Add health check endpoint
        CROW_ROUTE(app, "/api/sequence/health")
        .methods("GET"_method)
        ([this](const crow::request& req) {
            crow::json::wvalue result;
            result["status"] = "healthy";
            result["message"] = "Modern Sequence Controller is operational";
            result["controllers"] = {
                "sequence_management",
                "sequence_execution", 
                "target_management",
                "task_management"
            };
            result["exposure_sequence_initialized"] = (exposureSequence != nullptr);
            return crow::response(200, result);
        });
        
        // Add API documentation endpoint
        CROW_ROUTE(app, "/api/sequence/docs")
        .methods("GET"_method)
        ([](const crow::request& req) {
            crow::json::wvalue docs;
            docs["title"] = "Modern Sequence Controller API";
            docs["version"] = "1.0.0";
            docs["description"] = "RESTful API for astronomical sequence management";
            
            // Sequence Management endpoints
            docs["endpoints"]["sequence_management"] = {
                {"POST /api/sequence/create", "Create a new sequence"},
                {"POST /api/sequence/save", "Save sequence to file"},
                {"POST /api/sequence/load", "Load sequence from file"},
                {"GET /api/sequence/info", "Get sequence information"},
                {"GET /api/sequence/list", "List all saved sequences"},
                {"DELETE /api/sequence/delete", "Delete a sequence"},
                {"PUT /api/sequence/update", "Update sequence metadata"}
            };
            
            // Sequence Execution endpoints
            docs["endpoints"]["sequence_execution"] = {
                {"POST /api/execution/start", "Start sequence execution"},
                {"POST /api/execution/stop", "Stop sequence execution"},
                {"POST /api/execution/pause", "Pause sequence execution"},
                {"POST /api/execution/resume", "Resume paused sequence"},
                {"GET /api/execution/status", "Get execution status"},
                {"GET /api/execution/progress", "Get execution progress"},
                {"POST /api/execution/strategy", "Set execution strategy"},
                {"POST /api/execution/recovery", "Set recovery strategy"}
            };
            
            // Target Management endpoints
            docs["endpoints"]["target_management"] = {
                {"POST /api/targets/add", "Add target to sequence"},
                {"DELETE /api/targets/remove", "Remove target from sequence"},
                {"PUT /api/targets/update", "Update target parameters"},
                {"GET /api/targets/list", "List all targets"},
                {"GET /api/targets/status", "Get target status"},
                {"POST /api/targets/enable", "Enable target"},
                {"POST /api/targets/disable", "Disable target"}
            };
            
            // Task Management endpoints
            docs["endpoints"]["task_management"] = {
                {"POST /api/tasks/create", "Create a new task"},
                {"GET /api/tasks/types", "Get available task types"},
                {"GET /api/tasks/templates", "Get task templates"}
            };
            
            // Utility endpoints
            docs["endpoints"]["utility"] = {
                {"GET /api/sequence/health", "Health check"},
                {"GET /api/sequence/docs", "API documentation"}
            };
            
            return crow::response(200, docs);
        });
        
        LOG_F(INFO, "All routes registered successfully");
    }

    /**
     * @brief Get the ExposureSequence instance
     * @return Shared pointer to the ExposureSequence
     */
    std::shared_ptr<lithium::sequencer::ExposureSequence> getExposureSequence() const {
        return exposureSequence;
    }

    /**
     * @brief Set a new ExposureSequence instance
     * @param sequence The new ExposureSequence instance
     */
    void setExposureSequence(std::shared_ptr<lithium::sequencer::ExposureSequence> sequence) {
        exposureSequence = sequence;
        
        // Update all controllers with the new instance
        SequenceManagementController::setExposureSequence(sequence);
        SequenceExecutionController::setExposureSequence(sequence);
        TargetManagementController::setExposureSequence(sequence);
        
        LOG_F(INFO, "ExposureSequence instance updated for all controllers");
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_MODERN_SEQUENCE_CONTROLLER_HPP