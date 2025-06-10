#ifndef LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 对焦辅助任务 ====================

/**
 * @brief Automatic focus task.
 * Performs automatic focusing using star analysis.
 */
class AutoFocusTask : public TaskCreator<AutoFocusTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoFocusParameters(const json& params);
};

/**
 * @brief Focus test series task.
 * Performs focus test series for manual focus adjustment.
 */
class FocusSeriesTask : public TaskCreator<FocusSeriesTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusSeriesParameters(const json& params);
};

/**
 * @brief Temperature compensated focus task.
 * Performs temperature-based focus compensation.
 */
class TemperatureFocusTask : public TaskCreator<TemperatureFocusTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTemperatureFocusParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_FOCUS_TASKS_HPP
