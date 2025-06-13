#include "autofocus.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include "atom/error/exception.hpp"

namespace lithium::task::focuser {

AutofocusTask::AutofocusTask(const std::string& name) : BaseFocuserTask(name) {
    setTaskType("Autofocus");
    setPriority(8);                         // High priority for autofocus
    setTimeout(std::chrono::seconds(600));  // 10 minute timeout
    addHistoryEntry("AutofocusTask initialized");
}

void AutofocusTask::execute(const json& params) {
    addHistoryEntry("Autofocus task started");
    setErrorType(TaskErrorType::None);

    auto startTime = std::chrono::steady_clock::now();

    try {
        if (!validateParams(params)) {
            setErrorType(TaskErrorType::InvalidParameter);
            THROW_INVALID_ARGUMENT("Parameter validation failed");
        }

        validateAutofocusParams(params);

        if (!setupFocuser()) {
            setErrorType(TaskErrorType::DeviceError);
            THROW_RUNTIME_ERROR("Failed to setup focuser");
        }

        // Extract parameters
        std::string modeStr = params.value("mode", "full");
        AutofocusMode mode = parseMode(modeStr);
        std::string algorithmStr = params.value("algorithm", "vcurve");
        AutofocusAlgorithm algorithm = parseAlgorithm(algorithmStr);
        
        // Set parameters based on mode
        double exposureTime = params.value("exposure_time", 0.0);
        int stepSize = params.value("step_size", 0);
        int maxSteps = params.value("max_steps", 0);
        
        // Apply mode defaults if parameters not explicitly set
        if (exposureTime <= 0 || stepSize <= 0 || maxSteps <= 0) {
            auto [defaultExp, defaultStep, defaultSteps] = getModeDefaults(mode);
            if (exposureTime <= 0) exposureTime = defaultExp;
            if (stepSize <= 0) stepSize = defaultStep;
            if (maxSteps <= 0) maxSteps = defaultSteps;
        }
        
        double tolerance = params.value("tolerance", 0.1);
        bool backlashComp = params.value("backlash_compensation", true);
        bool tempComp = params.value("temperature_compensation", false);

        addHistoryEntry("Starting autofocus with " + algorithmStr +
                        " algorithm");
        spdlog::info(
            "Autofocus parameters: algorithm={}, exposure={:.1f}s, step={}, "
            "max_steps={}",
            algorithmStr, exposureTime, stepSize, maxSteps);

        // Perform backlash compensation if enabled
        if (backlashComp) {
            addHistoryEntry("Performing backlash compensation");
            if (!performBacklashCompensation(FocuserDirection::Out, stepSize)) {
                spdlog::warn("Backlash compensation failed, continuing anyway");
            }
        }

        // Perform autofocus
        FocusCurve curve =
            performAutofocus(algorithm, exposureTime, stepSize, maxSteps);

        if (!validateFocusCurve(curve)) {
            setErrorType(TaskErrorType::SystemError);
            THROW_RUNTIME_ERROR("Focus curve validation failed");
        }

        // Move to best position
        if (!moveToPosition(curve.bestPosition)) {
            setErrorType(TaskErrorType::DeviceError);
            THROW_RUNTIME_ERROR("Failed to move to best focus position");
        }

        // Apply temperature compensation if enabled
        if (tempComp) {
            auto currentTemp = getTemperature();
            if (currentTemp) {
                int compensatedPos = applyTemperatureCompensation(
                    curve.bestPosition, *currentTemp, 20.0);

                if (compensatedPos != curve.bestPosition) {
                    addHistoryEntry("Applying temperature compensation");
                    if (!moveToPosition(compensatedPos)) {
                        spdlog::warn("Temperature compensation move failed");
                    }
                }
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        addHistoryEntry("Autofocus completed successfully");
        spdlog::info(
            "Autofocus completed in {} ms. Best position: {}, Confidence: "
            "{:.2f}",
            duration.count(), curve.bestPosition, curve.confidence);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        addHistoryEntry("Autofocus failed: " + std::string(e.what()));

        if (getErrorType() == TaskErrorType::None) {
            setErrorType(TaskErrorType::SystemError);
        }

        spdlog::error("Autofocus task failed after {} ms: {}", duration.count(),
                      e.what());
        throw;
    }
}

FocusCurve AutofocusTask::performAutofocus(AutofocusAlgorithm algorithm,
                                           double exposureTime, int stepSize,
                                           int maxSteps) {
    addHistoryEntry("Starting autofocus sequence");

    auto startPos = getCurrentPosition();
    if (!startPos) {
        THROW_RUNTIME_ERROR("Cannot get starting position");
    }

    // Perform coarse sweep
    addHistoryEntry("Performing coarse focus sweep");
    std::vector<FocusPosition> coarsePositions =
        performCoarseSweep(*startPos, stepSize, maxSteps * 2, exposureTime);

    if (coarsePositions.empty()) {
        THROW_RUNTIME_ERROR("Coarse sweep failed - no positions measured");
    }

    // Find approximate best position from coarse sweep
    auto bestCoarse =
        std::min_element(coarsePositions.begin(), coarsePositions.end(),
                         [](const FocusPosition& a, const FocusPosition& b) {
                             return a.metrics.hfr < b.metrics.hfr;
                         });

    // Perform fine focus around best coarse position
    addHistoryEntry("Performing fine focus");
    std::vector<FocusPosition> finePositions =
        performFineFocus(bestCoarse->position, stepSize / 5, 10, exposureTime);

    // Combine all positions
    std::vector<FocusPosition> allPositions = coarsePositions;
    allPositions.insert(allPositions.end(), finePositions.begin(),
                        finePositions.end());

    // Analyze focus curve
    FocusCurve curve = analyzeFocusCurve(allPositions, algorithm);

    addHistoryEntry("Focus curve analysis completed");
    return curve;
}

std::vector<FocusPosition> AutofocusTask::performCoarseSweep(
    int startPos, int stepSize, int numSteps, double exposureTime) {
    std::vector<FocusPosition> positions;

    int halfSteps = numSteps / 2;

    for (int i = -halfSteps; i <= halfSteps;
         i += 2) {  // Skip every other position for speed
        int targetPos = startPos + (i * stepSize);

        if (!moveToPosition(targetPos)) {
            spdlog::warn("Failed to move to position {}, skipping", targetPos);
            continue;
        }

        FocusMetrics metrics = analyzeFocusQuality(exposureTime);

        FocusPosition focusPos;
        focusPos.position = targetPos;
        focusPos.metrics = metrics;
        focusPos.temperature = getTemperature().value_or(20.0);
        focusPos.timestamp = std::to_string(std::time(nullptr));

        positions.push_back(focusPos);

        spdlog::info("Coarse position {}: HFR={:.2f}, Stars={}", targetPos,
                     metrics.hfr, metrics.starCount);
    }

    return positions;
}

std::vector<FocusPosition> AutofocusTask::performFineFocus(
    int centerPos, int stepSize, int numSteps, double exposureTime) {
    std::vector<FocusPosition> positions;

    for (int i = -numSteps; i <= numSteps; ++i) {
        int targetPos = centerPos + (i * stepSize);

        if (!moveToPosition(targetPos)) {
            spdlog::warn("Failed to move to fine position {}, skipping",
                         targetPos);
            continue;
        }

        FocusMetrics metrics = analyzeFocusQuality(exposureTime);

        FocusPosition focusPos;
        focusPos.position = targetPos;
        focusPos.metrics = metrics;
        focusPos.temperature = getTemperature().value_or(20.0);
        focusPos.timestamp = std::to_string(std::time(nullptr));

        positions.push_back(focusPos);

        spdlog::info("Fine position {}: HFR={:.2f}, Stars={}", targetPos,
                     metrics.hfr, metrics.starCount);
    }

    return positions;
}

FocusCurve AutofocusTask::analyzeFocusCurve(
    const std::vector<FocusPosition>& positions, AutofocusAlgorithm algorithm) {
    FocusCurve curve;
    curve.positions = positions;

    switch (algorithm) {
        case AutofocusAlgorithm::VCurve: {
            auto [bestPos, confidence] = findBestPositionVCurve(positions);
            curve.bestPosition = bestPos;
            curve.confidence = confidence;
            curve.algorithm = "V-Curve";
            break;
        }
        case AutofocusAlgorithm::HyperbolicFit: {
            auto [bestPos, confidence] = findBestPositionHyperbolic(positions);
            curve.bestPosition = bestPos;
            curve.confidence = confidence;
            curve.algorithm = "Hyperbolic";
            break;
        }
        default: {
            // Simple minimum HFR
            auto bestIt = std::min_element(
                positions.begin(), positions.end(),
                [](const FocusPosition& a, const FocusPosition& b) {
                    return a.metrics.hfr < b.metrics.hfr;
                });

            curve.bestPosition = bestIt->position;
            curve.confidence = 0.8;  // Default confidence
            curve.algorithm = "Simple";
            break;
        }
    }

    return curve;
}

std::unique_ptr<Task> AutofocusTask::createEnhancedTask() {
    auto task = std::make_unique<Task>("Autofocus", [](const json& params) {
        try {
            AutofocusTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced Autofocus task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);
    task->setTimeout(std::chrono::seconds(600));
    task->setLogLevel(2);
    task->setTaskType("Autofocus");

    return task;
}

void AutofocusTask::defineParameters(Task& task) {
    task.addParamDefinition(
        "mode", "string", false, "full",
        "Autofocus mode: full, quick, fine, starless, high_precision");
    task.addParamDefinition(
        "algorithm", "string", false, "vcurve",
        "Autofocus algorithm: vcurve, hyperbolic, polynomial, simple");
    task.addParamDefinition("exposure_time", "double", false, 0.0,
                            "Exposure time for focus frames in seconds (0=auto)");
    task.addParamDefinition("step_size", "int", false, 0,
                            "Step size between focus positions (0=auto)");
    task.addParamDefinition("max_steps", "int", false, 0,
                            "Maximum number of steps from center position (0=auto)");
    task.addParamDefinition("tolerance", "double", false, 0.1,
                            "Focus tolerance for convergence");
    task.addParamDefinition("binning", "int", false, 1,
                            "Camera binning factor");
    task.addParamDefinition("backlash_compensation", "bool", false, true,
                            "Enable backlash compensation");
    task.addParamDefinition("temperature_compensation", "bool", false, false,
                            "Enable temperature compensation");
    task.addParamDefinition("min_stars", "int", false, 5,
                            "Minimum stars required for analysis");
    task.addParamDefinition("max_iterations", "int", false, 3,
                            "Max iterations for high precision mode");
}

void AutofocusTask::validateAutofocusParams(const json& params) {
    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"].get<double>();
        if (exposure <= 0 || exposure > 300) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be between 0 and 300 seconds");
        }
    }

    if (params.contains("step_size")) {
        int stepSize = params["step_size"].get<int>();
        if (stepSize < 1 || stepSize > 5000) {
            THROW_INVALID_ARGUMENT("Step size must be between 1 and 5000");
        }
    }

    if (params.contains("max_steps")) {
        int maxSteps = params["max_steps"].get<int>();
        if (maxSteps < 5 || maxSteps > 100) {
            THROW_INVALID_ARGUMENT("Max steps must be between 5 and 100");
        }
    }
}

AutofocusAlgorithm AutofocusTask::parseAlgorithm(
    const std::string& algorithmStr) {
    if (algorithmStr == "vcurve")
        return AutofocusAlgorithm::VCurve;
    if (algorithmStr == "hyperbolic")
        return AutofocusAlgorithm::HyperbolicFit;
    if (algorithmStr == "polynomial")
        return AutofocusAlgorithm::Polynomial;
    if (algorithmStr == "simple")
        return AutofocusAlgorithm::SimpleSweep;

    spdlog::warn("Unknown algorithm '{}', defaulting to vcurve", algorithmStr);
    return AutofocusAlgorithm::VCurve;
}

AutofocusMode AutofocusTask::parseMode(const std::string& modeStr) {
    if (modeStr == "full") return AutofocusMode::Full;
    if (modeStr == "quick") return AutofocusMode::Quick;
    if (modeStr == "fine") return AutofocusMode::Fine;
    if (modeStr == "starless") return AutofocusMode::Starless;
    if (modeStr == "high_precision") return AutofocusMode::HighPrecision;
    
    spdlog::warn("Unknown mode '{}', defaulting to full", modeStr);
    return AutofocusMode::Full;
}

std::tuple<double, int, int> AutofocusTask::getModeDefaults(AutofocusMode mode) {
    switch (mode) {
        case AutofocusMode::Quick:
            return {1.0, 150, 15};  // Faster exposure, larger steps, fewer steps
        case AutofocusMode::Fine:
            return {2.0, 30, 10};   // Smaller steps around current position
        case AutofocusMode::Starless:
            return {0.5, 200, 20};  // Short exposures for planetary/lunar
        case AutofocusMode::HighPrecision:
            return {3.0, 50, 15};   // More precise measurements
        case AutofocusMode::Full:
        default:
            return {2.0, 100, 25};  // Default balanced settings
    }
}

std::pair<int, double> AutofocusTask::findBestPositionVCurve(
    const std::vector<FocusPosition>& positions) {
    if (positions.size() < 3) {
        return {positions[0].position, 0.5};
    }

    // Find minimum HFR position as starting point
    auto minIt =
        std::min_element(positions.begin(), positions.end(),
                         [](const FocusPosition& a, const FocusPosition& b) {
                             return a.metrics.hfr < b.metrics.hfr;
                         });

    // Simple V-curve analysis - look for positions around minimum
    double confidence = 0.9;  // High confidence for V-curve

    // Check if we have a good V-shape by looking at neighbors
    if (minIt != positions.begin() && minIt != positions.end() - 1) {
        auto prevIt = minIt - 1;
        auto nextIt = minIt + 1;

        if (prevIt->metrics.hfr > minIt->metrics.hfr &&
            nextIt->metrics.hfr > minIt->metrics.hfr) {
            confidence = 0.95;  // Very high confidence - clear V-shape
        }
    }

    return {minIt->position, confidence};
}

std::pair<int, double> AutofocusTask::findBestPositionHyperbolic(
    const std::vector<FocusPosition>& positions) {
    // Simplified hyperbolic fitting - in a real implementation this would
    // use proper curve fitting algorithms
    auto minIt =
        std::min_element(positions.begin(), positions.end(),
                         [](const FocusPosition& a, const FocusPosition& b) {
                             return a.metrics.hfr < b.metrics.hfr;
                         });

    return {minIt->position, 0.85};  // Good confidence for hyperbolic fit
}

bool AutofocusTask::validateFocusCurve(const FocusCurve& curve) {
    if (curve.positions.empty()) {
        spdlog::error("Focus curve has no positions");
        return false;
    }

    if (curve.confidence < 0.5) {
        spdlog::error("Focus curve confidence too low: {:.2f}",
                      curve.confidence);
        return false;
    }

    // Check if best position is reasonable
    auto limits = getFocuserLimits();
    if (curve.bestPosition < limits.first ||
        curve.bestPosition > limits.second) {
        spdlog::error("Best focus position {} is out of range",
                      curve.bestPosition);
        return false;
    }

    return true;
}

int AutofocusTask::applyTemperatureCompensation(int basePosition,
                                                double currentTemp,
                                                double referenceTemp) {
    int compensation =
        calculateTemperatureCompensation(currentTemp, referenceTemp);
    return basePosition + compensation;
}

}  // namespace lithium::task::focuser
