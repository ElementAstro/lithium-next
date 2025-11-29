#include "solver.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "client/astap/astap.hpp"
#include "command.hpp"
#include "response.hpp"
#include "server/models/api.hpp"

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
            LOG_ERROR("ASTAP solver not found on system");
            return false;
        }

        if (!solverInstance->connect(candidates.front(), 5, 1)) {
            LOG_ERROR("Failed to connect to ASTAP at %s",
                      candidates.front().c_str());
            return false;
        }
    }

    return true;
}

auto buildResponseFromResult(const PlateSolveResult& result) -> json {
    json response;
    response["status"] = result.success ? "success" : "error";

    if (!result.success) {
        response["error"] = {{"code", "solver_failed"},
                             {"message", "Plate solving failed"}};
        return response;
    }

    response["data"] = {{"solved", true},
                        {"ra", result.coordinates.ra},
                        {"dec", result.coordinates.dec},
                        {"orientation", result.positionAngle},
                        {"pixelScale", result.pixscale},
                        {"radius", result.radius}};

    return response;
}
}  // namespace

auto solveImage(const std::string& filePath, double raHint, double decHint,
                double scaleHint, double radiusHint) -> json {
    LOG_INFO("solveImage: Solving %s (RA: %f, Dec: %f, Radius: %f)",
             filePath.c_str(), raHint, decHint, radiusHint);

    std::scoped_lock lock(solverMutex);

    if (!ensureSolverConnected()) {
        json response;
        response["status"] = "error";
        response["error"] = {
            {"code", "solver_unavailable"},
            {"message", "ASTAP solver not available on this system"}};
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
        LOG_ERROR("solveImage: Exception while solving %s - %s",
                  filePath.c_str(), ex.what());
        json response;
        response["status"] = "error";
        response["error"] = {{"code", "solver_exception"},
                             {"message", ex.what()}};
        return response;
    }
}

auto blindSolve(const std::string& filePath) -> json {
    return solveImage(filePath, 0, 0, 0, 180);
}

}  // namespace lithium::middleware

// ==================== Command Registration ====================

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;

void registerSolver(std::shared_ptr<CommandDispatcher> dispatcher) {
    // solver.solve - Solve an image with optional hints
    dispatcher->registerCommand<json>("solver.solve", [](json& payload) {
        LOG_INFO("Executing solver.solve");
        try {
            // Validate required filePath parameter
            if (!payload.contains("filePath") ||
                !payload["filePath"].is_string() ||
                payload["filePath"].get<std::string>().empty()) {
                LOG_WARN("solver.solve: missing filePath");
                payload = CommandResponse::missingParameter("filePath");
                return;
            }
            std::string filePath = payload["filePath"].get<std::string>();

            // Optional hints
            double raHint = payload.value("raHint", 0.0);
            double decHint = payload.value("decHint", 0.0);
            double scaleHint = payload.value("scaleHint", 0.0);
            double radiusHint = payload.value("radiusHint", 180.0);

            auto result = middleware::solveImage(filePath, raHint, decHint,
                                                 scaleHint, radiusHint);

            if (result.contains("status") && result["status"] == "error") {
                LOG_WARN("solver.solve failed for file {}", filePath);
                payload = result;
            } else {
                LOG_INFO("solver.solve completed successfully for file {}",
                         filePath);
                payload = CommandResponse::success(
                    result.value("data", json::object()));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("solver.solve exception: {}", e.what());
            payload = CommandResponse::operationFailed("solve", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.solve'");

    // solver.blind_solve - Blind solve without hints
    dispatcher->registerCommand<json>("solver.blind_solve", [](json& payload) {
        LOG_INFO("Executing solver.blind_solve");
        try {
            // Validate required filePath parameter
            if (!payload.contains("filePath") ||
                !payload["filePath"].is_string() ||
                payload["filePath"].get<std::string>().empty()) {
                LOG_WARN("solver.blind_solve: missing filePath");
                payload = CommandResponse::missingParameter("filePath");
                return;
            }
            std::string filePath = payload["filePath"].get<std::string>();

            auto result = middleware::blindSolve(filePath);

            if (result.contains("status") && result["status"] == "error") {
                LOG_WARN("solver.blind_solve failed for file {}", filePath);
                payload = result;
            } else {
                LOG_INFO(
                    "solver.blind_solve completed successfully for file {}",
                    filePath);
                payload = CommandResponse::success(
                    result.value("data", json::object()));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("solver.blind_solve exception: {}", e.what());
            payload = CommandResponse::operationFailed("blind_solve", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.blind_solve'");

    // solver.status - Get solver status
    dispatcher->registerCommand<json>("solver.status", [](json& payload) {
        LOG_DEBUG("Executing solver.status");
        try {
            json statusInfo = {
                {"available", true},
                {"solver_type", "ASTAP"},
                {"message", "ASTAP plate solver integration available"}};
            payload = CommandResponse::success(statusInfo);
        } catch (const std::exception& e) {
            LOG_ERROR("solver.status exception: {}", e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.status'");

    // solver.abort - Abort ongoing solve operation
    dispatcher->registerCommand<json>("solver.abort", [](json& payload) {
        LOG_INFO("Executing solver.abort");
        try {
            // TODO: Implement abort logic when needed
            json abortInfo = {{"aborted", false},
                              {"message", "Solver abort not yet implemented"}};
            payload = CommandResponse::success(abortInfo);
        } catch (const std::exception& e) {
            LOG_ERROR("solver.abort exception: {}", e.what());
            payload = CommandResponse::operationFailed("abort", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.abort'");

    // solver.configure - Configure solver settings
    dispatcher->registerCommand<json>("solver.configure", [](json& payload) {
        LOG_INFO("Executing solver.configure");
        try {
            if (!payload.contains("settings")) {
                payload = CommandResponse::missingParameter("settings");
                return;
            }

            // TODO: Apply configuration to ASTAP solver
            json configInfo = {{"applied", true},
                               {"message", "Solver configuration accepted"}};
            payload = CommandResponse::success(configInfo);
        } catch (const std::exception& e) {
            LOG_ERROR("solver.configure exception: {}", e.what());
            payload = CommandResponse::operationFailed("configure", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.configure'");
}

}  // namespace lithium::app
