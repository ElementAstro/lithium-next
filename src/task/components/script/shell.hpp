#ifndef LITHIUM_TASK_COMPONENTS_SCRIPT_SHELL_HPP
#define LITHIUM_TASK_COMPONENTS_SCRIPT_SHELL_HPP

#include "base.hpp"

namespace lithium::task::script {

/**
 * @class ShellScriptTask
 * @brief Executes shell/bash scripts with timeout and monitoring
 */
class ShellScriptTask : public BaseScriptTask {
public:
    ShellScriptTask(const std::string& name,
                    const std::string& scriptConfigPath = "");

    void setShellType(const std::string& shell);
    void setWorkingDirectory(const std::string& directory);
    void setEnvironmentVariable(const std::string& key,
                                const std::string& value);

protected:
    ScriptExecutionResult executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args) override;

private:
    void setupShellDefaults();
    std::string buildCommand(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args);

    std::string shellType_{"/bin/bash"};
    std::string workingDirectory_;
    std::unordered_map<std::string, std::string> environmentVars_;
};

}  // namespace lithium::task::script

#endif  // LITHIUM_TASK_COMPONENTS_SCRIPT_SHELL_HPP
