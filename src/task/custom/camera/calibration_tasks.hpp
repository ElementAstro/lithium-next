#ifndef LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP
#define LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

// ==================== 校准帧自动获取任务 ====================

/**
 * @brief Automatic calibration frame acquisition task.
 * Automatically acquires dark, bias, and flat frames for calibration.
 */
class AutoCalibrationTask : public TaskCreator<AutoCalibrationTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCalibrationParameters(const json& params);
};

/**
 * @brief Thermal cycle dark frame acquisition task.
 * Acquires dark frames across different temperatures for thermal calibration.
 */
class ThermalCycleTask : public TaskCreator<ThermalCycleTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateThermalParameters(const json& params);
};

/**
 * @brief Flat field sequence acquisition task.
 * Automatically acquires flat field frames with proper exposure control.
 */
class FlatFieldSequenceTask : public TaskCreator<FlatFieldSequenceTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFlatFieldParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP
