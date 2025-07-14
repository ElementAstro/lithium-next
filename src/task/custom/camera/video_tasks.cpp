#include "video_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <thread>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#define MOCK_CAMERA

namespace lithium::task::task {

// ==================== Mock Camera Class ====================
#ifdef MOCK_CAMERA
class MockCameraDevice {
public:
    static auto getInstance() -> MockCameraDevice& {
        static MockCameraDevice instance;
        return instance;
    }

    auto startVideo() -> bool {
        if (videoRunning_) {
            return false; // Already running
        }
        videoRunning_ = true;
        videoStartTime_ = std::chrono::steady_clock::now();
        frameCount_ = 0;
        return true;
    }

    auto stopVideo() -> bool {
        if (!videoRunning_) {
            return false; // Not running
        }
        videoRunning_ = false;
        return true;
    }

    auto isVideoRunning() const -> bool {
        return videoRunning_;
    }

    auto getVideoFrame() -> json {
        if (!videoRunning_) {
            throw atom::error::RuntimeError("Video is not running");
        }

        frameCount_++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - videoStartTime_);

        return json{
            {"frame_number", frameCount_},
            {"timestamp", elapsed.count()},
            {"width", 1920},
            {"height", 1080},
            {"format", "RGB24"},
            {"size", 1920 * 1080 * 3}
        };
    }

    auto getVideoStatus() -> json {
        return json{
            {"running", videoRunning_},
            {"frame_count", frameCount_},
            {"fps", calculateFPS()},
            {"duration", videoRunning_ ? std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - videoStartTime_).count() : 0}
        };
    }

private:
    bool videoRunning_ = false;
    int frameCount_ = 0;
    std::chrono::steady_clock::time_point videoStartTime_;

    auto calculateFPS() -> double {
        if (!videoRunning_ || frameCount_ == 0) return 0.0;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - videoStartTime_);
        return (frameCount_ * 1000.0) / elapsed.count();
    }
};
#endif

// ==================== StartVideoTask Implementation ====================

auto StartVideoTask::taskName() -> std::string {
    return "StartVideo";
}

void StartVideoTask::execute(const json& params) {
    try {
        validateVideoParameters(params);

        spdlog::info("Starting video stream with parameters: {}", params.dump());

#ifdef MOCK_CAMERA
        auto& camera = MockCameraDevice::getInstance();
        if (!camera.startVideo()) {
            throw atom::error::RuntimeError("Failed to start video stream - already running");
        }
#endif

        // Log success
        LOG_F(INFO, "Video stream started successfully");

        // Optional: Wait for stream to stabilize
        if (params.contains("stabilize_delay") && params["stabilize_delay"].is_number()) {
            int delay = params["stabilize_delay"];
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }

    } catch (const std::exception& e) {
        spdlog::error("StartVideoTask failed: {}", e.what());
        throw;
    }
}

auto StartVideoTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<StartVideoTask>("StartVideo",
        [](const json& params) {
            StartVideoTask taskInstance("StartVideo", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void StartVideoTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "stabilize_delay",
        .type = "integer",
        .required = false,
        .defaultValue = 1000,
        .description = "Delay in milliseconds to wait for stream stabilization"
    });

    task.addParameter({
        .name = "format",
        .type = "string",
        .required = false,
        .defaultValue = "RGB24",
        .description = "Video format (RGB24, YUV420, etc.)"
    });

    task.addParameter({
        .name = "fps",
        .type = "number",
        .required = false,
        .defaultValue = 30.0,
        .description = "Target frames per second"
    });
}

void StartVideoTask::validateVideoParameters(const json& params) {
    if (params.contains("stabilize_delay")) {
        int delay = params["stabilize_delay"];
        if (delay < 0 || delay > 10000) {
            throw atom::error::InvalidArgument("Stabilize delay must be between 0 and 10000 ms");
        }
    }

    if (params.contains("fps")) {
        double fps = params["fps"];
        if (fps <= 0 || fps > 120) {
            throw atom::error::InvalidArgument("FPS must be between 0 and 120");
        }
    }
}

void StartVideoTask::handleVideoError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::DeviceError);
    spdlog::error("Video task error: {}", e.what());
}

// ==================== StopVideoTask Implementation ====================

auto StopVideoTask::taskName() -> std::string {
    return "StopVideo";
}

void StopVideoTask::execute(const json& params) {
    try {
        spdlog::info("Stopping video stream");

#ifdef MOCK_CAMERA
        auto& camera = MockCameraDevice::getInstance();
        if (!camera.stopVideo()) {
            spdlog::warn("Video stream was not running");
        }
#endif

        LOG_F(INFO, "Video stream stopped successfully");

    } catch (const std::exception& e) {
        spdlog::error("StopVideoTask failed: {}", e.what());
        throw;
    }
}

auto StopVideoTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<StopVideoTask>("StopVideo",
        [](const json& params) {
            StopVideoTask taskInstance("StopVideo", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void StopVideoTask::defineParameters(Task& task) {
    // No parameters needed for stopping video
}

// ==================== GetVideoFrameTask Implementation ====================

auto GetVideoFrameTask::taskName() -> std::string {
    return "GetVideoFrame";
}

void GetVideoFrameTask::execute(const json& params) {
    try {
        validateFrameParameters(params);

#ifdef MOCK_CAMERA
        auto& camera = MockCameraDevice::getInstance();
        if (!camera.isVideoRunning()) {
            throw atom::error::RuntimeError("Video stream is not running");
        }

        auto frameData = camera.getVideoFrame();
        spdlog::info("Retrieved video frame: {}", frameData.dump());
#endif

        LOG_F(INFO, "Video frame retrieved successfully");

    } catch (const std::exception& e) {
        spdlog::error("GetVideoFrameTask failed: {}", e.what());
        throw;
    }
}

auto GetVideoFrameTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<GetVideoFrameTask>("GetVideoFrame",
        [](const json& params) {
            GetVideoFrameTask taskInstance("GetVideoFrame", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void GetVideoFrameTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "timeout",
        .type = "integer",
        .required = false,
        .defaultValue = 5000,
        .description = "Timeout in milliseconds for frame retrieval"
    });
}

void GetVideoFrameTask::validateFrameParameters(const json& params) {
    if (params.contains("timeout")) {
        int timeout = params["timeout"];
        if (timeout < 100 || timeout > 30000) {
            throw atom::error::InvalidArgument("Timeout must be between 100 and 30000 ms");
        }
    }
}

// ==================== RecordVideoTask Implementation ====================

auto RecordVideoTask::taskName() -> std::string {
    return "RecordVideo";
}

void RecordVideoTask::execute(const json& params) {
    try {
        validateRecordingParameters(params);

        int duration = params.value("duration", 10);
        std::string filename = params.value("filename", "video_recording.mp4");

        spdlog::info("Starting video recording for {} seconds to file: {}", duration, filename);

#ifdef MOCK_CAMERA
        auto& camera = MockCameraDevice::getInstance();

        // Start video if not already running
        bool wasRunning = camera.isVideoRunning();
        if (!wasRunning) {
            camera.startVideo();
        }

        // Simulate recording
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(duration);

        int framesCaptured = 0;
        while (std::chrono::steady_clock::now() < endTime) {
            camera.getVideoFrame();
            framesCaptured++;
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }

        // Stop video if we started it
        if (!wasRunning) {
            camera.stopVideo();
        }

        spdlog::info("Video recording completed. Captured {} frames", framesCaptured);
#endif

        LOG_F(INFO, "Video recording completed successfully");

    } catch (const std::exception& e) {
        spdlog::error("RecordVideoTask failed: {}", e.what());
        throw;
    }
}

auto RecordVideoTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<RecordVideoTask>("RecordVideo",
        [](const json& params) {
            RecordVideoTask taskInstance("RecordVideo", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void RecordVideoTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "duration",
        .type = "integer",
        .required = true,
        .defaultValue = 10,
        .description = "Recording duration in seconds"
    });

    task.addParameter({
        .name = "filename",
        .type = "string",
        .required = false,
        .defaultValue = "video_recording.mp4",
        .description = "Output filename for the video recording"
    });

    task.addParameter({
        .name = "quality",
        .type = "string",
        .required = false,
        .defaultValue = "high",
        .description = "Recording quality (low, medium, high)"
    });

    task.addParameter({
        .name = "fps",
        .type = "number",
        .required = false,
        .defaultValue = 30.0,
        .description = "Recording frame rate"
    });
}

void RecordVideoTask::validateRecordingParameters(const json& params) {
    if (params.contains("duration")) {
        int duration = params["duration"];
        if (duration <= 0 || duration > 3600) {
            throw atom::error::InvalidArgument("Duration must be between 1 and 3600 seconds");
        }
    }

    if (params.contains("fps")) {
        double fps = params["fps"];
        if (fps <= 0 || fps > 120) {
            throw atom::error::InvalidArgument("FPS must be between 0 and 120");
        }
    }
}

// ==================== VideoStreamMonitorTask Implementation ====================

auto VideoStreamMonitorTask::taskName() -> std::string {
    return "VideoStreamMonitor";
}

void VideoStreamMonitorTask::execute(const json& params) {
    try {
        int duration = params.value("monitor_duration", 30);
        spdlog::info("Monitoring video stream for {} seconds", duration);

#ifdef MOCK_CAMERA
        auto& camera = MockCameraDevice::getInstance();

        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(duration);

        while (std::chrono::steady_clock::now() < endTime) {
            auto status = camera.getVideoStatus();
            spdlog::info("Video status: {}", status.dump());

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
#endif

        LOG_F(INFO, "Video stream monitoring completed");

    } catch (const std::exception& e) {
        spdlog::error("VideoStreamMonitorTask failed: {}", e.what());
        throw;
    }
}

auto VideoStreamMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<VideoStreamMonitorTask>("VideoStreamMonitor",
        [](const json& params) {
            VideoStreamMonitorTask taskInstance("VideoStreamMonitor", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void VideoStreamMonitorTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "monitor_duration",
        .type = "integer",
        .required = false,
        .defaultValue = 30,
        .description = "Duration to monitor video stream in seconds"
    });

    task.addParameter({
        .name = "report_interval",
        .type = "integer",
        .required = false,
        .defaultValue = 5,
        .description = "Interval between status reports in seconds"
    });
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register StartVideoTask
AUTO_REGISTER_TASK(
    StartVideoTask, "StartVideo",
    (TaskInfo{
        .name = "StartVideo",
        .description = "Starts video streaming from the camera",
        .category = "Video",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"stabilize_delay", json{{"type", "integer"},
                                                {"minimum", 0},
                                                {"maximum", 10000}}},
                       {"format", json{{"type", "string"},
                                       {"enum", json::array({"RGB24", "YUV420", "MJPEG"})}}},
                       {"fps", json{{"type", "number"},
                                    {"minimum", 1},
                                    {"maximum", 120}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register StopVideoTask
AUTO_REGISTER_TASK(
    StopVideoTask, "StopVideo",
    (TaskInfo{
        .name = "StopVideo",
        .description = "Stops video streaming from the camera",
        .category = "Video",
        .requiredParameters = {},
        .parameterSchema = json{{"type", "object"}, {"properties", json{}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register GetVideoFrameTask
AUTO_REGISTER_TASK(
    GetVideoFrameTask, "GetVideoFrame",
    (TaskInfo{
        .name = "GetVideoFrame",
        .description = "Retrieves the current video frame",
        .category = "Video",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"timeout", json{{"type", "integer"},
                                        {"minimum", 100},
                                        {"maximum", 30000}}}}}},
        .version = "1.0.0",
        .dependencies = {"StartVideo"}}));

// Register RecordVideoTask
AUTO_REGISTER_TASK(
    RecordVideoTask, "RecordVideo",
    (TaskInfo{
        .name = "RecordVideo",
        .description = "Records video for a specified duration",
        .category = "Video",
        .requiredParameters = {"duration"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"duration", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 3600}}},
                       {"filename", json{{"type", "string"}}},
                       {"quality", json{{"type", "string"},
                                        {"enum", json::array({"low", "medium", "high"})}}},
                       {"fps", json{{"type", "number"},
                                    {"minimum", 1},
                                    {"maximum", 120}}}}},
                 {"required", json::array({"duration"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register VideoStreamMonitorTask
AUTO_REGISTER_TASK(
    VideoStreamMonitorTask, "VideoStreamMonitor",
    (TaskInfo{
        .name = "VideoStreamMonitor",
        .description = "Monitors video streaming status and performance",
        .category = "Video",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"monitor_duration", json{{"type", "integer"},
                                                 {"minimum", 1},
                                                 {"maximum", 3600}}},
                       {"report_interval", json{{"type", "integer"},
                                                {"minimum", 1},
                                                {"maximum", 60}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
