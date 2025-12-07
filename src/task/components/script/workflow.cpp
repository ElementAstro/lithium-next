#include "workflow.hpp"

#include "python.hpp"
#include "shell.hpp"

#include "../../core/factory.hpp"

#include "spdlog/spdlog.h"

namespace lithium::task::script {

ScriptWorkflowTask::ScriptWorkflowTask(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }) {
    shellTask_ = std::make_unique<ShellScriptTask>("workflow_shell");
    pythonTask_ = std::make_unique<PythonScriptTask>("workflow_python");

    setupWorkflowDefaults();
}

void ScriptWorkflowTask::setupWorkflowDefaults() {
    addParamDefinition("workflowName", "string", true, nullptr,
                       "Name of the workflow to execute");
    addParamDefinition("workflow", "object", false, json::object(),
                       "Workflow definition");
    addParamDefinition("maxConcurrentSteps", "number", false, 3,
                       "Maximum concurrent steps");
    addParamDefinition("timeout", "number", false, 3600,
                       "Workflow timeout in seconds");
    addParamDefinition("retryFailedSteps", "boolean", false, false,
                       "Retry failed steps");

    setTimeout(std::chrono::seconds(3600));  // 1 hour default
    setPriority(7);
    setTaskType("script_workflow");

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Workflow task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Workflow exception: " + std::string(e.what()));
    });
}

void ScriptWorkflowTask::execute(const json& params) {
    addHistoryEntry("Starting workflow execution");

    try {
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Workflow parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            throw std::invalid_argument(errorMsg);
        }

        std::string workflowName = params["workflowName"].get<std::string>();

        // Create workflow if definition provided
        if (params.contains("workflow") && params["workflow"].is_object()) {
            const auto& workflowDef = params["workflow"];
            std::vector<WorkflowStep> steps;

            if (workflowDef.contains("steps") &&
                workflowDef["steps"].is_array()) {
                for (const auto& stepJson : workflowDef["steps"]) {
                    WorkflowStep step;
                    step.taskId = stepJson["taskId"].get<std::string>();
                    step.scriptName = stepJson["scriptName"].get<std::string>();
                    step.type = stepJson.value("type", "auto") == "python"
                                    ? ScriptType::Python
                                    : ScriptType::Shell;
                    step.optional = stepJson.value("optional", false);
                    step.parameters =
                        stepJson.value("parameters", json::object());

                    if (stepJson.contains("dependencies") &&
                        stepJson["dependencies"].is_array()) {
                        for (const auto& dep : stepJson["dependencies"]) {
                            step.dependencies.push_back(dep.get<std::string>());
                        }
                    }

                    steps.push_back(step);
                }
            }

            createWorkflow(workflowName, steps);
        }

        // Execute the workflow
        executeWorkflow(workflowName, params);

        addHistoryEntry("Workflow execution completed successfully");

    } catch (const std::exception& e) {
        spdlog::error("Workflow execution failed: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        throw;
    }
}

void ScriptWorkflowTask::createWorkflow(
    const std::string& workflowName, const std::vector<WorkflowStep>& steps) {
    std::unique_lock lock(workflowMutex_);

    workflows_[workflowName] = steps;
    workflowStates_[workflowName] = WorkflowState::Created;
    completedSteps_[workflowName] = std::set<std::string>();

    addHistoryEntry("Workflow created: " + workflowName + " with " +
                    std::to_string(steps.size()) + " steps");
}

void ScriptWorkflowTask::executeWorkflow(const std::string& workflowName,
                                         const json& params) {
    std::unique_lock lock(workflowMutex_);

    auto workflowIt = workflows_.find(workflowName);
    if (workflowIt == workflows_.end()) {
        throw std::runtime_error("Workflow not found: " + workflowName);
    }

    workflowStates_[workflowName] = WorkflowState::Running;
    lock.unlock();

    spdlog::info("Executing workflow: {}", workflowName);
    addHistoryEntry("Starting workflow execution: " + workflowName);

    try {
        executeWorkflowSteps(workflowName);

        lock.lock();
        workflowStates_[workflowName] = WorkflowState::Completed;
        lock.unlock();

        spdlog::info("Workflow completed successfully: {}", workflowName);
        addHistoryEntry("Workflow completed: " + workflowName);

    } catch (const std::exception& e) {
        lock.lock();
        workflowStates_[workflowName] = WorkflowState::Failed;
        lock.unlock();

        throw std::runtime_error("Workflow execution failed: " +
                                 std::string(e.what()));
    }
}

