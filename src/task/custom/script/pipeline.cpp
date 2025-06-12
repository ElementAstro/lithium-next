#include "pipeline.hpp"
#include "factory.hpp"
#include "python.hpp"
#include "shell.hpp"
#include "spdlog/spdlog.h"

namespace lithium::task::script {

ScriptPipelineTask::ScriptPipelineTask(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }) {
    // Initialize script task instances
    shellTask_ = std::make_unique<ShellScriptTask>("pipeline_shell");
    pythonTask_ = std::make_unique<PythonScriptTask>("pipeline_python");

    setupPipelineDefaults();
}

void ScriptPipelineTask::setupPipelineDefaults() {
    addParamDefinition("pipeline", "array", true, json::array(),
                       "Array of pipeline steps");
    addParamDefinition("sharedContext", "object", false, json::object(),
                       "Shared context between steps");
    addParamDefinition("continueOnError", "boolean", false, false,
                       "Continue pipeline on step failure");
    addParamDefinition("maxParallelSteps", "number", false, 1,
                       "Maximum parallel steps");

    setTimeout(std::chrono::seconds(1800));  // 30 minutes default
    setPriority(6);
    setTaskType("script_pipeline");

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Pipeline task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Pipeline exception: " + std::string(e.what()));
    });
}

void ScriptPipelineTask::execute(const json& params) {
    addHistoryEntry("Starting pipeline execution");

    try {
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Pipeline parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            throw std::invalid_argument(errorMsg);
        }

        // Load pipeline steps from parameters
        if (params.contains("pipeline") && params["pipeline"].is_array()) {
            pipeline_.clear();
            for (const auto& stepJson : params["pipeline"]) {
                PipelineStep step;
                step.scriptName = stepJson["scriptName"].get<std::string>();
                step.type = stepJson.value("type", "auto") == "python"
                                ? ScriptType::Python
                                : ScriptType::Shell;
                step.continueOnError = stepJson.value("continueOnError", false);

                if (stepJson.contains("args") && stepJson["args"].is_object()) {
                    step.args =
                        stepJson["args"]
                            .get<
                                std::unordered_map<std::string, std::string>>();
                }

                pipeline_.push_back(step);
            }
        }

        // Set shared context
        sharedContext_ = params.value("sharedContext", json::object());

        // Execute pipeline
        executePipeline(sharedContext_);

        addHistoryEntry("Pipeline execution completed successfully");

    } catch (const std::exception& e) {
        handleStepError(PipelineStep{}, e.what());
        throw;
    }
}

void ScriptPipelineTask::executePipeline(const json& sharedContext) {
    spdlog::info("Executing pipeline with {} steps", pipeline_.size());
    addHistoryEntry("Executing pipeline with " +
                    std::to_string(pipeline_.size()) + " steps");

    json currentContext = sharedContext;

    for (size_t i = 0; i < pipeline_.size(); ++i) {
        const auto& step = pipeline_[i];

        try {
            spdlog::info("Executing pipeline step {}: {}", i + 1,
                         step.scriptName);
            addHistoryEntry("Step " + std::to_string(i + 1) + ": " +
                            step.scriptName);

            // Prepare parameters for the step
            json stepParams = json{{"scriptName", step.scriptName},
                                   {"args", step.args},
                                   {"sharedContext", currentContext}};

            // Execute based on script type
            BaseScriptTask* taskToUse = nullptr;
            if (step.type == ScriptType::Python) {
                taskToUse = pythonTask_.get();
            } else {
                taskToUse = shellTask_.get();
            }

            if (taskToUse) {
                taskToUse->execute(stepParams);

                // Update shared context with step results
                currentContext["lastStepResult"] =
                    json{{"stepName", step.scriptName},
                         {"stepIndex", i},
                         {"success", true}};
            }

        } catch (const std::exception& e) {
            std::string errorMsg = "Pipeline step " + std::to_string(i + 1) +
                                   " failed: " + e.what();

            if (!step.continueOnError) {
                handleStepError(step, errorMsg);
                throw std::runtime_error(errorMsg);
            } else {
                spdlog::warn("Pipeline step failed but continuing: {}",
                             errorMsg);
                addHistoryEntry("Step " + std::to_string(i + 1) +
                                " failed (continuing): " + errorMsg);

                // Update context with failure info
                currentContext["lastStepResult"] =
                    json{{"stepName", step.scriptName},
                         {"stepIndex", i},
                         {"success", false},
                         {"error", e.what()}};
            }
        }
    }

    // Store final context
    sharedContext_ = currentContext;
}

void ScriptPipelineTask::addPipelineStep(const PipelineStep& step) {
    pipeline_.push_back(step);
    addHistoryEntry("Added pipeline step: " + step.scriptName);
}

void ScriptPipelineTask::clearPipeline() {
    pipeline_.clear();
    addHistoryEntry("Pipeline cleared");
}

void ScriptPipelineTask::setSharedContext(const json& context) {
    sharedContext_ = context;
    addHistoryEntry("Shared context updated");
}

std::future<bool> ScriptPipelineTask::executeAsync(const json& params) {
    return std::async(std::launch::async, [this, params]() {
        try {
            execute(params);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Async pipeline execution failed: {}", e.what());
            return false;
        }
    });
}

void ScriptPipelineTask::handleStepError(const PipelineStep& step,
                                         const std::string& error) {
    spdlog::error("Pipeline step error [{}]: {}", step.scriptName, error);
    setErrorType(TaskErrorType::SystemError);
    addHistoryEntry("Pipeline step error (" + step.scriptName + "): " + error);
}

// Register with factory
namespace {
static auto pipeline_task_registrar = TaskRegistrar<ScriptPipelineTask>(
    "script_pipeline",
    TaskInfo{
        .name = "script_pipeline",
        .description = "Execute a sequence of scripts as a pipeline",
        .category = "automation",
        .requiredParameters = {"pipeline"},
        .parameterSchema =
            json{{"pipeline",
                  {{"type", "array"},
                   {"description", "Array of pipeline steps"},
                   {"items",
                    {{"type", "object"},
                     {"properties",
                      {{"scriptName", {{"type", "string"}}},
                       {"type",
                        {{"type", "string"},
                         {"enum", json::array({"shell", "python", "auto"})}}},
                       {"args", {{"type", "object"}}},
                       {"continueOnError", {{"type", "boolean"}}}}},
                     {"required", json::array({"scriptName"})}}}}},
                 {"sharedContext",
                  {{"type", "object"},
                   {"description", "Shared context between steps"}}},
                 {"continueOnError",
                  {{"type", "boolean"},
                   {"description", "Continue on step failure"}}},
                 {"maxParallelSteps",
                  {{"type", "number"},
                   {"description", "Max parallel steps"},
                   {"default", 1}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<ScriptPipelineTask> {
        return std::make_unique<ScriptPipelineTask>(name);
    });
}  // namespace

}  // namespace lithium::task::script