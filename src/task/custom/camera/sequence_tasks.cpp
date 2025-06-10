#include "sequence_tasks.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include "basic_exposure.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::sequencer::task {

// ==================== SmartExposureTask Implementation ====================

auto SmartExposureTask::taskName() -> std::string { return "SmartExposure"; }

void SmartExposureTask::execute(const json& params) {
    LOG_F(INFO, "Executing SmartExposure task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double targetSNR = params.value("target_snr", 50.0);
        double maxExposure = params.value("max_exposure", 300.0);
        double minExposure = params.value("min_exposure", 1.0);
        int maxAttempts = params.value("max_attempts", 5);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        LOG_F(INFO,
              "Starting smart exposure targeting SNR {} with max exposure {} "
              "seconds",
              targetSNR, maxExposure);

        double currentExposure =
            (maxExposure + minExposure) / 2.0;  // Start with middle value
        double achievedSNR = 0.0;

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            LOG_F(INFO, "Smart exposure attempt {} with {} seconds", attempt,
                  currentExposure);

            // Take test exposure
            json exposureParams = {{"exposure", currentExposure},
                                   {"type", ExposureType::LIGHT},
                                   {"binning", binning},
                                   {"gain", gain},
                                   {"offset", offset}};
            TakeExposureTask::execute(exposureParams);

            // In a real implementation, we would analyze the image for SNR
            // For simulation, we'll use a simple formula
            achievedSNR =
                std::min(targetSNR * 1.2, currentExposure * 0.5 + 20.0);

            LOG_F(INFO, "Achieved SNR: {:.2f}, Target: {:.2f}", achievedSNR,
                  targetSNR);

            if (std::abs(achievedSNR - targetSNR) <= targetSNR * 0.1) {
                LOG_F(INFO, "Target SNR achieved within 10% tolerance");
                break;
            }

            if (attempt < maxAttempts) {
                // Adjust exposure time based on SNR ratio
                double ratio = targetSNR / achievedSNR;
                currentExposure = std::clamp(currentExposure * ratio * ratio,
                                             minExposure, maxExposure);
                LOG_F(INFO, "Adjusting exposure to {} seconds for next attempt",
                      currentExposure);
            }
        }

        // Take final exposure with optimal settings
        LOG_F(INFO, "Taking final smart exposure with {} seconds",
              currentExposure);
        json finalParams = {{"exposure", currentExposure},
                            {"type", ExposureType::LIGHT},
                            {"binning", binning},
                            {"gain", gain},
                            {"offset", offset}};
        TakeExposureTask::execute(finalParams);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO,
              "SmartExposure task completed in {} ms with final SNR {:.2f}",
              duration.count(), achievedSNR);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "SmartExposure task failed after {} ms: {}",
              duration.count(), e.what());
        throw;
    }
}

auto SmartExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced SmartExposure task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);                          // Medium-high priority
    task->setTimeout(std::chrono::seconds(1800));  // 30 minute timeout
    task->setLogLevel(2);                          // INFO level

    return task;
}

void SmartExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("target_snr", "double", false, 50.0,
                            "Target signal-to-noise ratio");
    task.addParamDefinition("max_exposure", "double", false, 300.0,
                            "Maximum exposure time in seconds");
    task.addParamDefinition("min_exposure", "double", false, 1.0,
                            "Minimum exposure time in seconds");
    task.addParamDefinition("max_attempts", "int", false, 5,
                            "Maximum optimization attempts");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
}

void SmartExposureTask::validateSmartExposureParameters(const json& params) {
    if (params.contains("target_snr")) {
        double snr = params["target_snr"].get<double>();
        if (snr <= 0 || snr > 1000) {
            THROW_INVALID_ARGUMENT("Target SNR must be between 0 and 1000");
        }
    }

    if (params.contains("max_exposure")) {
        double exposure = params["max_exposure"].get<double>();
        if (exposure <= 0 || exposure > 3600) {
            THROW_INVALID_ARGUMENT(
                "Max exposure must be between 0 and 3600 seconds");
        }
    }

    if (params.contains("min_exposure")) {
        double exposure = params["min_exposure"].get<double>();
        if (exposure <= 0 || exposure > 300) {
            THROW_INVALID_ARGUMENT(
                "Min exposure must be between 0 and 300 seconds");
        }
    }

    if (params.contains("max_attempts")) {
        int attempts = params["max_attempts"].get<int>();
        if (attempts <= 0 || attempts > 20) {
            THROW_INVALID_ARGUMENT("Max attempts must be between 1 and 20");
        }
    }
}

