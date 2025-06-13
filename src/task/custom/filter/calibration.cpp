#include "calibration.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

#include "spdlog/spdlog.h"

namespace lithium::task::filter {

FilterCalibrationTask::FilterCalibrationTask(const std::string& name)
    : BaseFilterTask(name) {
    setupCalibrationDefaults();
}

void FilterCalibrationTask::setupCalibrationDefaults() {
    // Calibration type and filters
    addParamDefinition("calibration_type", "string", true, nullptr,
                       "Type of calibration (dark, flat, bias, all)");
    addParamDefinition("filters", "array", false, json::array(),
                       "List of filters to calibrate");

    // Dark frame settings
    addParamDefinition("dark_exposures", "array", false,
                       json::array({1.0, 60.0, 300.0}), "Dark exposure times");
    addParamDefinition("dark_count", "number", false, 10,
                       "Number of dark frames per exposure");

    // Flat frame settings
    addParamDefinition("flat_exposure", "number", false, 1.0,
                       "Flat field exposure time");
    addParamDefinition("flat_count", "number", false, 10,
                       "Number of flat frames per filter");
    addParamDefinition("auto_flat_exposure", "boolean", false, true,
                       "Auto-determine flat exposure");
    addParamDefinition("target_adu", "number", false, 25000.0,
                       "Target ADU for flat frames");

    // Bias frame settings
    addParamDefinition("bias_count", "number", false, 50,
                       "Number of bias frames");

    // Camera settings
    addParamDefinition("gain", "number", false, 100, "Camera gain setting");
    addParamDefinition("offset", "number", false, 10, "Camera offset setting");
    addParamDefinition("temperature", "number", false, -10.0,
                       "Target camera temperature");

    setTaskType("filter_calibration");
    setTimeout(std::chrono::hours(6));  // 6 hours for full calibration
    setPriority(4);

    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Filter calibration task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Calibration exception: " + std::string(e.what()));
    });
}

void FilterCalibrationTask::execute(const json& params) {
    addHistoryEntry("Starting filter calibration task");

    try {
        validateFilterParams(params);

        // Parse parameters into settings
        CalibrationSettings settings;

        std::string typeStr = params["calibration_type"].get<std::string>();
        settings.type = stringToCalibationType(typeStr);

        if (params.contains("filters") && params["filters"].is_array()) {
            for (const auto& filter : params["filters"]) {
                settings.filters.push_back(filter.get<std::string>());
            }
        }

        // Dark settings
        if (params.contains("dark_exposures") &&
            params["dark_exposures"].is_array()) {
            settings.darkExposures.clear();
            for (const auto& exp : params["dark_exposures"]) {
                settings.darkExposures.push_back(exp.get<double>());
            }
        }
        settings.darkCount = params.value("dark_count", 10);

        // Flat settings
        settings.flatExposure = params.value("flat_exposure", 1.0);
        settings.flatCount = params.value("flat_count", 10);
        settings.autoFlatExposure = params.value("auto_flat_exposure", true);
        settings.targetADU = params.value("target_adu", 25000.0);

        // Bias settings
        settings.biasCount = params.value("bias_count", 50);

        // Camera settings
        settings.gain = params.value("gain", 100);
        settings.offset = params.value("offset", 10);
        settings.temperature = params.value("temperature", -10.0);

        currentSettings_ = settings;

        // Execute the calibration
        bool success = executeCalibration(settings);

        if (!success) {
            setErrorType(TaskErrorType::SystemError);
            throw std::runtime_error("Filter calibration failed");
        }

        addHistoryEntry("Filter calibration completed successfully");

    } catch (const std::exception& e) {
        handleFilterError("calibration", e.what());
        throw;
    }
}

bool FilterCalibrationTask::executeCalibration(
    const CalibrationSettings& settings) {
    spdlog::info("Starting filter calibration sequence");
    addHistoryEntry("Starting calibration sequence");

    calibrationStartTime_ = std::chrono::steady_clock::now();
    calibrationProgress_ = 0.0;
    completedFrames_ = 0;

    // Calculate total frames
    totalFrames_ = 0;
    if (settings.type == CalibrationType::Dark ||
        settings.type == CalibrationType::All) {
        totalFrames_ += settings.darkExposures.size() * settings.darkCount;
    }
    if (settings.type == CalibrationType::Flat ||
        settings.type == CalibrationType::All) {
        totalFrames_ += settings.filters.size() * settings.flatCount;
    }
    if (settings.type == CalibrationType::Bias ||
        settings.type == CalibrationType::All) {
        totalFrames_ += settings.biasCount;
    }

    try {
        // Wait for target temperature
        if (!waitForTemperature(settings.temperature)) {
            spdlog::warn(
                "Could not reach target temperature, continuing anyway");
            addHistoryEntry(
                "Temperature warning: Could not reach target temperature");
        }

        // Execute calibration based on type
        bool success = true;

        if (settings.type == CalibrationType::Bias ||
            settings.type == CalibrationType::All) {
            success &= captureBiasFrames(settings.biasCount, settings.gain,
                                         settings.offset, settings.temperature);
        }

        if (settings.type == CalibrationType::Dark ||
            settings.type == CalibrationType::All) {
            success &= captureDarkFrames(settings.darkExposures,
                                         settings.darkCount, settings.gain,
                                         settings.offset, settings.temperature);
        }

        if (settings.type == CalibrationType::Flat ||
            settings.type == CalibrationType::All) {
            success &= captureFlatFrames(
                settings.filters, settings.flatExposure, settings.flatCount,
                settings.gain, settings.offset, settings.autoFlatExposure,
                settings.targetADU);
        }

        if (success) {
            calibrationProgress_ = 100.0;
            spdlog::info("Filter calibration completed successfully");
            addHistoryEntry("Calibration completed successfully");
        }

        return success;

    } catch (const std::exception& e) {
        spdlog::error("Calibration execution failed: {}", e.what());
        addHistoryEntry("Calibration execution failed: " +
                        std::string(e.what()));
        return false;
    }
}

