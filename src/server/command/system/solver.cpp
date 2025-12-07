/*
 * solver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver command handlers - Multi-solver support via plugin system

**************************************************/

#include "solver.hpp"

#include "../command.hpp"
#include "../response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "client/solver/service/solver_manager.hpp"
#include "server/models/api.hpp"

#include <mutex>
#include <optional>

namespace lithium::middleware {

using json = nlohmann::json;
using solver::SolverManager;

namespace {
std::mutex solverMutex;
bool initialized = false;

auto ensureSolverInitialized() -> bool {
    if (initialized) {
        return true;
    }

    auto& manager = SolverManager::getInstance();
    if (!manager.initialize({})) {
        LOG_ERROR("Failed to initialize SolverManager");
        return false;
    }

    // Auto-select best available solver
    if (!manager.autoSelectSolver()) {
        LOG_WARN("No solvers available for auto-selection");
    }

    initialized = true;
    return true;
}

auto buildResponseFromResult(const client::PlateSolveResult& result) -> json {
    json response;
    response["status"] = result.success ? "success" : "error";

    if (!result.success) {
        response["error"] = {{"code", "solver_failed"},
                             {"message", result.errorMessage.empty()
                                             ? "Plate solving failed"
                                             : result.errorMessage}};
        return response;
    }

    response["data"] = {{"solved", true},
                        {"ra", result.coordinates.ra},
                        {"dec", result.coordinates.dec},
                        {"orientation", result.positionAngle},
                        {"pixelScale", result.pixelScale},
                        {"radius", result.radius},
                        {"solveTime", result.solveTime}};

    return response;
}
}  // namespace

auto solveImage(const std::string& filePath, double raHint, double decHint,
                double scaleHint, double radiusHint) -> json {
    LOG_INFO("solveImage: Solving {} (RA: {}, Dec: {}, Radius: {})",
             filePath, raHint, decHint, radiusHint);

    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return {{"status", "error"},
                {"error",
                 {{"code", "solver_unavailable"},
                  {"message", "Solver system not initialized"}}}};
    }

    auto& manager = SolverManager::getInstance();
    auto activeSolver = manager.getActiveSolver();

    if (!activeSolver) {
        return {{"status", "error"},
                {"error",
                 {{"code", "no_active_solver"},
                  {"message", "No active solver configured"}}}};
    }

    // Build solve request
    solver::SolveRequest request;
    request.imagePath = filePath;

    if (raHint != 0.0 || decHint != 0.0) {
        request.raHint = raHint;
        request.decHint = decHint;
    }

    if (scaleHint > 0.0) {
        request.scaleHint = scaleHint;
    }

    if (radiusHint > 0.0 && radiusHint < 180.0) {
        request.radiusHint = radiusHint;
    }

    try {
        auto result = manager.solve(request);
        return buildResponseFromResult(result);
    } catch (const std::exception& ex) {
        LOG_ERROR("solveImage: Exception while solving {} - {}", filePath,
                  ex.what());
        return {{"status", "error"},
                {"error", {{"code", "solver_exception"}, {"message", ex.what()}}}};
    }
}

auto blindSolve(const std::string& filePath) -> json {
    LOG_INFO("blindSolve: Blind solving {}", filePath);

    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return {{"status", "error"},
                {"error",
                 {{"code", "solver_unavailable"},
                  {"message", "Solver system not initialized"}}}};
    }

    auto& manager = SolverManager::getInstance();

    try {
        auto result = manager.blindSolve(filePath);
        return buildResponseFromResult(result);
    } catch (const std::exception& ex) {
        LOG_ERROR("blindSolve: Exception while solving {} - {}", filePath,
                  ex.what());
        return {{"status", "error"},
                {"error", {{"code", "solver_exception"}, {"message", ex.what()}}}};
    }
}

auto getSolverStatus() -> json {
    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return {{"available", false},
                {"message", "Solver system not initialized"}};
    }

    return SolverManager::getInstance().getStatus();
}

auto listAvailableSolvers() -> json {
    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return json::array();
    }

    auto solvers = SolverManager::getInstance().getAvailableSolvers();
    json result = json::array();

    for (const auto& solver : solvers) {
        result.push_back({{"typeName", solver.typeName},
                          {"displayName", solver.displayName},
                          {"version", solver.version},
                          {"priority", solver.priority},
                          {"enabled", solver.enabled}});
    }

    return result;
}

auto selectSolver(const std::string& solverType) -> bool {
    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return false;
    }

    return SolverManager::getInstance().setActiveSolver(solverType);
}

auto configureSolver(const json& config) -> bool {
    std::scoped_lock lock(solverMutex);

    if (!ensureSolverInitialized()) {
        return false;
    }

    return SolverManager::getInstance().configure(config);
}

auto abortSolve() -> bool {
    std::scoped_lock lock(solverMutex);

    auto& manager = SolverManager::getInstance();
    if (manager.isSolving()) {
        manager.abort();
        return true;
    }
    return false;
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

    // solver.status - Get comprehensive solver status
    dispatcher->registerCommand<json>("solver.status", [](json& payload) {
        LOG_DEBUG("Executing solver.status");
        try {
            auto status = middleware::getSolverStatus();
            payload = CommandResponse::success(status);
        } catch (const std::exception& e) {
            LOG_ERROR("solver.status exception: {}", e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.status'");

    // solver.list - List all available solvers
    dispatcher->registerCommand<json>("solver.list", [](json& payload) {
        LOG_DEBUG("Executing solver.list");
        try {
            auto solvers = middleware::listAvailableSolvers();
            payload = CommandResponse::success({{"solvers", solvers}});
        } catch (const std::exception& e) {
            LOG_ERROR("solver.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.list'");

    // solver.select - Select active solver type
    dispatcher->registerCommand<json>("solver.select", [](json& payload) {
        LOG_INFO("Executing solver.select");
        try {
            if (!payload.contains("solverType") ||
                !payload["solverType"].is_string()) {
                payload = CommandResponse::missingParameter("solverType");
                return;
            }

            std::string solverType = payload["solverType"].get<std::string>();
            bool success = middleware::selectSolver(solverType);

            if (success) {
                payload = CommandResponse::success(
                    {{"selected", true}, {"solverType", solverType}});
            } else {
                payload = {{"status", "error"},
                           {"error",
                            {{"code", "solver_not_found"},
                             {"message",
                              "Solver type '" + solverType + "' not available"}}}};
            }
        } catch (const std::exception& e) {
            LOG_ERROR("solver.select exception: {}", e.what());
            payload = CommandResponse::operationFailed("select", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.select'");

    // solver.abort - Abort ongoing solve operation
    dispatcher->registerCommand<json>("solver.abort", [](json& payload) {
        LOG_INFO("Executing solver.abort");
        try {
            bool aborted = middleware::abortSolve();
            payload = CommandResponse::success(
                {{"aborted", aborted},
                 {"message", aborted ? "Solve operation aborted"
                                     : "No solve operation in progress"}});
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

            bool success = middleware::configureSolver(payload["settings"]);

            if (success) {
                payload = CommandResponse::success(
                    {{"applied", true},
                     {"message", "Solver configuration applied"}});
            } else {
                payload = {
                    {"status", "error"},
                    {"error",
                     {{"code", "config_failed"},
                      {"message", "Failed to apply solver configuration"}}}};
            }
        } catch (const std::exception& e) {
            LOG_ERROR("solver.configure exception: {}", e.what());
            payload = CommandResponse::operationFailed("configure", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.configure'");

    // solver.options - Get/set solver-specific options
    dispatcher->registerCommand<json>("solver.options", [](json& payload) {
        LOG_DEBUG("Executing solver.options");
        try {
            std::string solverType = payload.value("solverType", "");
            auto& manager = solver::SolverManager::getInstance();

            if (payload.contains("set") && payload["set"].is_object()) {
                // Set options
                bool success = manager.configure(payload["set"]);
                payload = CommandResponse::success(
                    {{"applied", success},
                     {"message", success ? "Options applied"
                                         : "Failed to apply options"}});
            } else {
                // Get options schema
                auto schema = manager.getOptionsSchema(solverType);
                auto config = manager.getConfiguration();
                payload = CommandResponse::success(
                    {{"schema", schema}, {"current", config}});
            }
        } catch (const std::exception& e) {
            LOG_ERROR("solver.options exception: {}", e.what());
            payload = CommandResponse::operationFailed("options", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'solver.options'");
}

}  // namespace lithium::app
