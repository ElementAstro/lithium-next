#ifndef LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

// ==================== 对焦辅助任务 ====================

/**
 * @brief Automatic focus task.
 * Performs automatic focusing using star analysis.
 */
class AutoFocusTask : public Task {
public:
    AutoFocusTask()
        : Task("AutoFocus",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoFocusParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Focus test series task.
 * Performs focus test series for manual focus adjustment.
 */
class FocusSeriesTask : public Task {
public:
    FocusSeriesTask()
        : Task("FocusSeries",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusSeriesParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Temperature compensated focus task.
 * Performs temperature-based focus compensation.
 */
class TemperatureFocusTask : public Task {
public:
    TemperatureFocusTask()
        : Task("TemperatureFocus",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTemperatureFocusParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP
