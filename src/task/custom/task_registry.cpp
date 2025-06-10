/**
 * @file task_registry.cpp
 * @brief Task Registry implementation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "task_registry.hpp"
#include "atom/log/loguru.hpp"
#include "device/manager.hpp"

// Include camera task headers
#include "camera/basic_exposure.hpp"
#include "camera/calibration_tasks.hpp"
#include "camera/filter_tasks.hpp"
#include "camera/focus_tasks.hpp"
#include "camera/guide_tasks.hpp"
#include "camera/platesolve_tasks.hpp"
#include "camera/safety_tasks.hpp"
#include "camera/sequence_tasks.hpp"

namespace lithium::sequencer {

bool TaskRegistry::initialized_ = false;

void TaskRegistry::registerAllTasks() {
    if (initialized_) {
        LOG_F(WARNING, "Task registry already initialized");
        return;
    }

    LOG_F(INFO, "Registering all custom tasks...");

    registerCoreTasks();
    registerDeviceTasks();
    registerScriptTasks();
    registerConfigTasks();
    registerCameraTasks();

    initialized_ = true;
    LOG_F(INFO, "Task registry initialization completed");
}

void TaskRegistry::registerCoreTasks() {
    LOG_F(INFO, "Registering core tasks...");

    // Register basic task types that don't require special initialization
    TaskInfo basicTaskInfo;
    basicTaskInfo.name = "basic_task";
    basicTaskInfo.description = "Basic task implementation";
    basicTaskInfo.category = "core";
    basicTaskInfo.version = "1.0.0";
    basicTaskInfo.isEnabled = true;

    TaskFactory::getInstance().registerTask<Task>(
        "basic_task",
        [](const std::string& name, const json& config) -> std::unique_ptr<Task> {
            return std::make_unique<Task>(name, [](const json&) {
                LOG_F(INFO, "Executing basic task");
            });
        },
        basicTaskInfo
    );
}

void TaskRegistry::registerDeviceTasks() {
    LOG_F(INFO, "Registering device tasks...");

    // Device management tasks
    TaskInfo deviceTaskInfo;
    deviceTaskInfo.name = "device_task";
    deviceTaskInfo.description = "Device management and control task";
    deviceTaskInfo.category = "device";
    deviceTaskInfo.version = "1.0.0";
    deviceTaskInfo.requiredParameters = {"operation"};
    deviceTaskInfo.parameterSchema = json{
        {"operation", {{"type", "string"}, {"enum", json::array({"connect", "disconnect", "scan", "initialize", "configure"})}}},
        {"deviceName", {{"type", "string"}}},
        {"deviceType", {{"type", "string"}}},
        {"timeout", {{"type", "number"}, {"default", 5000}}},
        {"retryCount", {{"type", "number"}, {"default", 0}}}
    };
    deviceTaskInfo.isEnabled = true;

    TaskFactory::getInstance().registerTask<DeviceTask>(
        "device_task",
        [](const std::string& name, const json& config) -> std::unique_ptr<DeviceTask> {
            // Get device manager instance
            static DeviceManager deviceManager; // Simplified for example
            return std::make_unique<DeviceTask>(name, deviceManager);
        },
        deviceTaskInfo
    );

    // Add specific device operation tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(DeviceTask, "device_connect", "device", 
        "Connect to a specific device", std::vector<std::string>{});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(DeviceTask, "device_initialize", "device", 
        "Initialize a connected device", std::vector<std::string>{"device_connect"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(DeviceTask, "device_configure", "device", 
        "Configure an initialized device", std::vector<std::string>{"device_initialize"});
}

void TaskRegistry::registerScriptTasks() {
    LOG_F(INFO, "Registering script tasks...");

    TaskInfo scriptTaskInfo;
    scriptTaskInfo.name = "script_task";
    scriptTaskInfo.description = "Script execution and management task";
    scriptTaskInfo.category = "script";
    scriptTaskInfo.version = "1.0.0";
    scriptTaskInfo.requiredParameters = {"operation"};
    scriptTaskInfo.parameterSchema = json{
        {"operation", {{"type", "string"}, {"enum", json::array({"execute", "register", "update", "delete"})}}},
        {"scriptName", {{"type", "string"}}},
        {"scriptContent", {{"type", "string"}}},
        {"scriptPath", {{"type", "string"}}},
        {"parameters", {{"type", "object"}}}
    };
    scriptTaskInfo.isEnabled = true;

    TaskFactory::getInstance().registerTask<task::ScriptTask>(
        "script_task",
        [](const std::string& name, const json& config) -> std::unique_ptr<task::ScriptTask> {
            std::string scriptConfigPath = config.value("scriptConfigPath", "");
            std::string analyzerConfigPath = config.value("analyzerConfigPath", "");
            return std::make_unique<task::ScriptTask>(name, scriptConfigPath, analyzerConfigPath);
        },
        scriptTaskInfo
    );

    // Register specialized script tasks
    LITHIUM_REGISTER_TASK(task::ScriptTask, "python_script", "script", 
        "Execute Python scripts");
    
    LITHIUM_REGISTER_TASK(task::ScriptTask, "shell_script", "script", 
        "Execute shell scripts");
    
    LITHIUM_REGISTER_TASK(task::ScriptTask, "custom_script", "script", 
        "Execute custom user scripts");
}

void TaskRegistry::registerConfigTasks() {
    LOG_F(INFO, "Registering configuration tasks...");

    TaskInfo configTaskInfo;
    configTaskInfo.name = "config_task";
    configTaskInfo.description = "Configuration management task";
    configTaskInfo.category = "config";
    configTaskInfo.version = "1.0.0";
    configTaskInfo.requiredParameters = {"operation"};
    configTaskInfo.parameterSchema = json{
        {"operation", {{"type", "string"}, {"enum", json::array({"set", "get", "delete", "load", "save", "merge", "list"})}}},
        {"key_path", {{"type", "string"}}},
        {"value", {{"type", "object"}}},
        {"file_path", {{"type", "string"}}},
        {"merge_data", {{"type", "object"}}}
    };
    configTaskInfo.isEnabled = true;

    TaskFactory::getInstance().registerTask<task::TaskConfigManagement>(
        "config_task",
        [](const std::string& name, const json& config) -> std::unique_ptr<task::TaskConfigManagement> {
            return std::make_unique<task::TaskConfigManagement>(name);
        },
        configTaskInfo
    );

    // Register specialized config tasks
    LITHIUM_REGISTER_TASK(task::TaskConfigManagement, "config_set", "config", 
        "Set configuration value");
    
    LITHIUM_REGISTER_TASK(task::TaskConfigManagement, "config_get", "config", 
        "Get configuration value");
    
    LITHIUM_REGISTER_TASK(task::TaskConfigManagement, "config_load", "config", 
        "Load configuration from file");
    
    LITHIUM_REGISTER_TASK(task::TaskConfigManagement, "config_save", "config", 
        "Save configuration to file");
}

void TaskRegistry::registerCameraTasks() {
    LOG_F(INFO, "Registering camera tasks...");

    // Basic exposure tasks
    TaskInfo exposureTaskInfo;
    exposureTaskInfo.name = "basic_exposure";
    exposureTaskInfo.description = "Basic camera exposure task";
    exposureTaskInfo.category = "camera";
    exposureTaskInfo.version = "1.0.0";
    exposureTaskInfo.requiredParameters = {"exposure_time", "gain"};
    exposureTaskInfo.parameterSchema = json{
        {"exposure_time", {{"type", "number"}, {"minimum", 0.001}}},
        {"gain", {{"type", "number"}, {"minimum", 0}}},
        {"binning", {{"type", "number"}, {"default", 1}}},
        {"filter", {{"type", "string"}}},
        {"count", {{"type", "number"}, {"default", 1}}}
    };
    exposureTaskInfo.dependencies = {"device_connect", "device_initialize"};
    exposureTaskInfo.isEnabled = true;

    TaskFactory::getInstance().registerTask<camera::BasicExposureTask>(
        "basic_exposure",
        [](const std::string& name, const json& config) -> std::unique_ptr<camera::BasicExposureTask> {
            return std::make_unique<camera::BasicExposureTask>(name);
        },
        exposureTaskInfo
    );

    // Calibration tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::CalibrationTask, "dark_calibration", "camera", 
        "Dark frame calibration", std::vector<std::string>{"basic_exposure"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::CalibrationTask, "flat_calibration", "camera", 
        "Flat frame calibration", std::vector<std::string>{"basic_exposure"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::CalibrationTask, "bias_calibration", "camera", 
        "Bias frame calibration", std::vector<std::string>{"basic_exposure"});

    // Filter tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::FilterTask, "filter_change", "camera", 
        "Change camera filter", std::vector<std::string>{"device_initialize"});

    // Focus tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::FocusTask, "auto_focus", "camera", 
        "Automatic focusing", std::vector<std::string>{"device_initialize"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::FocusTask, "focus_calibration", "camera", 
        "Focus calibration", std::vector<std::string>{"auto_focus"});

    // Guide tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::GuideTask, "start_guiding", "camera", 
        "Start autoguiding", std::vector<std::string>{"device_initialize"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::GuideTask, "stop_guiding", "camera", 
        "Stop autoguiding", std::vector<std::string>{});

    // Platesolve tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::PlateSolveTask, "plate_solve", "camera", 
        "Plate solving", std::vector<std::string>{"basic_exposure"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::PlateSolveTask, "goto_target", "camera", 
        "Go to target coordinates", std::vector<std::string>{"plate_solve"});

    // Safety tasks
    LITHIUM_REGISTER_TASK(camera::SafetyTask, "safety_check", "camera", 
        "Safety monitoring");
    
    LITHIUM_REGISTER_TASK(camera::SafetyTask, "emergency_stop", "camera", 
        "Emergency stop all operations");

    // Sequence tasks
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::SequenceTask, "imaging_sequence", "camera", 
        "Complete imaging sequence", std::vector<std::string>{"auto_focus", "start_guiding"});
    
    LITHIUM_REGISTER_TASK_WITH_DEPS(camera::SequenceTask, "calibration_sequence", "camera", 
        "Complete calibration sequence", std::vector<std::string>{"basic_exposure"});
}

size_t TaskRegistry::registerTasksFromDirectory(const std::string& directory) {
    LOG_F(INFO, "Registering tasks from directory: {}", directory);
    
    // Implementation would scan directory for task definitions
    // For now, return 0 as placeholder
    size_t count = 0;
    
    LOG_F(INFO, "Registered {} tasks from directory", count);
    return count;
}

void TaskRegistry::initialize() {
    if (!initialized_) {
        registerAllTasks();
    }
}

void TaskRegistry::shutdown() {
    if (initialized_) {
        TaskFactory::getInstance().clear();
        initialized_ = false;
        LOG_F(INFO, "Task registry shutdown completed");
    }
}

bool TaskRegistry::isInitialized() {
    return initialized_;
}

} // namespace lithium::sequencer
