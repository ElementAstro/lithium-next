/**
 * @file astrometry_tasks.cpp
 * @brief Implementation of astrometry tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "astrometry_tasks.hpp"
#include <cmath>
#include <thread>

namespace lithium::task::astrometry {

// Constants
constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;
constexpr double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

// ============================================================================
// PlateSolveTask Implementation
// ============================================================================

void PlateSolveTask::setupParameters() {
    addParamDefinition("image_path", "string", true, nullptr,
                       "Path to image file");
    addParamDefinition("ra_hint", "number", false, nullptr,
                       "RA hint in degrees");
    addParamDefinition("dec_hint", "number", false, nullptr,
                       "Dec hint in degrees");
    addParamDefinition("radius", "number", false, 10.0,
                       "Search radius in degrees");
    addParamDefinition("downsample", "integer", false, 2,
                       "Image downsample factor");
    addParamDefinition("timeout", "integer", false, 120,
                       "Solve timeout in seconds");
    addParamDefinition("solver", "string", false, "astrometry",
                       "Solver to use");
}

void PlateSolveTask::executeImpl(const json& params) {
    auto pathResult = ParamValidator::required(params, "image_path");
    if (!pathResult) {
        throw std::invalid_argument("Image path is required");
    }

    std::string imagePath = params["image_path"].get<std::string>();
    int timeout = params.value("timeout", 120);

    logProgress("Starting plate solve for: " + imagePath);

    json hints;
    if (params.contains("ra_hint") && params.contains("dec_hint")) {
        hints["ra"] = params["ra_hint"];
        hints["dec"] = params["dec_hint"];
        hints["radius"] = params.value("radius", 10.0);
        logProgress("Using coordinate hints");
    } else {
        logProgress("No hints provided, using blind solve");
    }

    PlateSolveResult result = solve(imagePath, hints);

    if (result.success) {
        logProgress("Plate solve successful");
        logProgress("RA: " + std::to_string(result.ra) +
                    "°, Dec: " + std::to_string(result.dec) + "°");
        logProgress("Rotation: " + std::to_string(result.rotation) + "°");
        logProgress("Pixel scale: " + std::to_string(result.pixelScale) +
                    " arcsec/pixel");
        logProgress(
            "Solve time: " + std::to_string(result.solveTime) + " seconds",
            1.0);
    } else {
        throw std::runtime_error("Plate solve failed");
    }
}

PlateSolveResult PlateSolveTask::solve(const std::string& imagePath,
                                       const json& hints) {
    // Simulate plate solving
    std::this_thread::sleep_for(std::chrono::seconds(2));

    PlateSolveResult result;
    result.success = true;
    result.ra = hints.contains("ra") ? hints["ra"].get<double>() : 180.0;
    result.dec = hints.contains("dec") ? hints["dec"].get<double>() : 45.0;
    result.rotation = 12.5;
    result.pixelScale = 1.2;
    result.fovWidth = 2.0;
    result.fovHeight = 1.5;
    result.solver = "astrometry";
    result.solveTime = 2.0;

    return result;
}

// ============================================================================
// PlateSolveExposureTask Implementation
// ============================================================================

void PlateSolveExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 5.0, "Solve exposure time");
    addParamDefinition("binning_x", "integer", false, 2, "Binning X for solve");
    addParamDefinition("binning_y", "integer", false, 2, "Binning Y for solve");
    addParamDefinition("gain", "integer", false, 200,
                       "Gain for solve exposure");
    addParamDefinition("ra_hint", "number", false, nullptr,
                       "RA hint in degrees");
    addParamDefinition("dec_hint", "number", false, nullptr,
                       "Dec hint in degrees");
    addParamDefinition("radius", "number", false, 5.0,
                       "Search radius in degrees");
    addParamDefinition("timeout", "integer", false, 60,
                       "Solve timeout in seconds");
}

void PlateSolveExposureTask::executeImpl(const json& params) {
    double exposure = params.value("exposure", 5.0);

    logProgress("Taking solve exposure: " + std::to_string(exposure) + "s");

    // Simulate taking exposure
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(exposure * 200)));

    logProgress("Exposure complete, starting plate solve", 0.5);

    // Simulate plate solving
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Simulated result
    double ra =
        params.contains("ra_hint") ? params["ra_hint"].get<double>() : 180.0;
    double dec =
        params.contains("dec_hint") ? params["dec_hint"].get<double>() : 45.0;

    logProgress("Solve successful: RA=" + std::to_string(ra) +
                    "°, Dec=" + std::to_string(dec) + "°",
                1.0);
}

// ============================================================================
// CenteringTask Implementation
// ============================================================================

void CenteringTask::setupParameters() {
    addParamDefinition("target_ra", "number", true, nullptr,
                       "Target RA in degrees");
    addParamDefinition("target_dec", "number", true, nullptr,
                       "Target Dec in degrees");
    addParamDefinition("tolerance", "number", false, 30.0,
                       "Centering tolerance in arcsec");
    addParamDefinition("max_iterations", "integer", false, 5,
                       "Maximum centering iterations");
    addParamDefinition("exposure", "number", false, 5.0, "Solve exposure time");
    addParamDefinition("timeout", "integer", false, 300,
                       "Total timeout in seconds");
}

void CenteringTask::executeImpl(const json& params) {
    auto raResult = ParamValidator::required(params, "target_ra");
    auto decResult = ParamValidator::required(params, "target_dec");

    if (!raResult || !decResult) {
        throw std::invalid_argument("Target RA and Dec are required");
    }

    double targetRA = params["target_ra"].get<double>();
    double targetDec = params["target_dec"].get<double>();
    double tolerance =
        params.value("tolerance", 30.0) / 3600.0;  // Convert arcsec to degrees
    int maxIterations = params.value("max_iterations", 5);

    logProgress("Centering on RA=" + std::to_string(targetRA) +
                "°, Dec=" + std::to_string(targetDec) + "°");

    for (int i = 0; i < maxIterations; ++i) {
        if (!shouldContinue()) {
            logProgress("Centering cancelled");
            return;
        }

        double progress = static_cast<double>(i) / maxIterations;
        logProgress("Centering iteration " + std::to_string(i + 1) + "/" +
                        std::to_string(maxIterations),
                    progress);

        // Simulate plate solve
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Simulate current position (converging to target)
        double factor = 1.0 / (i + 2);
        double currentRA = targetRA + factor * 0.1;
        double currentDec = targetDec + factor * 0.05;

        double separation =
            calculateSeparation(targetRA, targetDec, currentRA, currentDec);
        logProgress("Current separation: " + std::to_string(separation * 3600) +
                    " arcsec");

        if (separation < tolerance) {
            logProgress("Target centered within tolerance", 1.0);
            return;
        }

        // Simulate slew correction
        logProgress("Applying correction slew");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    throw std::runtime_error("Failed to center within maximum iterations");
}

double CenteringTask::calculateSeparation(double ra1, double dec1, double ra2,
                                          double dec2) {
    // Haversine formula for angular separation
    double dRA = (ra2 - ra1) * DEG_TO_RAD;
    double dDec = (dec2 - dec1) * DEG_TO_RAD;
    double dec1Rad = dec1 * DEG_TO_RAD;
    double dec2Rad = dec2 * DEG_TO_RAD;

    double a = std::sin(dDec / 2) * std::sin(dDec / 2) +
               std::cos(dec1Rad) * std::cos(dec2Rad) * std::sin(dRA / 2) *
                   std::sin(dRA / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return c * RAD_TO_DEG;
}

// ============================================================================
// SyncToSolveTask Implementation
// ============================================================================

void SyncToSolveTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 5.0, "Solve exposure time");
    addParamDefinition("binning_x", "integer", false, 2, "Binning X");
    addParamDefinition("binning_y", "integer", false, 2, "Binning Y");
}

void SyncToSolveTask::executeImpl(const json& params) {
    double exposure = params.value("exposure", 5.0);

    logProgress("Taking sync exposure");
    std::this_thread::sleep_for(
        std::chrono::milliseconds(static_cast<int>(exposure * 200)));

    logProgress("Plate solving for sync", 0.4);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Simulated solve result
    double solvedRA = 180.0;
    double solvedDec = 45.0;

    logProgress("Syncing mount to solved position", 0.8);
    logProgress("RA=" + std::to_string(solvedRA) +
                "°, Dec=" + std::to_string(solvedDec) + "°");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    logProgress("Mount synced successfully", 1.0);
}

// ============================================================================
// BlindSolveTask Implementation
// ============================================================================

void BlindSolveTask::setupParameters() {
    addParamDefinition("image_path", "string", false, nullptr,
                       "Path to image (or take new)");
    addParamDefinition("exposure", "number", false, 10.0,
                       "Exposure if taking new image");
    addParamDefinition("timeout", "integer", false, 300,
                       "Solve timeout in seconds");
    addParamDefinition("downsample", "integer", false, 4, "Downsample factor");
}

void BlindSolveTask::executeImpl(const json& params) {
    int timeout = params.value("timeout", 300);

    if (!params.contains("image_path")) {
        double exposure = params.value("exposure", 10.0);
        logProgress("Taking blind solve exposure: " + std::to_string(exposure) +
                    "s");
        std::this_thread::sleep_for(
            std::chrono::milliseconds(static_cast<int>(exposure * 200)));
    } else {
        logProgress("Using existing image: " +
                    params["image_path"].get<std::string>());
    }

    logProgress("Starting blind plate solve (no hints)", 0.3);
    logProgress("This may take several minutes...");

    // Simulate longer blind solve
    for (int i = 0; i < 5; ++i) {
        if (!shouldContinue()) {
            logProgress("Blind solve cancelled");
            return;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        logProgress("Searching...", 0.3 + 0.1 * i);
    }

    // Simulated result
    double ra = 123.456;
    double dec = 34.567;

    logProgress("Blind solve successful");
    logProgress(
        "RA=" + std::to_string(ra) + "°, Dec=" + std::to_string(dec) + "°",
        1.0);
}

}  // namespace lithium::task::astrometry
