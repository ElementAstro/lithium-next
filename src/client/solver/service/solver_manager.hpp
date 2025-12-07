/*
 * solver_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver manager - facade for solver operations

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_MANAGER_HPP
#define LITHIUM_CLIENT_SOLVER_MANAGER_HPP

#include <future>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>

#include "atom/type/json.hpp"

#include "../common/solver_types.hpp"
#include "client/common/solver_client.hpp"

namespace lithium::solver {

// Forward declarations
class SolverTypeRegistry;
class SolverFactory;
class SolverPluginLoader;

/**
 * @brief Solve request parameters
 */
struct SolveRequest {
    std::string imagePath;                  ///< Path to image file
    std::optional<double> raHint;           ///< RA hint in degrees
    std::optional<double> decHint;          ///< Dec hint in degrees
    std::optional<double> scaleHint;        ///< Scale hint (arcsec/pixel)
    std::optional<double> radiusHint;       ///< Search radius in degrees
    std::optional<int> downsample;          ///< Downsample factor
    std::optional<int> timeout;             ///< Timeout in seconds
    nlohmann::json extraOptions;            ///< Solver-specific options

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j = {{"imagePath", imagePath}};
        if (raHint) j["raHint"] = *raHint;
        if (decHint) j["decHint"] = *decHint;
        if (scaleHint) j["scaleHint"] = *scaleHint;
        if (radiusHint) j["radiusHint"] = *radiusHint;
        if (downsample) j["downsample"] = *downsample;
        if (timeout) j["timeout"] = *timeout;
        if (!extraOptions.empty()) j["extraOptions"] = extraOptions;
        return j;
    }

    static auto fromJson(const nlohmann::json& j) -> SolveRequest {
        SolveRequest req;
        if (j.contains("imagePath")) req.imagePath = j["imagePath"];
        if (j.contains("raHint")) req.raHint = j["raHint"];
        if (j.contains("decHint")) req.decHint = j["decHint"];
        if (j.contains("scaleHint")) req.scaleHint = j["scaleHint"];
        if (j.contains("radiusHint")) req.radiusHint = j["radiusHint"];
        if (j.contains("downsample")) req.downsample = j["downsample"];
        if (j.contains("timeout")) req.timeout = j["timeout"];
        if (j.contains("extraOptions")) req.extraOptions = j["extraOptions"];
        return req;
    }
};

/**
 * @brief Unified solver manager
 *
 * Provides a high-level facade for solver operations:
 * - Solver selection and management
 * - Solve operations (sync and async)
 * - Configuration management
 * - Status tracking
 */
class SolverManager {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> SolverManager&;

    // Disable copy and move
    SolverManager(const SolverManager&) = delete;
    SolverManager& operator=(const SolverManager&) = delete;
    SolverManager(SolverManager&&) = delete;
    SolverManager& operator=(SolverManager&&) = delete;

    // ==================== Solver Selection ====================

    /**
     * @brief Set the active solver type
     * @param solverType Type name of solver to use
     * @return true if solver set successfully
     */
    auto setActiveSolver(const std::string& solverType) -> bool;

    /**
     * @brief Get the active solver
     * @return Active solver or nullptr
     */
    [[nodiscard]] auto getActiveSolver() const
        -> std::shared_ptr<client::SolverClient>;

    /**
     * @brief Get the active solver type name
     * @return Type name or empty string
     */
    [[nodiscard]] auto getActiveSolverType() const -> std::string;

    /**
     * @brief Get available solver types
     * @return Vector of solver type info
     */
    [[nodiscard]] auto getAvailableSolvers() const
        -> std::vector<SolverTypeInfo>;

    /**
     * @brief Auto-select the best available solver
     * @return true if a solver was selected
     */
    auto autoSelectSolver() -> bool;

    // ==================== Solve Operations ====================

    /**
     * @brief Solve an image synchronously
     * @param request Solve request parameters
     * @return Solve result
     */
    auto solve(const SolveRequest& request) -> client::PlateSolveResult;

    /**
     * @brief Solve an image asynchronously
     * @param request Solve request parameters
     * @return Future with solve result
     */
    auto solveAsync(const SolveRequest& request)
        -> std::future<client::PlateSolveResult>;

    /**
     * @brief Blind solve (no hints)
     * @param imagePath Path to image
     * @return Solve result
     */
    auto blindSolve(const std::string& imagePath) -> client::PlateSolveResult;

    /**
     * @brief Abort current solve operation
     */
    void abort();

    // ==================== Status ====================

    /**
     * @brief Check if solving is in progress
     * @return true if solving
     */
    [[nodiscard]] auto isSolving() const -> bool;

    /**
     * @brief Get last solve result
     * @return Last result
     */
    [[nodiscard]] auto getLastResult() const -> client::PlateSolveResult;

    /**
     * @brief Get manager status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> nlohmann::json;

    // ==================== Configuration ====================

    /**
     * @brief Configure the active solver
     * @param config Configuration options
     * @return true if configured successfully
     */
    auto configure(const nlohmann::json& config) -> bool;

    /**
     * @brief Get current configuration
     * @return Configuration JSON
     */
    [[nodiscard]] auto getConfiguration() const -> nlohmann::json;

    /**
     * @brief Get solver-specific options schema
     * @param solverType Solver type (empty for active)
     * @return JSON Schema
     */
    [[nodiscard]] auto getOptionsSchema(const std::string& solverType = {}) const
        -> nlohmann::json;

    // ==================== Initialization ====================

    /**
     * @brief Initialize the manager
     * @param config Initial configuration
     * @return true if initialized
     */
    auto initialize(const nlohmann::json& config = {}) -> bool;

    /**
     * @brief Shutdown the manager
     */
    void shutdown();

    /**
     * @brief Load solver plugins from directory
     * @param directory Plugin directory
     * @return Number of plugins loaded
     */
    auto loadPlugins(const std::filesystem::path& directory) -> size_t;

private:
    SolverManager() = default;
    ~SolverManager() = default;

    /**
     * @brief Create solver from type
     */
    auto createSolverFromType(const std::string& typeName)
        -> std::shared_ptr<client::SolverClient>;

    mutable std::shared_mutex mutex_;

    std::shared_ptr<client::SolverClient> activeSolver_;
    std::string activeSolverType_;
    client::PlateSolveResult lastResult_;

    nlohmann::json configuration_;
    bool initialized_{false};
};

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_MANAGER_HPP