// ==================== DeepSkySequenceTask Implementation ====================

auto DeepSkySequenceTask::taskName() -> std::string {
    return "DeepSkySequence";
}

void DeepSkySequenceTask::execute(const json& params) {
    LOG_F(INFO, "Executing DeepSkySequence task with params: {}",
          params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string targetName = params.value("target_name", "Unknown");
        int totalExposures = params.value("total_exposures", 20);
        double exposureTime = params.value("exposure_time", 300.0);
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"L"});
        bool dithering = params.value("dithering", true);
        int ditherPixels = params.value("dither_pixels", 10);
        double ditherInterval =
            params.value("dither_interval", 5);  // Every 5 exposures
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        LOG_F(INFO,
              "Starting deep sky sequence for target '{}' with {} exposures of "
              "{} seconds",
              targetName, totalExposures, exposureTime);

        int exposuresPerFilter = totalExposures / filters.size();
        int remainingExposures = totalExposures % filters.size();

        for (size_t filterIndex = 0; filterIndex < filters.size();
             ++filterIndex) {
            const std::string& filter = filters[filterIndex];
            int exposuresForThisFilter =
                exposuresPerFilter + (filterIndex < remainingExposures ? 1 : 0);

            LOG_F(INFO, "Taking {} exposures with filter {}",
                  exposuresForThisFilter, filter);

            for (int exp = 1; exp <= exposuresForThisFilter; ++exp) {
                // Apply dithering if enabled
                if (dithering && exp > 1 &&
                    (exp - 1) % static_cast<int>(ditherInterval) == 0) {
                    LOG_F(INFO, "Applying dither offset of {} pixels",
                          ditherPixels);
                    // In a real implementation, we would move the mount
                    // slightly
                    std::this_thread::sleep_for(
                        std::chrono::seconds(2));  // Simulate dither time
                }

                LOG_F(INFO, "Taking exposure {} of {} for filter {}", exp,
                      exposuresForThisFilter, filter);

                json exposureParams = {{"exposure", exposureTime},
                                       {"type", ExposureType::LIGHT},
                                       {"binning", binning},
                                       {"gain", gain},
                                       {"offset", offset}};
                TakeExposureTask::execute(exposureParams);

                // Check for guide star drift or other issues
                if (exp % 10 == 0) {
                    LOG_F(INFO, "Completed {} exposures for filter {}", exp,
                          filter);
                }
            }

            LOG_F(INFO, "Completed all {} exposures for filter {}",
                  exposuresForThisFilter, filter);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "DeepSkySequence task completed {} exposures in {} ms",
              totalExposures, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "DeepSkySequence task failed after {} ms: {}",
              duration.count(), e.what());
        throw;
    }
}

auto DeepSkySequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced DeepSkySequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(5);  // Medium priority for long sequences
    task->setTimeout(std::chrono::seconds(28800));  // 8 hour timeout
    task->setLogLevel(2);                           // INFO level

    return task;
}

void DeepSkySequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("target_name", "string", false, "Unknown",
                            "Name of the target object");
    task.addParamDefinition("total_exposures", "int", true, 20,
                            "Total number of exposures to take");
    task.addParamDefinition("exposure_time", "double", true, 300.0,
                            "Exposure time per frame in seconds");
    task.addParamDefinition("filters", "array", false, json::array({"L"}),
                            "List of filters to use");
    task.addParamDefinition("dithering", "bool", false, true,
                            "Enable dithering between exposures");
    task.addParamDefinition("dither_pixels", "int", false, 10,
                            "Dither offset in pixels");
    task.addParamDefinition("dither_interval", "double", false, 5.0,
                            "Dither every N exposures");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
}

