#ifndef LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP
#define LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base.hpp"

// Forward declarations
namespace lithium {
class IsolatedPythonRunner;
class PythonToolRegistry;
}  // namespace lithium

namespace lithium::task::script {

/**
 * @brief Execution mode for pipeline steps
 */
enum class PipelineExecutionMode {
    Embedded,  ///< Use embedded Python interpreter
    Isolated,  ///< Use isolated subprocess
    ToolCall   ///< Call registered Python tool
};

/**
 * @brief Progress callback for pipeline execution
 */
using PipelineProgressCallback =
    std::function<void(size_t currentStep, size_t totalSteps, float progress,
                       std::string_view stepName, std::string_view message)>;

/**
 * @struct PipelineStep
 * @brief Represents a single step in a script pipeline.
 *
 * Each PipelineStep defines a script to execute, its type (shell or Python),
 * the arguments to pass, and whether the pipeline should continue if this step
 * fails.
 */
struct PipelineStep {
    std::string stepId;      ///< Unique identifier for this step
    std::string scriptName;  ///< Name or path of the script to execute.
    ScriptType type{ScriptType::Auto};  ///< Type of the script.
    PipelineExecutionMode executionMode{PipelineExecutionMode::Embedded};
    std::unordered_map<std::string, std::string>
        args;                     ///< Arguments to pass to the script.
    bool continueOnError{false};  ///< If true, pipeline continues on failure.
    int maxRetries{0};            ///< Maximum retry attempts on failure
    std::chrono::seconds timeout{300};  ///< Step timeout (5 min default)

    // For tool call mode
    std::string toolName;      ///< Registered tool name
    std::string functionName;  ///< Function to call on the tool

    // Conditional execution
    std::string condition;  ///< JSON expression for conditional execution
};

/**
 * @struct PipelineStepResult
 * @brief Result of executing a single pipeline step
 */
struct PipelineStepResult {
    std::string stepId;
    bool success{false};
    json output;
    std::string error;
    std::chrono::milliseconds executionTime{0};
    int retryCount{0};
};

/**
 * @struct PipelineResult
 * @brief Overall result of pipeline execution
 */
struct PipelineResult {
    bool success{false};
    std::vector<PipelineStepResult> stepResults;
    json finalContext;
    std::chrono::milliseconds totalExecutionTime{0};
    size_t successfulSteps{0};
    size_t failedSteps{0};
    size_t skippedSteps{0};
};

/**
 * @class ScriptPipelineTask
 * @brief Executes a sequence of scripts as a pipeline.
 *
 * ScriptPipelineTask manages and executes a series of scripts (steps) in order.
 * Each step can be a shell or Python script, and may have its own arguments and
 * error handling policy. The pipeline can be executed synchronously or
 * asynchronously, and supports a shared context for passing data between steps.
 *
 * Enhanced features:
 * - Isolated Python execution for security
 * - Tool registry integration for calling registered tools
 * - Retry support with configurable attempts
 * - Progress callbacks for monitoring
 * - Conditional step execution
 */
class ScriptPipelineTask : public Task {
public:
    /**
     * @brief Constructs a ScriptPipelineTask with the given name.
     * @param name The name of the pipeline task.
     */
    explicit ScriptPipelineTask(const std::string& name);

    /**
     * @brief Destructor
     */
    ~ScriptPipelineTask() override;

    /**
     * @brief Executes the pipeline synchronously with the provided parameters.
     * @param params JSON object containing pipeline configuration and shared
     * context.
     */
    void execute(const json& params) override;

    /**
     * @brief Adds a step to the pipeline.
     * @param step The PipelineStep to add.
     */
    void addPipelineStep(const PipelineStep& step);

    /**
     * @brief Removes all steps from the pipeline.
     */
    void clearPipeline();

    /**
     * @brief Sets the shared context for the pipeline.
     * @param context JSON object to be shared among steps.
     */
    void setSharedContext(const json& context);

    /**
     * @brief Executes the pipeline asynchronously.
     * @param params Optional JSON parameters for execution.
     * @return std::future<PipelineResult> containing the execution result.
     */
    std::future<PipelineResult> executeAsync(const json& params = {});

    /**
     * @brief Sets the progress callback for monitoring execution
     */
    void setProgressCallback(PipelineProgressCallback callback);

    /**
     * @brief Cancels the current pipeline execution
     */
    void cancel();

    /**
     * @brief Checks if the pipeline is currently running
     */
    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Gets the current step index (0-based)
     */
    [[nodiscard]] std::optional<size_t> getCurrentStepIndex() const noexcept;

    /**
     * @brief Sets the default execution mode for steps
     */
    void setDefaultExecutionMode(PipelineExecutionMode mode);

    /**
     * @brief Enables or disables isolated execution for Python scripts
     */
    void setIsolatedExecution(bool enabled);

    /**
     * @brief Sets the tool registry for tool call mode
     */
    void setToolRegistry(std::shared_ptr<PythonToolRegistry> registry);

protected:
    /**
     * @brief Internal method to execute all pipeline steps in order.
     * @param sharedContext JSON object shared between steps.
     * @return PipelineResult containing results from all steps
     */
    PipelineResult executePipeline(const json& sharedContext);

    /**
     * @brief Executes a single step with retry support
     */
    PipelineStepResult executeStep(const PipelineStep& step, json& context);

    /**
     * @brief Executes a step in isolated mode
     */
    PipelineStepResult executeStepIsolated(const PipelineStep& step,
                                           const json& context);

    /**
     * @brief Executes a step via tool registry
     */
    PipelineStepResult executeStepToolCall(const PipelineStep& step,
                                           const json& context);

    /**
     * @brief Evaluates condition for conditional execution
     */
    bool evaluateCondition(const std::string& condition,
                           const json& context) const;

    /**
     * @brief Handles errors that occur during a pipeline step.
     * @param step The PipelineStep that failed.
     * @param error The error message.
     */
    void handleStepError(const PipelineStep& step, const std::string& error);

private:
    /**
     * @brief Sets up default parameter definitions and pipeline properties.
     */
    void setupPipelineDefaults();

    std::vector<PipelineStep> pipeline_;  ///< List of steps in the pipeline.
    json sharedContext_;                  ///< Shared context for all steps.
    std::unique_ptr<BaseScriptTask> shellTask_;   ///< Shell script task
    std::unique_ptr<BaseScriptTask> pythonTask_;  ///< Python script task

    // Enhanced components
    std::unique_ptr<IsolatedPythonRunner> isolatedRunner_;
    std::shared_ptr<PythonToolRegistry> toolRegistry_;

    // Execution state
    std::atomic<bool> running_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<size_t> currentStep_{0};

    // Configuration
    PipelineExecutionMode defaultMode_{PipelineExecutionMode::Embedded};
    bool useIsolation_{false};

    // Callbacks
    PipelineProgressCallback progressCallback_;
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP
