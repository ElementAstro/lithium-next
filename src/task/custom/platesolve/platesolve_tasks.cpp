// ==================== Includes and Declarations ====================
#include "platesolve_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <random>
#include <thread>
#include "../factory.hpp"
#include "atom/error/exception.hpp"
#include "../camera/basic_exposure.hpp"

namespace lithium::task::task {

// ==================== Mock Classes for Testing ====================
#ifdef MOCK_CAMERA
struct Coordinates {
    double ra{0.0};   // Right Ascension in hours
    double dec{0.0};  // Declination in degrees
};

class MockPlateSolver {
public:
    MockPlateSolver() = default;

    bool solve(const std::string& imagePath) {
        spdlog::info("Plate solving image: {}", imagePath);
        std::this_thread::sleep_for(
            std::chrono::seconds(3));  // Simulate solving time

        // Mock solution with some randomization
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> raDist(0.0, 24.0);
        std::uniform_real_distribution<> decDist(-90.0, 90.0);

        solved_ = true;
        coordinates_.ra = raDist(gen);
        coordinates_.dec = decDist(gen);
        return true;
    }

    Coordinates getCoordinates() const { return coordinates_; }
    bool isSolved() const { return solved_; }
    double getRotation() const { return rotation_; }
    double getPixelScale() const { return pixelScale_; }

private:
    bool solved_{false};
    Coordinates coordinates_;
    double rotation_{0.0};
    double pixelScale_{1.5};  // arcsec/pixel
};

class MockMount {
public:
    MockMount() = default;

    Coordinates getCurrentPosition() const { return currentPos_; }
    void slewTo(const Coordinates& coords) {
        spdlog::info("Slewing to RA: {:.3f}h, Dec: {:.3f}°", coords.ra,
                     coords.dec);
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Simulate slew
        currentPos_ = coords;
    }

    bool isSlewing() const { return false; }

private:
    Coordinates currentPos_{12.0, 45.0};  // Default position
};
#endif

// ==================== PlateSolveExposureTask Implementation
// ====================

auto PlateSolveExposureTask::taskName() -> std::string {
    return "PlateSolveExposure";
}

void PlateSolveExposureTask::execute(const json& params) {
    executeImpl(params);
}

void PlateSolveExposureTask::executeImpl(const json& params) {
    spdlog::info("Executing PlateSolveExposure task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double exposure = params.value("exposure", 5.0);
        int binning =
            params.value("binning", 2);  // Higher binning for faster solving
        int maxAttempts = params.value("max_attempts", 3);
        double timeout = params.value("timeout", 60.0);

        spdlog::info(
            "Taking plate solve exposure: {:.1f}s, binning {}x{}, max {} "
            "attempts",
            exposure, binning, binning, maxAttempts);

#ifdef MOCK_CAMERA
        auto plateSolver = std::make_shared<MockPlateSolver>();
#endif

        bool solved = false;

        for (int attempt = 1; attempt <= maxAttempts && !solved; ++attempt) {
            spdlog::info("Plate solve attempt {} of {}", attempt, maxAttempts);

            // Take exposure for plate solving
            json exposureParams = {{"exposure", exposure},
                                   {"type", ExposureType::LIGHT},
                                   {"binning", binning},
                                   {"gain", params.value("gain", 100)},
                                   {"offset", params.value("offset", 10)}};

            auto exposureTask = TakeExposureTask::createEnhancedTask();
            exposureTask->execute(exposureParams);

            // Attempt plate solving
            std::string imagePath =
                "/tmp/platesolve_" + std::to_string(attempt) + ".fits";

            auto solveStart = std::chrono::steady_clock::now();
#ifdef MOCK_CAMERA
            if (plateSolver->solve(imagePath)) {
                auto coordinates = plateSolver->getCoordinates();
                double rotation = plateSolver->getRotation();
                double pixelScale = plateSolver->getPixelScale();

                spdlog::info(
                    "Plate solve SUCCESS: RA={:.3f}h, Dec={:.3f}°, "
                    "Rotation={:.1f}°, Scale={:.2f}\"/px",
                    coordinates.ra, coordinates.dec, rotation, pixelScale);
                solved = true;
            } else {
                auto solveTime =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - solveStart);
                spdlog::warn("Plate solve attempt {} failed after {} seconds",
                             attempt, solveTime.count());

                if (attempt < maxAttempts) {
                    spdlog::info("Retrying with increased exposure time");
                    exposure *= 1.5;  // Increase exposure for next attempt
                }
            }
#else
            THROW_RUNTIME_ERROR(
                "Plate solving only supported in MOCK_CAMERA mode.");
#endif
        }

        if (!solved) {
            THROW_RUNTIME_ERROR("Plate solving failed after {} attempts",
                                maxAttempts);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("PlateSolveExposure completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("PlateSolveExposure task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto PlateSolveExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            PlateSolveExposureTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced PlateSolveExposure task failed: {}",
                          e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);  // High priority for astrometry
    task->setTimeout(std::chrono::seconds(300));  // 5 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void PlateSolveExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", false, 5.0,
                            "Plate solve exposure time");
    task.addParamDefinition("binning", "int", false, 2,
                            "Camera binning for solving");
    task.addParamDefinition("max_attempts", "int", false, 3,
                            "Maximum solve attempts");
    task.addParamDefinition("timeout", "double", false, 60.0,
                            "Solve timeout in seconds");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void PlateSolveExposureTask::validatePlateSolveParameters(const json& params) {
    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 120) {
            THROW_INVALID_ARGUMENT(
                "Plate solve exposure must be between 0 and 120 seconds");
        }
    }

