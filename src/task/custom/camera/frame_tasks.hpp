#ifndef LITHIUM_TASK_CAMERA_FRAME_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FRAME_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Frame format configuration task.
 * Manages camera frame format settings including resolution, binning, and file types.
 */
class FrameConfigTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateFrameParameters(const json& params);
    static void handleFrameError(Task& task, const std::exception& e);
};

/**
 * @brief ROI (Region of Interest) configuration task.
 * Sets up subframe regions for targeted imaging.
 */
class ROIConfigTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateROIParameters(const json& params);
};

/**
 * @brief Binning configuration task.
 * Manages pixel binning settings for improved sensitivity or speed.
 */
class BinningConfigTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateBinningParameters(const json& params);
};

/**
 * @brief Frame information query task.
 * Retrieves detailed information about current frame settings.
 */
class FrameInfoTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

/**
 * @brief Upload mode configuration task.
 * Configures where and how images are uploaded after capture.
 */
class UploadModeTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateUploadParameters(const json& params);
};

/**
 * @brief Frame statistics task.
 * Analyzes frame data and provides statistical information.
 */
class FrameStatsTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_FRAME_TASKS_HPP
