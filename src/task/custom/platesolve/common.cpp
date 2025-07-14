#include "common.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <filesystem>
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "client/astap/astap.hpp"
#include "client/astrometry/astrometry.hpp"
#include "client/astrometry/remote/client.hpp"
#include "constant/constant.hpp"
#include "tools/convert.hpp"   // 新增：坐标转换工具
#include "tools/croods.hpp"   // 新增：天文常量与角度转换

namespace lithium::task::platesolve {

PlateSolveTaskBase::PlateSolveTaskBase(const std::string& name)
    : Task(name, [](const json&) {
          // Default empty action - derived classes will override execute()
      }) {}

auto PlateSolveTaskBase::getLocalSolverInstance(const std::string& solverType)
    -> std::shared_ptr<AtomSolver> {
    if (solverType == "astrometry") {
        // Try to get local astrometry solver
        auto solver = GetPtr<AstrometrySolver>("astrometry_solver");
        if (!solver) {
            spdlog::error(
                "Local astrometry solver not found in global manager");
            THROW_RUNTIME_ERROR("Local astrometry solver not available");
        }
        return std::static_pointer_cast<AtomSolver>(solver.value());
    } else if (solverType == "astap") {
        // Try to get ASTAP solver
        auto solver = GetPtr<AstapSolver>("astap_solver");
        if (!solver) {
            spdlog::error("ASTAP solver not found in global manager");
            THROW_RUNTIME_ERROR("ASTAP solver not available");
        }
        return std::static_pointer_cast<AtomSolver>(solver.value());
    } else {
        spdlog::error("Unknown local solver type: {}", solverType);
        THROW_INVALID_ARGUMENT("Unknown local solver type: {}", solverType);
    }
}

auto PlateSolveTaskBase::getRemoteAstrometryClient()
    -> std::shared_ptr<astrometry::AstrometryClient> {
    auto client =
        GetPtr<astrometry::AstrometryClient>("remote_astrometry_client");
    if (!client) {
        spdlog::error("Remote astrometry client not found in global manager");
        THROW_RUNTIME_ERROR("Remote astrometry client not available");
    }
    return client.value();
}

auto PlateSolveTaskBase::performPlateSolve(const std::string& imagePath,
                                           const PlateSolveConfig& config)
    -> PlatesolveResult {
    PlatesolveResult result;
    auto startTime = std::chrono::steady_clock::now();

    try {
        if (config.useRemoteSolver) {
            // Use remote astrometry.net service
            result = performRemotePlateSolve(imagePath, config);
        } else {
            // Use local solver
            result = performLocalPlateSolve(imagePath, config);
        }

        auto endTime = std::chrono::steady_clock::now();
        result.solveTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = "Plate solving error: " + std::string(e.what());
        spdlog::error("Plate solving failed: {}", e.what());
    }

    return result;
}

auto PlateSolveTaskBase::performLocalPlateSolve(const std::string& imagePath,
                                                const PlateSolveConfig& config)
    -> PlatesolveResult {
    PlatesolveResult result;
    result.solverUsed = config.solverType;
    result.usedRemote = false;

    try {
        // Get local solver instance
        auto solver = getLocalSolverInstance(config.solverType);

        // Prepare initial coordinates if available
        std::optional<Coordinates> initialCoords;
        if (config.useInitialCoordinates && config.raHint.has_value() &&
            config.decHint.has_value()) {
            initialCoords =
                Coordinates{config.raHint.value(), config.decHint.value()};
        }

        // Perform the solve
        auto solveResult = solver->solve(
            imagePath, initialCoords, config.fovWidth, config.fovHeight, 1920,
            1080);  // Default image dimensions

        // Convert result
        result.success = solveResult.success;
        result.coordinates = solveResult.coordinates;
        result.pixelScale = solveResult.pixscale;
        result.rotation = solveResult.positionAngle;
        result.fovWidth = config.fovWidth;
        result.fovHeight = config.fovHeight;

        if (!result.success) {
            result.errorMessage =
                "Local plate solving failed - no solution found";
        } else {
            spdlog::info(
                "Local plate solve successful: RA={:.6f}°, Dec={:.6f}°",
                result.coordinates.ra, result.coordinates.dec);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Local plate solving error: " + std::string(e.what());
        spdlog::error("Local plate solving failed: {}", e.what());
    }

    return result;
}

auto PlateSolveTaskBase::performRemotePlateSolve(const std::string& imagePath,
                                                 const PlateSolveConfig& config)
    -> PlatesolveResult {
    PlatesolveResult result;
    result.solverUsed = "remote_astrometry";
    result.usedRemote = true;

    try {
        // Get remote client instance
        auto client = getRemoteAstrometryClient();

        // Check if image file exists
        if (!std::filesystem::exists(imagePath)) {
            result.errorMessage = "Image file not found: " + imagePath;
            return result;
        }

        spdlog::info("Starting remote plate solve for image: {}", imagePath);

        // Configure submission parameters
        astrometry::SubmissionParams params;
        params.file_path = imagePath;
        params.publicly_visible = config.publiclyVisible;
        params.allow_commercial_use = config.license;
        params.allow_modifications = config.license;

        // Set scale estimate if available
        if (config.scaleEstimate > 0) {
            params.scale_type = astrometry::ScaleType::Estimate;
            params.scale_units = astrometry::ScaleUnits::ArcSecPerPix;
            params.scale_est = config.scaleEstimate;
            params.scale_err = config.scaleError;
        }

        // Set position hint if available
        if (config.raHint.has_value() && config.decHint.has_value()) {
            params.center_ra = config.raHint.value();
            params.center_dec = config.decHint.value();
            params.radius = config.searchRadius;
        }

        // Submit image for solving
        auto submissionId = client->submit_file(params);
        if (submissionId <= 0) {
            result.errorMessage = "Failed to submit image to remote service";
            return result;
        }

        spdlog::info("Submitted to remote service, submission ID: {}",
                     submissionId);

        // Wait for solving to complete with timeout
        auto timeoutSec = static_cast<int>(config.timeout);
        auto jobId = client->wait_for_job_completion(submissionId, timeoutSec);

        if (jobId > 0) {
            // Get job information
            auto jobInfo = client->get_job_info(jobId);

            if (jobInfo.status == "success" &&
                jobInfo.calibration.has_value()) {
                // Parse successful result
                result.success = true;
                result.coordinates.ra = jobInfo.calibration->ra;
                result.coordinates.dec = jobInfo.calibration->dec;
                result.rotation = jobInfo.calibration->orientation;
                result.pixelScale = jobInfo.calibration->pixscale;
                result.fovWidth = jobInfo.calibration->radius * 2.0;
                result.fovHeight = jobInfo.calibration->radius * 2.0;

                spdlog::info(
                    "Remote plate solve successful: RA={:.6f}°, Dec={:.6f}°",
                    result.coordinates.ra, result.coordinates.dec);
            } else {
                result.success = false;
                result.errorMessage =
                    "Remote solving failed with status: " + jobInfo.status;
            }
        } else {
            result.success = false;
            result.errorMessage = "Remote solving timeout or failure";
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage =
            "Remote plate solving error: " + std::string(e.what());
        spdlog::error("Remote plate solving failed: {}", e.what());
    }

    return result;
}

auto PlateSolveTaskBase::getCameraInstance() -> std::shared_ptr<void> {
    auto camera = GetPtr<void>(Constants::MAIN_CAMERA);
    if (!camera) {
        spdlog::error("Camera device not found in global manager");
        THROW_RUNTIME_ERROR("Camera device not available");
    }
    return camera.value();
}

auto PlateSolveTaskBase::getMountInstance() -> std::shared_ptr<void> {
    auto mount = GetPtr<void>(Constants::MAIN_TELESCOPE);
    if (!mount) {
        spdlog::error("Mount device not found in global manager");
        THROW_RUNTIME_ERROR("Mount device not available");
    }
    return mount.value();
}

auto PlateSolveTaskBase::hoursTodegrees(double hours) -> double {
    // 使用现有组件
    return lithium::tools::hourToDegree(hours);
}

auto PlateSolveTaskBase::degreesToHours(double degrees) -> double {
    // 使用现有组件
    return lithium::tools::degreeToHour(degrees);
}

auto PlateSolveTaskBase::calculateAngularDistance(const Coordinates& pos1,
                                                  const Coordinates& pos2)
    -> double {
    // 使用现有组件（如 convert.hpp 未提供，建议补充到 convert.hpp）
    // 这里直接实现，建议后续迁移到 convert.hpp
    double ra1 = lithium::tools::degreeToRad(pos1.ra);
    double dec1 = lithium::tools::degreeToRad(pos1.dec);
    double ra2 = lithium::tools::degreeToRad(pos2.ra);
    double dec2 = lithium::tools::degreeToRad(pos2.dec);

    double dra = ra2 - ra1;
    double ddec = dec2 - dec1;

    double a = std::sin(ddec / 2.0) * std::sin(ddec / 2.0) +
               std::cos(dec1) * std::cos(dec2) * std::sin(dra / 2.0) * std::sin(dra / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

    return lithium::tools::radToDegree(c);  // 使用组件转换回度
}

auto PlateSolveTaskBase::degreesToArcsec(double degrees) -> double {
    // 使用 croods.hpp 的 radiansToArcseconds
    return lithium::tools::radiansToArcseconds(lithium::tools::degreeToRad(degrees));
}

auto PlateSolveTaskBase::arcsecToDegrees(double arcsec) -> double {
    // 使用 croods.hpp 的 arcsecondsToRadians
    return lithium::tools::radToDegree(lithium::tools::arcsecondsToRadians(arcsec));
}

}  // namespace lithium::task::platesolve
