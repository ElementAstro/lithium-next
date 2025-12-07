/**
 * @file focus_tasks.hpp
 * @brief Focus assistance tasks
 */

#ifndef LITHIUM_TASK_COMPONENTS_CAMERA_FOCUS_TASKS_HPP
#define LITHIUM_TASK_COMPONENTS_CAMERA_FOCUS_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Automatic focus task using star analysis
 */
class AutoFocusTask : public CameraTaskBase {
public:
    AutoFocusTask() : CameraTaskBase("AutoFocus") { setupParameters(); }
    AutoFocusTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "AutoFocus"; }
    static std::string getTaskTypeName() { return "AutoFocus"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    FocusResult findBestFocus(int startPos, int stepSize, int numSteps,
                              const json& params);
    double measureFocusMetric(int position, const json& params);
};

/**
 * @brief Focus test series for manual adjustment
 */
class FocusSeriesTask : public CameraTaskBase {
public:
    FocusSeriesTask() : CameraTaskBase("FocusSeries") { setupParameters(); }
    FocusSeriesTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "FocusSeries"; }
    static std::string getTaskTypeName() { return "FocusSeries"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Temperature-compensated focus adjustment
 */
class TemperatureFocusTask : public CameraTaskBase {
public:
    TemperatureFocusTask() : CameraTaskBase("TemperatureFocus") {
        setupParameters();
    }
    TemperatureFocusTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "TemperatureFocus"; }
    static std::string getTaskTypeName() { return "TemperatureFocus"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    int calculateCompensation(double currentTemp, double referenceTemp,
                              double coefficient);
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_COMPONENTS_CAMERA_FOCUS_TASKS_HPP