void ScriptWorkflowTask::executeWorkflowSteps(const std::string& workflowName) {
    auto workflowIt = workflows_.find(workflowName);
    if (workflowIt == workflows_.end()) {
        return;
    }

    const auto& steps = workflowIt->second;
    std::vector<std::thread> activeThreads;
    std::mutex completionMutex;

    // Execute steps based on dependencies
    for (const auto& step : steps) {
        // Wait for dependencies
        while (!checkStepDependencies(step)) {
            std::unique_lock lock(workflowMutex_);
            workflowCondition_.wait_for(
                lock, std::chrono::seconds(1), [this, workflowName]() {
                    return workflowStates_[workflowName] !=
                           WorkflowState::Running;
                });

            if (workflowStates_[workflowName] != WorkflowState::Running) {
                // Workflow was paused or aborted
                for (auto& thread : activeThreads) {
                    if (thread.joinable()) {
                        thread.join();
                    }
                }
                return;
            }
        }

        // Execute step in separate thread
        activeThreads.emplace_back(
            [this, step, workflowName, &completionMutex]() {
                try {
                    executeWorkflowStep(step);

                    std::lock_guard lock(completionMutex);
                    std::unique_lock wfLock(workflowMutex_);
                    completedSteps_[workflowName].insert(step.taskId);
                    wfLock.unlock();

                    workflowCondition_.notify_all();

                } catch (const std::exception& e) {
                    if (!step.optional) {
                        spdlog::error("Required workflow step failed: {} - {}",
                                      step.taskId, e.what());
                        std::unique_lock wfLock(workflowMutex_);
                        workflowStates_[workflowName] = WorkflowState::Failed;
                        wfLock.unlock();
                        workflowCondition_.notify_all();
                    } else {
                        spdlog::warn("Optional workflow step failed: {} - {}",
                                     step.taskId, e.what());
                    }
                }
            });
    }

    // Wait for all threads to complete
    for (auto& thread : activeThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

bool ScriptWorkflowTask::checkStepDependencies(const WorkflowStep& step) {
    std::shared_lock lock(workflowMutex_);

    for (const auto& workflowName : workflows_ | std::views::keys) {
        const auto& completed = completedSteps_[workflowName];

        for (const auto& dependency : step.dependencies) {
            if (completed.find(dependency) == completed.end()) {
                return false;
            }
        }
    }

    return true;
}

void ScriptWorkflowTask::executeWorkflowStep(const WorkflowStep& step) {
    spdlog::info("Executing workflow step: {}", step.taskId);
    addHistoryEntry("Executing step: " + step.taskId + " (" + step.scriptName +
                    ")");

    json stepParams =
        json{{"scriptName", step.scriptName}, {"args", step.parameters}};

    BaseScriptTask* taskToUse = nullptr;
    if (step.type == ScriptType::Python) {
        taskToUse = pythonTask_.get();
    } else {
        taskToUse = shellTask_.get();
    }

    if (taskToUse) {
        taskToUse->execute(stepParams);
    }
}

void ScriptWorkflowTask::pauseWorkflow(const std::string& workflowName) {
    std::unique_lock lock(workflowMutex_);
    workflowStates_[workflowName] = WorkflowState::Paused;
    addHistoryEntry("Workflow paused: " + workflowName);
    workflowCondition_.notify_all();
}

void ScriptWorkflowTask::resumeWorkflow(const std::string& workflowName) {
    std::unique_lock lock(workflowMutex_);
    if (workflowStates_[workflowName] == WorkflowState::Paused) {
        workflowStates_[workflowName] = WorkflowState::Running;
        addHistoryEntry("Workflow resumed: " + workflowName);
        workflowCondition_.notify_all();
    }
}

void ScriptWorkflowTask::abortWorkflow(const std::string& workflowName) {
    std::unique_lock lock(workflowMutex_);
    workflowStates_[workflowName] = WorkflowState::Aborted;
    addHistoryEntry("Workflow aborted: " + workflowName);
    workflowCondition_.notify_all();
}

WorkflowState ScriptWorkflowTask::getWorkflowState(
    const std::string& workflowName) const {
    std::shared_lock lock(workflowMutex_);
    auto it = workflowStates_.find(workflowName);
    if (it != workflowStates_.end()) {
        return it->second;
    }
    return WorkflowState::Created;
}

// Register with factory
namespace {
static auto workflow_task_registrar = TaskRegistrar<ScriptWorkflowTask>(
    "script_workflow",
    TaskInfo{
        .name = "script_workflow",
        .description = "Execute complex script workflows with dependencies",
        .category = "automation",
        .requiredParameters = {"workflowName"},
        .parameterSchema =
            json{
                {"workflowName",
                 {{"type", "string"}, {"description", "Name of the workflow"}}},
                {"workflow",
                 {{"type", "object"},
                  {"description", "Workflow definition"},
                  {"properties",
                   {{"steps",
                     {{"type", "array"},
                      {"items",
                       {{"type", "object"},
                        {"properties",
                         {{"taskId", {{"type", "string"}}},
                          {"scriptName", {{"type", "string"}}},
                          {"type",
                           {{"type", "string"},
                            {"enum",
                             json::array({"shell", "python", "auto"})}}},
                          {"dependencies",
                           {{"type", "array"},
                            {"items", {{"type", "string"}}}}},
                          {"parameters", {{"type", "object"}}},
                          {"optional", {{"type", "boolean"}}}}},
                        {"required",
                         json::array({"taskId", "scriptName"})}}}}}}}}},
                {"maxConcurrentSteps", {{"type", "number"}, {"default", 3}}},
                {"timeout", {{"type", "number"}, {"default", 3600}}},
                {"retryFailedSteps",
                 {{"type", "boolean"}, {"default", false}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<ScriptWorkflowTask> {
        return std::make_unique<ScriptWorkflowTask>(name);
    });
}  // namespace

}  // namespace lithium::task::script
