#include "focus_workflow_example.hpp"
#include <spdlog/spdlog.h>

namespace lithium::task::example {

auto FocusWorkflowExample::createComprehensiveFocusWorkflow() 
    -> std::vector<std::unique_ptr<lithium::task::task::Task>> {
    
    std::vector<std::unique_ptr<lithium::task::task::Task>> workflow;
    
    // Step 1: Star detection and analysis
    auto starDetection = lithium::task::task::StarDetectionTask::createEnhancedTask();
    starDetection->addHistoryEntry("Workflow step 1: Star detection");
    
    // Step 2: Focus calibration (depends on star detection)
    auto focusCalibration = lithium::task::task::FocusCalibrationTask::createEnhancedTask();
    focusCalibration->addDependency(starDetection->getUUID());
    focusCalibration->addHistoryEntry("Workflow step 2: Focus calibration");
    
    // Step 3: Backlash compensation (can run in parallel with calibration)
    auto backlashComp = lithium::task::task::BacklashCompensationTask::createEnhancedTask();
    backlashComp->addHistoryEntry("Workflow step 3: Backlash compensation");
    
    // Step 4: Auto focus (depends on calibration and backlash compensation)
    auto autoFocus = lithium::task::task::AutoFocusTask::createEnhancedTask();
    autoFocus->addDependency(focusCalibration->getUUID());
    autoFocus->addDependency(backlashComp->getUUID());
    autoFocus->addHistoryEntry("Workflow step 4: Auto focus");
    
    // Step 5: Focus validation (depends on auto focus)
    auto focusValidation = lithium::task::task::FocusValidationTask::createEnhancedTask();
    focusValidation->addDependency(autoFocus->getUUID());
    focusValidation->addHistoryEntry("Workflow step 5: Focus validation");
    
    // Step 6: Temperature monitoring (can start after validation)
    auto tempMonitoring = lithium::task::task::FocusMonitoringTask::createEnhancedTask();
    tempMonitoring->addDependency(focusValidation->getUUID());
    tempMonitoring->addHistoryEntry("Workflow step 6: Temperature monitoring");
    
    // Add all tasks to workflow
    workflow.push_back(std::move(starDetection));
    workflow.push_back(std::move(focusCalibration));
    workflow.push_back(std::move(backlashComp));
    workflow.push_back(std::move(autoFocus));
    workflow.push_back(std::move(focusValidation));
    workflow.push_back(std::move(tempMonitoring));
    
    spdlog::info("Created comprehensive focus workflow with {} tasks", workflow.size());
    return workflow;
}

auto FocusWorkflowExample::createSimpleAutoFocusWorkflow() 
    -> std::vector<std::unique_ptr<lithium::task::task::Task>> {
    
    std::vector<std::unique_ptr<lithium::task::task::Task>> workflow;
    
    // Simple workflow: Backlash -> AutoFocus -> Validation
    auto backlashComp = lithium::task::task::BacklashCompensationTask::createEnhancedTask();
    backlashComp->addHistoryEntry("Simple workflow: Backlash compensation");
    
    auto autoFocus = lithium::task::task::AutoFocusTask::createEnhancedTask();
    autoFocus->addDependency(backlashComp->getUUID());
    autoFocus->addHistoryEntry("Simple workflow: Auto focus");
    
    auto validation = lithium::task::task::FocusValidationTask::createEnhancedTask();
    validation->addDependency(autoFocus->getUUID());
    validation->addHistoryEntry("Simple workflow: Validation");
    
    workflow.push_back(std::move(backlashComp));
    workflow.push_back(std::move(autoFocus));
    workflow.push_back(std::move(validation));
    
    spdlog::info("Created simple autofocus workflow with {} tasks", workflow.size());
    return workflow;
}

auto FocusWorkflowExample::createTemperatureCompensatedWorkflow() 
    -> std::vector<std::unique_ptr<lithium::task::task::Task>> {
    
    std::vector<std::unique_ptr<lithium::task::task::Task>> workflow;
    
    // Temperature compensation workflow
    auto autoFocus = lithium::task::task::AutoFocusTask::createEnhancedTask();
    autoFocus->addHistoryEntry("Temperature workflow: Initial focus");
    
    auto tempFocus = lithium::task::task::TemperatureFocusTask::createEnhancedTask();
    tempFocus->addDependency(autoFocus->getUUID());
    tempFocus->addHistoryEntry("Temperature workflow: Temperature compensation");
    
    auto monitoring = lithium::task::task::FocusMonitoringTask::createEnhancedTask();
    monitoring->addDependency(tempFocus->getUUID());
    monitoring->addHistoryEntry("Temperature workflow: Continuous monitoring");
    
    workflow.push_back(std::move(autoFocus));
    workflow.push_back(std::move(tempFocus));
    workflow.push_back(std::move(monitoring));
    
    spdlog::info("Created temperature compensated workflow with {} tasks", workflow.size());
    return workflow;
}

void FocusWorkflowExample::setupTaskDependencies(
    const std::vector<std::unique_ptr<lithium::task::task::Task>>& tasks) {
    
    spdlog::info("Setting up task dependencies for {} tasks", tasks.size());
    
    for (const auto& task : tasks) {
        const auto& dependencies = task->getDependencies();
        if (!dependencies.empty()) {
            spdlog::info("Task '{}' has {} dependencies:", 
                        task->getName(), dependencies.size());
            
            for (const auto& depId : dependencies) {
                spdlog::info("  - Dependency: {}", depId);
                
                // In a real implementation, you would set dependency status
                // when the dependency task completes
                // task->setDependencyStatus(depId, true);
            }
            
            if (task->isDependencySatisfied()) {
                spdlog::info("Task '{}' dependencies are satisfied", task->getName());
            } else {
                spdlog::info("Task '{}' is waiting for dependencies", task->getName());
            }
        }
    }
}

}  // namespace lithium::task::example