    if (params.contains("max_attempts")) {
        int attempts = params["max_attempts"].get<int>();
        if (attempts < 1 || attempts > 10) {
            THROW_INVALID_ARGUMENT("Max attempts must be between 1 and 10");
        }
    }
}

// ==================== CenteringTask Implementation ====================

auto CenteringTask::taskName() -> std::string { return "Centering"; }

void CenteringTask::execute(const json& params) { executeImpl(params); }

void CenteringTask::executeImpl(const json& params) {
    spdlog::info("Executing Centering task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double targetRA = params.at("target_ra").get<double>();
        double targetDec = params.at("target_dec").get<double>();
        double tolerance = params.value("tolerance", 30.0);  // arcseconds
        int maxIterations = params.value("max_iterations", 5);

        spdlog::info(
            "Centering on target: RA={:.3f}h, Dec={:.3f}°, tolerance={:.1f}\"",
            targetRA, targetDec, tolerance);

#ifdef MOCK_CAMERA
        auto plateSolver = std::make_shared<MockPlateSolver>();
        auto mount = std::make_shared<MockMount>();
#endif

#ifdef MOCK_CAMERA
        Coordinates target{targetRA, targetDec};
        bool centered = false;

        for (int iteration = 1; iteration <= maxIterations && !centered;
             ++iteration) {
            spdlog::info("Centering iteration {} of {}", iteration,
                         maxIterations);

            // Take plate solve exposure
            json psParams = {{"exposure", params.value("exposure", 5.0)},
                             {"binning", 2},
                             {"max_attempts", 2}};

            PlateSolveExposureTask psTask;
            psTask.execute(psParams);

            // Get current position from plate solution
            auto currentPos = plateSolver->getCoordinates();

            // Calculate offset
            double raOffset = (target.ra - currentPos.ra) * 15.0 * 3600.0 *
                              cos(target.dec * M_PI / 180.0);  // arcsec
            double decOffset =
                (target.dec - currentPos.dec) * 3600.0;  // arcsec
            double totalOffset =
                sqrt(raOffset * raOffset + decOffset * decOffset);

            spdlog::info("Current position: RA={:.3f}h, Dec={:.3f}°",
                         currentPos.ra, currentPos.dec);
            spdlog::info("Offset: RA={:.1f}\", Dec={:.1f}\", Total={:.1f}\"",
                         raOffset, decOffset, totalOffset);

            if (totalOffset <= tolerance) {
                spdlog::info("Target centered within tolerance ({:.1f}\")",
                             totalOffset);
                centered = true;
            } else {
                // Apply correction
                Coordinates correctedTarget{
                    target.ra + (raOffset / (15.0 * 3600.0 *
                                             cos(target.dec * M_PI / 180.0))),
                    target.dec + (decOffset / 3600.0)};

                spdlog::info(
                    "Applying correction: slewing to RA={:.3f}h, Dec={:.3f}°",
                    correctedTarget.ra, correctedTarget.dec);
                mount->slewTo(correctedTarget);

                // Wait for slew to complete
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }

        if (!centered) {
            THROW_RUNTIME_ERROR("Failed to center target within {} iterations",
                                maxIterations);
        }
#else
        THROW_RUNTIME_ERROR("Centering only supported in MOCK_CAMERA mode.");
#endif

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("Centering completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("Centering task failed after {} ms: {}", duration.count(),
                      e.what());
        throw;
    }
}

auto CenteringTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            CenteringTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced Centering task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);
    task->setTimeout(std::chrono::seconds(600));  // 10 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void CenteringTask::defineParameters(Task& task) {
    task.addParamDefinition("target_ra", "double", true, 12.0,
                            "Target Right Ascension in hours");
    task.addParamDefinition("target_dec", "double", true, 45.0,
                            "Target Declination in degrees");
    task.addParamDefinition("tolerance", "double", false, 30.0,
                            "Centering tolerance in arcseconds");
    task.addParamDefinition("max_iterations", "int", false, 5,
                            "Maximum centering iterations");
    task.addParamDefinition("exposure", "double", false, 5.0,
                            "Plate solve exposure time");
}

