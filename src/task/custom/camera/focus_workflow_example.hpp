#ifndef LITHIUM_TASK_CAMERA_FOCUS_WORKFLOW_EXAMPLE_HPP
#define LITHIUM_TASK_CAMERA_FOCUS_WORKFLOW_EXAMPLE_HPP

#include "focus_tasks.hpp"
#include <memory>
#include <vector>

namespace lithium::task::example {

/**
 * @brief Example focus workflow demonstrating the enhanced Task features
 * and task dependency management for complex focusing operations.
 */
class FocusWorkflowExample {
public:
    /**
     * @brief Creates a comprehensive focus workflow with dependencies
     * This example shows how to chain multiple focus tasks together
     * with proper dependency management and error handling.
     */
    static auto createComprehensiveFocusWorkflow() -> std::vector<std::unique_ptr<lithium::task::task::Task>>;

    /**
     * @brief Creates a simple autofocus workflow
     * Demonstrates basic autofocus with validation and backlash compensation
     */
    static auto createSimpleAutoFocusWorkflow() -> std::vector<std::unique_ptr<lithium::task::task::Task>>;

    /**
     * @brief Creates a temperature-compensated focus workflow
     * Shows how to set up temperature monitoring and compensation
     */
    static auto createTemperatureCompensatedWorkflow() -> std::vector<std::unique_ptr<lithium::task::task::Task>>;

    /**
     * @brief Demonstrates how to set up task dependencies
     */
    static void setupTaskDependencies(
        const std::vector<std::unique_ptr<lithium::task::task::Task>>& tasks);

private:
    static constexpr const char* WORKFLOW_VERSION = "1.0.0";
};

}  // namespace lithium::task::example

#endif  // LITHIUM_TASK_CAMERA_FOCUS_WORKFLOW_EXAMPLE_HPP
