// ==================== Includes and Declarations ====================
#include "basic_exposure.hpp"
#include "common.hpp"

#include <chrono>
#include <memory>
#include <thread>

#include "../../task.hpp"
#include "config/configor.hpp"

#include "atom/error/exception.hpp"
#include "atom/function/concept.hpp"
#include "atom/function/enum.hpp"
#include "atom/function/global_ptr.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#include <spdlog/spdlog.h>
#include "constant/constant.hpp"

// ==================== Enum Traits and Formatters ====================

// Outside any namespace - use full qualification for both
template <>
struct atom::meta::EnumTraits<lithium::task::camera::ExposureType> {
    static constexpr std::array<lithium::task::camera::ExposureType, 5> values =
        {lithium::task::camera::ExposureType::LIGHT,
         lithium::task::camera::ExposureType::DARK,
         lithium::task::camera::ExposureType::BIAS,
         lithium::task::camera::ExposureType::FLAT,
         lithium::task::camera::ExposureType::SNAPSHOT};
    static constexpr std::array<std::string_view, 5> names = {
        "LIGHT", "DARK", "BIAS", "FLAT", "SNAPSHOT"};
};

#define MOCK_CAMERA
namespace lithium::task::camera {

// ==================== Mock Camera Class ====================
#ifdef MOCK_CAMERA
class MockCamera {
public:
    MockCamera() = default;

    bool getExposureStatus() const { return exposureStatus_; }
    void setGain(int g) { gain_ = g; }
    int getGain() const { return gain_; }
    void setOffset(int o) { offset_ = o; }
    int getOffset() const { return offset_; }
    void setBinning(int bx, int by) {
        binningX_ = bx;
        binningY_ = by;
    }
    std::tuple<int, int> getBinning() const { return {binningX_, binningY_}; }
    void startExposure(double t) {
        exposureStatus_ = true;
        exposureTime_ = t;
    }
    void saveExposureResult() { exposureStatus_ = false; }
    bool setFrame(int x, int y, int width, int height) {
        frameX_ = x;
        frameY_ = y;
        frameWidth_ = width;
        frameHeight_ = height;
        return true;
    }
    std::tuple<int, int> getFrame() const {
        return {frameWidth_, frameHeight_};
    }

private:
    bool exposureStatus_{};
    double exposureTime_{};
    int gain_{};
    int offset_{};
    int binningX_{};
    int binningY_{};
    int frameX_{};
    int frameY_{};
    int frameWidth_{};
    int frameHeight_{};
};
#endif

// ==================== TakeExposureTask Implementation ====================

auto TakeExposureTask::taskName() -> std::string { return "TakeExposure"; }

void TakeExposureTask::execute(const json& params) {
    spdlog::info("Executing TakeExposure task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double time = params.at("exposure").get<double>();
        ExposureType type = params.value("type", ExposureType::LIGHT);

        // Validate exposure parameters
        if (time < 0 || time > 7200) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be between 0 and 7200 seconds");
        }

        spdlog::info("Starting {} exposure for {} seconds",
                     static_cast<int>(type), time);

#ifdef MOCK_CAMERA
        // Mock camera implementation
        auto camera = std::make_unique<MockCamera>();

        // Apply camera settings if provided
        if (params.contains("gain")) {
            camera->setGain(params["gain"].get<int>());
        }
        if (params.contains("offset")) {
            camera->setOffset(params["offset"].get<int>());
        }
        if (params.contains("binning")) {
            auto binning = params["binning"];
            camera->setBinning(binning["x"].get<int>(),
                               binning["y"].get<int>());
        }

        camera->startExposure(time);

        // Simulate exposure duration
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>(time * 100)));  // Scaled down for simulation

        camera->saveExposureResult();
#endif

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        spdlog::info("TakeExposure task completed successfully in {} ms",
                     duration.count());

    } catch (const std::exception& e) {
        spdlog::error("TakeExposure task failed: {}", e.what());
        throw;
    }
}

