#ifndef LITHIUM_TASK_ADVANCED_OBSERVATORY_AUTOMATION_TASK_HPP
#define LITHIUM_TASK_ADVANCED_OBSERVATORY_AUTOMATION_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Complete Observatory Automation Task
 *
 * Manages complete observatory startup, operation, and shutdown sequences
 * including roof control, equipment initialization, and safety checks.
 */
class ObservatoryAutomationTask : public Task {
public:
    ObservatoryAutomationTask()
        : Task("ObservatoryAutomation",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "ObservatoryAutomation"; }

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateObservatoryAutomationParameters(const json& params);

private:
    void executeImpl(const json& params);
    void performStartupSequence();
    void performShutdownSequence();
    void initializeEquipment();
    void performSafetyChecks();
    void openRoof();
    void closeRoof();
    void parkTelescope();
    void unparkTelescope();
    void coolCamera(double targetTemperature);
    void warmCamera();
    bool checkEquipmentStatus();
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_OBSERVATORY_AUTOMATION_TASK_HPP
