/**
 * @file safety_tasks.hpp
 * @brief Safety monitoring and shutdown tasks
 */

#ifndef LITHIUM_TASK_COMPONENTS_CAMERA_SAFETY_TASKS_HPP
#define LITHIUM_TASK_COMPONENTS_CAMERA_SAFETY_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Weather monitoring task
 */
class WeatherMonitorTask : public CameraTaskBase {
public:
    WeatherMonitorTask() : CameraTaskBase("WeatherMonitor") {
        setupParameters();
    }
    WeatherMonitorTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "WeatherMonitor"; }
    static std::string getTaskTypeName() { return "WeatherMonitor"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Cloud detection task
 */
class CloudDetectionTask : public CameraTaskBase {
public:
    CloudDetectionTask() : CameraTaskBase("CloudDetection") {
        setupParameters();
    }
    CloudDetectionTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "CloudDetection"; }
    static std::string getTaskTypeName() { return "CloudDetection"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Safety shutdown task
 */
class SafetyShutdownTask : public CameraTaskBase {
public:
    SafetyShutdownTask() : CameraTaskBase("SafetyShutdown") {
        setupParameters();
    }
    SafetyShutdownTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "SafetyShutdown"; }
    static std::string getTaskTypeName() { return "SafetyShutdown"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_COMPONENTS_CAMERA_SAFETY_TASKS_HPP
