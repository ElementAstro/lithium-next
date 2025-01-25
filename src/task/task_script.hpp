#ifndef LITHIUM_TASK_SCRIPT_HPP
#define LITHIUM_TASK_SCRIPT_HPP

#include "task.hpp"
#include <string>

namespace lithium::sequencer::task {

class TaskScript : public Task {
public:
    TaskScript(const std::string& scriptPath);
    void execute(const json& params) override;

private:
    std::string scriptPath_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_SCRIPT_HPP