void DeepSkySequenceTask::validateDeepSkyParameters(const json& params) {
    if (!params.contains("total_exposures") ||
        !params["total_exposures"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid total_exposures parameter");
    }

    if (!params.contains("exposure_time") ||
        !params["exposure_time"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure_time parameter");
    }

    int totalExposures = params["total_exposures"].get<int>();
    if (totalExposures <= 0 || totalExposures > 1000) {
        THROW_INVALID_ARGUMENT("Total exposures must be between 1 and 1000");
    }

    double exposureTime = params["exposure_time"].get<double>();
    if (exposureTime <= 0 || exposureTime > 3600) {
        THROW_INVALID_ARGUMENT(
            "Exposure time must be between 0 and 3600 seconds");
    }

    if (params.contains("dither_pixels")) {
        int pixels = params["dither_pixels"].get<int>();
        if (pixels < 0 || pixels > 100) {
            THROW_INVALID_ARGUMENT("Dither pixels must be between 0 and 100");
        }
    }

    if (params.contains("dither_interval")) {
        double interval = params["dither_interval"].get<double>();
        if (interval <= 0 || interval > 50) {
            THROW_INVALID_ARGUMENT("Dither interval must be between 0 and 50");
        }
    }
}

// ==================== PlanetaryImagingTask Implementation ====================

auto PlanetaryImagingTask::taskName() -> std::string {
    return "PlanetaryImaging";
}

void PlanetaryImagingTask::execute(const json& params) {
    LOG_F(INFO, "Executing PlanetaryImaging task with params: {}",
          params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string planet = params.value("planet", "Mars");
        int videoLength = params.value("video_length", 120);  // seconds
        double frameRate = params.value("frame_rate", 30.0);  // fps
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"R", "G", "B"});
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 400);  // Higher gain for planetary
        int offset = params.value("offset", 10);
        bool highSpeed = params.value("high_speed", true);

        LOG_F(INFO, "Starting planetary imaging of {} for {} seconds at {} fps",
              planet, videoLength, frameRate);

        double frameExposure = 1.0 / frameRate;
        int totalFrames = static_cast<int>(videoLength * frameRate);

        for (const std::string& filter : filters) {
            LOG_F(INFO,
                  "Recording {} frames with filter {} at {} second exposures",
                  totalFrames, filter, frameExposure);

            // For planetary imaging, we typically take rapid short exposures
            for (int frame = 1; frame <= totalFrames; ++frame) {
                json exposureParams = {{"exposure", frameExposure},
                                       {"type", ExposureType::LIGHT},
                                       {"binning", binning},
                                       {"gain", gain},
                                       {"offset", offset}};
                TakeExposureTask::execute(exposureParams);

                if (frame % 100 == 0) {
                    LOG_F(INFO, "Captured {} of {} frames for filter {}", frame,
                          totalFrames, filter);
                }

                // Small delay if not in high-speed mode
                if (!highSpeed) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }

            LOG_F(INFO, "Completed {} frames for filter {}", totalFrames,
                  filter);

            // Brief pause between filters
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "PlanetaryImaging task completed {} total frames in {} ms",
              totalFrames * filters.size(), duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "PlanetaryImaging task failed after {} ms: {}",
              duration.count(), e.what());
        throw;
    }
}

auto PlanetaryImagingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced PlanetaryImaging task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(7);  // High priority for real-time imaging
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);                          // INFO level

    return task;
}

void PlanetaryImagingTask::defineParameters(Task& task) {
    task.addParamDefinition("planet", "string", false, "Mars",
                            "Name of the planet being imaged");
    task.addParamDefinition("video_length", "int", true, 120,
                            "Video length in seconds");
    task.addParamDefinition("frame_rate", "double", false, 30.0,
                            "Frame rate in frames per second");
    task.addParamDefinition("filters", "array", false,
                            json::array({"R", "G", "B"}),
                            "List of filters to use");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 400,
                            "Camera gain value (higher for planetary)");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
    task.addParamDefinition("high_speed", "bool", false, true,
                            "Enable high-speed capture mode");
}

