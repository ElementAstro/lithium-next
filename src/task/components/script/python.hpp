#ifndef LITHIUM_TASK_COMPONENTS_SCRIPT_PYTHON_HPP
#define LITHIUM_TASK_COMPONENTS_SCRIPT_PYTHON_HPP

#include "base.hpp"

#include "script/python_caller.hpp"

namespace lithium::task::script {

/**
 * @class PythonScriptTask
 * @brief Executes Python scripts with enhanced integration
 */
class PythonScriptTask : public BaseScriptTask {
public:
    PythonScriptTask(const std::string& name,
                     const std::string& scriptConfigPath = "");

    void loadPythonModule(const std::string& moduleName,
                          const std::string& alias = "");

    template <typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                  const std::string& functionName,
                                  Args... args);

    template <typename T>
    T getPythonVariable(const std::string& alias, const std::string& varName);

    void setPythonVariable(const std::string& alias, const std::string& varName,
                           const py::object& value);

protected:
    ScriptExecutionResult executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args) override;

private:
    void initializePythonEnvironment();
    void setupPythonDefaults();

    std::unique_ptr<PythonWrapper> pythonWrapper_;
    std::map<std::string, py::object> compiledScripts_;
};

// Template implementations
template <typename ReturnType, typename... Args>
ReturnType PythonScriptTask::callPythonFunction(const std::string& alias,
                                                const std::string& functionName,
                                                Args... args) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        addHistoryEntry("Calling Python function: " + alias +
                        "::" + functionName);
        return pythonWrapper_->call_function<ReturnType>(alias, functionName,
                                                         args...);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Python function call failed: " + std::string(e.what()));
        throw;
    }
}

template <typename T>
T PythonScriptTask::getPythonVariable(const std::string& alias,
                                      const std::string& varName) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        return pythonWrapper_->get_variable<T>(alias, varName);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Failed to get Python variable: " + std::string(e.what()));
        throw;
    }
}

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_COMPONENTS_SCRIPT_PYTHON_HPP
