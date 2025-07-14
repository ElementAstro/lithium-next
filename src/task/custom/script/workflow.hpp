#ifndef LITHIUM_TASK_SCRIPT_WORKFLOW_TASK_HPP
#define LITHIUM_TASK_SCRIPT_WORKFLOW_TASK_HPP

#include <condition_variable>
#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "base.hpp"

namespace lithium::task::script {

/**
 * @enum WorkflowState
 * @brief Represents the current state of a workflow.
 */
enum class WorkflowState {
    Created,    ///< Workflow has been created but not started.
    Running,    ///< Workflow is currently running.
    Paused,     ///< Workflow execution is paused.
    Completed,  ///< Workflow has completed successfully.
    Failed,     ///< Workflow execution failed.
    Aborted     ///< Workflow was aborted by the user.
};

/**
 * @struct WorkflowStep
 * @brief Represents a single step in a workflow.
 *
 * Each WorkflowStep defines a script to execute, its type, dependencies on
 * other steps, parameters to pass, and whether the step is optional.
 */
struct WorkflowStep {
    std::string taskId;      ///< Unique identifier for this step.
    std::string scriptName;  ///< Name or path of the script to execute.
    ScriptType type;         ///< Type of the script (Shell, Python, or Auto).
    std::vector<std::string> dependencies;  ///< List of taskIds that must
                                            ///< complete before this step runs.
    json parameters;  ///< Parameters to pass to the script.
    bool optional{
        false};  ///< If true, workflow continues even if this step fails.
};

/**
 * @class ScriptWorkflowTask
 * @brief Manages and executes complex script workflows with dependencies.
 *
 * ScriptWorkflowTask allows the definition and execution of workflows
 * consisting of multiple steps, where each step can depend on the completion of
 * other steps. Steps can be shell or Python scripts, and may be marked as
 * optional. The class supports pausing, resuming, and aborting workflows, and
 * provides thread-safe state management.
 */
class ScriptWorkflowTask : public Task {
public:
    /**
     * @brief Constructs a ScriptWorkflowTask with the given name.
     * @param name The name of the workflow task.
     */
    ScriptWorkflowTask(const std::string& name);

    /**
     * @brief Executes the workflow task with the provided parameters.
     * @param params JSON object containing workflow configuration.
     */
    void execute(const json& params) override;

    /**
     * @brief Creates a new workflow with the specified steps.
     * @param workflowName The unique name of the workflow.
     * @param steps Vector of WorkflowStep objects defining the workflow.
     */
    void createWorkflow(const std::string& workflowName,
                        const std::vector<WorkflowStep>& steps);

    /**
     * @brief Executes the specified workflow.
     * @param workflowName The name of the workflow to execute.
     * @param params Optional JSON parameters for execution.
     */
    void executeWorkflow(const std::string& workflowName,
                         const json& params = {});

    /**
     * @brief Pauses the execution of the specified workflow.
     * @param workflowName The name of the workflow to pause.
     */
    void pauseWorkflow(const std::string& workflowName);

    /**
     * @brief Resumes the execution of a paused workflow.
     * @param workflowName The name of the workflow to resume.
     */
    void resumeWorkflow(const std::string& workflowName);

    /**
     * @brief Aborts the execution of the specified workflow.
     * @param workflowName The name of the workflow to abort.
     */
    void abortWorkflow(const std::string& workflowName);

    /**
     * @brief Gets the current state of the specified workflow.
     * @param workflowName The name of the workflow.
     * @return WorkflowState representing the workflow's current state.
     */
    WorkflowState getWorkflowState(const std::string& workflowName) const;

protected:
    /**
     * @brief Executes all steps of the specified workflow, respecting
     * dependencies.
     * @param workflowName The name of the workflow to execute.
     */
    void executeWorkflowSteps(const std::string& workflowName);

    /**
     * @brief Checks if all dependencies for a workflow step are satisfied.
     * @param step The WorkflowStep to check.
     * @return True if all dependencies are completed, false otherwise.
     */
    bool checkStepDependencies(const WorkflowStep& step);

    /**
     * @brief Executes a single workflow step.
     * @param step The WorkflowStep to execute.
     */
    void executeWorkflowStep(const WorkflowStep& step);

private:
    /**
     * @brief Sets up default parameter definitions and workflow properties.
     */
    void setupWorkflowDefaults();

    std::map<std::string, std::vector<WorkflowStep>>
        workflows_;  ///< Map of workflow names to their steps.
    std::map<std::string, WorkflowState>
        workflowStates_;  ///< Map of workflow names to their current state.
    std::map<std::string, std::set<std::string>>
        completedSteps_;  ///< Map of workflow names to completed step IDs.

    mutable std::shared_mutex
        workflowMutex_;  ///< Mutex for thread-safe workflow state access.
    std::condition_variable_any
        workflowCondition_;  ///< Condition variable for workflow state changes.

    std::unique_ptr<BaseScriptTask>
        shellTask_;  ///< Task instance for shell scripts.
    std::unique_ptr<BaseScriptTask>
        pythonTask_;  ///< Task instance for Python scripts.
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_SCRIPT_WORKFLOW_TASK_HPP
