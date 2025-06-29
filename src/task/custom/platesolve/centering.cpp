#include "centering.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include "atom/error/exception.hpp"
#include "tools/convert.hpp"
#include "tools/croods.hpp"

namespace lithium::task::platesolve {

CenteringTask::CenteringTask() : PlateSolveTaskBase("Centering") {
    // Initialize plate solve task
    plateSolveTask_ = std::make_unique<PlateSolveExposureTask>();

    // Configure task properties
    setTaskType("Centering");
    setPriority(8);  // High priority for precision pointing
    setTimeout(std::chrono::seconds(600));  // 10 minute timeout
    setLogLevel(2);

    // Define parameters
    defineParameters(*this);
}

void CenteringTask::execute(const json& params) {
    auto startTime = std::chrono::steady_clock::now();

    try {
        addHistoryEntry("Starting centering task");
        spdlog::info("Executing Centering task with params: {}",
                     params.dump(4));

        // Validate parameters
        if (!validateParams(params)) {
            auto errors = getParamErrors();
            std::string errorMsg = "Parameter validation failed: ";
            for (const auto& error : errors) {
                errorMsg += error + "; ";
            }
            setErrorType(TaskErrorType::InvalidParameter);
            throw std::runtime_error(errorMsg);
        }

        // Execute the task and store result
        auto result = executeImpl(params);
        setResult(json{{"success", result.success},
                       {"final_position",
                        {{"ra", result.finalPosition.ra},
                         {"dec", result.finalPosition.dec}}},
                       {"target_position",
                        {{"ra", result.targetPosition.ra},
                         {"dec", result.targetPosition.dec}}},
                       {"final_offset_arcsec", result.finalOffset},
                       {"iterations", result.iterations},
                       {"solve_results", json::array()}});

        // Add solve results
        auto& solveResultsJson = getResult()["solve_results"];
        for (const auto& solveResult : result.solveResults) {
            solveResultsJson.push_back(
                json{{"success", solveResult.success},
                     {"coordinates",
                      {{"ra", solveResult.coordinates.ra},
                       {"dec", solveResult.coordinates.dec}}},
                     {"solve_time_ms", solveResult.solveTime.count()}});
        }

        Task::execute(params);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        addHistoryEntry("Centering completed successfully");
        spdlog::info("Centering completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Centering failed: " + std::string(e.what()));
        spdlog::error("Centering failed after {} ms: {}", duration.count(),
                      e.what());
        throw;
    }
}

auto CenteringTask::taskName() -> std::string { return "Centering"; }

auto CenteringTask::createEnhancedTask()
    -> std::unique_ptr<lithium::task::Task> {
    auto task = std::make_unique<CenteringTask>();
    return std::move(task);
}

auto CenteringTask::executeImpl(const json& params) -> CenteringResult {
    auto config = parseConfig(params);
    validateConfig(config);

    CenteringResult result;
    // 使用convert.hpp的hourToDegree
    result.targetPosition.ra = lithium::tools::hourToDegree(config.targetRA);
    result.targetPosition.dec = config.targetDec;

    try {
        spdlog::info(
            "Centering on target: RA={:.6f}°, Dec={:.6f}°, tolerance={:.1f}\"",
            result.targetPosition.ra, result.targetPosition.dec,
            config.tolerance);

        bool centered = false;

        for (int iteration = 1; iteration <= config.maxIterations && !centered;
             ++iteration) {
            addHistoryEntry("Centering iteration " + std::to_string(iteration) +
                            " of " + std::to_string(config.maxIterations));
            spdlog::info("Centering iteration {} of {}", iteration,
                         config.maxIterations);

            // Perform plate solve
            auto solveResult = performCenteringIteration(config, iteration);
            result.solveResults.push_back(solveResult);
            result.iterations = iteration;

            if (!solveResult.success) {
                spdlog::error("Plate solve failed in iteration {}", iteration);
                continue;
            }

            // Update current position
            result.finalPosition = solveResult.coordinates;

            // 使用croods.hpp的normalizeAngle360
            result.finalPosition.ra =
                lithium::tools::normalizeAngle360(result.finalPosition.ra);
            result.finalPosition.dec =
                lithium::tools::normalizeDeclination(result.finalPosition.dec);

            // 计算角距离（单位：度），再转为角秒
            double angularDistance = std::hypot(
                result.finalPosition.ra - result.targetPosition.ra,
                result.finalPosition.dec - result.targetPosition.dec);
            // 使用croods.hpp的radiansToArcseconds
            double offsetArcsec = lithium::tools::radiansToArcseconds(
                angularDistance * M_PI / 180.0);
            result.finalOffset = offsetArcsec;

            spdlog::info("Current position: RA={:.6f}°, Dec={:.6f}°",
                         result.finalPosition.ra, result.finalPosition.dec);
            spdlog::info("Offset from target: {:.2f}\" ({:.6f}°)", offsetArcsec,
                         angularDistance);

            if (offsetArcsec <= config.tolerance) {
                spdlog::info("Target centered within tolerance ({:.1f}\")",
                             offsetArcsec);
                addHistoryEntry("Target successfully centered");
                centered = true;
                result.success = true;
            } else {
                // 计算修正量时也用normalizeAngle360
                auto correction = calculateCorrection(result.finalPosition,
                                                      result.targetPosition);

                spdlog::info("Applying correction: RA={:.6f}°, Dec={:.6f}°",
                             correction.ra, correction.dec);
                addHistoryEntry("Applying telescope correction");

                applyTelecopeCorrection(correction);

                // Wait for mount to settle
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }

        if (!centered) {
            result.success = false;
            std::string errorMsg = "Failed to center target within " +
                                   std::to_string(config.maxIterations) +
                                   " iterations";
            spdlog::error(errorMsg);
            THROW_RUNTIME_ERROR(errorMsg);
        }

    } catch (const std::exception& e) {
        result.success = false;
        spdlog::error("Centering failed: {}", e.what());
        throw;
    }

    return result;
}

auto CenteringTask::performCenteringIteration(const CenteringConfig& config,
                                              int iteration)
    -> PlatesolveResult {
    // Prepare plate solve parameters
    json platesolveParams = {
        {"exposure", config.platesolve.exposure},
        {"binning", config.platesolve.binning},
        {"max_attempts", 2},  // Fewer attempts for centering iterations
        {"gain", config.platesolve.gain},
        {"offset", config.platesolve.offset},
        {"solver_type", config.platesolve.solverType},
        {"fov_width", config.platesolve.fovWidth},
        {"fov_height", config.platesolve.fovHeight}};

    // Execute plate solve
    auto result = plateSolveTask_->executeImpl(platesolveParams);
    return result;
}

auto CenteringTask::calculateCorrection(const Coordinates& currentPos,
                                        const Coordinates& targetPos)
    -> Coordinates {
    Coordinates correction;

    // 使用normalizeAngle360确保RA差值正确
    double raOffsetDeg =
        lithium::tools::normalizeAngle360(targetPos.ra - currentPos.ra);
    double decOffsetDeg = targetPos.dec - currentPos.dec;

    // Apply spherical coordinate correction for RA
    correction.ra = raOffsetDeg / std::cos(targetPos.dec * M_PI / 180.0);
    correction.dec = decOffsetDeg;

    return correction;
}

void CenteringTask::applyTelecopeCorrection(const Coordinates& correction) {
    try {
        // Get mount instance
        auto mount = getMountInstance();

        // In a real implementation, you would call mount slew methods here
        // For now, we'll just log the action
        spdlog::info(
            "Applying telescope correction: RA offset={:.6f}°, Dec "
            "offset={:.6f}°",
            correction.ra, correction.dec);

        // Simulate slew time
        std::this_thread::sleep_for(std::chrono::seconds(2));

    } catch (const std::exception& e) {
        spdlog::error("Failed to apply telescope correction: {}", e.what());
        throw;
    }
}

auto CenteringTask::parseConfig(const json& params) -> CenteringConfig {
    CenteringConfig config;

    config.targetRA = params.at("target_ra").get<double>();
    config.targetDec = params.at("target_dec").get<double>();
    config.tolerance = params.value("tolerance", 30.0);
    config.maxIterations = params.value("max_iterations", 5);

    // Parse plate solve config
    config.platesolve.exposure = params.value("exposure", 5.0);
    config.platesolve.binning = params.value("binning", 2);
    config.platesolve.gain = params.value("gain", 100);
    config.platesolve.offset = params.value("offset", 10);
    config.platesolve.solverType = params.value("solver_type", "astrometry");
    config.platesolve.fovWidth = params.value("fov_width", 1.0);
    config.platesolve.fovHeight = params.value("fov_height", 1.0);

    return config;
}

void CenteringTask::validateConfig(const CenteringConfig& config) {
    if (config.targetRA < 0 || config.targetRA >= 24) {
        THROW_INVALID_ARGUMENT("Target RA must be between 0 and 24 hours");
    }

    if (config.targetDec < -90 || config.targetDec > 90) {
        THROW_INVALID_ARGUMENT("Target Dec must be between -90 and 90 degrees");
    }

    if (config.tolerance <= 0 || config.tolerance > 300) {
        THROW_INVALID_ARGUMENT(
            "Tolerance must be between 0 and 300 arcseconds");
    }

    if (config.maxIterations < 1 || config.maxIterations > 10) {
        THROW_INVALID_ARGUMENT("Max iterations must be between 1 and 10");
    }
}

void CenteringTask::defineParameters(lithium::task::Task& task) {
    task.addParamDefinition("target_ra", "number", true, json(12.0),
                            "Target Right Ascension in hours (0-24)");
    task.addParamDefinition("target_dec", "number", true, json(45.0),
                            "Target Declination in degrees (-90 to 90)");
    task.addParamDefinition("tolerance", "number", false, json(30.0),
                            "Centering tolerance in arcseconds");
    task.addParamDefinition("max_iterations", "integer", false, json(5),
                            "Maximum centering iterations");
    task.addParamDefinition("exposure", "number", false, json(5.0),
                            "Plate solve exposure time");
    task.addParamDefinition("binning", "integer", false, json(2),
                            "Camera binning factor");
    task.addParamDefinition("gain", "integer", false, json(100), "Camera gain");
    task.addParamDefinition("offset", "integer", false, json(10),
                            "Camera offset");
    task.addParamDefinition("solver_type", "string", false, json("astrometry"),
                            "Solver type (astrometry/astap)");
    task.addParamDefinition("fov_width", "number", false, json(1.0),
                            "Field of view width in degrees");
    task.addParamDefinition("fov_height", "number", false, json(1.0),
                            "Field of view height in degrees");
}

}  // namespace lithium::task::platesolve
