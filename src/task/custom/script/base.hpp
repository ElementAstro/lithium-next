#ifndef LITHIUM_TASK_BASE_SCRIPT_TASK_HPP
#define LITHIUM_TASK_BASE_SCRIPT_TASK_HPP

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include "atom/type/json.hpp"
#include "script/check.hpp"
#include "script/sheller.hpp"
#include "task.hpp"

namespace lithium::task::script {
using json = nlohmann::json;

/**
 * @enum ScriptType
 * @brief Represents the type of script to be executed.
 */
enum class ScriptType {
    Shell,   ///< Shell or Bash script
    Python,  ///< Python script
    Auto     ///< Automatically detect script type
};

/**
 * @struct ScriptExecutionResult
 * @brief Stores the result of a script execution.
 */
struct ScriptExecutionResult {
    bool success;        ///< Whether the script executed successfully
    int exitCode;        ///< Exit code returned by the script
    std::string output;  ///< Standard output or result of the script
    std::string error;   ///< Error message or standard error output
    std::chrono::milliseconds
        executionTime;  ///< Time taken to execute the script
};

/**
 * @class BaseScriptTask
 * @brief Abstract base class for all script execution tasks.
 *
 * This class provides common functionality for script execution tasks,
 * including parameter validation, error handling, and script type detection.
 * Derived classes should implement the executeScript() method for specific
 * script types (e.g., shell, Python).
 */
class BaseScriptTask : public Task {
public:
    /**
     * @brief Constructs a BaseScriptTask.
     * @param name The name of the task.
     * @param scriptConfigPath Optional path to the script configuration file.
     */
    BaseScriptTask(const std::string& name,
                   const std::string& scriptConfigPath = "");

    /**
     * @brief Executes the script task with the given parameters.
     * @param params JSON object containing task parameters.
     */
    void execute(const json& params) override;

protected:
    /**
     * @brief Executes the script with the specified name and arguments.
     * @param scriptName The name or path of the script to execute.
     * @param args Arguments to pass to the script.
     * @return ScriptExecutionResult containing execution details.
     */
    virtual ScriptExecutionResult executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args) = 0;

    /**
     * @brief Validates the script parameters using the Task API.
     * @param params JSON object containing parameters to validate.
     * @throws std::invalid_argument if validation fails.
     */
    void validateScriptParams(const json& params);

    /**
     * @brief Sets up default parameter definitions and task properties.
     */
    void setupScriptDefaults();

    /**
     * @brief Detects the type of script based on its content.
     * @param content The script content as a string.
     * @return ScriptType indicating the detected script type.
     */
    ScriptType detectScriptType(const std::string& content);

    /**
     * @brief Handles script execution errors and updates task state.
     * @param scriptName The name of the script that failed.
     * @param error The error message.
     */
    void handleScriptError(const std::string& scriptName,
                           const std::string& error);

    std::shared_ptr<ScriptManager>
        scriptManager_;  ///< Manages script registration and execution.
    std::unique_ptr<ScriptAnalyzer>
        scriptAnalyzer_;  ///< Optional analyzer for script validation and
                          ///< analysis.
    std::string scriptConfigPath_;  ///< Path to the script configuration file.
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_BASE_SCRIPT_TASK_HPP