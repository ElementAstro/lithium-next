#include "exposure.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include "../camera/basic_exposure.hpp"
#include "atom/error/exception.hpp"

namespace lithium::task::platesolve {

PlateSolveExposureTask::PlateSolveExposureTask()
    : PlateSolveTaskBase("PlateSolveExposure") {
    // The action is set through the base class constructor
    // We'll override execute() instead

    // Configure task properties
    setTaskType("PlateSolveExposure");
    setPriority(8);                         // High priority for astrometry
    setTimeout(std::chrono::seconds(300));  // 5 minute timeout
    setLogLevel(2);

    // Define parameters
    defineParameters(*this);
}

void PlateSolveExposureTask::execute(const json& params) {
    auto startTime = std::chrono::steady_clock::now();

    try {
        addHistoryEntry("Starting plate solve exposure task");
        spdlog::info("Executing PlateSolveExposure task with params: {}",
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
        setResult(json{
            {"success", result.success},
            {"coordinates",
             {{"ra", result.coordinates.ra}, {"dec", result.coordinates.dec}}},
            {"pixel_scale", result.pixelScale},
            {"rotation", result.rotation},
            {"solve_time_ms", result.solveTime.count()},
            {"error_message", result.errorMessage}});

        Task::execute(params);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        addHistoryEntry("Plate solve exposure completed successfully");
        spdlog::info("PlateSolveExposure completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Plate solve exposure failed: " +
                        std::string(e.what()));
        spdlog::error("PlateSolveExposure failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto PlateSolveExposureTask::taskName() -> std::string {
    return "PlateSolveExposure";
}

auto PlateSolveExposureTask::createEnhancedTask()
    -> std::unique_ptr<lithium::task::Task> {
    auto task = std::make_unique<PlateSolveExposureTask>();
    return std::move(task);
}

auto PlateSolveExposureTask::executeImpl(const json& params)
    -> PlatesolveResult {
    auto config = parseConfig(params);
    validateConfig(config);

    PlatesolveResult result;
    auto startTime = std::chrono::steady_clock::now();

    try {
        spdlog::info(
            "Taking plate solve exposure: {:.1f}s, binning {}x{}, max {} "
            "attempts",
            config.exposure, config.binning, config.binning,
            config.maxAttempts);

        for (int attempt = 1; attempt <= config.maxAttempts; ++attempt) {
            addHistoryEntry("Plate solve attempt " + std::to_string(attempt) +
                            " of " + std::to_string(config.maxAttempts));
            spdlog::info("Plate solve attempt {} of {}", attempt,
                         config.maxAttempts);  // Take exposure
            std::string imagePath = takeExposure(config);

            // Perform plate solving using the base class method
            result = performPlateSolve(imagePath, config);

            if (result.success) {
                auto endTime = std::chrono::steady_clock::now();
                result.solveTime =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        endTime - startTime);

                spdlog::info(
                    "Plate solve SUCCESS: RA={:.6f}°, Dec={:.6f}°, "
                    "Rotation={:.2f}°, Scale={:.3f}\"/px",
                    result.coordinates.ra, result.coordinates.dec,
                    result.rotation, result.pixelScale);

                addHistoryEntry("Plate solve successful");
                break;
            } else {
                spdlog::warn("Plate solve attempt {} failed: {}", attempt,
                             result.errorMessage);
                addHistoryEntry("Plate solve attempt " +
                                std::to_string(attempt) + " failed");

                if (attempt < config.maxAttempts) {
                    spdlog::info("Retrying with increased exposure time");
                    config.exposure *=
                        1.5;  // Increase exposure for next attempt
                }
            }
        }

        if (!result.success) {
            result.errorMessage = "Plate solving failed after " +
                                  std::to_string(config.maxAttempts) +
                                  " attempts";
            THROW_RUNTIME_ERROR(result.errorMessage);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
        throw;
    }

    return result;
}

auto PlateSolveExposureTask::takeExposure(const PlateSolveConfig& config)
    -> std::string {
    // Create exposure parameters
    json exposureParams = {{"exposure", config.exposure},
                           {"type", "LIGHT"},
                           {"binning", config.binning},
                           {"gain", config.gain},
                           {"offset", config.offset}};

    // Use basic exposure task
    auto exposureTask =
        lithium::task::camera::TakeExposureTask::createEnhancedTask();
    exposureTask->execute(exposureParams);

    // Generate unique filename for plate solve image
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    std::string imagePath =
        "/tmp/platesolve_" + std::to_string(timestamp) + ".fits";

    // For now, return the path - in a real implementation, the exposure task
    // would save the image
    return imagePath;
}

auto PlateSolveExposureTask::parseConfig(const json& params)
    -> PlateSolveConfig {
    PlateSolveConfig config;

    config.exposure = params.value("exposure", 5.0);
    config.binning = params.value("binning", 2);
    config.maxAttempts = params.value("max_attempts", 3);
    config.timeout = params.value("timeout", 60.0);
    config.gain = params.value("gain", 100);
    config.offset = params.value("offset", 10);
    config.solverType = params.value("solver_type", "astrometry");
    config.useInitialCoordinates =
        params.value("use_initial_coordinates", false);
    config.fovWidth = params.value("fov_width", 1.0);
    config.fovHeight = params.value("fov_height", 1.0);

    return config;
}

void PlateSolveExposureTask::validateConfig(const PlateSolveConfig& config) {
    if (config.exposure <= 0 || config.exposure > 120) {
        THROW_INVALID_ARGUMENT(
            "Plate solve exposure must be between 0 and 120 seconds");
    }

    if (config.binning < 1 || config.binning > 4) {
        throw std::invalid_argument("Binning must be between 1 and 4");
    }

    if (config.maxAttempts < 1 || config.maxAttempts > 10) {
        throw std::invalid_argument("Max attempts must be between 1 and 10");
    }

    if (config.solverType != "astrometry" && config.solverType != "astap" &&
        config.solverType != "remote") {
        throw std::invalid_argument(
            "Solver type must be 'astrometry', 'astap', or 'remote'");
    }

    // Validate remote solver configuration
    if (config.useRemoteSolver && config.apiKey.empty()) {
        throw std::invalid_argument("API key is required for remote solving");
    }

    if (config.scaleEstimate <= 0) {
        throw std::invalid_argument("Scale estimate must be positive");
    }

    if (config.scaleError < 0 || config.scaleError > 1) {
        THROW_INVALID_ARGUMENT("Scale error must be between 0 and 1");
    }
}

void PlateSolveExposureTask::defineParameters(lithium::task::Task& task) {
    // Basic exposure parameters
    task.addParamDefinition("exposure", "number", false, json(5.0),
                            "Plate solve exposure time in seconds");
    task.addParamDefinition("binning", "integer", false, json(2),
                            "Camera binning factor");
    task.addParamDefinition("max_attempts", "integer", false, json(3),
                            "Maximum solve attempts");
    task.addParamDefinition("timeout", "number", false, json(60.0),
                            "Solve timeout in seconds");
    task.addParamDefinition("gain", "integer", false, json(100), "Camera gain");
    task.addParamDefinition("offset", "integer", false, json(10),
                            "Camera offset");

    // Solver configuration
    task.addParamDefinition("solver_type", "string", false, json("astrometry"),
                            "Solver type (astrometry/astap/remote)");
    task.addParamDefinition("use_initial_coordinates", "boolean", false,
                            json(false), "Use initial coordinates hint");
    task.addParamDefinition("fov_width", "number", false, json(1.0),
                            "Field of view width in degrees");
    task.addParamDefinition("fov_height", "number", false, json(1.0),
                            "Field of view height in degrees");

    // Remote solver parameters
    task.addParamDefinition("use_remote_solver", "boolean", false, json(false),
                            "Use remote astrometry.net service");
    task.addParamDefinition("api_key", "string", false, json(""),
                            "API key for remote astrometry.net service");
    task.addParamDefinition("publicly_visible", "boolean", false, json(false),
                            "Make submission publicly visible");
    task.addParamDefinition("license", "string", false, json("default"),
                            "License type (default/yes/no/shareAlike)");

    // Advanced options
    task.addParamDefinition("scale_estimate", "number", false, json(1.0),
                            "Pixel scale estimate in arcsec/pixel");
    task.addParamDefinition("scale_error", "number", false, json(0.1),
                            "Scale estimate error tolerance (0-1)");
    task.addParamDefinition("search_radius", "number", false, json(2.0),
                            "Search radius around hint position in degrees");
    task.addParamDefinition("ra_hint", "number", false, json(),
                            "RA hint in degrees (optional)");
    task.addParamDefinition("dec_hint", "number", false, json(),
                            "Dec hint in degrees (optional)");
}

}  // namespace lithium::task::platesolve