bool FilterCalibrationTask::captureDarkFrames(
    const std::vector<double>& exposures, int count, int gain, int offset,
    double temperature) {
    spdlog::info("Capturing dark frames for {} exposure times",
                 exposures.size());
    addHistoryEntry("Starting dark frame capture");

    try {
        // Ensure camera is covered/closed for dark frames
        // This would typically involve closing a camera cover or moving to a
        // dark position

        for (double exposure : exposures) {
            spdlog::info("Capturing {} dark frames at {} seconds exposure",
                         count, exposure);
            addHistoryEntry("Capturing " + std::to_string(count) +
                            " dark frames at " + std::to_string(exposure) +
                            "s exposure");

            for (int i = 0; i < count; ++i) {
                // Simulate dark frame capture
                spdlog::debug("Capturing dark frame {}/{} ({}s)", i + 1, count,
                              exposure);

                // Here you would interface with the actual camera to capture a
                // dark frame For simulation, just wait for the exposure time
                auto exposureMs = static_cast<int>(exposure * 1000);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(exposureMs));

                completedFrames_++;
                updateProgress(completedFrames_, totalFrames_);

                // Check for cancellation
                if (getStatus() == TaskStatus::Failed) {
                    return false;
                }
            }
        }

        addHistoryEntry("Dark frame capture completed");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Dark frame capture failed: {}", e.what());
        addHistoryEntry("Dark frame capture failed: " + std::string(e.what()));
        return false;
    }
}

bool FilterCalibrationTask::captureFlatFrames(
    const std::vector<std::string>& filters, double exposure, int count,
    int gain, int offset, bool autoExposure, double targetADU) {
    spdlog::info("Capturing flat frames for {} filters", filters.size());
    addHistoryEntry("Starting flat frame capture");

    try {
        // Ensure flat field light source is available
        // This would typically involve positioning a flat panel or pointing at
        // twilight sky

        for (const auto& filterName : filters) {
            spdlog::info("Capturing flat frames for filter: {}", filterName);
            addHistoryEntry("Capturing flat frames for filter: " + filterName);

            // Change to the specified filter
            if (!changeFilter(filterName)) {
                spdlog::error("Failed to change to filter: {}", filterName);
                continue;  // Skip this filter
            }

            // Wait for filter to settle
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Determine optimal exposure if auto mode is enabled
            double finalExposure = exposure;
            if (autoExposure) {
                finalExposure = determineOptimalFlatExposure(
                    filterName, targetADU, gain, offset);
                spdlog::info("Optimal flat exposure for {}: {}s", filterName,
                             finalExposure);
                addHistoryEntry("Optimal flat exposure for " + filterName +
                                ": " + std::to_string(finalExposure) + "s");
            }

            // Capture flat frames
            for (int i = 0; i < count; ++i) {
                spdlog::debug("Capturing flat frame {}/{} for {} ({}s)", i + 1,
                              count, filterName, finalExposure);

                // Here you would interface with the actual camera to capture a
                // flat frame For simulation, just wait for the exposure time
                auto exposureMs = static_cast<int>(finalExposure * 1000);
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(exposureMs));

                completedFrames_++;
                updateProgress(completedFrames_, totalFrames_);

                // Check for cancellation
                if (getStatus() == TaskStatus::Failed) {
                    return false;
                }
            }
        }

        addHistoryEntry("Flat frame capture completed");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Flat frame capture failed: {}", e.what());
        addHistoryEntry("Flat frame capture failed: " + std::string(e.what()));
        return false;
    }
}

bool FilterCalibrationTask::captureBiasFrames(int count, int gain, int offset,
                                              double temperature) {
    spdlog::info("Capturing {} bias frames", count);
    addHistoryEntry("Starting bias frame capture");

    try {
        // Bias frames are zero-second exposures
        for (int i = 0; i < count; ++i) {
            spdlog::debug("Capturing bias frame {}/{}", i + 1, count);

            // Here you would interface with the actual camera to capture a bias
            // frame For simulation, just a minimal delay
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            completedFrames_++;
            updateProgress(completedFrames_, totalFrames_);

            // Check for cancellation
            if (getStatus() == TaskStatus::Failed) {
                return false;
            }
        }

        addHistoryEntry("Bias frame capture completed");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Bias frame capture failed: {}", e.what());
        addHistoryEntry("Bias frame capture failed: " + std::string(e.what()));
        return false;
    }
}

