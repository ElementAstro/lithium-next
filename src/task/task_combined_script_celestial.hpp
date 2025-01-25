#ifndef LITHIUM_TASK_COMBINED_SCRIPT_CELESTIAL_HPP
#define LITHIUM_TASK_COMBINED_SCRIPT_CELESTIAL_HPP

#include "task.hpp"
#include <string>
#include <unordered_map>

namespace lithium::sequencer::task {

class TaskCombinedScriptCelestial : public Task {
public:
    TaskCombinedScriptCelestial(const std::string& scriptPath, const std::string& searchParams);
    void execute(const json& params) override;

private:
    std::string scriptPath_;
    std::string searchParams_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_COMBINED_SCRIPT_CELESTIAL_HPP
