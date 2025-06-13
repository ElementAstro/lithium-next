#ifndef LITHIUM_TASK_FOCUSER_FOCUS_TASKS_HPP
#define LITHIUM_TASK_FOCUSER_FOCUS_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

// ==================== Focus-Related Task Suite ====================

/**
 * @brief Automatic focus task.
 * Performs automatic focusing using star analysis with advanced error handling,
 * progress tracking, and parameter validation.
 */
class AutoFocusTask : public Task {
public:
    AutoFocusTask()
        : Task("AutoFocus",
               [this](const json& params) { this->executeImpl(params); }) {
        initializeTask();
    }

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoFocusParameters(const json& params);

private:
    void executeImpl(const json& params);
    void initializeTask();
    void trackPerformanceMetrics();
    void setupDependencies();
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

/**
 * @brief Focus validation task.
 * Validates focus quality by analyzing star characteristics and provides
 * quality metrics for the current focus position.
 */
class FocusValidationTask : public Task {
public:
    FocusValidationTask()
        : Task("FocusValidation",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusValidationParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Backlash compensation task.
 * Handles focuser backlash compensation by performing controlled movements
 * to eliminate mechanical play in the focuser system.
 */
class BacklashCompensationTask : public Task {
public:
    BacklashCompensationTask()
        : Task("BacklashCompensation",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateBacklashCompensationParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Focus calibration task.
 * Calibrates the focuser by mapping positions to known reference points
 * and establishing focus curves for different conditions.
 */
class FocusCalibrationTask : public Task {
public:
    FocusCalibrationTask()
        : Task("FocusCalibration",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusCalibrationParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Star detection and analysis task.
 * Detects stars in the field of view and provides detailed analysis
 * for focus optimization including HFR, FWHM, and star profile metrics.
 */
class StarDetectionTask : public Task {
public:
    StarDetectionTask()
        : Task("StarDetection",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateStarDetectionParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Focus monitoring task.
 * Continuously monitors focus quality and detects focus drift over time.
 * Can trigger automatic refocusing when quality degrades below threshold.
 */
class FocusMonitoringTask : public Task {
public:
    FocusMonitoringTask()
        : Task("FocusMonitoring",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFocusMonitoringParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_FOCUSER_FOCUS_TASKS_HPP
