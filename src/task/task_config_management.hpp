#ifndef LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
#define LITHIUM_TASK_CONFIG_MANAGEMENT_HPP

#include "task.hpp"
#include "atom/type/json.hpp"

namespace lithium::sequencer::task {

class TaskConfigManagement : public Task {
public:
    TaskConfigManagement(const json& configParams);

    void execute(const json& params) override;

private:
    json configParams_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
