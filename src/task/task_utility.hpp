#ifndef LITHIUM_TASK_UTILITY_HPP
#define LITHIUM_TASK_UTILITY_HPP

#include "task.hpp"
#include "atom/type/json.hpp"

namespace lithium::sequencer::task {

class TaskUtility : public Task {
public:
    TaskUtility(const json& utilityParams);

    void execute(const json& params) override;

private:
    json utilityParams_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_UTILITY_HPP
