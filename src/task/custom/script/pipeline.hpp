#ifndef LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP
#define LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP

#include <future>
#include <string>
#include <unordered_map>
#include <vector>
#include "base.hpp"

namespace lithium::task::script {

/**
 * @struct PipelineStep
 * @brief Represents a single step in a script pipeline.
 *
 * Each PipelineStep defines a script to execute, its type (shell or Python),
 * the arguments to pass, and whether the pipeline should continue if this step
 * fails.
 */
struct PipelineStep {
    std::string scriptName;  ///< Name or path of the script to execute.
    ScriptType type;         ///< Type of the script (Shell, Python, or Auto).
    std::unordered_map<std::string, std::string>
        args;  ///< Arguments to pass to the script.
    bool continueOnError{
        false};  ///< If true, pipeline continues even if this step fails.
};

/**
 * @class ScriptPipelineTask
 * @brief Executes a sequence of scripts as a pipeline.
 *
 * ScriptPipelineTask manages and executes a series of scripts (steps) in order.
 * Each step can be a shell or Python script, and may have its own arguments and
 * error handling policy. The pipeline can be executed synchronously or
 * asynchronously, and supports a shared context for passing data between steps.
 */
class ScriptPipelineTask : public Task {
public:
    /**
     * @brief Constructs a ScriptPipelineTask with the given name.
     * @param name The name of the pipeline task.
     */
    ScriptPipelineTask(const std::string& name);

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
     * @return std::future<bool> that resolves to true if the pipeline succeeds,
     * false otherwise.
     */
    std::future<bool> executeAsync(const json& params = {});

protected:
    /**
     * @brief Internal method to execute all pipeline steps in order.
     * @param sharedContext JSON object shared between steps.
     */
    void executePipeline(const json& sharedContext);

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
    std::unique_ptr<BaseScriptTask>
        shellTask_;  ///< Task instance for shell scripts.
    std::unique_ptr<BaseScriptTask>
        pythonTask_;  ///< Task instance for Python scripts.
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_SCRIPT_PIPELINE_TASK_HPP