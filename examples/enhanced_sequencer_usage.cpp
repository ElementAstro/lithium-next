// example_enhanced_usage.cpp - Comprehensive usage examples for the enhanced sequence system
// This project is licensed under the terms of the GPL3 license.

#include <iostream>
#include <thread>
#include <chrono>
#include "task/custom/enhanced_sequencer.hpp"
#include "task/custom/task_manager.hpp"
#include "task/custom/task_templates.hpp"
#include "task/custom/factory.hpp"

using namespace lithium::sequencer;
using namespace std::chrono_literals;

// Example 1: Basic Task Creation and Execution
void basicTaskExample() {
    std::cout << "\n=== Basic Task Creation and Execution ===\n";
    
    auto manager = std::make_unique<TaskManager>();
    auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
    auto& factory = TaskFactory::getInstance();
    
    // Create a simple script task
    auto scriptTask = factory.createTask("script_task", "hello_world", json{
        {"script_path", "/bin/echo"},
        {"script_type", "shell"},
        {"arguments", json::array({"Hello from Enhanced Sequencer!"})}
    });
    
    if (scriptTask) {
        auto taskId = manager->addTask(std::move(scriptTask));
        
        // Execute the task
        json sequence = json::array();
        sequence.push_back({{"task_id", taskId}});
        
        std::cout << "Executing basic script task...\n";
        sequencer->executeSequence(sequence);
        
        // Check result
        auto status = manager->getTaskStatus(taskId);
        std::cout << "Task completed with status: " << static_cast<int>(status) << "\n";
    }
}

// Example 2: Template-Based Task Creation
void templateExample() {
    std::cout << "\n=== Template-Based Task Creation ===\n";
    
    auto manager = std::make_unique<TaskManager>();
    auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
    auto templates = std::make_unique<TaskTemplateManager>();
    
    // Create imaging task from template
    json imagingParams = {
        {"target", "M31 Andromeda Galaxy"},
        {"exposure_time", 300},
        {"filter", "Ha"},
        {"count", 5},
        {"binning", 1},
        {"gain", 1.0},
        {"cooling", true},
        {"temperature", -10.0}
    };
    
    auto imagingTask = templates->createTask("imaging", "m31_imaging", imagingParams);
    if (imagingTask) {
        auto taskId = manager->addTask(std::move(imagingTask));
        
        std::cout << "Created imaging task for M31 with:\n";
        std::cout << "  - Exposure: " << imagingParams["exposure_time"] << "s\n";
        std::cout << "  - Filter: " << imagingParams["filter"] << "\n";
        std::cout << "  - Count: " << imagingParams["count"] << " frames\n";
        
        // Execute the task
        json sequence = json::array();
        sequence.push_back({{"task_id", taskId}});
        
        // Set parallel execution for efficiency
        sequencer->setExecutionStrategy(ExecutionStrategy::Parallel);
        sequencer->executeSequence(sequence);
    }
    
    // Create calibration task from template
    json calibrationParams = {
        {"frame_type", "dark"},
        {"exposure_time", 300},
        {"count", 10},
        {"binning", 1},
        {"temperature", -10.0}
    };
    
    auto calibrationTask = templates->createTask("calibration", "dark_frames", calibrationParams);
    if (calibrationTask) {
        auto taskId = manager->addTask(std::move(calibrationTask));
        
        std::cout << "Created calibration task for dark frames:\n";
        std::cout << "  - Type: " << calibrationParams["frame_type"] << "\n";
        std::cout << "  - Exposure: " << calibrationParams["exposure_time"] << "s\n";
        std::cout << "  - Count: " << calibrationParams["count"] << " frames\n";
    }
}

// Example 3: Complex Workflow with Dependencies
void complexWorkflowExample() {
    std::cout << "\n=== Complex Workflow with Dependencies ===\n";
    
    auto manager = std::make_unique<TaskManager>();
    auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
    auto templates = std::make_unique<TaskTemplateManager>();
    auto& factory = TaskFactory::getInstance();
    
    // Configure sequencer for adaptive execution
    sequencer->setExecutionStrategy(ExecutionStrategy::Adaptive);
    sequencer->enableMonitoring(true);
    
    std::vector<std::string> taskIds;
    
    // 1. System safety check
    auto safetyTask = templates->createTask("safety_check", "initial_safety", json{});
    auto safetyId = manager->addTask(std::move(safetyTask));
    taskIds.push_back(safetyId);
    std::cout << "Added safety check task\n";
    
    // 2. Device initialization
    auto deviceTask = factory.createTask("device_task", "device_init", json{
        {"operation", "initialize"},
        {"deviceName", "primary_camera"},
        {"deviceType", "camera"},
        {"timeout", 10000}
    });
    auto deviceId = manager->addTask(std::move(deviceTask));
    manager->addDependency(deviceId, safetyId); // Depends on safety check
    taskIds.push_back(deviceId);
    std::cout << "Added device initialization task\n";
    
    // 3. Configuration setup
    auto configTask = factory.createTask("config_task", "setup_config", json{
        {"operation", "set"},
        {"key_path", "imaging.session_name"},
        {"value", "M31_Session_" + std::to_string(std::time(nullptr))}
    });
    auto configId = manager->addTask(std::move(configTask));
    manager->addDependency(configId, deviceId); // Depends on device init
    taskIds.push_back(configId);
    std::cout << "Added configuration task\n";
    
    // 4. Auto-focus
    auto focusTask = templates->createTask("focus", "auto_focus", json{
        {"focus_method", "star"},
        {"step_size", 5.0},
        {"max_steps", 50},
        {"samples", 5},
        {"tolerance", 2.0}
    });
    auto focusId = manager->addTask(std::move(focusTask));
    manager->addDependency(focusId, configId); // Depends on config
    taskIds.push_back(focusId);
    std::cout << "Added auto-focus task\n";
    
    // 5. Plate solving
    auto platesolveTask = templates->createTask("platesolve", "plate_solve", json{
        {"target", "M31"},
        {"exposure_time", 5},
        {"timeout", 60}
    });
    auto platesolveId = manager->addTask(std::move(platesolveTask));
    manager->addDependency(platesolveId, focusId); // Depends on focus
    taskIds.push_back(platesolveId);
    std::cout << "Added plate solving task\n";
    
    // 6. Multiple imaging tasks (can run in parallel)
    std::vector<std::string> imagingIds;
    std::vector<std::string> filters = {"Ha", "OIII", "SII"};
    
    for (const auto& filter : filters) {
        auto imagingTask = templates->createTask("imaging", "imaging_" + filter, json{
            {"target", "M31"},
            {"filter", filter},
            {"exposure_time", 300},
            {"count", 3}, // Reduced for demo
            {"binning", 1},
            {"gain", 1.0}
        });
        auto imagingId = manager->addTask(std::move(imagingTask));
        manager->addDependency(imagingId, platesolveId); // All depend on plate solve
        imagingIds.push_back(imagingId);
        taskIds.push_back(imagingId);
        std::cout << "Added imaging task for " << filter << " filter\n";
    }
    
    // Create sequence with all tasks
    json sequence = json::array();
    for (const auto& taskId : taskIds) {
        sequence.push_back({{"task_id", taskId}});
    }
    
    std::cout << "\nStarting complex workflow execution...\n";
    std::cout << "Total tasks: " << taskIds.size() << "\n";
    
    // Execute in background thread to allow monitoring
    std::thread executionThread([&sequencer, &sequence]() {
        sequencer->executeSequence(sequence);
    });
    
    // Monitor progress
    int lastProgress = -1;
    while (sequencer->isRunning()) {
        auto metrics = sequencer->getMetrics();
        int progress = metrics.value("progress_percentage", 0);
        
        if (progress != lastProgress) {
            std::cout << "Progress: " << progress << "% ";
            std::cout << "(Completed: " << metrics.value("completed_tasks", 0);
            std::cout << ", Failed: " << metrics.value("failed_tasks", 0) << ")\n";
            lastProgress = progress;
        }
        
        std::this_thread::sleep_for(1s);
    }
    
    if (executionThread.joinable()) {
        executionThread.join();
    }
    
    std::cout << "Workflow completed!\n";
    
    // Display final results
    auto finalMetrics = sequencer->getMetrics();
    std::cout << "\nFinal Results:\n";
    std::cout << "  Total tasks: " << finalMetrics.value("total_tasks", 0) << "\n";
    std::cout << "  Completed: " << finalMetrics.value("completed_tasks", 0) << "\n";
    std::cout << "  Failed: " << finalMetrics.value("failed_tasks", 0) << "\n";
    std::cout << "  Average execution time: " << finalMetrics.value("average_execution_time", 0) << "ms\n";
}

// Example 4: Parallel Execution Demonstration
void parallelExecutionExample() {
    std::cout << "\n=== Parallel Execution Demonstration ===\n";
    
    auto manager = std::make_unique<TaskManager>();
    auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
    auto& factory = TaskFactory::getInstance();
    
    // Configure for parallel execution
    sequencer->setExecutionStrategy(ExecutionStrategy::Parallel);
    sequencer->setConcurrencyLimit(4);
    
    std::vector<std::string> taskIds;
    
    // Create multiple independent tasks that can run in parallel
    for (int i = 0; i < 8; ++i) {
        auto task = factory.createTask("script_task", "parallel_task_" + std::to_string(i), json{
            {"script_path", "/bin/sleep"},
            {"script_type", "shell"},
            {"arguments", json::array({"2"})} // Sleep for 2 seconds
        });
        
        if (task) {
            auto taskId = manager->addTask(std::move(task));
            taskIds.push_back(taskId);
        }
    }
    
    std::cout << "Created " << taskIds.size() << " independent tasks\n";
    std::cout << "Concurrency limit: 4 tasks\n";
    
    // Create sequence
    json sequence = json::array();
    for (const auto& taskId : taskIds) {
        sequence.push_back({{"task_id", taskId}});
    }
    
    // Measure execution time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "Starting parallel execution...\n";
    sequencer->executeSequence(sequence);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    
    std::cout << "Parallel execution completed in " << duration.count() << " seconds\n";
    std::cout << "(Sequential would take ~" << (taskIds.size() * 2) << " seconds)\n";
    
    // Check all tasks completed successfully
    int completed = 0;
    for (const auto& taskId : taskIds) {
        if (manager->getTaskStatus(taskId) == TaskStatus::Completed) {
            completed++;
        }
    }
    
    std::cout << "Successfully completed: " << completed << "/" << taskIds.size() << " tasks\n";
}

// Example 5: Error Handling and Recovery
void errorHandlingExample() {
    std::cout << "\n=== Error Handling and Recovery ===\n";
    
    auto manager = std::make_unique<TaskManager>();
    auto sequencer = std::make_unique<EnhancedSequencer>(manager.get());
    auto& factory = TaskFactory::getInstance();
    
    // Create a task that will succeed
    auto successTask = factory.createTask("script_task", "success_task", json{
        {"script_path", "/bin/true"},
        {"script_type", "shell"}
    });
    auto successId = manager->addTask(std::move(successTask));
    
    // Create a task that will fail
    auto failTask = factory.createTask("script_task", "fail_task", json{
        {"script_path", "/bin/false"},
        {"script_type", "shell"},
        {"retry_count", 2}
    });
    auto failId = manager->addTask(std::move(failTask));
    
    // Create a task that depends on the failing task
    auto dependentTask = factory.createTask("script_task", "dependent_task", json{
        {"script_path", "/bin/echo"},
        {"script_type", "shell"},
        {"arguments", json::array({"This should not execute"})}
    });
    auto dependentId = manager->addTask(std::move(dependentTask));
    manager->addDependency(dependentId, failId);
    
    std::cout << "Created tasks: 1 success, 1 fail (with retry), 1 dependent\n";
    
    // Execute sequence
    json sequence = json::array();
    sequence.push_back({{"task_id", successId}});
    sequence.push_back({{"task_id", failId}});
    sequence.push_back({{"task_id", dependentId}});
    
    std::cout << "Executing sequence with error handling...\n";
    sequencer->executeSequence(sequence);
    
    // Check results
    std::cout << "\nTask Results:\n";
    std::cout << "  Success task: " << static_cast<int>(manager->getTaskStatus(successId)) << "\n";
    std::cout << "  Fail task: " << static_cast<int>(manager->getTaskStatus(failId)) << "\n";
    std::cout << "  Dependent task: " << static_cast<int>(manager->getTaskStatus(dependentId)) << "\n";
    
    // Get error information
    auto failResult = manager->getTaskResult(failId);
    if (failResult.contains("error")) {
        std::cout << "  Error details: " << failResult["error"] << "\n";
    }
}

