/**
 * @file calibration_tasks.hpp
 * @brief Calibration frame acquisition tasks
 */

#ifndef LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP
#define LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Automatic calibration frame acquisition task
 * 
 * Acquires dark, bias, and flat frames for image calibration.
 */
class AutoCalibrationTask : public CameraTaskBase {
public:
    AutoCalibrationTask() : CameraTaskBase("AutoCalibration") { setupParameters(); }
    AutoCalibrationTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "AutoCalibration"; }
    static std::string getTaskTypeName() { return "AutoCalibration"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    void acquireDarkFrames(const json& params);
    void acquireBiasFrames(const json& params);
    void acquireFlatFrames(const json& params);
};

/**
 * @brief Thermal cycle dark frame acquisition
 * 
 * Acquires dark frames at different temperatures for thermal calibration.
 */
class ThermalCycleTask : public CameraTaskBase {
public:
    ThermalCycleTask() : CameraTaskBase("ThermalCycle") { setupParameters(); }
    ThermalCycleTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "ThermalCycle"; }
    static std::string getTaskTypeName() { return "ThermalCycle"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Flat field sequence with automatic exposure
 * 
 * Acquires flat frames with automatic exposure adjustment to reach target ADU.
 */
class FlatFieldSequenceTask : public CameraTaskBase {
public:
    FlatFieldSequenceTask() : CameraTaskBase("FlatFieldSequence") { setupParameters(); }
    FlatFieldSequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "FlatFieldSequence"; }
    static std::string getTaskTypeName() { return "FlatFieldSequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    double calculateFlatExposure(double currentExposure, int measuredADU, int targetADU);
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_CALIBRATION_TASKS_HPP