auto TakeExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>("TakeExposure", [](const json& params) {
        TakeExposureTask takeExposureTask("TakeExposure", [](const json&) {});
        takeExposureTask.execute(params);
    });

    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::minutes(5));
    task->setLogLevel(2);

    return task;
}

void TakeExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "number", true, nullptr,
                            "Exposure time in seconds (0-7200)");
    task.addParamDefinition("type", "string", false, "light",
                            "Exposure type: light/dark/bias/flat/snapshot");
    task.addParamDefinition("gain", "number", false, nullptr,
                            "Camera gain (0-1000)");
    task.addParamDefinition("offset", "number", false, nullptr,
                            "Camera offset (0-255)");
    task.addParamDefinition("binning", "object", false, nullptr,
                            "Binning settings {x: int, y: int}");
    task.addParamDefinition("fileName", "string", false, nullptr,
                            "Output filename");
    task.addParamDefinition("path", "string", false, nullptr,
                            "Output directory path");
}

void TakeExposureTask::validateCameraParameters(const json& params) {
    if (!params.contains("exposure")) {
        THROW_INVALID_ARGUMENT("Missing required parameter: exposure");
    }

    double exposure = params["exposure"].get<double>();
    if (exposure < 0 || exposure > 7200) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0 and 7200 seconds");
    }

    if (params.contains("gain")) {
        int gain = params["gain"].get<int>();
        if (gain < 0 || gain > 1000) {
            THROW_INVALID_ARGUMENT("Gain must be between 0 and 1000");
        }
    }

    if (params.contains("offset")) {
        int offset = params["offset"].get<int>();
        if (offset < 0 || offset > 255) {
            THROW_INVALID_ARGUMENT("Offset must be between 0 and 255");
        }
    }
}

void TakeExposureTask::handleCameraError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::DeviceError);
    task.addHistoryEntry("Camera error occurred: " + std::string(e.what()));
    spdlog::error("Camera error in TakeExposureTask: {}", e.what());
}

// ==================== TakeManyExposureTask Implementation ====================

auto TakeManyExposureTask::taskName() -> std::string {
    return "TakeManyExposure";
}

void TakeManyExposureTask::execute(const json& params) {
    spdlog::info("Executing TakeManyExposure task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int count = params.at("count").get<int>();
        double time = params.at("exposure").get<double>();
        ExposureType type = params.at("type").get<ExposureType>();
        int binning = params.at("binning").get<int>();
        int gain = params.at("gain").get<int>();
        int offset = params.at("offset").get<int>();

        spdlog::info(
            "Starting {} exposures of {} seconds each with binning {} and "
            "gain {} and offset {}",
            count, time, binning, gain, offset);

        for (int i = 0; i < count; ++i) {
            spdlog::info("Taking exposure {} of {}", i + 1, count);

            // Add delay between exposures if specified
            if (params.contains("delay") && i > 0) {
                double delay = params["delay"].get<double>();
                if (delay > 0) {
                    spdlog::info("Waiting {} seconds before next exposure",
                                 delay);
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        static_cast<int>(delay * 1000)));
                }
            }

            TakeExposureTask takeExposureTask("TakeExposure",
                                              [](const json&) {});
            takeExposureTask.execute(params);
            spdlog::info("Exposure {} of {} completed", i + 1, count);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("TakeManyExposure task completed {} exposures in {} ms",
                     count, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("TakeManyExposure task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto TakeManyExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            TakeManyExposureTask takeManyExposureTask("TakeManyExposure",
                                                      [](const json&) {});
            takeManyExposureTask.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced TakeManyExposure task failed: {}",
                          e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);  // Medium-high priority for sequences
    task->setTimeout(
        std::chrono::seconds(3600));  // 1 hour timeout for sequences
    task->setLogLevel(2);             // INFO level

    return task;
}

void TakeManyExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("count", "int", true, 1,
                            "Number of exposures to take");
    task.addParamDefinition("exposure", "double", true, 1.0,
                            "Exposure time in seconds");
    task.addParamDefinition(
        "type", "string", true, "light",
        "Exposure type (light, dark, bias, flat, snapshot)");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
    task.addParamDefinition("delay", "double", false, 0.0,
                            "Delay between exposures in seconds");
}

void TakeManyExposureTask::validateSequenceParameters(const json& params) {
    TakeExposureTask::validateCameraParameters(params);

    if (!params.contains("count") || !params["count"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid count parameter");
    }

    int count = params["count"].get<int>();
    if (count <= 0 || count > 1000) {
        THROW_INVALID_ARGUMENT("Count must be between 1 and 1000");
    }

    if (params.contains("delay")) {
        double delay = params["delay"].get<double>();
        if (delay < 0 || delay > 600) {  // Max 10 minutes delay
            THROW_INVALID_ARGUMENT("Delay must be between 0 and 600 seconds");
        }
    }
}

void TakeManyExposureTask::handleSequenceError(Task& task,
                                               const std::exception& e) {
    std::string error = e.what();

    if (error.find("timeout") != std::string::npos) {
        task.setErrorType(TaskErrorType::Timeout);
    } else if (error.find("count") != std::string::npos) {
        task.setErrorType(TaskErrorType::InvalidParameter);
    } else {
        TakeExposureTask::handleCameraError(task, e);
    }

    task.addHistoryEntry("Sequence error: " + error);
    spdlog::error("Camera sequence task error handled: {}", error);
}

// ==================== SubframeExposureTask Implementation ====================

auto SubframeExposureTask::taskName() -> std::string {
    return "SubframeExposure";
}

void SubframeExposureTask::execute(const json& params) {
    spdlog::info("Executing SubframeExposure task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double time = params.at("exposure").get<double>();
        ExposureType type = params.at("type").get<ExposureType>();
        int x = params.at("x").get<int>();
        int y = params.at("y").get<int>();
        int width = params.at("width").get<int>();
        int height = params.at("height").get<int>();
        int binning = params.at("binning").get<int>();
        int gain = params.at("gain").get<int>();
        int offset = params.at("offset").get<int>();

        spdlog::info(
            "Starting {} subframe exposure for {} seconds at ({},{}) size "
            "{}x{} with binning {} and gain {} and offset {}",
            type, time, x, y, width, height, binning, gain, offset);

#ifdef MOCK_CAMERA
        std::shared_ptr<MockCamera> camera = std::make_shared<MockCamera>();
#else
        std::shared_ptr<AtomCamera> camera =
            GetPtr<AtomCamera>(Constants::MAIN_CAMERA).value();
#endif

        if (!camera) {
            spdlog::error("Main camera not set");
            THROW_RUNTIME_ERROR("Main camera not set");
        }
        if (camera->getExposureStatus()) {
            spdlog::error("Main camera is busy");
            THROW_RUNTIME_ERROR("Main camera is busy");
        }

        std::shared_ptr<ConfigManager> configManager =
            GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
        configManager->set("/lithium/device/camera/is_exposure", true);
        spdlog::info("Camera exposure status set to true");

        // Set camera frame
        spdlog::info("Setting camera frame to ({},{}) size {}x{}", x, y, width,
                     height);
        if (!camera->setFrame(x, y, width, height)) {
            spdlog::error("Failed to set camera frame");
            THROW_RUNTIME_ERROR("Failed to set camera frame");
        }

        // Set camera settings
        if (camera->getGain() != gain) {
            spdlog::info("Setting camera gain to {}", gain);
            camera->setGain(gain);
        }
        if (camera->getOffset() != offset) {
            spdlog::info("Setting camera offset to {}", offset);
            camera->setOffset(offset);
        }
#ifdef MOCK_CAMERA
        if (camera->getBinning() != std::tuple{binning, binning}) {
            spdlog::info("Setting camera binning to {}x{}", binning, binning);
            camera->setBinning(binning, binning);
        }
#else
        if (camera->getBinning().value() !=
            std::tuple{binning, binning, binning, binning}) {
            spdlog::info("Setting camera binning to {}x{}", binning, binning);
            camera->setBinning(binning, binning);
        }
#endif

        // Start exposure
        spdlog::info("Starting subframe camera exposure for {} seconds", time);
        camera->startExposure(time);

        // Wait for exposure to complete
        auto exposureStartTime = std::chrono::steady_clock::now();
        auto timeoutDuration = std::chrono::seconds(static_cast<int>(time + 1));

        while (camera->getExposureStatus()) {
            auto elapsed = std::chrono::steady_clock::now() - exposureStartTime;
            if (elapsed > timeoutDuration) {
                spdlog::error("Subframe exposure timeout");
                THROW_RUNTIME_ERROR("Subframe exposure timeout");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        configManager->set("/lithium/device/camera/is_exposure", false);
        spdlog::info("Subframe exposure completed");

        spdlog::info("Saving subframe exposure result");
        camera->saveExposureResult();

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("SubframeExposure task completed in {} ms",
                     duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("SubframeExposure task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto SubframeExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            SubframeExposureTask subframeExposureTask("SubframeExposure",
                                                      [](const json&) {});
            subframeExposureTask.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced SubframeExposure task failed: {}",
                          e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(7);  // High priority for camera tasks
    task->setTimeout(std::chrono::seconds(300));  // 5 minute timeout
    task->setLogLevel(2);                         // INFO level

    return task;
}

void SubframeExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", true, 1.0,
                            "Exposure time in seconds");
    task.addParamDefinition(
        "type", "string", true, "light",
        "Exposure type (light, dark, bias, flat, snapshot)");
    task.addParamDefinition("x", "int", true, 0, "Subframe X position");
    task.addParamDefinition("y", "int", true, 0, "Subframe Y position");
    task.addParamDefinition("width", "int", true, 100, "Subframe width");
    task.addParamDefinition("height", "int", true, 100, "Subframe height");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
}

void SubframeExposureTask::validateSubframeParameters(const json& params) {
    TakeExposureTask::validateCameraParameters(params);

    if (!params.contains("x") || !params["x"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid x parameter");
    }
    if (!params.contains("y") || !params["y"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid y parameter");
    }
    if (!params.contains("width") || !params["width"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid width parameter");
    }
    if (!params.contains("height") || !params["height"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid height parameter");
    }

    int x = params["x"].get<int>();
    int y = params["y"].get<int>();
    int width = params["width"].get<int>();
    int height = params["height"].get<int>();

    if (x < 0 || y < 0) {
        THROW_INVALID_ARGUMENT("Subframe position must be non-negative");
    }
    if (width <= 0 || height <= 0) {
        THROW_INVALID_ARGUMENT("Subframe dimensions must be positive");
    }
    if (width > 4096 || height > 4096) {
        THROW_INVALID_ARGUMENT("Subframe dimensions too large (max 4096x4096)");
    }
}

// ==================== CameraSettingsTask Implementation ====================

auto CameraSettingsTask::taskName() -> std::string { return "CameraSettings"; }

void CameraSettingsTask::execute(const json& params) {
    spdlog::info("Executing CameraSettings task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int gain = params.at("gain").get<int>();
        int offset = params.at("offset").get<int>();
        int binning = params.at("binning").get<int>();

        // Optional temperature setting
        double targetTemp = params.value("temperature", -999.0);
        bool coolingEnabled = params.value("cooling", false);

        spdlog::info(
            "Setting camera: gain={}, offset={}, binning={}x{}, cooling={}",
            gain, offset, binning, binning, coolingEnabled);

#ifdef MOCK_CAMERA
        std::shared_ptr<MockCamera> camera = std::make_shared<MockCamera>();
#else
        std::shared_ptr<AtomCamera> camera =
            GetPtr<AtomCamera>(Constants::MAIN_CAMERA).value();
#endif

        if (!camera) {
            spdlog::error("Main camera not set");
            THROW_RUNTIME_ERROR("Main camera not set");
        }

        // Apply camera settings
        spdlog::info("Setting camera gain to {}", gain);
        camera->setGain(gain);

        spdlog::info("Setting camera offset to {}", offset);
        camera->setOffset(offset);

        spdlog::info("Setting camera binning to {}x{}", binning, binning);
        camera->setBinning(binning, binning);

        // Apply temperature settings if specified
        if (targetTemp > -999.0 && coolingEnabled) {
            spdlog::info("Setting camera target temperature to {} °C",
                         targetTemp);
            // Note: MockCamera doesn't have temperature control
#ifndef MOCK_CAMERA
            camera->setCoolerEnabled(true);
            camera->setTargetTemperature(targetTemp);
#endif
        } else if (!coolingEnabled) {
            spdlog::info("Disabling camera cooling");
#ifndef MOCK_CAMERA
            camera->setCoolerEnabled(false);
#endif
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("CameraSettings task completed in {} ms",
                     duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("CameraSettings task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto CameraSettingsTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            CameraSettingsTask cameraSettingsTask("CameraSettings",
                                                  [](const json&) {});
            cameraSettingsTask.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced CameraSettings task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);                        // High priority for settings
    task->setTimeout(std::chrono::seconds(60));  // 1 minute timeout
    task->setLogLevel(2);                        // INFO level

    return task;
}

void CameraSettingsTask::defineParameters(Task& task) {
    task.addParamDefinition("gain", "int", true, 100,
                            "Camera gain value (0-1000)");
    task.addParamDefinition("offset", "int", true, 10,
                            "Camera offset/brightness value (0-255)");
    task.addParamDefinition("binning", "int", true, 1,
                            "Camera binning factor (1-4)");
    task.addParamDefinition("temperature", "double", false, -999.0,
                            "Target temperature in Celsius");
    task.addParamDefinition("cooling", "bool", false, false,
                            "Enable camera cooling");
}

void CameraSettingsTask::validateSettingsParameters(const json& params) {
    if (!params.contains("gain") || !params["gain"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid gain parameter");
    }
    if (!params.contains("offset") || !params["offset"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid offset parameter");
    }
    if (!params.contains("binning") || !params["binning"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid binning parameter");
    }

    int gain = params["gain"].get<int>();
    int offset = params["offset"].get<int>();
    int binning = params["binning"].get<int>();

    if (gain < 0 || gain > 1000) {
        THROW_INVALID_ARGUMENT("Gain must be between 0 and 1000");
    }
    if (offset < 0 || offset > 255) {
        THROW_INVALID_ARGUMENT("Offset must be between 0 and 255");
    }
    if (binning < 1 || binning > 4) {
        THROW_INVALID_ARGUMENT("Binning must be between 1 and 4");
    }

    if (params.contains("temperature")) {
        double temp = params["temperature"].get<double>();
        if (temp < -50.0 || temp > 50.0) {
            THROW_INVALID_ARGUMENT("Temperature must be between -50 and 50 °C");
        }
    }
}

// ==================== CameraPreviewTask Implementation ====================

auto CameraPreviewTask::taskName() -> std::string { return "CameraPreview"; }

void CameraPreviewTask::execute(const json& params) {
    spdlog::info("Executing CameraPreview task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double time = params.value("exposure", 1.0);
        int binning = params.value("binning", 2);  // Default 2x2 for preview
        int gain = params.value("gain", 200);      // Higher gain for preview
        int offset = params.value("offset", 10);
        bool autoStretch = params.value("auto_stretch", true);

        spdlog::info(
            "Starting preview exposure for {} seconds with binning {}x{} and "
            "gain {}",
            time, binning, binning, gain);

        // Create a modified params object for the exposure
        json exposureParams = {{"exposure", time},
                               {"type", ExposureType::SNAPSHOT},
                               {"binning", binning},
                               {"gain", gain},
                               {"offset", offset}};

        // Use TakeExposureTask for the actual exposure
        TakeExposureTask takeExposureTask("TakeExposure", [](const json&) {});
        takeExposureTask.execute(exposureParams);

        // Apply auto-stretch if requested
        if (autoStretch) {
            spdlog::info("Applying auto-stretch to preview image");
            // Note: Auto-stretch implementation would go here
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("CameraPreview task completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("CameraPreview task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto CameraPreviewTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            CameraPreviewTask cameraPreviewTask("CameraPreview",
                                                [](const json&) {});
            cameraPreviewTask.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced CameraPreview task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(5);                         // Medium priority for preview
    task->setTimeout(std::chrono::seconds(120));  // 2 minute timeout
    task->setLogLevel(2);                         // INFO level

    return task;
}

void CameraPreviewTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", false, 1.0,
                            "Preview exposure time in seconds");
    task.addParamDefinition("binning", "int", false, 2,
                            "Camera binning factor for preview");
    task.addParamDefinition("gain", "int", false, 200,
                            "Camera gain value for preview");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
    task.addParamDefinition("auto_stretch", "bool", false, true,
                            "Apply automatic histogram stretch");
}

void CameraPreviewTask::validatePreviewParameters(const json& params) {
    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {  // Max 1 minute for preview
            THROW_INVALID_ARGUMENT(
                "Preview exposure time must be between 0 and 60 seconds");
        }
    }

    if (params.contains("binning")) {
        int binning = params["binning"].get<int>();
        if (binning < 1 || binning > 4) {
            THROW_INVALID_ARGUMENT("Binning must be between 1 and 4");
        }
    }

    if (params.contains("gain")) {
        int gain = params["gain"].get<int>();
        if (gain < 0 || gain > 1000) {
            THROW_INVALID_ARGUMENT("Gain must be between 0 and 1000");
        }
    }

    if (params.contains("offset")) {
        int offset = params["offset"].get<int>();
        if (offset < 0 || offset > 255) {
            THROW_INVALID_ARGUMENT("Offset must be between 0 and 255");
        }
    }
}

}  // namespace lithium::task::camera

// ==================== Task Registration Section ====================
namespace {
using namespace lithium::task;
using namespace lithium::task::camera;

// Register TakeExposureTask
AUTO_REGISTER_TASK(
    TakeExposureTask, "TakeExposure",
    (TaskInfo{
        .name = "TakeExposure",
        .description = "Takes a single camera exposure with specified parameters",
        .category = "Camera",
        .requiredParameters = {"exposure"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"exposure", json{{"type", "number"},
                                         {"minimum", 0},
                                         {"maximum", 7200}}},
                       {"type", json{{"type", "string"},
                                     {"enum", json::array({"light", "dark", "bias", "flat", "snapshot"})}}},
                       {"gain", json{{"type", "integer"},
                                     {"minimum", 0},
                                     {"maximum", 1000}}},
                       {"offset", json{{"type", "integer"},
                                       {"minimum", 0},
                                       {"maximum", 255}}},
                       {"binning", json{{"type", "object"},
                                        {"properties",
                                         json{{"x", json{{"type", "integer"}}},
                                              {"y", json{{"type", "integer"}}}}}}},
                       {"fileName", json{{"type", "string"}}},
                       {"path", json{{"type", "string"}}}}},
                 {"required", json::array({"exposure"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register TakeManyExposureTask
AUTO_REGISTER_TASK(
    TakeManyExposureTask, "TakeManyExposure",
    (TaskInfo{.name = "TakeManyExposure",
              .description = "Takes multiple exposures with the same settings",
              .category = "Camera",
              .requiredParameters = {"count", "exposure"},
              .parameterSchema =
                  json{{"type", "object"},
                       {"properties",
                        json{{"count", json{{"type", "integer"},
                                            {"minimum", 1},
                                            {"maximum", 1000}}},
                             {"exposure", json{{"type", "number"},
                                               {"minimum", 0},
                                               {"maximum", 7200}}},
                             {"type",
                              json{
                                  {"type", "string"},
                                  {"enum", json::array({"light", "dark", "bias",
                                                        "flat", "snapshot"})}}},
                             {"binning", json{{"type", "integer"},
                                              {"minimum", 1},
                                              {"maximum", 4}}},
                             {"gain",
                              json{{"type", "integer"},
                                   {"minimum", 0},
                                   {"maximum", 1000}}},
                             {"offset", json{{"type", "integer"},
                                             {"minimum", 0},
                                             {"maximum", 255}}},
                             {"delay", json{{"type", "number"},
                                            {"minimum", 0},
                                            {"maximum", 600}}}}},
                       {"required", json::array({"count", "exposure"})}},
              .version = "1.0.0",
              .dependencies = {"TakeExposure"}}));

// Register SubframeExposureTask
AUTO_REGISTER_TASK(
    SubframeExposureTask, "SubframeExposure",
    (TaskInfo{
        .name = "SubframeExposure",
        .description = "Takes an exposure of a subframe region of the sensor",
        .category = "Camera",
        .requiredParameters = {"exposure", "x", "y", "width", "height"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"exposure", json{{"type", "number"},
                                         {"minimum", 0},
                                         {"maximum", 7200}}},
                       {"type",
                        json{{"type", "string"},
                             {"enum", json::array({"light", "dark", "bias",
                                                   "flat", "snapshot"})}}},
                       {"x", json{{"type", "integer"}, {"minimum", 0}}},
                       {"y", json{{"type", "integer"}, {"minimum", 0}}},
                       {"width", json{{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 4096}}},
                       {"height", json{{"type", "integer"},
                                       {"minimum", 1},
                                       {"maximum", 4096}}},
                       {"binning", json{{"type", "integer"},
                                        {"minimum", 1},
                                        {"maximum", 4}}},
                       {"gain", json{{"type", "integer"},
                                     {"minimum", 0},
                                     {"maximum", 1000}}},
                       {"offset", json{{"type", "integer"},
                                       {"minimum", 0},
                                       {"maximum", 255}}}}},
                 {"required",
                  json::array({"exposure", "x", "y", "width", "height"})}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register CameraSettingsTask
AUTO_REGISTER_TASK(
    CameraSettingsTask, "CameraSettings",
    (TaskInfo{
        .name = "CameraSettings",
        .description =
            "Configures camera settings like gain, offset and binning",
        .category = "Camera",
        .requiredParameters = {"gain", "offset", "binning"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties", json{{"gain", json{{"type", "integer"},
                                                   {"minimum", 0},
                                                   {"maximum", 1000}}},
                                     {"offset", json{{"type", "integer"},
                                                     {"minimum", 0},
                                                     {"maximum", 255}}},
                                     {"binning", json{{"type", "integer"},
                                                      {"minimum", 1},
                                                      {"maximum", 4}}},
                                     {"temperature", json{{"type", "number"},
                                                          {"minimum", -50},
                                                          {"maximum", 50}}},
                                     {"cooling", json{{"type", "boolean"}}}}},
                 {"required", json::array({"gain", "offset", "binning"})}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register CameraPreviewTask
AUTO_REGISTER_TASK(
    CameraPreviewTask, "CameraPreview",
    (TaskInfo{
        .name = "CameraPreview",
        .description = "Takes a quick preview exposure with optimized settings",
        .category = "Camera",
        .requiredParameters = {},
        .parameterSchema = json{{"type", "object"},
                                {"properties",
                                 json{{"exposure", json{{"type", "number"},
                                                        {"minimum", 0},
                                                        {"maximum", 60}}},
                                      {"binning", json{{"type", "integer"},
                                                       {"minimum", 1},
                                                       {"maximum", 4}}},
                                      {"gain", json{{"type", "integer"},
                                                    {"minimum", 0},
                                                    {"maximum", 1000}}},
                                      {"offset", json{{"type", "integer"},
                                                      {"minimum", 0},
                                                      {"maximum", 255}}},
                                      {"auto_stretch",
                                       json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

}  // namespace
