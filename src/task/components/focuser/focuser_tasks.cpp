/**
 * @file focuser_tasks.cpp
 * @brief Implementation of focuser tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "focuser_tasks.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

namespace lithium::task::focuser {

// ============================================================================
// AutoFocusTask Implementation
// ============================================================================

void AutoFocusTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 3.0, "Focus exposure time");
    addParamDefinition("step_size", "integer", false, 100, "Focuser step size");
    addParamDefinition("max_steps", "integer", false, 15,
                       "Maximum number of steps");
    addParamDefinition("method", "string", false, "hfd",
                       "Focus method (hfd/fwhm/contrast)");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
    addParamDefinition("initial_position", "integer", false, -1,
                       "Initial focuser position (-1=current)");
    addParamDefinition("backlash_comp", "integer", false, 0,
                       "Backlash compensation");
}

void AutoFocusTask::executeImpl(const json& params) {
    int stepSize = params.value("step_size", 100);
    int maxSteps = params.value("max_steps", 15);
    int initialPos = params.value("initial_position", -1);

    logProgress("Starting autofocus with " + std::to_string(maxSteps) +
                " steps");

    // Get current position if not specified
    int currentPos = (initialPos >= 0) ? initialPos : 50000;  // Default center

    // V-curve focus routine
    FocusResult result = findBestFocus(currentPos, stepSize, maxSteps, params);

    if (result.success) {
        logProgress("Best focus at position " +
                    std::to_string(result.position) + " with metric " +
                    std::to_string(result.metric));

        // Move to best position
        logProgress("Moving to optimal focus position");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        logProgress("Autofocus complete", 1.0);
    } else {
        throw std::runtime_error("Autofocus failed to find optimal position");
    }
}

FocusResult AutoFocusTask::findBestFocus(int startPos, int stepSize,
                                         int numSteps, const json& params) {
    FocusResult best;
    best.metric =
        std::numeric_limits<double>::max();  // Looking for minimum HFD

    std::vector<std::pair<int, double>> measurements;
    int halfSteps = numSteps / 2;

    // Move to start position
    int pos = startPos - (halfSteps * stepSize);

    for (int i = 0; i < numSteps; ++i) {
        if (!shouldContinue()) {
            logProgress("Autofocus cancelled");
            return best;
        }

        double progress = static_cast<double>(i) / numSteps;
        logProgress("Focus step " + std::to_string(i + 1) + "/" +
                        std::to_string(numSteps) + " at position " +
                        std::to_string(pos),
                    progress);

        double metric = measureFocusMetric(pos, params);
        measurements.push_back({pos, metric});

        if (metric < best.metric) {
            best.metric = metric;
            best.position = pos;
            best.success = true;
        }

        pos += stepSize;
    }

    // Optionally refine with parabola fitting
    if (measurements.size() >= 3) {
        logProgress("Refining focus with curve fitting");
        // Simple minimum finding (real implementation would use parabola fit)
    }

    return best;
}

double AutoFocusTask::measureFocusMetric(int position, const json& params) {
    // Simulate taking a focus exposure
    double exposure = params.value("exposure", 3.0);

    // Simulate exposure time
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(exposure * 100)));

    // Simulate HFD measurement (V-curve shape simulation)
    int optimalPos = 50000;
    double distance = std::abs(position - optimalPos);
    double hfd = 2.0 + (distance / 5000.0) * (distance / 5000.0);

    logProgress("Position " + std::to_string(position) +
                " HFD: " + std::to_string(hfd));
    return hfd;
}

// ============================================================================
// FocusSeriesTask Implementation
// ============================================================================

void FocusSeriesTask::setupParameters() {
    addParamDefinition("start_position", "integer", true, nullptr,
                       "Start focuser position");
    addParamDefinition("end_position", "integer", true, nullptr,
                       "End focuser position");
    addParamDefinition("step_size", "integer", true, nullptr,
                       "Step size between positions");
    addParamDefinition("exposure", "number", false, 3.0,
                       "Exposure time per frame");
    addParamDefinition("binning_x", "integer", false, 1, "Binning X");
    addParamDefinition("binning_y", "integer", false, 1, "Binning Y");
    addParamDefinition("gain", "integer", false, 100, "Camera gain");
}

void FocusSeriesTask::executeImpl(const json& params) {
    auto startPosResult = ParamValidator::required(params, "start_position");
    auto endPosResult = ParamValidator::required(params, "end_position");
    auto stepResult = ParamValidator::required(params, "step_size");

    if (!startPosResult || !endPosResult || !stepResult) {
        throw std::invalid_argument("Missing required parameters");
    }

    int startPos = params["start_position"].get<int>();
    int endPos = params["end_position"].get<int>();
    int stepSize = params["step_size"].get<int>();
    double exposure = params.value("exposure", 3.0);

    int numSteps = std::abs(endPos - startPos) / stepSize + 1;
    int direction = (endPos > startPos) ? 1 : -1;

    logProgress("Starting focus series: " + std::to_string(startPos) + " to " +
                std::to_string(endPos) + " in " + std::to_string(numSteps) +
                " steps");

    int pos = startPos;
    for (int i = 0; i < numSteps; ++i) {
        if (!shouldContinue()) {
            logProgress("Focus series cancelled");
            return;
        }

        double progress = static_cast<double>(i) / numSteps;
        logProgress("Position " + std::to_string(pos), progress);

        // Move focuser (simulated)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Take exposure (simulated)
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(exposure * 100)));

        pos += direction * stepSize;
    }

    logProgress("Focus series complete", 1.0);
}

// ============================================================================
// TemperatureFocusTask Implementation
// ============================================================================

void TemperatureFocusTask::setupParameters() {
    addParamDefinition("coefficient", "number", false, -1.5,
                       "Steps per degree C");
    addParamDefinition("reference_temp", "number", false, 20.0,
                       "Reference temperature");
    addParamDefinition("reference_position", "integer", false, 50000,
                       "Reference focus position");
    addParamDefinition("current_temp", "number", false, nullptr,
                       "Current temperature (auto if not set)");
    addParamDefinition("max_adjustment", "integer", false, 500,
                       "Maximum position adjustment");
}

void TemperatureFocusTask::executeImpl(const json& params) {
    double coefficient = params.value("coefficient", -1.5);
    double refTemp = params.value("reference_temp", 20.0);
    int refPosition = params.value("reference_position", 50000);
    int maxAdjust = params.value("max_adjustment", 500);

    // Get current temperature (simulated)
    double currentTemp = params.contains("current_temp")
                             ? params["current_temp"].get<double>()
                             : 15.0;  // Simulated reading

    logProgress("Current temperature: " + std::to_string(currentTemp) + "°C");
    logProgress("Reference: " + std::to_string(refTemp) + "°C at position " +
                std::to_string(refPosition));

    int compensation = calculateCompensation(currentTemp, refTemp, coefficient);
    compensation = std::clamp(compensation, -maxAdjust, maxAdjust);

    int targetPosition = refPosition + compensation;

    logProgress("Temperature delta: " + std::to_string(currentTemp - refTemp) +
                "°C");
    logProgress("Compensation: " + std::to_string(compensation) + " steps");
    logProgress("Moving to position " + std::to_string(targetPosition));

    // Move focuser (simulated)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    logProgress("Temperature focus compensation complete", 1.0);
}

int TemperatureFocusTask::calculateCompensation(double currentTemp,
                                                double referenceTemp,
                                                double coefficient) {
    double delta = currentTemp - referenceTemp;
    return static_cast<int>(delta * coefficient);
}

// ============================================================================
// MoveFocuserTask Implementation
// ============================================================================

void MoveFocuserTask::setupParameters() {
    addParamDefinition("position", "integer", true, nullptr,
                       "Target absolute position");
    addParamDefinition("speed", "integer", false, 100,
                       "Movement speed (1-100%)");
}

void MoveFocuserTask::executeImpl(const json& params) {
    auto posResult = ParamValidator::required(params, "position");
    if (!posResult) {
        throw std::invalid_argument("Position is required");
    }

    int targetPos = params["position"].get<int>();
    int speed = params.value("speed", 100);

    logProgress("Moving focuser to position " + std::to_string(targetPos));

    // Simulate movement time based on distance and speed
    int moveTime = 1000 * 100 / speed;
    std::this_thread::sleep_for(std::chrono::milliseconds(moveTime));

    logProgress("Focuser at position " + std::to_string(targetPos), 1.0);
}

// ============================================================================
// MoveFocuserRelativeTask Implementation
// ============================================================================

void MoveFocuserRelativeTask::setupParameters() {
    addParamDefinition("steps", "integer", true, nullptr,
                       "Relative steps (positive=out, negative=in)");
    addParamDefinition("speed", "integer", false, 100,
                       "Movement speed (1-100%)");
}

void MoveFocuserRelativeTask::executeImpl(const json& params) {
    auto stepsResult = ParamValidator::required(params, "steps");
    if (!stepsResult) {
        throw std::invalid_argument("Steps is required");
    }

    int steps = params["steps"].get<int>();
    int speed = params.value("speed", 100);

    std::string direction = (steps >= 0) ? "out" : "in";
    logProgress("Moving focuser " + direction + " by " +
                std::to_string(std::abs(steps)) + " steps");

    // Simulate movement time
    int moveTime = std::abs(steps) / 10 * 100 / speed;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(std::max(moveTime, 100)));

    logProgress("Relative move complete", 1.0);
}

}  // namespace lithium::task::focuser
