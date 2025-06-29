#include "focus_optimization_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <cmath>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto FocusOptimizationTask::taskName() -> std::string { return "FocusOptimization"; }

void FocusOptimizationTask::execute(const json& params) { executeImpl(params); }

void FocusOptimizationTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing FocusOptimization task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string focusMode = params.value("focus_mode", "initial");
        std::string algorithm = params.value("algorithm", "hfr");
        int stepSize = params.value("step_size", 100);
        int maxSteps = params.value("max_steps", 20);
        double tolerancePercent = params.value("tolerance_percent", 5.0);
        bool temperatureCompensation = params.value("temperature_compensation", true);
        double tempCoefficient = params.value("temp_coefficient", -2.0);
        double monitorInterval = params.value("monitor_interval_minutes", 30.0);
        bool continuousMonitoring = params.value("continuous_monitoring", false);
        double targetHFR = params.value("target_hfr", 2.5);
        int sampleCount = params.value("sample_count", 3);

        LOG_F(INFO, "Starting focus optimization - Mode: {}, Algorithm: {}, Target HFR: {:.2f}",
              focusMode, algorithm, targetHFR);

        if (focusMode == "initial") {
            performInitialFocus();

        } else if (focusMode == "periodic") {
            performPeriodicFocus();

        } else if (focusMode == "temperature_compensation") {
            performTemperatureCompensation();

        } else if (focusMode == "continuous") {
            startContinuousMonitoring(monitorInterval);

        } else {
            THROW_INVALID_ARGUMENT("Invalid focus mode: " + focusMode);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(INFO, "FocusOptimization task '{}' ({}) completed in {} minutes",
              getName(), focusMode, duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "FocusOptimization task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void FocusOptimizationTask::performInitialFocus() {
    LOG_F(INFO, "Performing initial focus optimization");

    // Step 1: Rough focus to get in the ballpark
    LOG_F(INFO, "Step 1: Rough focus sweep");

    // Move to starting position (simulate)
    LOG_F(INFO, "Moving focuser to starting position");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Perform coarse sweep
    double bestPosition = 5000; // Simulate optimal position
    double bestHFR = 999.9;

    for (int step = 0; step < 10; ++step) {
        LOG_F(INFO, "Coarse focus step {} - Position: {}",
              step + 1, 4000 + step * 200);

        // Take test exposure
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Measure HFR (simulated)
        double currentHFR = 5.0 - std::abs(step - 5) * 0.5 + (rand() % 100) / 1000.0;

        LOG_F(INFO, "Measured HFR: {:.3f}", currentHFR);

        if (currentHFR < bestHFR) {
            bestHFR = currentHFR;
            bestPosition = 4000 + step * 200;
        }
    }

    LOG_F(INFO, "Coarse focus completed - Best position: {:.0f}, HFR: {:.3f}",
          bestPosition, bestHFR);

    // Step 2: Fine focus around best position
    LOG_F(INFO, "Step 2: Fine focus optimization");
    buildFocusCurve();
    findOptimalFocus();

    LOG_F(INFO, "Initial focus optimization completed");
}

void FocusOptimizationTask::performPeriodicFocus() {
    LOG_F(INFO, "Performing periodic focus check");

    // Check current focus quality
    double currentHFR = measureFocusQuality();
    LOG_F(INFO, "Current focus HFR: {:.3f}", currentHFR);

    // Check if refocus is needed
    double targetHFR = 2.5; // Should come from parameters
    double tolerance = 0.3;

    if (currentHFR > targetHFR + tolerance) {
        LOG_F(INFO, "Focus drift detected (HFR: {:.3f} > {:.3f}), performing refocus",
              currentHFR, targetHFR + tolerance);

        buildFocusCurve();
        findOptimalFocus();

        // Verify focus improvement
        double newHFR = measureFocusQuality();
        LOG_F(INFO, "Focus optimization result - Old HFR: {:.3f}, New HFR: {:.3f}",
              currentHFR, newHFR);
    } else {
        LOG_F(INFO, "Focus is within tolerance, no adjustment needed");
    }
}

void FocusOptimizationTask::performTemperatureCompensation() {
    LOG_F(INFO, "Performing temperature compensation");

    // Get current temperature (simulated)
    double currentTemp = 15.0 + (rand() % 20) - 10; // -5 to 25°C
    static double lastTemp = currentTemp;

    double tempChange = currentTemp - lastTemp;
    LOG_F(INFO, "Temperature change: {:.2f}°C (from {:.1f}°C to {:.1f}°C)",
          tempChange, lastTemp, currentTemp);

    if (std::abs(tempChange) > 2.0) { // Threshold for compensation
        // Calculate focus adjustment
        double tempCoeff = -2.0; // steps per degree (from params)
        int focusAdjustment = static_cast<int>(tempChange * tempCoeff);

        LOG_F(INFO, "Applying temperature compensation: {} steps", focusAdjustment);

        // Apply focus adjustment (simulated)
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Verify focus after compensation
        double newHFR = measureFocusQuality();
        LOG_F(INFO, "Focus after temperature compensation: {:.3f} HFR", newHFR);

        lastTemp = currentTemp;
    } else {
        LOG_F(INFO, "Temperature change too small for compensation");
    }
}

double FocusOptimizationTask::measureFocusQuality() {
    LOG_F(INFO, "Measuring focus quality");

    // Take multiple samples for accuracy
    double totalHFR = 0.0;
    int sampleCount = 3;

    for (int i = 0; i < sampleCount; ++i) {
        LOG_F(INFO, "Taking focus measurement {} of {}", i + 1, sampleCount);

        // Simulate exposure and HFR calculation
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Simulate HFR measurement with some noise
        double hfr = 2.2 + (rand() % 100) / 500.0; // 2.2 to 2.4
        totalHFR += hfr;

        LOG_F(INFO, "Sample {} HFR: {:.3f}", i + 1, hfr);
    }

    double avgHFR = totalHFR / sampleCount;
    LOG_F(INFO, "Average HFR: {:.3f}", avgHFR);

    return avgHFR;
}

void FocusOptimizationTask::buildFocusCurve() {
    LOG_F(INFO, "Building focus curve");

    // Fine focus sweep around current position
    std::vector<std::pair<int, double>> focusCurve;

    for (int step = -5; step <= 5; ++step) {
        int position = 5000 + step * 50; // Simulate positions

        LOG_F(INFO, "Focus curve point {} - Position: {}", step + 6, position);

        // Move focuser
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Take measurement
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Simulate V-curve with minimum at step 0
        double hfr = 2.0 + std::abs(step) * 0.1 + (rand() % 50) / 1000.0;
        focusCurve.push_back({position, hfr});

        LOG_F(INFO, "Position: {}, HFR: {:.3f}", position, hfr);
    }

    LOG_F(INFO, "Focus curve completed with {} points", focusCurve.size());
}

void FocusOptimizationTask::findOptimalFocus() {
    LOG_F(INFO, "Finding optimal focus position");

    // In real implementation, this would analyze the focus curve
    // and find the minimum HFR position using curve fitting

    // Simulate finding optimal position
    int optimalPosition = 5000; // Simulate result

    LOG_F(INFO, "Moving to optimal focus position: {}", optimalPosition);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify final focus
    double finalHFR = measureFocusQuality();
    LOG_F(INFO, "Optimal focus achieved - Position: {}, HFR: {:.3f}",
          optimalPosition, finalHFR);
}

bool FocusOptimizationTask::checkFocusDrift() {
    LOG_F(INFO, "Checking for focus drift");

    double currentHFR = measureFocusQuality();
    double targetHFR = 2.5; // Should come from stored value
    double tolerance = 0.2;

    bool driftDetected = currentHFR > (targetHFR + tolerance);

    LOG_F(INFO, "Focus drift check - Current: {:.3f}, Target: {:.3f}, Drift: {}",
          currentHFR, targetHFR, driftDetected ? "YES" : "NO");

    return driftDetected;
}

void FocusOptimizationTask::startContinuousMonitoring(double intervalMinutes) {
    LOG_F(INFO, "Starting continuous focus monitoring with {:.1f} minute intervals",
          intervalMinutes);

    // Simulate continuous monitoring for demonstration
    for (int cycle = 1; cycle <= 5; ++cycle) {
        LOG_F(INFO, "Focus monitoring cycle {}", cycle);

        if (checkFocusDrift()) {
            LOG_F(INFO, "Focus drift detected, performing correction");
            buildFocusCurve();
            findOptimalFocus();
        }

        // Wait for next monitoring cycle
        if (cycle < 5) { // Don't wait after last cycle
            LOG_F(INFO, "Waiting {:.1f} minutes until next focus check", intervalMinutes);
            std::this_thread::sleep_for(
                std::chrono::minutes(static_cast<int>(intervalMinutes)));
        }
    }

    LOG_F(INFO, "Continuous focus monitoring completed");
}

void FocusOptimizationTask::validateFocusOptimizationParameters(const json& params) {
    if (params.contains("focus_mode")) {
        std::string mode = params["focus_mode"].get<std::string>();
        if (mode != "initial" && mode != "periodic" &&
            mode != "temperature_compensation" && mode != "continuous") {
            THROW_INVALID_ARGUMENT("Invalid focus mode: " + mode);
        }
    }

    if (params.contains("step_size")) {
        int stepSize = params["step_size"].get<int>();
        if (stepSize <= 0 || stepSize > 1000) {
            THROW_INVALID_ARGUMENT("Step size must be between 1 and 1000");
        }
    }

    if (params.contains("max_steps")) {
        int maxSteps = params["max_steps"].get<int>();
        if (maxSteps <= 0 || maxSteps > 100) {
            THROW_INVALID_ARGUMENT("Max steps must be between 1 and 100");
        }
    }

    if (params.contains("target_hfr")) {
        double targetHFR = params["target_hfr"].get<double>();
        if (targetHFR <= 0 || targetHFR > 10) {
            THROW_INVALID_ARGUMENT("Target HFR must be between 0 and 10");
        }
    }
}

auto FocusOptimizationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<FocusOptimizationTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced FocusOptimization task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void FocusOptimizationTask::defineParameters(Task& task) {
    task.addParamDefinition("focus_mode", "string", false, "initial",
                            "Focus mode: initial, periodic, temperature_compensation, continuous");
    task.addParamDefinition("algorithm", "string", false, "hfr",
                            "Focus algorithm: hfr, fwhm, star_count");
    task.addParamDefinition("step_size", "int", false, 100,
                            "Focus step size");
    task.addParamDefinition("max_steps", "int", false, 20,
                            "Maximum number of focus steps");
    task.addParamDefinition("tolerance_percent", "double", false, 5.0,
                            "Focus tolerance percentage");
    task.addParamDefinition("temperature_compensation", "bool", false, true,
                            "Enable temperature compensation");
    task.addParamDefinition("temp_coefficient", "double", false, -2.0,
                            "Temperature coefficient (steps per degree)");
    task.addParamDefinition("monitor_interval_minutes", "double", false, 30.0,
                            "Monitoring interval in minutes");
    task.addParamDefinition("continuous_monitoring", "bool", false, false,
                            "Enable continuous monitoring");
    task.addParamDefinition("target_hfr", "double", false, 2.5,
                            "Target HFR value");
    task.addParamDefinition("sample_count", "int", false, 3,
                            "Number of samples per measurement");
}

}  // namespace lithium::task::task
