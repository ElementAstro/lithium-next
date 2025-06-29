#include "lrgb_sequence.hpp"

#include "change.hpp"

#include <algorithm>
#include <future>

#include "spdlog/spdlog.h"

namespace lithium::task::filter {

LRGBSequenceTask::LRGBSequenceTask(const std::string& name)
    : BaseFilterTask(name) {
    setupLRGBDefaults();
}

void LRGBSequenceTask::setupLRGBDefaults() {
    // LRGB-specific parameters
    addParamDefinition("luminance_exposure", "number", false, 60.0,
                       "Luminance exposure time in seconds");
    addParamDefinition("red_exposure", "number", false, 60.0,
                       "Red exposure time in seconds");
    addParamDefinition("green_exposure", "number", false, 60.0,
                       "Green exposure time in seconds");
    addParamDefinition("blue_exposure", "number", false, 60.0,
                       "Blue exposure time in seconds");

    addParamDefinition("luminance_count", "number", false, 10,
                       "Number of luminance frames");
    addParamDefinition("red_count", "number", false, 5, "Number of red frames");
    addParamDefinition("green_count", "number", false, 5,
                       "Number of green frames");
    addParamDefinition("blue_count", "number", false, 5,
                       "Number of blue frames");

    addParamDefinition("gain", "number", false, 100, "Camera gain setting");
    addParamDefinition("offset", "number", false, 10, "Camera offset setting");
    addParamDefinition("start_with_luminance", "boolean", false, true,
                       "Start sequence with luminance filter");
    addParamDefinition("interleaved", "boolean", false, false,
                       "Use interleaved LRGB pattern");
    addParamDefinition("settling_time", "number", false, 2.0,
                       "Filter settling time in seconds");

    setTaskType("lrgb_sequence");
    setTimeout(std::chrono::hours(4));  // 4 hours for long sequences
    setPriority(6);

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("LRGB sequence task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("LRGB sequence exception: " + std::string(e.what()));
        isCancelled_ = true;
    });
}

void LRGBSequenceTask::execute(const json& params) {
    addHistoryEntry("Starting LRGB sequence");

    try {
        validateFilterParams(params);

        // Parse parameters into settings structure
        LRGBSettings settings;
        settings.luminanceExposure = params.value("luminance_exposure", 60.0);
        settings.redExposure = params.value("red_exposure", 60.0);
        settings.greenExposure = params.value("green_exposure", 60.0);
        settings.blueExposure = params.value("blue_exposure", 60.0);

        settings.luminanceCount = params.value("luminance_count", 10);
        settings.redCount = params.value("red_count", 5);
        settings.greenCount = params.value("green_count", 5);
        settings.blueCount = params.value("blue_count", 5);

        settings.gain = params.value("gain", 100);
        settings.offset = params.value("offset", 10);
        settings.startWithLuminance =
            params.value("start_with_luminance", true);
        settings.interleaved = params.value("interleaved", false);

        bool success = executeSequence(settings);

        if (!success) {
            setErrorType(TaskErrorType::SystemError);
            throw std::runtime_error("LRGB sequence execution failed");
        }

        addHistoryEntry("LRGB sequence completed successfully");

    } catch (const std::exception& e) {
        handleFilterError("LRGB", e.what());
        throw;
    }
}

bool LRGBSequenceTask::executeSequence(const LRGBSettings& settings) {
    currentSettings_ = settings;
    sequenceStartTime_ = std::chrono::steady_clock::now();
    sequenceProgress_ = 0.0;
    isPaused_ = false;
    isCancelled_ = false;

    // Calculate total frames
    totalFrames_ = settings.luminanceCount + settings.redCount +
                   settings.greenCount + settings.blueCount;
    completedFrames_ = 0;

    spdlog::info("Starting LRGB sequence: L={}, R={}, G={}, B={} frames",
                 settings.luminanceCount, settings.redCount,
                 settings.greenCount, settings.blueCount);

    addHistoryEntry("LRGB sequence parameters: L=" +
                    std::to_string(settings.luminanceCount) +
                    ", R=" + std::to_string(settings.redCount) +
                    ", G=" + std::to_string(settings.greenCount) +
                    ", B=" + std::to_string(settings.blueCount) + " frames");

    try {
        if (settings.interleaved) {
            return executeInterleavedPattern(settings);
        } else {
            return executeSequentialPattern(settings);
        }
    } catch (const std::exception& e) {
        spdlog::error("LRGB sequence execution failed: {}", e.what());
        return false;
    }
}

std::future<bool> LRGBSequenceTask::executeSequenceAsync(
    const LRGBSettings& settings) {
    return std::async(std::launch::async,
                      [this, settings]() { return executeSequence(settings); });
}

