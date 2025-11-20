#ifndef LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

// ==================== 安全和监控任务 ====================

/**
 * @brief Weather monitoring task.
 * Monitors weather conditions and performs safety imaging.
 */
class WeatherMonitorTask : public Task {
public:
    WeatherMonitorTask()
        : Task("WeatherMonitor",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateWeatherParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Cloud detection task.
 * Performs cloud detection using all-sky camera.
 */
class CloudDetectionTask : public Task {
public:
    CloudDetectionTask()
        : Task("CloudDetection",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCloudDetectionParameters(const json& params);
    static void validateCloudParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Safety shutdown task.
 * Performs safe shutdown of imaging equipment.
 */
class SafetyShutdownTask : public Task {
public:
    SafetyShutdownTask()
        : Task("SafetyShutdown",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSafetyParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_SAFETY_TASKS_HPP
