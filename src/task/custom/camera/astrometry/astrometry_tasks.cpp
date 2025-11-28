/**
 * @file astrometry_tasks.cpp
 * @brief Implementation of astrometry tasks
 */

#include "astrometry_tasks.hpp"
#include "../exposure/exposure_tasks.hpp"
#include <thread>

namespace lithium::task::camera {

// PlateSolveExposureTask
void PlateSolveExposureTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 5.0, "Solve exposure time");
    addParamDefinition("binning", "object", false, json{{"x", 2}, {"y", 2}}, "Binning");
    addParamDefinition("hint_ra", "number", false, 0.0, "RA hint (hours)");
    addParamDefinition("hint_dec", "number", false, 0.0, "Dec hint (degrees)");
    addParamDefinition("search_radius", "number", false, 15.0, "Search radius (degrees)");
    addParamDefinition("timeout", "number", false, 120.0, "Solve timeout");
    addParamDefinition("downsample", "integer", false, 2, "Image downsample");
}

void PlateSolveExposureTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
}

void PlateSolveExposureTask::executeImpl(const json& params) {
    double exposure = params.value("exposure", 5.0);

    logProgress("Taking plate solve exposure");

    json expParams = {
        {"exposure", exposure},
        {"type", "light"},
        {"binning", params.value("binning", json{{"x", 2}, {"y", 2}})}
    };

    TakeExposureTask solveExp;
    solveExp.execute(expParams);

    logProgress("Running plate solver");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Simulated solve result
    double solvedRA = params.value("hint_ra", 12.5);
    double solvedDec = params.value("hint_dec", 45.0);
    double rotation = 15.3;

    logProgress("Solved: RA=" + std::to_string(solvedRA) + "h, Dec=" + 
               std::to_string(solvedDec) + "°, Rotation=" + std::to_string(rotation) + "°");
    logProgress("Plate solve complete", 1.0);
}

// CenteringTask
void CenteringTask::setupParameters() {
    addParamDefinition("target_ra", "number", true, nullptr, "Target RA (hours)");
    addParamDefinition("target_dec", "number", true, nullptr, "Target Dec (degrees)");
    addParamDefinition("tolerance", "number", false, 10.0, "Centering tolerance (arcsec)");
    addParamDefinition("max_iterations", "integer", false, 5, "Max centering attempts");
    addParamDefinition("exposure", "number", false, 5.0, "Solve exposure");
}

void CenteringTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
    validateRequired(params, "target_ra");
    validateRequired(params, "target_dec");

    double ra = params["target_ra"].get<double>();
    double dec = params["target_dec"].get<double>();
    validateCoordinates(ra, dec);
}

void CenteringTask::executeImpl(const json& params) {
    double targetRA = params["target_ra"].get<double>();
    double targetDec = params["target_dec"].get<double>();
    double tolerance = params.value("tolerance", 10.0);
    int maxIter = params.value("max_iterations", 5);

    logProgress("Centering on RA=" + std::to_string(targetRA) + "h, Dec=" + 
               std::to_string(targetDec) + "°");

    for (int i = 0; i < maxIter; ++i) {
        double progress = static_cast<double>(i) / maxIter;
        logProgress("Centering attempt " + std::to_string(i + 1), progress);

        // Plate solve
        json solveParams = params;
        solveParams["hint_ra"] = targetRA;
        solveParams["hint_dec"] = targetDec;

        PlateSolveExposureTask solve;
        solve.execute(solveParams);

        // Simulate error measurement
        double error = tolerance * (1.0 - (i + 1) * 0.3);
        logProgress("Centering error: " + std::to_string(error) + " arcsec");

        if (error <= tolerance) {
            logProgress("Target centered within tolerance");
            break;
        }

        // Simulate slew correction
        logProgress("Applying correction slew");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    logProgress("Centering complete", 1.0);
}

}  // namespace lithium::task::camera
