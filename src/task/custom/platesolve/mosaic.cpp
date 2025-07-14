#include "task/custom/camera/basic_exposure.hpp"
#include "mosaic.hpp"

#include "atom/function/global_ptr.hpp"
#include "atom/error/exception.hpp"
#include "constant/constant.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include "tools/convert.hpp"
#include "tools/croods.hpp"

namespace lithium::task::platesolve {

MosaicTask::MosaicTask()
    : PlateSolveTaskBase("Mosaic") {

    // Initialize centering task
    centeringTask_ = std::make_unique<CenteringTask>();

    // Configure task properties
    setTaskType("Mosaic");
    setPriority(6);  // Medium-high priority for long sequences
    setTimeout(std::chrono::seconds(14400));  // 4 hour timeout for large mosaics
    setLogLevel(2);

    // Define parameters
    defineParameters(*this);
}

void MosaicTask::execute(const json& params) {
    auto startTime = std::chrono::steady_clock::now();

    try {
        addHistoryEntry("Starting mosaic task");
        spdlog::info("Executing Mosaic task with params: {}", params.dump(4));

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
            {"total_positions", result.totalPositions},
            {"completed_positions", result.completedPositions},
            {"total_frames", result.totalFrames},
            {"completed_frames", result.completedFrames},
            {"total_time_ms", result.totalTime.count()},
            {"centering_results", json::array()}
        });

        // Add centering results
        auto& centeringResultsJson = getResult()["centering_results"];
        for (const auto& centeringResult : result.centeringResults) {
            centeringResultsJson.push_back(json{
                {"success", centeringResult.success},
                {"final_position", {
                    {"ra", centeringResult.finalPosition.ra},
                    {"dec", centeringResult.finalPosition.dec}
                }},
                {"final_offset_arcsec", centeringResult.finalOffset},
                {"iterations", centeringResult.iterations}
            });
        }

