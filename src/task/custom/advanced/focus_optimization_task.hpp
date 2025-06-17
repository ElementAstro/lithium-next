#ifndef LITHIUM_TASK_ADVANCED_FOCUS_OPTIMIZATION_TASK_HPP
#define LITHIUM_TASK_ADVANCED_FOCUS_OPTIMIZATION_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Advanced Focus Optimization Task
 * 
 * Performs comprehensive focus optimization using multiple algorithms
 * including temperature compensation and periodic refocusing.
 */
class FocusOptimizationTask : public Task {
public:
    FocusOptimizationTask()
        : Task("FocusOptimization",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "FocusOptimization"; }

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusOptimizationParameters(const json& params);

private:
    void executeImpl(const json& params);
    void performInitialFocus();
    void performPeriodicFocus();
    void performTemperatureCompensation();
    double measureFocusQuality();
    void buildFocusCurve();
    void findOptimalFocus();
    bool checkFocusDrift();
    void startContinuousMonitoring(double intervalMinutes);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_FOCUS_OPTIMIZATION_TASK_HPP