// Example 6: Using Utility Functions
void utilityFunctionsExample() {
    std::cout << "\n=== Utility Functions Example ===\n";
    
    using namespace TaskUtils;
    
    // Generate common task parameters
    std::cout << "Generating imaging parameters:\n";
    auto imagingParams = CommonTasks::generateImagingParameters(
        "NGC2264", "Ha", 600, 20, 2, 2.0, true, -15.0
    );
    std::cout << "  Target: " << imagingParams["target"] << "\n";
    std::cout << "  Filter: " << imagingParams["filter"] << "\n";
    std::cout << "  Exposure: " << imagingParams["exposure_time"] << "s\n";
    std::cout << "  Count: " << imagingParams["count"] << " frames\n";
    
    std::cout << "\nGenerating calibration parameters:\n";
    auto calibrationParams = CommonTasks::generateCalibrationParameters(
        "flat", 5.0, 30, 1, -15.0
    );
    std::cout << "  Type: " << calibrationParams["frame_type"] << "\n";
    std::cout << "  Exposure: " << calibrationParams["exposure_time"] << "s\n";
    std::cout << "  Count: " << calibrationParams["count"] << " frames\n";
    
    std::cout << "\nGenerating focus parameters:\n";
    auto focusParams = CommonTasks::generateFocusParameters(
        "hfd", 10.0, 30, 3, 1.5
    );
    std::cout << "  Method: " << focusParams["focus_method"] << "\n";
    std::cout << "  Step size: " << focusParams["step_size"] << "\n";
    std::cout << "  Max steps: " << focusParams["max_steps"] << "\n";
    
    // Test sequence optimization
    std::cout << "\nTesting sequence optimization:\n";
    json tasks = json::array();
    for (int i = 0; i < 5; ++i) {
        tasks.push_back({
            {"name", "task_" + std::to_string(i)},
            {"priority", (i % 3) + 1},
            {"estimated_duration", (i + 1) * 30},
            {"dependencies", json::array()}
        });
    }
    
    auto optimized = SequencePatterns::optimizeSequence(tasks, {
        .minimizeTime = true,
        .balanceLoad = true,
        .respectPriority = true,
        .maxParallelTasks = 3
    });
    
    std::cout << "  Original tasks: " << tasks.size() << "\n";
    std::cout << "  Optimized sequence length: " << optimized.size() << "\n";
    
    // Validate parameters
    std::cout << "\nTesting parameter validation:\n";
    json validParams = {
        {"exposure_time", 300},
        {"count", 10},
        {"binning", 1}
    };
    
    json invalidParams = {
        {"exposure_time", -300}, // Invalid negative value
        {"count", 0},           // Invalid zero count
        {"binning", "invalid"}  // Invalid type
    };
    
    bool validResult = TaskValidation::validateTaskParameters(validParams);
    bool invalidResult = TaskValidation::validateTaskParameters(invalidParams);
    
    std::cout << "  Valid parameters: " << (validResult ? "PASS" : "FAIL") << "\n";
    std::cout << "  Invalid parameters: " << (invalidResult ? "FAIL (unexpected)" : "FAIL (expected)") << "\n";
}

// Main function to run all examples
int main() {
    std::cout << "Enhanced Sequence System Usage Examples\n";
    std::cout << "=====================================\n";
    
    try {
        // Run all examples
        basicTaskExample();
        templateExample();
        complexWorkflowExample();
        parallelExecutionExample();
        errorHandlingExample();
        utilityFunctionsExample();
        
        std::cout << "\n=== All Examples Completed Successfully ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error running examples: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
