#ifndef LITHIUM_TASK_CAMERA_VIDEO_TASKS_HPP
#define LITHIUM_TASK_CAMERA_VIDEO_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Start video streaming task.
 * Controls video streaming functionality of the camera.
 */
class StartVideoTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateVideoParameters(const json& params);
    static void handleVideoError(Task& task, const std::exception& e);
};

/**
 * @brief Stop video streaming task.
 * Stops active video streaming.
 */
class StopVideoTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

/**
 * @brief Get video frame task.
 * Retrieves the current video frame.
 */
class GetVideoFrameTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFrameParameters(const json& params);
};

/**
 * @brief Video recording task.
 * Records video for a specified duration with configurable parameters.
 */
class RecordVideoTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateRecordingParameters(const json& params);
};

/**
 * @brief Video streaming monitor task.
 * Monitors video streaming status and performance metrics.
 */
class VideoStreamMonitorTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_VIDEO_TASKS_HPP
