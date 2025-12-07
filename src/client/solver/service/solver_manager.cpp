/*
 * solver_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "solver_manager.hpp"

#include "solver_factory.hpp"
#include "solver_type_registry.hpp"
#include "../plugin/solver_plugin_loader.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::solver {

auto SolverManager::getInstance() -> SolverManager& {
    static SolverManager instance;
    return instance;
}

auto SolverManager::setActiveSolver(const std::string& solverType) -> bool {
    auto& registry = SolverTypeRegistry::getInstance();

    // Check if type exists and is enabled
    if (!registry.hasType(solverType)) {
        LOG_ERROR("Solver type '{}' not registered", solverType);
        return false;
    }

    auto typeInfo = registry.getTypeInfo(solverType);
    if (!typeInfo || !typeInfo->enabled) {
        LOG_ERROR("Solver type '{}' is not enabled", solverType);
        return false;
    }

    // Create solver instance
    auto solver = createSolverFromType(solverType);
    if (!solver) {
        LOG_ERROR("Failed to create solver of type '{}'", solverType);
        return false;
    }

    // Set as active
    {
        std::unique_lock lock(mutex_);
        activeSolver_ = solver;
        activeSolverType_ = solverType;
    }

    LOG_INFO("Set active solver to: {}", solverType);
    return true;
}

auto SolverManager::getActiveSolver() const
    -> std::shared_ptr<client::SolverClient> {
    std::shared_lock lock(mutex_);
    return activeSolver_;
}

auto SolverManager::getActiveSolverType() const -> std::string {
    std::shared_lock lock(mutex_);
    return activeSolverType_;
}

auto SolverManager::getAvailableSolvers() const -> std::vector<SolverTypeInfo> {
    return SolverTypeRegistry::getInstance().getEnabledTypes();
}

auto SolverManager::autoSelectSolver() -> bool {
    auto& registry = SolverTypeRegistry::getInstance();
    auto bestType = registry.getBestType();

    if (!bestType) {
        LOG_WARN("No solver types available for auto-selection");
        return false;
    }

    return setActiveSolver(bestType->typeName);
}

auto SolverManager::solve(const SolveRequest& request)
    -> client::PlateSolveResult {
    std::shared_ptr<client::SolverClient> solver;
    {
        std::shared_lock lock(mutex_);
        solver = activeSolver_;
    }

    if (!solver) {
        LOG_ERROR("No active solver set");
        client::PlateSolveResult result;
        result.errorMessage = "No active solver";
        return result;
    }

    // Build coordinates hint if provided
    std::optional<client::Coordinates> coords;
    if (request.raHint && request.decHint) {
        coords = client::Coordinates{*request.raHint, *request.decHint};
    }

    // Calculate FOV from hints
    double fov = 5.0;  // Default FOV
    if (request.radiusHint) {
        fov = *request.radiusHint * 2.0;
    } else if (request.scaleHint) {
        // Estimate FOV from scale (assume 1024px reference)
        fov = (*request.scaleHint / 3600.0) * 1024.0;
    }

    try {
        auto result = solver->solve(
            request.imagePath,
            coords,
            fov, fov,
            0, 0  // Image dimensions (0 = auto-detect)
        );

        // Store last result
        {
            std::unique_lock lock(mutex_);
            lastResult_ = result;
        }

        return result;

    } catch (const std::exception& e) {
        LOG_ERROR("Solve exception: {}", e.what());
        client::PlateSolveResult result;
        result.errorMessage = e.what();
        return result;
    }
}

auto SolverManager::solveAsync(const SolveRequest& request)
    -> std::future<client::PlateSolveResult> {
    return std::async(std::launch::async, [this, request]() {
        return solve(request);
    });
}

auto SolverManager::blindSolve(const std::string& imagePath)
    -> client::PlateSolveResult {
    SolveRequest request;
    request.imagePath = imagePath;
    request.radiusHint = 180.0;  // Full sky search
    return solve(request);
}

void SolverManager::abort() {
    std::shared_ptr<client::SolverClient> solver;
    {
        std::shared_lock lock(mutex_);
        solver = activeSolver_;
    }

    if (solver) {
        solver->abort();
        LOG_INFO("Solve operation aborted");
    }
}

auto SolverManager::isSolving() const -> bool {
    std::shared_lock lock(mutex_);
    return activeSolver_ && activeSolver_->isSolving();
}

auto SolverManager::getLastResult() const -> client::PlateSolveResult {
    std::shared_lock lock(mutex_);
    return lastResult_;
}

auto SolverManager::getStatus() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json status;
    status["initialized"] = initialized_;
    status["activeSolverType"] = activeSolverType_;
    status["isSolving"] = activeSolver_ ? activeSolver_->isSolving() : false;
    status["hasActiveSolver"] = activeSolver_ != nullptr;

    // Available solvers
    auto available = SolverTypeRegistry::getInstance().getEnabledTypes();
    nlohmann::json solvers = nlohmann::json::array();
    for (const auto& type : available) {
        solvers.push_back({
            {"typeName", type.typeName},
            {"displayName", type.displayName},
            {"version", type.version},
            {"priority", type.priority}
        });
    }
    status["availableSolvers"] = solvers;

    // Last result summary
    if (lastResult_.success) {
        status["lastResult"] = {
            {"success", true},
            {"ra", lastResult_.coordinates.ra},
            {"dec", lastResult_.coordinates.dec},
            {"pixelScale", lastResult_.pixelScale},
            {"positionAngle", lastResult_.positionAngle},
            {"solveTime", lastResult_.solveTime}
        };
    } else {
        status["lastResult"] = {
            {"success", false},
            {"error", lastResult_.errorMessage}
        };
    }

    return status;
}

auto SolverManager::configure(const nlohmann::json& config) -> bool {
    std::unique_lock lock(mutex_);

    configuration_.merge_patch(config);

    // Apply configuration to active solver if present
    if (activeSolver_) {
        if (config.contains("options")) {
            client::SolverOptions opts;
            auto& optsJson = config["options"];

            if (optsJson.contains("scaleLow")) {
                opts.scaleLow = optsJson["scaleLow"];
            }
            if (optsJson.contains("scaleHigh")) {
                opts.scaleHigh = optsJson["scaleHigh"];
            }
            if (optsJson.contains("timeout")) {
                opts.timeout = optsJson["timeout"];
            }
            if (optsJson.contains("downsample")) {
                opts.downsample = optsJson["downsample"];
            }

            activeSolver_->setOptions(opts);
        }
    }

    LOG_DEBUG("Solver configuration updated");
    return true;
}

auto SolverManager::getConfiguration() const -> nlohmann::json {
    std::shared_lock lock(mutex_);
    return configuration_;
}

auto SolverManager::getOptionsSchema(const std::string& solverType) const
    -> nlohmann::json {
    std::string type = solverType;
    if (type.empty()) {
        std::shared_lock lock(mutex_);
        type = activeSolverType_;
    }

    if (type.empty()) {
        return nlohmann::json::object();
    }

    auto& registry = SolverTypeRegistry::getInstance();
    auto info = registry.getTypeInfo(type);

    if (info) {
        return info->optionSchema;
    }

    return nlohmann::json::object();
}

auto SolverManager::initialize(const nlohmann::json& config) -> bool {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        LOG_WARN("SolverManager already initialized");
        return true;
    }

    configuration_ = config;

    // Initialize registry
    SolverTypeRegistry::getInstance().initializeBuiltInTypes();

    initialized_ = true;
    LOG_INFO("SolverManager initialized");

    return true;
}

void SolverManager::shutdown() {
    std::unique_lock lock(mutex_);

    // Clear active solver
    if (activeSolver_) {
        activeSolver_->abort();
        activeSolver_.reset();
    }

    activeSolverType_.clear();
    initialized_ = false;

    // Shutdown plugin loader
    SolverPluginLoader::getInstance().shutdownAll();

    LOG_INFO("SolverManager shut down");
}

auto SolverManager::loadPlugins(const std::filesystem::path& directory)
    -> size_t {
    return SolverPluginLoader::getInstance().loadAllPlugins(
        directory, configuration_);
}

auto SolverManager::createSolverFromType(const std::string& typeName)
    -> std::shared_ptr<client::SolverClient> {
    auto& factory = SolverFactory::getInstance();
    return factory.createSolver(typeName, configuration_);
}

}  // namespace lithium::solver
