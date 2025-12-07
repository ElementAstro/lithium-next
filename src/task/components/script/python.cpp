#include "python.hpp"

#include "../../core/factory.hpp"

#include "spdlog/spdlog.h"

namespace lithium::task::script {

PythonScriptTask::PythonScriptTask(const std::string& name,
                                   const std::string& scriptConfigPath)
    : BaseScriptTask(name, scriptConfigPath) {
    initializePythonEnvironment();
    setupPythonDefaults();
}

void PythonScriptTask::setupPythonDefaults() {
    addParamDefinition("pythonPath", "string", false, "python3",
                       "Python interpreter path");
    addParamDefinition("virtualEnv", "string", false, "",
                       "Virtual environment path");
    addParamDefinition("requirements", "array", false, json::array(),
                       "Required Python packages");
    addParamDefinition("moduleImports", "object", false, json::object(),
                       "Modules to import");

    setTaskType("python_script");
}

void PythonScriptTask::initializePythonEnvironment() {
    try {
        pythonWrapper_ = std::make_unique<PythonWrapper>();
        addHistoryEntry("Python environment initialized");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize Python environment: {}", e.what());
        throw std::runtime_error("Python initialization failed: " +
                                 std::string(e.what()));
    }
}

ScriptExecutionResult PythonScriptTask::executeScript(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args) {
    spdlog::info("Executing Python script: {}", scriptName);
    addHistoryEntry("Executing Python script: " + scriptName);

    auto startTime = std::chrono::steady_clock::now();

    try {
        if (!pythonWrapper_) {
            throw std::runtime_error("Python wrapper not initialized");
        }

        // Set script arguments as Python variables
        for (const auto& [key, value] : args) {
            pythonWrapper_->set_variable("script_args", key, py::str(value));
        }

        // Check if script is already compiled
        py::object compiledScript;
        auto it = compiledScripts_.find(scriptName);
        if (it != compiledScripts_.end()) {
            compiledScript = it->second;
        } else {
            // Load and compile script
            std::string scriptContent =
                scriptManager_->getScriptContent(scriptName);
            if (scriptContent.empty()) {
                throw std::runtime_error("Script not found or empty: " +
                                         scriptName);
            }

            compiledScript = py::eval<py::eval_statements>(scriptContent);
            compiledScripts_[scriptName] = compiledScript;
        }

        // Execute the compiled script
        py::object result =
            pythonWrapper_->eval_expression(scriptName, compiledScript);

        auto endTime = std::chrono::steady_clock::now();
        auto executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        // Extract result information
        std::string output;
        if (!result.is_none()) {
            output = py::str(result);
        }

        return ScriptExecutionResult{.success = true,
                                     .exitCode = 0,
                                     .output = output,
                                     .error = "",
                                     .executionTime = executionTime};

    } catch (const py::error_already_set& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        std::string error = "Python error: " + std::string(e.what());
        spdlog::error("Python script execution failed: {}", error);

        return ScriptExecutionResult{.success = false,
                                     .exitCode = -1,
                                     .output = "",
                                     .error = error,
                                     .executionTime = executionTime};

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        return ScriptExecutionResult{.success = false,
                                     .exitCode = -1,
                                     .output = "",
                                     .error = e.what(),
                                     .executionTime = executionTime};
    }
}

void PythonScriptTask::loadPythonModule(const std::string& moduleName,
                                        const std::string& alias) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        std::string moduleAlias = alias.empty() ? moduleName : alias;
        pythonWrapper_->load_module(moduleName, moduleAlias);
        addHistoryEntry("Loaded Python module: " + moduleName + " as " +
                        moduleAlias);
    } catch (const std::exception& e) {
        handleScriptError(moduleName, "Failed to load Python module: " +
                                          std::string(e.what()));
        throw;
    }
}

void PythonScriptTask::setPythonVariable(const std::string& alias,
                                         const std::string& varName,
                                         const py::object& value) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        pythonWrapper_->set_variable(alias, varName, value);
        addHistoryEntry("Set Python variable: " + alias + "." + varName);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Failed to set Python variable: " + std::string(e.what()));
        throw;
    }
}

// Register with factory
namespace {
static auto python_script_task_registrar = TaskRegistrar<PythonScriptTask>(
    "python_script",
    TaskInfo{
        .name = "python_script",
        .description = "Execute Python scripts with enhanced integration",
        .category = "automation",
        .requiredParameters = {"scriptName"},
        .parameterSchema =
            json{{"scriptName",
                  {{"type", "string"}, {"description", "Script name or path"}}},
                 {"scriptContent",
                  {{"type", "string"},
                   {"description", "Inline script content"}}},
                 {"pythonPath",
                  {{"type", "string"},
                   {"description", "Python interpreter path"},
                   {"default", "python3"}}},
                 {"virtualEnv",
                  {{"type", "string"},
                   {"description", "Virtual environment path"}}},
                 {"timeout",
                  {{"type", "number"},
                   {"description", "Timeout in seconds"},
                   {"default", 30}}},
                 {"args",
                  {{"type", "object"},
                   {"description", "Script arguments"},
                   {"default", json::object()}}},
                 {"requirements",
                  {{"type", "array"},
                   {"description", "Required Python packages"}}},
                 {"moduleImports",
                  {{"type", "object"}, {"description", "Modules to import"}}}},
        .version = "1.0.0",
        .dependencies = {"python3"},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<PythonScriptTask> {
        return std::make_unique<PythonScriptTask>(
            name, config.value("scriptConfigPath", ""));
    });
}  // namespace

}  // namespace lithium::task::script