        Task::execute(params);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        addHistoryEntry("Mosaic completed successfully");
        spdlog::info("Mosaic completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Mosaic failed: " + std::string(e.what()));
        spdlog::error("Mosaic failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto MosaicTask::taskName() -> std::string {
    return "Mosaic";
}

auto MosaicTask::createEnhancedTask() -> std::unique_ptr<lithium::task::Task> {
    auto task = std::make_unique<MosaicTask>();
    return std::move(task);
}

auto MosaicTask::executeImpl(const json& params) -> MosaicResult {
    auto config = parseConfig(params);
    validateConfig(config);

    MosaicResult result;
    auto startTime = std::chrono::steady_clock::now();

    try {
        spdlog::info("Starting {}x{} mosaic centered at RA={:.6f}°, Dec={:.6f}°, {:.1f}% overlap",
                     config.gridWidth, config.gridHeight,
                     lithium::tools::hourToDegree(config.centerRA), config.centerDec, config.overlap);

        // Calculate grid positions
        auto positions = calculateGridPositions(config);

        result.totalPositions = static_cast<int>(positions.size());
        result.totalFrames = result.totalPositions * config.framesPerPosition;

        addHistoryEntry("Calculated " + std::to_string(result.totalPositions) + " mosaic positions");

        // Process each position
        for (size_t i = 0; i < positions.size(); ++i) {
            const auto& position = positions[i];
            int positionIndex = static_cast<int>(i) + 1;

            spdlog::info("Mosaic position {} of {}: RA={:.6f}°, Dec={:.6f}° (Grid: {}, {})",
                         positionIndex, result.totalPositions,
                         position.ra, position.dec,
                         (i % config.gridWidth) + 1,
                         (i / config.gridWidth) + 1);

            addHistoryEntry("Processing position " + std::to_string(positionIndex) + " of " + std::to_string(result.totalPositions));

            try {
                // Process this position
                auto centeringResult = processPosition(position, config, positionIndex, result.totalPositions);
                result.centeringResults.push_back(centeringResult);

                if (centeringResult.success) {
                    // Take exposures at this position
                    int framesCompleted = takeExposuresAtPosition(config, positionIndex);
                    result.completedFrames += framesCompleted;
                    result.completedPositions++;

                    spdlog::info("Position {} completed: {} frames taken", positionIndex, framesCompleted);
                } else {
                    spdlog::warn("Position {} failed centering, skipping exposures", positionIndex);
                }

            } catch (const std::exception& e) {
                spdlog::error("Failed to process position {}: {}", positionIndex, e.what());
                addHistoryEntry("Position " + std::to_string(positionIndex) + " failed: " + e.what());
                // Continue with next position
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        result.totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        result.success = (result.completedPositions > 0);

        spdlog::info("Mosaic completed: {}/{} positions, {}/{} frames in {} ms",
                     result.completedPositions, result.totalPositions,
                     result.completedFrames, result.totalFrames,
                     result.totalTime.count());

        if (!result.success) {
            THROW_RUNTIME_ERROR("Mosaic failed - no positions completed successfully");
        }

    } catch (const std::exception& e) {
        result.success = false;
        spdlog::error("Mosaic failed: {}", e.what());
        throw;
    }

    return result;
}

auto MosaicTask::calculateGridPositions(const MosaicConfig& config) -> std::vector<Coordinates> {
    std::vector<Coordinates> positions;
    positions.reserve(config.gridWidth * config.gridHeight);

    // Convert center to degrees
    double centerRADeg = lithium::tools::hourToDegree(config.centerRA);
    double centerDecDeg = config.centerDec;

    // Calculate field of view (assuming 1 degree field for now)
    double fieldWidth = 1.0;   // degrees
    double fieldHeight = 1.0;  // degrees

    // Calculate step size with overlap
    double stepRA = fieldWidth * (100.0 - config.overlap) / 100.0;
    double stepDec = fieldHeight * (100.0 - config.overlap) / 100.0;

    // Calculate starting position (bottom-left of grid)
    double startRA = centerRADeg - (config.gridWidth - 1) * stepRA / 2.0;
    double startDec = centerDecDeg - (config.gridHeight - 1) * stepDec / 2.0;

    // Generate grid positions
    for (int row = 0; row < config.gridHeight; ++row) {
        for (int col = 0; col < config.gridWidth; ++col) {
            Coordinates pos;
            pos.ra = lithium::tools::normalizeAngle360(startRA + col * stepRA);
            pos.dec = lithium::tools::normalizeDeclination(startDec + row * stepDec);
            positions.push_back(pos);
        }
    }

    return positions;
}

auto MosaicTask::processPosition(const Coordinates& position, const MosaicConfig& config,
                                int positionIndex, int totalPositions) -> CenteringResult {
    try {
        // Initial slew to position (in real implementation)
        spdlog::info("Slewing to position: RA={:.6f}°, Dec={:.6f}°", position.ra, position.dec);

        // Simulate slew time
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Center on position if auto-centering is enabled
        if (config.autoCenter) {
            json centeringParams = {
                {"target_ra", lithium::tools::degreeToHour(position.ra)},
                {"target_dec", position.dec},
                {"tolerance", config.centering.tolerance},
                {"max_iterations", config.centering.maxIterations},
                {"exposure", config.centering.platesolve.exposure},
                {"binning", config.centering.platesolve.binning},
                {"gain", config.centering.platesolve.gain},
                {"offset", config.centering.platesolve.offset},
                {"solver_type", config.centering.platesolve.solverType}
            };

            return centeringTask_->executeImpl(centeringParams);
        } else {
            // No centering - just return success with current position
            CenteringResult result;
            result.success = true;
            result.finalPosition = position;
            result.targetPosition = position;
            result.finalOffset = 0.0;
            result.iterations = 0;
            return result;
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to process position {}: {}", positionIndex, e.what());

        CenteringResult result;
        result.success = false;
        result.targetPosition = position;
        return result;
    }
}

auto MosaicTask::takeExposuresAtPosition(const MosaicConfig& config, int positionIndex) -> int {
    int successfulFrames = 0;

    try {
        for (int frame = 0; frame < config.framesPerPosition; ++frame) {
            spdlog::info("Taking frame {} of {} at position {}",
                         frame + 1, config.framesPerPosition, positionIndex);

            json exposureParams = {
                {"exposure", config.frameExposure},
                {"type", "LIGHT"},
                {"gain", config.gain},
                {"offset", config.offset}
            };

            // Use basic exposure task
            auto exposureTask = lithium::task::task::TakeExposureTask::createEnhancedTask();
            exposureTask->execute(exposureParams);

            successfulFrames++;
            addHistoryEntry("Completed frame " + std::to_string(frame + 1) + " at position " + std::to_string(positionIndex));
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to complete all exposures at position {}: {}", positionIndex, e.what());
    }

    return successfulFrames;
}

auto MosaicTask::parseConfig(const json& params) -> MosaicConfig {
    MosaicConfig config;

    config.centerRA = params.at("center_ra").get<double>();
    config.centerDec = params.at("center_dec").get<double>();
    config.gridWidth = params.at("grid_width").get<int>();
    config.gridHeight = params.at("grid_height").get<int>();
    config.overlap = params.value("overlap", 20.0);
    config.frameExposure = params.value("frame_exposure", 300.0);
    config.framesPerPosition = params.value("frames_per_position", 1);
    config.autoCenter = params.value("auto_center", true);
    config.gain = params.value("gain", 100);
    config.offset = params.value("offset", 10);

    // Parse centering config
    config.centering.tolerance = params.value("centering_tolerance", 60.0);  // Larger tolerance for mosaic
    config.centering.maxIterations = params.value("centering_max_iterations", 3);
    config.centering.platesolve.exposure = params.value("centering_exposure", 5.0);
    config.centering.platesolve.binning = params.value("centering_binning", 2);
    config.centering.platesolve.gain = params.value("centering_gain", 100);
    config.centering.platesolve.offset = params.value("centering_offset", 10);
    config.centering.platesolve.solverType = params.value("solver_type", "astrometry");

    return config;
}

void MosaicTask::validateConfig(const MosaicConfig& config) {
    if (config.centerRA < 0 || config.centerRA >= 24) {
        THROW_INVALID_ARGUMENT("Center RA must be between 0 and 24 hours");
    }

    if (config.centerDec < -90 || config.centerDec > 90) {
        THROW_INVALID_ARGUMENT("Center Dec must be between -90 and 90 degrees");
    }

    if (config.gridWidth < 1 || config.gridWidth > 10) {
        THROW_INVALID_ARGUMENT("Grid width must be between 1 and 10");
    }

    if (config.gridHeight < 1 || config.gridHeight > 10) {
        THROW_INVALID_ARGUMENT("Grid height must be between 1 and 10");
    }

    if (config.overlap < 0 || config.overlap > 50) {
        THROW_INVALID_ARGUMENT("Overlap must be between 0 and 50 percent");
    }

    if (config.frameExposure <= 0 || config.frameExposure > 3600) {
        THROW_INVALID_ARGUMENT("Frame exposure must be between 0 and 3600 seconds");
    }

    if (config.framesPerPosition < 1 || config.framesPerPosition > 10) {
        THROW_INVALID_ARGUMENT("Frames per position must be between 1 and 10");
    }
}

void MosaicTask::defineParameters(lithium::task::Task& task) {
    task.addParamDefinition("center_ra", "number", true, json(12.0),
                            "Mosaic center RA in hours (0-24)");
    task.addParamDefinition("center_dec", "number", true, json(45.0),
                            "Mosaic center Dec in degrees (-90 to 90)");
    task.addParamDefinition("grid_width", "integer", true, json(2),
                            "Number of columns in mosaic grid (1-10)");
    task.addParamDefinition("grid_height", "integer", true, json(2),
                            "Number of rows in mosaic grid (1-10)");
    task.addParamDefinition("overlap", "number", false, json(20.0),
                            "Frame overlap percentage (0-50)");
    task.addParamDefinition("frame_exposure", "number", false, json(300.0),
                            "Exposure time per frame in seconds");
    task.addParamDefinition("frames_per_position", "integer", false, json(1),
                            "Number of frames per mosaic position (1-10)");
    task.addParamDefinition("auto_center", "boolean", false, json(true),
                            "Auto-center each position");
    task.addParamDefinition("gain", "integer", false, json(100),
                            "Camera gain");
    task.addParamDefinition("offset", "integer", false, json(10),
                            "Camera offset");
    task.addParamDefinition("centering_tolerance", "number", false, json(60.0),
                            "Centering tolerance in arcseconds");
    task.addParamDefinition("centering_max_iterations", "integer", false, json(3),
                            "Maximum centering iterations");
    task.addParamDefinition("centering_exposure", "number", false, json(5.0),
                            "Centering exposure time");
    task.addParamDefinition("centering_binning", "integer", false, json(2),
                            "Centering binning factor");
    task.addParamDefinition("solver_type", "string", false, json("astrometry"),
                            "Solver type (astrometry/astap)");
}

}  // namespace lithium::task::platesolve
