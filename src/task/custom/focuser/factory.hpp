#pragma once

#include "base.hpp"
#include "position.hpp"
#include "autofocus.hpp"
#include "temperature.hpp"
#include "validation.hpp"
#include "backlash.hpp"
#include "calibration.hpp"
#include "star_analysis.hpp"
#include <memory>
#include <string>
#include <map>
#include <functional>

namespace lithium::task::custom::focuser {

/**
 * @brief Factory for creating focuser tasks
 * 
 * Provides a centralized way to create and register focuser tasks,
 * following the same pattern as FilterTaskFactory.
 */
class FocuserTaskFactory {
public:
    // Task creation function type
    using TaskCreator = std::function<std::shared_ptr<Task>(const Json::Value&)>;

    // Register all focuser tasks
    static void registerAllTasks();

    // Create task by name
    static std::shared_ptr<Task> createTask(const std::string& task_name, const Json::Value& params);

    // Get list of available task names
    static std::vector<std::string> getAvailableTaskNames();

    // Check if task name is registered
    static bool isTaskRegistered(const std::string& task_name);

    // Register a custom task creator
    static void registerTask(const std::string& task_name, TaskCreator creator);

private:
    static std::map<std::string, TaskCreator>& getTaskRegistry();

    // Individual task creators
    static std::shared_ptr<Task> createPositionTask(const Json::Value& params);
    static std::shared_ptr<Task> createAutofocusTask(const Json::Value& params);
    static std::shared_ptr<Task> createTemperatureCompensationTask(const Json::Value& params);
    static std::shared_ptr<Task> createTemperatureMonitorTask(const Json::Value& params);
    static std::shared_ptr<Task> createValidationTask(const Json::Value& params);
    static std::shared_ptr<Task> createQualityCheckerTask(const Json::Value& params);
    static std::shared_ptr<Task> createBacklashCompensationTask(const Json::Value& params);
    static std::shared_ptr<Task> createBacklashDetectorTask(const Json::Value& params);
    static std::shared_ptr<Task> createCalibrationTask(const Json::Value& params);
    static std::shared_ptr<Task> createQuickCalibrationTask(const Json::Value& params);
    static std::shared_ptr<Task> createStarAnalysisTask(const Json::Value& params);
    static std::shared_ptr<Task> createSimpleStarDetectorTask(const Json::Value& params);

    // Helper functions for parameter extraction
    static std::shared_ptr<device::Focuser> extractFocuser(const Json::Value& params);
    static std::shared_ptr<device::Camera> extractCamera(const Json::Value& params);
    static std::shared_ptr<device::TemperatureSensor> extractTemperatureSensor(const Json::Value& params);
};

/**
 * @brief Configuration builder for focuser tasks
 */
class FocuserTaskConfigBuilder {
public:
    FocuserTaskConfigBuilder();

    // Device configuration
    FocuserTaskConfigBuilder& withFocuser(const std::string& focuser_name);
    FocuserTaskConfigBuilder& withCamera(const std::string& camera_name);
    FocuserTaskConfigBuilder& withTemperatureSensor(const std::string& sensor_name);

    // Position task configuration
    FocuserTaskConfigBuilder& withAbsolutePosition(int position);
    FocuserTaskConfigBuilder& withRelativePosition(int steps);
    FocuserTaskConfigBuilder& withSync(bool enable = true);

    // Autofocus configuration
    FocuserTaskConfigBuilder& withAutofocusAlgorithm(const std::string& algorithm);
    FocuserTaskConfigBuilder& withFocusRange(int start, int end);
    FocuserTaskConfigBuilder& withStepSize(int coarse, int fine);
    FocuserTaskConfigBuilder& withMaxIterations(int iterations);

    // Temperature configuration
    FocuserTaskConfigBuilder& withTemperatureCoefficient(double coefficient);
    FocuserTaskConfigBuilder& withMonitoringInterval(int seconds);
    FocuserTaskConfigBuilder& withAutoCompensation(bool enable = true);

    // Validation configuration
    FocuserTaskConfigBuilder& withQualityThresholds(double hfr_threshold, double fwhm_threshold);
    FocuserTaskConfigBuilder& withMinStars(int min_stars);
    FocuserTaskConfigBuilder& withValidationInterval(int seconds);
    FocuserTaskConfigBuilder& withAutoCorrection(bool enable = true);