double FilterCalibrationTask::determineOptimalFlatExposure(
    const std::string& filterName, double targetADU, int gain, int offset) {
    spdlog::info("Determining optimal flat exposure for filter: {}",
                 filterName);
    addHistoryEntry("Determining optimal flat exposure for: " + filterName);

    try {
        // Start with a test exposure
        double testExposure = 0.1;  // 100ms
        double currentADU = 0.0;
        int maxIterations = 10;

        for (int iteration = 0; iteration < maxIterations; ++iteration) {
            spdlog::debug("Test exposure {}: {}s", iteration + 1, testExposure);

            // Simulate taking a test exposure and measuring ADU
            // In real implementation, this would capture an actual frame and
            // analyze it
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(testExposure * 1000)));

            // Simulate ADU measurement (this would be real frame analysis)
            // For different filters, simulate different light transmission
            double filterFactor = 1.0;
            if (filterName == "Red")
                filterFactor = 0.8;
            else if (filterName == "Green")
                filterFactor = 0.9;
            else if (filterName == "Blue")
                filterFactor = 0.7;
            else if (filterName == "Ha")
                filterFactor = 0.3;
            else if (filterName == "OIII")
                filterFactor = 0.2;
            else if (filterName == "SII")
                filterFactor = 0.25;

            currentADU = (testExposure * gain * filterFactor * 10000.0) /
                         (1.0 + offset * 0.01);

            spdlog::debug("Test exposure {}s resulted in {} ADU", testExposure,
                          currentADU);

            // Check if we're close enough to target
            if (std::abs(currentADU - targetADU) < targetADU * 0.1) {
                spdlog::info("Optimal exposure found: {}s (ADU: {})",
                             testExposure, currentADU);
                return testExposure;
            }

            // Adjust exposure based on current ADU
            double ratio = targetADU / currentADU;
            testExposure *= ratio;

            // Clamp exposure to reasonable limits
            testExposure = std::max(0.001, std::min(testExposure, 60.0));
        }

        spdlog::warn("Could not determine optimal exposure, using: {}s",
                     testExposure);
        return testExposure;

    } catch (const std::exception& e) {
        spdlog::error("Failed to determine optimal flat exposure: {}",
                      e.what());
        return 1.0;  // Return default exposure
    }
}

double FilterCalibrationTask::getCalibrationProgress() const {
    return calibrationProgress_.load();
}

std::chrono::seconds FilterCalibrationTask::getEstimatedRemainingTime() const {
    if (completedFrames_ == 0 || totalFrames_ == 0) {
        return std::chrono::seconds(0);
    }

    auto elapsed = std::chrono::steady_clock::now() - calibrationStartTime_;
    auto avgTimePerFrame = elapsed / completedFrames_;
    auto remainingFrames = totalFrames_ - completedFrames_;

    return std::chrono::duration_cast<std::chrono::seconds>(avgTimePerFrame *
                                                            remainingFrames);
}

CalibrationType FilterCalibrationTask::stringToCalibationType(
    const std::string& typeStr) const {
    if (typeStr == "dark")
        return CalibrationType::Dark;
    if (typeStr == "flat")
        return CalibrationType::Flat;
    if (typeStr == "bias")
        return CalibrationType::Bias;
    if (typeStr == "all")
        return CalibrationType::All;

    throw std::invalid_argument("Invalid calibration type: " + typeStr);
}

void FilterCalibrationTask::updateProgress(int completedFrames,
                                           int totalFrames) {
    if (totalFrames > 0) {
        double progress =
            (static_cast<double>(completedFrames) / totalFrames) * 100.0;
        calibrationProgress_ = progress;

        // Update task progress
        // setProgress(progress); // 已移除未声明函数
    }
}

bool FilterCalibrationTask::waitForTemperature(double targetTemperature,
                                               int timeoutMinutes) {
    spdlog::info("Waiting for camera to reach target temperature: {}°C",
                 targetTemperature);
    addHistoryEntry("Waiting for target temperature: " +
                    std::to_string(targetTemperature) + "°C");

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::minutes(timeoutMinutes);

    while (true) {
        // Simulate temperature reading (in real implementation, query camera
        // temperature)
        double currentTemp = -5.0;  // Simulated current temperature

        if (std::abs(currentTemp - targetTemperature) <= 1.0) {
            spdlog::info("Target temperature reached: {}°C", currentTemp);
            addHistoryEntry("Target temperature reached: " +
                            std::to_string(currentTemp) + "°C");
            return true;
        }

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > timeout) {
            spdlog::warn(
                "Temperature timeout reached, current: {}°C, target: {}°C",
                currentTemp, targetTemperature);
            return false;
        }

        // Wait before next temperature check
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

}  // namespace lithium::task::filter