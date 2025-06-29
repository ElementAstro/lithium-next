#ifndef LITHIUM_TASK_ADVANCED_INTELLIGENT_SEQUENCE_TASK_HPP
#define LITHIUM_TASK_ADVANCED_INTELLIGENT_SEQUENCE_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Intelligent Imaging Sequence Task
 *
 * Advanced multi-target imaging sequence with intelligent decision making,
 * weather monitoring, and dynamic target selection based on conditions.
 * Inspired by NINA's advanced sequencer with conditions and triggers.
 */
class IntelligentSequenceTask : public Task {
public:
    IntelligentSequenceTask()
        : Task("IntelligentSequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "IntelligentSequence"; }

    // Enhanced functionality
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateIntelligentSequenceParameters(const json& params);

private:
    void executeImpl(const json& params);
    json selectBestTarget(const std::vector<json>& targets);
    bool checkWeatherConditions();
    bool checkTargetVisibility(const json& target);
    void executeTargetSequence(const json& target);
    double calculateTargetPriority(const json& target);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_INTELLIGENT_SEQUENCE_TASK_HPP