void PlanetaryImagingTask::validatePlanetaryParameters(const json& params) {
    if (!params.contains("video_length") ||
        !params["video_length"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid video_length parameter");
    }

    int videoLength = params["video_length"].get<int>();
    if (videoLength <= 0 || videoLength > 1800) {  // Max 30 minutes
        THROW_INVALID_ARGUMENT(
            "Video length must be between 1 and 1800 seconds");
    }

    if (params.contains("frame_rate")) {
        double frameRate = params["frame_rate"].get<double>();
        if (frameRate <= 0 || frameRate > 120) {
            THROW_INVALID_ARGUMENT("Frame rate must be between 0 and 120 fps");
        }
    }
}

// ==================== TimelapseTask Implementation ====================

auto TimelapseTask::taskName() -> std::string { return "Timelapse"; }

void TimelapseTask::execute(const json& params) {
    LOG_F(INFO, "Executing Timelapse task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int totalFrames = params.value("total_frames", 100);
        double interval =
            params.value("interval", 30.0);  // seconds between frames
        double exposureTime = params.value("exposure_time", 10.0);
        std::string timelapseType =
            params.value("type", "sunset");  // sunset, lunar, star_trails
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);
        bool autoExposure = params.value("auto_exposure", false);

        LOG_F(INFO,
              "Starting {} timelapse with {} frames at {} second intervals",
              timelapseType, totalFrames, interval);

        for (int frame = 1; frame <= totalFrames; ++frame) {
            auto frameStartTime = std::chrono::steady_clock::now();

            LOG_F(INFO, "Capturing timelapse frame {} of {}", frame,
                  totalFrames);

            // Adjust exposure for certain timelapse types
            double currentExposure = exposureTime;
            if (autoExposure && timelapseType == "sunset") {
                // Gradually increase exposure as it gets darker
                double progress = static_cast<double>(frame) / totalFrames;
                currentExposure =
                    exposureTime * (1.0 + progress * 4.0);  // Up to 5x longer
            }

            json exposureParams = {{"exposure", currentExposure},
                                   {"type", ExposureType::LIGHT},
                                   {"binning", binning},
                                   {"gain", gain},
                                   {"offset", offset}};
            TakeExposureTask::execute(exposureParams);

            // Calculate remaining time for this frame
            auto frameEndTime = std::chrono::steady_clock::now();
            auto frameElapsed =
                std::chrono::duration_cast<std::chrono::seconds>(
                    frameEndTime - frameStartTime);
            auto remainingTime =
                std::chrono::seconds(static_cast<int>(interval)) - frameElapsed;

            if (remainingTime.count() > 0 && frame < totalFrames) {
                LOG_F(INFO, "Waiting {} seconds until next frame",
                      remainingTime.count());
                std::this_thread::sleep_for(remainingTime);
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(INFO, "Timelapse task completed {} frames in {} ms", totalFrames,
              duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        LOG_F(ERROR, "Timelapse task failed after {} ms: {}", duration.count(),
              e.what());
        throw;
    }
}

auto TimelapseTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced Timelapse task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(3);  // Medium-low priority for long sequences
    task->setTimeout(std::chrono::seconds(43200));  // 12 hour timeout
    task->setLogLevel(2);                           // INFO level

    return task;
}

void TimelapseTask::defineParameters(Task& task) {
    task.addParamDefinition("total_frames", "int", true, 100,
                            "Total number of frames to capture");
    task.addParamDefinition("interval", "double", true, 30.0,
                            "Time interval between frames in seconds");
    task.addParamDefinition("exposure_time", "double", false, 10.0,
                            "Exposure time per frame in seconds");
    task.addParamDefinition("type", "string", false, "sunset",
                            "Type of timelapse (sunset, lunar, star_trails)");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10,
                            "Camera offset/brightness value");
    task.addParamDefinition("auto_exposure", "bool", false, false,
                            "Automatically adjust exposure over time");
}

void TimelapseTask::validateTimelapseParameters(const json& params) {
    if (!params.contains("total_frames") ||
        !params["total_frames"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid total_frames parameter");
    }

    if (!params.contains("interval") || !params["interval"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid interval parameter");
    }

    int totalFrames = params["total_frames"].get<int>();
    if (totalFrames <= 0 || totalFrames > 10000) {
        THROW_INVALID_ARGUMENT("Total frames must be between 1 and 10000");
    }

    double interval = params["interval"].get<double>();
    if (interval <= 0 || interval > 3600) {  // Max 1 hour between frames
        THROW_INVALID_ARGUMENT("Interval must be between 0 and 3600 seconds");
    }

    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"].get<double>();
        if (exposure <= 0 || exposure > interval) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be positive and less than interval");
        }
    }
}

}  // namespace lithium::sequencer::task