    // Backlash configuration
    FocuserTaskConfigBuilder& withBacklashMeasurement(int range, int steps);
    FocuserTaskConfigBuilder& withBacklashCompensation(int inward, int outward);
    FocuserTaskConfigBuilder& withOvershoot(int steps);

    // Calibration configuration
    FocuserTaskConfigBuilder& withCalibrationRange(int start, int end);
    FocuserTaskConfigBuilder& withCalibrationSteps(int coarse, int fine, int ultra_fine);
    FocuserTaskConfigBuilder& withTemperatureCalibration(bool enable = true);
    FocuserTaskConfigBuilder& withModelCreation(bool enable = true);

    // Star analysis configuration
    FocuserTaskConfigBuilder& withDetectionThreshold(double sigma);
    FocuserTaskConfigBuilder& withStarRadius(int min_radius, int max_radius);
    FocuserTaskConfigBuilder& withDetailedAnalysis(bool enable = true);

    // Build configuration
    Json::Value build() const;

private:
    Json::Value config_;
};

/**
 * @brief Workflow builder for common focuser task sequences
 */
class FocuserWorkflowBuilder {
public:
    struct WorkflowStep {
        std::string task_name;
        Json::Value parameters;
        bool required = true;           // If false, continue on failure
        std::string description;
    };

    FocuserWorkflowBuilder();

    // Predefined workflows
    static std::vector<WorkflowStep> createBasicAutofocusWorkflow();
    static std::vector<WorkflowStep> createFullCalibrationWorkflow();
    static std::vector<WorkflowStep> createMaintenanceWorkflow();
    static std::vector<WorkflowStep> createQuickFocusWorkflow();

    // Custom workflow building
    FocuserWorkflowBuilder& addStep(const std::string& task_name, 
                                   const Json::Value& parameters,
                                   bool required = true,
                                   const std::string& description = "");
    
    FocuserWorkflowBuilder& addAutofocus(const Json::Value& config = Json::Value::null);
    FocuserWorkflowBuilder& addValidation(const Json::Value& config = Json::Value::null);
    FocuserWorkflowBuilder& addTemperatureCompensation(const Json::Value& config = Json::Value::null);
    FocuserWorkflowBuilder& addBacklashCalibration(const Json::Value& config = Json::Value::null);
    FocuserWorkflowBuilder& addStarAnalysis(const Json::Value& config = Json::Value::null);

    // Conditional steps
    FocuserWorkflowBuilder& addConditionalStep(const std::string& condition,
                                              const WorkflowStep& step);

    std::vector<WorkflowStep> build() const;

private:
    std::vector<WorkflowStep> steps_;
};

/**
 * @brief Auto-registration helper for focuser tasks
 */
class FocuserTaskRegistrar {
public:
    FocuserTaskRegistrar(const std::string& task_name, FocuserTaskFactory::TaskCreator creator);
};

// Macro for auto-registering focuser tasks
#define AUTO_REGISTER_FOCUSER_TASK(name, creator_func) \
    namespace { \
        static FocuserTaskRegistrar name##_registrar(#name, creator_func); \
    }

/**
 * @brief Task parameter validation utilities
 */
class FocuserTaskValidator {
public:
    // Common validation functions
    static bool validateDeviceParameter(const Json::Value& params, const std::string& device_type);
    static bool validatePositionParameter(const Json::Value& params);
    static bool validateRangeParameter(const Json::Value& params, const std::string& param_name);
    static bool validateThresholdParameter(const Json::Value& params, const std::string& param_name);

    // Specific task validations
    static bool validateAutofocusParameters(const Json::Value& params);
    static bool validateTemperatureParameters(const Json::Value& params);
    static bool validateValidationParameters(const Json::Value& params);
    static bool validateBacklashParameters(const Json::Value& params);
    static bool validateCalibrationParameters(const Json::Value& params);
    static bool validateStarAnalysisParameters(const Json::Value& params);

    // Get validation error messages
    static std::vector<std::string> getValidationErrors(const std::string& task_name, 
                                                       const Json::Value& params);
};

} // namespace lithium::task::custom::focuser