bool LRGBSequenceTask::executeSequentialPattern(const LRGBSettings& settings) {
    addHistoryEntry("Executing sequential LRGB pattern");

    // Determine sequence order
    std::vector<std::pair<std::string, std::pair<double, int>>> sequence;

    if (settings.startWithLuminance) {
        sequence = {{"Luminance",
                     {settings.luminanceExposure, settings.luminanceCount}},
                    {"Red", {settings.redExposure, settings.redCount}},
                    {"Green", {settings.greenExposure, settings.greenCount}},
                    {"Blue", {settings.blueExposure, settings.blueCount}}};
    } else {
        sequence = {{"Red", {settings.redExposure, settings.redCount}},
                    {"Green", {settings.greenExposure, settings.greenCount}},
                    {"Blue", {settings.blueExposure, settings.blueCount}},
                    {"Luminance",
                     {settings.luminanceExposure, settings.luminanceCount}}};
    }

    for (const auto& [filterName, exposureAndCount] : sequence) {
        if (isCancelled_) {
            addHistoryEntry("LRGB sequence cancelled");
            return false;
        }

        // Wait if paused
        while (isPaused_ && !isCancelled_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        double exposure = exposureAndCount.first;
        int count = exposureAndCount.second;

        if (count > 0) {
            spdlog::info("Capturing {} frames with {} filter ({}s exposure)",
                         count, filterName, exposure);

            bool success = captureFilterFrames(filterName, exposure, count,
                                               settings.gain, settings.offset);
            if (!success) {
                return false;
            }
        }
    }

    return true;
}

bool LRGBSequenceTask::executeInterleavedPattern(const LRGBSettings& settings) {
    addHistoryEntry("Executing interleaved LRGB pattern");

    // Create interleaved sequence
    std::vector<std::tuple<std::string, double, int>> filters = {
        {"Luminance", settings.luminanceExposure, settings.luminanceCount},
        {"Red", settings.redExposure, settings.redCount},
        {"Green", settings.greenExposure, settings.greenCount},
        {"Blue", settings.blueExposure, settings.blueCount}};

    // Find maximum count to determine number of rounds
    int maxCount = std::max({settings.luminanceCount, settings.redCount,
                             settings.greenCount, settings.blueCount});

    for (int round = 0; round < maxCount; ++round) {
        if (isCancelled_) {
            addHistoryEntry("LRGB sequence cancelled");
            return false;
        }

        for (const auto& [filterName, exposure, totalCount] : filters) {
            if (round < totalCount) {
                // Wait if paused
                while (isPaused_ && !isCancelled_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                if (isCancelled_)
                    break;

                spdlog::info("Capturing frame {} of {} with {} filter",
                             round + 1, totalCount, filterName);

                bool success = captureFilterFrames(
                    filterName, exposure, 1, settings.gain, settings.offset);
                if (!success) {
                    return false;
                }
            }
        }

        if (isCancelled_)
            break;
    }

    return !isCancelled_;
}

bool LRGBSequenceTask::captureFilterFrames(const std::string& filterName,
                                           double exposure, int count, int gain,
                                           int offset) {
    try {
        // Change to the specified filter
        FilterChangeTask filterChanger("temp_filter_change");
        json changeParams = {
            {"filterName", filterName}, {"timeout", 30}, {"verify", true}};

        filterChanger.execute(changeParams);

        // Wait for settling
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Simulate frame capture
        for (int i = 0; i < count; ++i) {
            if (isCancelled_) {
                return false;
            }

            while (isPaused_ && !isCancelled_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            spdlog::info(
                "Capturing frame {} of {} with {} filter ({}s exposure)", i + 1,
                count, filterName, exposure);

            addHistoryEntry("Capturing " + filterName + " frame " +
                            std::to_string(i + 1) + "/" +
                            std::to_string(count));

            // Simulate exposure time
            auto frameStart = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(
                    exposure * 100)));  // Scaled down for simulation

            completedFrames_++;
            updateProgress(completedFrames_, totalFrames_);

            addHistoryEntry("Frame completed: " + filterName + " " +
                            std::to_string(i + 1) + "/" +
                            std::to_string(count));
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to capture {} frames: {}", filterName, e.what());
        handleFilterError(filterName,
                          "Frame capture failed: " + std::string(e.what()));
        return false;
    }
}

double LRGBSequenceTask::getSequenceProgress() const {
    return sequenceProgress_.load();
}

std::chrono::seconds LRGBSequenceTask::getEstimatedRemainingTime() const {
    if (completedFrames_ == 0) {
        return std::chrono::seconds(0);
    }

    auto elapsed = std::chrono::steady_clock::now() - sequenceStartTime_;
    double elapsedSeconds = std::chrono::duration<double>(elapsed).count();

    double framesPerSecond = completedFrames_ / elapsedSeconds;
    int remainingFrames = totalFrames_ - completedFrames_;

    return std::chrono::seconds(
        static_cast<int>(remainingFrames / framesPerSecond));
}

void LRGBSequenceTask::pauseSequence() {
    isPaused_ = true;
    addHistoryEntry("LRGB sequence paused");
    spdlog::info("LRGB sequence paused");
}

void LRGBSequenceTask::resumeSequence() {
    isPaused_ = false;
    addHistoryEntry("LRGB sequence resumed");
    spdlog::info("LRGB sequence resumed");
}

void LRGBSequenceTask::cancelSequence() {
    isCancelled_ = true;
    addHistoryEntry("LRGB sequence cancelled");
    spdlog::info("LRGB sequence cancelled");
}

void LRGBSequenceTask::updateProgress(int completedFrames, int totalFrames) {
    if (totalFrames > 0) {
        double progress =
            (static_cast<double>(completedFrames) / totalFrames) * 100.0;
        sequenceProgress_ = progress;

        if (completedFrames % 5 == 0) {  // Log every 5 frames
            spdlog::info("LRGB sequence progress: {:.1f}% ({}/{})", progress,
                         completedFrames, totalFrames);
        }
    }
}

}  // namespace lithium::task::filter
