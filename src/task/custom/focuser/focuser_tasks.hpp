/**
 * @file focuser_tasks.hpp
 * @brief Focuser-related tasks for autofocus and temperature compensation
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_FOCUSER_TASKS_HPP
#define LITHIUM_TASK_FOCUSER_TASKS_HPP

#include "../common/task_base.hpp"
#include "../common/types.hpp"
#include "../common/validation.hpp"

namespace lithium::task::focuser {

using namespace lithium::task;

/**
 * @brief Automatic focus task using star analysis
 *
 * Performs automated focus by taking a series of exposures at different
 * focuser positions and finding the optimal position using the specified
 * focus metric (HFD, FWHM, or Contrast).
 */
class AutoFocusTask : public TaskBase {
public:
    AutoFocusTask() : TaskBase("AutoFocus") { setupParameters(); }
    AutoFocusTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "AutoFocus"; }
    static std::string getStaticTaskTypeName() { return "AutoFocus"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    FocusResult findBestFocus(int startPos, int stepSize, int numSteps,
                               const json& params);
    double measureFocusMetric(int position, const json& params);
};

/**
 * @brief Focus test series for manual adjustment
 *
 * Takes a series of exposures at evenly spaced focuser positions
 * to help users manually analyze and determine the best focus position.
 */
class FocusSeriesTask : public TaskBase {
public:
    FocusSeriesTask() : TaskBase("FocusSeries") { setupParameters(); }
    FocusSeriesTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "FocusSeries"; }
    static std::string getStaticTaskTypeName() { return "FocusSeries"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Temperature-compensated focus adjustment
 *
 * Automatically adjusts focus position based on temperature changes
 * using a calibrated coefficient (steps per degree Celsius).
 */
class TemperatureFocusTask : public TaskBase {
public:
    TemperatureFocusTask() : TaskBase("TemperatureFocus") { setupParameters(); }
    TemperatureFocusTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "TemperatureFocus"; }
    static std::string getStaticTaskTypeName() { return "TemperatureFocus"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    int calculateCompensation(double currentTemp, double referenceTemp, double coefficient);
};

/**
 * @brief Move focuser to absolute position
 */
class MoveFocuserTask : public TaskBase {
public:
    MoveFocuserTask() : TaskBase("MoveFocuser") { setupParameters(); }
    MoveFocuserTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "MoveFocuser"; }
    static std::string getStaticTaskTypeName() { return "MoveFocuser"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Move focuser by relative steps
 */
class MoveFocuserRelativeTask : public TaskBase {
public:
    MoveFocuserRelativeTask() : TaskBase("MoveFocuserRelative") { setupParameters(); }
    MoveFocuserRelativeTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "MoveFocuserRelative"; }
    static std::string getStaticTaskTypeName() { return "MoveFocuserRelative"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::focuser

#endif  // LITHIUM_TASK_FOCUSER_TASKS_HPP
