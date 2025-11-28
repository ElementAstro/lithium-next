#include "solver.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "server/models/api.hpp"
#include "client/astap/astap.hpp"

#include <mutex>
#include <optional>

namespace lithium::middleware {

using json = nlohmann::json;

namespace {
std::mutex solverMutex;
std::unique_ptr<AstapSolver> solverInstance;

auto ensureSolverConnected() -> bool {
    if (!solverInstance) {
        solverInstance = std::make_unique<AstapSolver>("server_astap");
    }

    if (!solverInstance->isConnected()) {
        auto candidates = solverInstance->scan();
        if (candidates.empty()) {
            LOG_ERROR( "ASTAP solver not found on system");
            return false;
        }

        if (!solverInstance->connect(candidates.front(), 5, 1)) {
            LOG_ERROR( "Failed to connect to ASTAP at %s", candidates.front().c_str());
            return false;
        }
    }

    return true;
}

auto buildResponseFromResult(const PlateSolveResult& result) -> json {
    json response;
    response["status"] = result.success ? "success" : "error";

    if (!result.success) {
        response["error"] = {
            {"code", "solver_failed"},
            {"message", "Plate solving failed"}
        };
        return response;
    }

    response["data"] = {
        {"solved", true},
        {"ra", result.coordinates.ra},
        {"dec", result.coordinates.dec},
        {"orientation", result.positionAngle},
        {"pixelScale", result.pixscale},
        {"radius", result.radius}
    };

    return response;
}
}  // namespace

auto solveImage(const std::string& filePath, 
                double raHint, 
                double decHint, 
                double scaleHint, 
                double radiusHint) -> json {
    LOG_INFO( "solveImage: Solving %s (RA: %f, Dec: %f, Radius: %f)", 
          filePath.c_str(), raHint, decHint, radiusHint);

    std::scoped_lock lock(solverMutex);

    if (!ensureSolverConnected()) {
        json response;
        response["status"] = "error";
        response["error"] = {
            {"code", "solver_unavailable"},
            {"message", "ASTAP solver not available on this system"}
        };
        return response;
    }

    std::optional<Coordinates> coords;
    if (raHint != 0.0 || decHint != 0.0) {
        coords = Coordinates{raHint, decHint};
    }

    // Use radius hint to derive approximate FOV height (ASTAP expects degrees)
    double fov = radiusHint > 0.0 ? radiusHint * 2.0 : 5.0;
    // Fallback to scale-derived FOV if radius not provided
    if (radiusHint <= 0.0 && scaleHint > 0.0) {
        // Assume 1024px reference frame
        constexpr double referencePixels = 1024.0;
        fov = (scaleHint / 3600.0) * referencePixels;
    }

    try {
        auto result = solverInstance->solve(filePath, coords, fov, fov, 0, 0);
        return buildResponseFromResult(result);
    } catch (const std::exception& ex) {
        LOG_ERROR( "solveImage: Exception while solving %s - %s", filePath.c_str(), ex.what());
        json response;
        response["status"] = "error";
        response["error"] = {
            {"code", "solver_exception"},
            {"message", ex.what()}
        };
        return response;
    }
}

auto blindSolve(const std::string& filePath) -> json {
    return solveImage(filePath, 0, 0, 0, 180);
}

} // namespace lithium::middleware
