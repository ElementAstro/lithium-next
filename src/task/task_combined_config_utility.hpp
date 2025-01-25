#ifndef LITHIUM_TASK_COMBINED_CONFIG_UTILITY_HPP
#define LITHIUM_TASK_COMBINED_CONFIG_UTILITY_HPP

#include "task.hpp"
#include "atom/type/json.hpp"

namespace lithium::sequencer::task {

class TaskCombinedConfigUtility : public Task {
public:
    TaskCombinedConfigUtility(const json& combinedParams);

    void execute(const json& params) override;

private:
    json combinedParams_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_COMBINED_CONFIG_UTILITY_HPP
