#ifndef LITHIUM_TASK_COMPONENTS_SCRIPT_WORKFLOW_HPP
#define LITHIUM_TASK_COMPONENTS_SCRIPT_WORKFLOW_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "base.hpp"
#include "pipeline.hpp"  // For PipelineExecutionMode

// Forward declarations
namespace lithium {
class IsolatedPythonRunner;
class PythonToolRegistry;
}  // namespace lithium

namespace lithium::task::script {

/**
 * @brief Workflow event types for callbacks
 */
enum class WorkflowEventType {
    Started,
    StepStarted,
    StepCompleted,
    StepFailed,
    StepRetrying,
    Paused,
    Resumed,
    Completed,
    Failed,
    Aborted
};

/**
 * @brief Workflow event callback
 */
using WorkflowEventCallback =
    std::function<void(WorkflowEventType event, std::string_view workflowName,
                       std::string_view stepId, const json& data)>;

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
    ScriptType type{ScriptType::Auto};  ///< Type of the script.
    PipelineExecutionMode executionMode{PipelineExecutionMode::Embedded};
    std::vector<std::string> dependencies;  ///< Steps that must complete first
    json parameters;                    ///< Parameters to pass to the script.
    bool optional{false};               ///< Continue workflow on failure
    int maxRetries{0};                  ///< Maximum retry attempts
    std::chrono::seconds timeout{600};  ///< Step timeout (10 min default)

    // Tool call mode
    std::string toolName;      ///< Registered tool name
    std::string functionName;  ///< Function to call

    // Conditional execution
    std::string condition;  ///< JSON expression for conditional execution
    std::string onSuccess;  ///< Step to run on success (optional)
    std::string onFailure;  ///< Step to run on failure (optional)
};

/**
 * @struct WorkflowStepResult
 * @brief Result of executing a workflow step
 */
struct WorkflowStepResult {
    std::string taskId;
    bool success{false};
    json output;
    std::string error;
    std::chrono::milliseconds executionTime{0};
    int retryCount{0};
    WorkflowState finalState{WorkflowState::Created};
};

/**
 * @struct WorkflowResult
 * @brief Overall result of workflow execution
 */
struct WorkflowResult {
    std::string workflowName;
    bool success{false};
    WorkflowState finalState{WorkflowState::Created};
    std::map<std::string, WorkflowStepResult> stepResults;
    json finalContext;
    std::chrono::milliseconds totalExecutionTime{0};
    size_t successfulSteps{0};
    size_t failedSteps{0};
    size_t skippedSteps{0};
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
 *
 * Enhanced features:
 * - Isolated Python execution for security
 * - Tool registry integration for calling registered tools
 * - Retry support with configurable attempts
 * - Event callbacks for monitoring
 * - Conditional step execution with success/failure handlers
 * - Parallel step execution based on dependencies
 */
class ScriptWorkflowTask : public Task {
public:
    /**
     * @brief Constructs a ScriptWorkflowTask with the given name.
     * @param name The name of the workflow task.
     */
    explicit ScriptWorkflowTask(const std::string& name);

    /**
     * @brief Destructor
     */
    ~ScriptWorkflowTask() override;

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
     * @return WorkflowResult containing execution details
     */
    WorkflowResult executeWorkflow(const std::string& workflowName,
                                   const json& params = {});

    /**
     * @brief Executes a workflow asynchronously
     */
    std::future<WorkflowResult> executeWorkflowAsync(
        const std::string& workflowName, const json& params = {});

    /**
     * @brief Pauses the execution of the specified workflow.
     */
    void pauseWorkflow(const std::string& workflowName);

    /**
     * @brief Resumes the execution of a paused workflow.
     */
    void resumeWorkflow(const std::string& workflowName);

    /**
     * @brief Aborts the execution of the specified workflow.
     */
    void abortWorkflow(const std::string& workflowName);

    /**
     * @brief Gets the current state of the specified workflow.
     */
    [[nodiscard]] WorkflowState getWorkflowState(
        const std::string& workflowName) const;

    /**
     * @brief Gets the result of a completed workflow
     */
    [[nodiscard]] std::optional<WorkflowResult> getWorkflowResult(
        const std::string& workflowName) const;

    /**
     * @brief Sets the event callback for workflow monitoring
     */
    void setEventCallback(WorkflowEventCallback callback);

    /**
     * @brief Sets the tool registry for tool call mode
     */
    void setToolRegistry(std::shared_ptr<PythonToolRegistry> registry);

    /**
     * @brief Enables or disables isolated execution for Python scripts
     */
    void setIsolatedExecution(bool enabled);

    /**
     * @brief Sets the maximum concurrent steps
     */
    void setMaxConcurrentSteps(size_t maxSteps);

    /**
     * @brief Gets list of all registered workflow names
     */
    [[nodiscard]] std::vector<std::string> getWorkflowNames() const;

    /**
     * @brief Checks if a workflow exists
     */
    [[nodiscard]] bool hasWorkflow(const std::string& workflowName) const;

    /**
     * @brief Deletes a workflow definition
     */
    bool deleteWorkflow(const std::string& workflowName);

protected:
    /**
     * @brief Executes all steps of the specified workflow, respecting
     * dependencies.
     */
    WorkflowResult executeWorkflowSteps(const std::string& workflowName);

    /**
     * @brief Checks if all dependencies for a workflow step are satisfied.
     */
    bool checkStepDependencies(const WorkflowStep& step,
                               const std::string& workflowName);

    /**
     * @brief Executes a single workflow step with retry support
     */
    WorkflowStepResult executeWorkflowStep(const WorkflowStep& step,
                                           json& context);

    /**
     * @brief Executes a step in isolated mode
     */
    WorkflowStepResult executeStepIsolated(const WorkflowStep& step,
                                           const json& context);

    /**
     * @brief Executes a step via tool registry
     */
    WorkflowStepResult executeStepToolCall(const WorkflowStep& step,
                                           const json& context);

    /**
     * @brief Evaluates condition for conditional execution
     */
    bool evaluateCondition(const std::string& condition,
                           const json& context) const;

    /**
     * @brief Emits a workflow event
     */
    void emitEvent(WorkflowEventType event, const std::string& workflowName,
                   const std::string& stepId = "", const json& data = {});

private:
    /**
     * @brief Sets up default parameter definitions and workflow properties.
     */
    void setupWorkflowDefaults();

    std::map<std::string, std::vector<WorkflowStep>> workflows_;
    std::map<std::string, WorkflowState> workflowStates_;
    std::map<std::string, std::set<std::string>> completedSteps_;
    std::map<std::string, WorkflowResult> workflowResults_;
    std::map<std::string, json> workflowContexts_;

    mutable std::shared_mutex workflowMutex_;
    std::condition_variable_any workflowCondition_;

    std::unique_ptr<BaseScriptTask> shellTask_;
    std::unique_ptr<BaseScriptTask> pythonTask_;

    // Enhanced components
    std::unique_ptr<IsolatedPythonRunner> isolatedRunner_;
    std::shared_ptr<PythonToolRegistry> toolRegistry_;

    // Configuration
    bool useIsolation_{false};
    size_t maxConcurrentSteps_{4};

    // Callbacks
    WorkflowEventCallback eventCallback_;
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_COMPONENTS_SCRIPT_WORKFLOW_HPP
