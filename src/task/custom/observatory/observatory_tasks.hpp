/**
 * @file observatory_tasks.hpp
 * @brief Observatory tasks for safety monitoring and shutdown
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_OBSERVATORY_TASKS_HPP
#define LITHIUM_TASK_OBSERVATORY_TASKS_HPP

#include "../common/task_base.hpp"
#include "../common/types.hpp"
#include "../common/validation.hpp"

namespace lithium::task::observatory {

using namespace lithium::task;

/**
 * @brief Weather monitoring task
 *
 * Monitors weather conditions and triggers alerts or actions
 * based on configurable thresholds.
 */
class WeatherMonitorTask : public TaskBase {
public:
    WeatherMonitorTask() : TaskBase("WeatherMonitor") { setupParameters(); }
    WeatherMonitorTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "WeatherMonitor"; }
    static std::string getStaticTaskTypeName() { return "WeatherMonitor"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    SafetyStatus checkWeather();
};

/**
 * @brief Cloud detection task
 *
 * Monitors sky conditions using cloud sensors or sky quality meters
 * to detect cloud cover and its impact on observations.
 */
class CloudDetectionTask : public TaskBase {
public:
    CloudDetectionTask() : TaskBase("CloudDetection") { setupParameters(); }
    CloudDetectionTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "CloudDetection"; }
    static std::string getStaticTaskTypeName() { return "CloudDetection"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    double measureCloudCover();
};

/**
 * @brief Safety shutdown task
 *
 * Performs a controlled shutdown of all observatory equipment
 * in response to unsafe conditions.
 */
class SafetyShutdownTask : public TaskBase {
public:
    SafetyShutdownTask() : TaskBase("SafetyShutdown") { setupParameters(); }
    SafetyShutdownTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SafetyShutdown"; }
    static std::string getStaticTaskTypeName() { return "SafetyShutdown"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    void stopImaging();
    void stopGuiding();
    void parkMount();
    void closeDome();
    void warmCamera();
};

/**
 * @brief Observatory startup task
 *
 * Performs a controlled startup sequence for all observatory equipment.
 */
class ObservatoryStartupTask : public TaskBase {
public:
    ObservatoryStartupTask() : TaskBase("ObservatoryStartup") { setupParameters(); }
    ObservatoryStartupTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "ObservatoryStartup"; }
    static std::string getStaticTaskTypeName() { return "ObservatoryStartup"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Dome control task
 *
 * Controls dome position, slaving, and shutter operations.
 */
class DomeControlTask : public TaskBase {
public:
    DomeControlTask() : TaskBase("DomeControl") { setupParameters(); }
    DomeControlTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "DomeControl"; }
    static std::string getStaticTaskTypeName() { return "DomeControl"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Flat panel control task
 *
 * Controls flat panel brightness for flat field acquisition.
 */
class FlatPanelTask : public TaskBase {
public:
    FlatPanelTask() : TaskBase("FlatPanel") { setupParameters(); }
    FlatPanelTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "FlatPanel"; }
    static std::string getStaticTaskTypeName() { return "FlatPanel"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Safety check task
 *
 * Performs a comprehensive safety check of all observatory systems.
 */
class SafetyCheckTask : public TaskBase {
public:
    SafetyCheckTask() : TaskBase("SafetyCheck") { setupParameters(); }
    SafetyCheckTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SafetyCheck"; }
    static std::string getStaticTaskTypeName() { return "SafetyCheck"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    SafetyStatus performCheck();
};

}  // namespace lithium::task::observatory

#endif  // LITHIUM_TASK_OBSERVATORY_TASKS_HPP