void CenteringTask::validateCenteringParameters(const json& params) {
    if (!params.contains("target_ra") || !params.contains("target_dec")) {
        THROW_INVALID_ARGUMENT("Missing target_ra or target_dec parameters");
    }

    double ra = params["target_ra"].get<double>();
    double dec = params["target_dec"].get<double>();

    if (ra < 0 || ra >= 24) {
        THROW_INVALID_ARGUMENT("Target RA must be between 0 and 24 hours");
    }

    if (dec < -90 || dec > 90) {
        THROW_INVALID_ARGUMENT("Target Dec must be between -90 and 90 degrees");
    }
}

// ==================== MosaicTask Implementation ====================

auto MosaicTask::taskName() -> std::string { return "Mosaic"; }

void MosaicTask::execute(const json& params) { executeImpl(params); }

void MosaicTask::executeImpl(const json& params) {
    spdlog::info("Executing Mosaic task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double centerRA = params.at("center_ra").get<double>();
        double centerDec = params.at("center_dec").get<double>();
        int gridWidth = params.at("grid_width").get<int>();
        int gridHeight = params.at("grid_height").get<int>();
        double overlap = params.value("overlap", 20.0);  // percentage
        double frameExposure = params.value("frame_exposure", 300.0);
        int framesPerPosition = params.value("frames_per_position", 1);

        spdlog::info(
            "Starting {}x{} mosaic centered at RA={:.3f}h, Dec={:.3f}°, "
            "{:.1f}% overlap",
            gridWidth, gridHeight, centerRA, centerDec, overlap);

#ifdef MOCK_CAMERA
        auto mount = std::make_shared<MockMount>();
#endif

        // Calculate field of view (assuming 1 degree field)
        double fieldWidth = 1.0;   // degrees
        double fieldHeight = 1.0;  // degrees

        // Calculate step size with overlap
        double stepRA = fieldWidth * (100.0 - overlap) / 100.0;
        double stepDec = fieldHeight * (100.0 - overlap) / 100.0;

        int totalPositions = gridWidth * gridHeight;
        int currentPosition = 0;

        // Calculate starting position (bottom-left of grid)
        double startRA = centerRA - (gridWidth - 1) * stepRA / 2.0;
        double startDec = centerDec - (gridHeight - 1) * stepDec / 2.0;

        for (int row = 0; row < gridHeight; ++row) {
            for (int col = 0; col < gridWidth; ++col) {
                currentPosition++;

                double posRA = startRA + col * stepRA;
                double posDec = startDec + row * stepDec;

                spdlog::info(
                    "Mosaic position {} of {}: RA={:.3f}h, Dec={:.3f}° (Grid: "
                    "{}, {})",
                    currentPosition, totalPositions, posRA, posDec, col + 1,
                    row + 1);

                // Slew to position
#ifdef MOCK_CAMERA
                Coordinates mosaicPos{posRA, posDec};
                mount->slewTo(mosaicPos);
                std::this_thread::sleep_for(std::chrono::seconds(2));
#else
                THROW_RUNTIME_ERROR(
                    "Mosaic slewing only supported in MOCK_CAMERA mode.");
#endif

                // Center on position if requested
                if (params.value("auto_center", true)) {
                    json centerParams = {
                        {"target_ra", posRA},
                        {"target_dec", posDec},
                        {"tolerance", 60.0},  // Larger tolerance for mosaic
                        {"max_iterations", 3}};

                    CenteringTask centeringTask;
                    centeringTask.execute(centerParams);
                }

                // Take exposures at this position
                for (int frame = 0; frame < framesPerPosition; ++frame) {
                    spdlog::info("Taking frame {} of {} at position {}",
                                 frame + 1, framesPerPosition, currentPosition);

                    json exposureParams = {
                        {"exposure", frameExposure},
                        {"type", ExposureType::LIGHT},
                        {"gain", params.value("gain", 100)},
                        {"offset", params.value("offset", 10)}};

                    auto exposureTask = TakeExposureTask::createEnhancedTask();
                    exposureTask->execute(exposureParams);
                }
            }
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info(
            "Mosaic completed {} positions with {} total frames in {} ms",
            totalPositions, totalPositions * framesPerPosition,
            duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("Mosaic task failed after {} ms: {}", duration.count(),
                      e.what());
        throw;
    }
}

auto MosaicTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            MosaicTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced Mosaic task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(
        std::chrono::seconds(14400));  // 4 hour timeout for large mosaics
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void MosaicTask::defineParameters(Task& task) {
    task.addParamDefinition("center_ra", "double", true, 12.0,
                            "Mosaic center RA in hours");
    task.addParamDefinition("center_dec", "double", true, 45.0,
                            "Mosaic center Dec in degrees");
    task.addParamDefinition("grid_width", "int", true, 2,
                            "Number of columns in mosaic grid");
    task.addParamDefinition("grid_height", "int", true, 2,
                            "Number of rows in mosaic grid");
    task.addParamDefinition("overlap", "double", false, 20.0,
                            "Frame overlap percentage");
    task.addParamDefinition("frame_exposure", "double", false, 300.0,
                            "Exposure time per frame");
    task.addParamDefinition("frames_per_position", "int", false, 1,
                            "Frames per mosaic position");
    task.addParamDefinition("auto_center", "bool", false, true,
                            "Auto-center each position");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void MosaicTask::validateMosaicParameters(const json& params) {
    if (!params.contains("center_ra") || !params.contains("center_dec") ||
        !params.contains("grid_width") || !params.contains("grid_height")) {
        THROW_INVALID_ARGUMENT("Missing required mosaic parameters");
    }

    int width = params["grid_width"].get<int>();
    int height = params["grid_height"].get<int>();

    if (width < 1 || width > 10 || height < 1 || height > 10) {
        THROW_INVALID_ARGUMENT("Grid dimensions must be between 1 and 10");
    }

    if (params.contains("overlap")) {
        double overlap = params["overlap"].get<double>();
        if (overlap < 0 || overlap > 50) {
            THROW_INVALID_ARGUMENT("Overlap must be between 0 and 50 percent");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

AUTO_REGISTER_TASK(
    PlateSolveExposureTask, "PlateSolveExposure",
    (TaskInfo{
        "PlateSolveExposure",
        "Take an exposure and perform plate solving",
        "Astrometry",
        {},
        json{{"type", "object"},
             {"properties",
              json{{"exposure",
                    json{{"type", "number"}, {"minimum", 0}, {"maximum", 120}}},
                   {"binning",
                    json{{"type", "integer"}, {"minimum", 1}, {"maximum", 4}}},
                   {"max_attempts",
                    json{{"type", "integer"}, {"minimum", 1}, {"maximum", 10}}},
                   {"timeout",
                    json{{"type", "number"}, {"minimum", 1}, {"maximum", 600}}},
                   {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                   {"offset", json{{"type", "integer"}, {"minimum", 0}}}}}},
        "1.0.0",
        {}}));

// Register CenteringTask
AUTO_REGISTER_TASK(
    CenteringTask, "Centering",
    (TaskInfo{
        "Centering",
        "Center the telescope on a target using plate solving",
        "Astrometry",
        {"target_ra", "target_dec"},
        json{
            {"type", "object"},
            {"properties",
             json{{"target_ra",
                   json{{"type", "number"}, {"minimum", 0}, {"maximum", 24}}},
                  {"target_dec",
                   json{{"type", "number"}, {"minimum", -90}, {"maximum", 90}}},
                  {"tolerance",
                   json{{"type", "number"}, {"minimum", 1}, {"maximum", 300}}},
                  {"max_iterations",
                   json{{"type", "integer"}, {"minimum", 1}, {"maximum", 10}}},
                  {"exposure", json{{"type", "number"},
                                    {"minimum", 0},
                                    {"maximum", 120}}}}}},
        "1.0.0",
        {}}));

// Register MosaicTask
AUTO_REGISTER_TASK(
    MosaicTask, "Mosaic",
    (TaskInfo{
        "Mosaic",
        "Perform a mosaic sequence with auto-centering and exposures",
        "Astrometry",
        {"center_ra", "center_dec", "grid_width", "grid_height"},
        json{
            {"type", "object"},
            {"properties",
             json{{"center_ra",
                   json{{"type", "number"}, {"minimum", 0}, {"maximum", 24}}},
                  {"center_dec",
                   json{{"type", "number"}, {"minimum", -90}, {"maximum", 90}}},
                  {"grid_width",
                   json{{"type", "integer"}, {"minimum", 1}, {"maximum", 10}}},
                  {"grid_height",
                   json{{"type", "integer"}, {"minimum", 1}, {"maximum", 10}}},
                  {"overlap",
                   json{{"type", "number"}, {"minimum", 0}, {"maximum", 50}}},
                  {"frame_exposure",
                   json{{"type", "number"}, {"minimum", 0}, {"maximum", 3600}}},
                  {"frames_per_position",
                   json{{"type", "integer"}, {"minimum", 1}, {"maximum", 10}}},
                  {"auto_center", json{{"type", "boolean"}}},
                  {"gain", json{{"type", "integer"}, {"minimum", 0}}},
                  {"offset", json{{"type", "integer"}, {"minimum", 0}}}}}},
        "1.0.0",
        {}}));

}  // namespace
