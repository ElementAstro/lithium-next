/*
 * astap_solver_plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASTAP Solver Plugin - Implements ISolverPlugin for ASTAP solver

**************************************************/

#ifndef LITHIUM_CLIENT_ASTAP_SOLVER_PLUGIN_HPP
#define LITHIUM_CLIENT_ASTAP_SOLVER_PLUGIN_HPP

#include "client/solver/plugin/solver_plugin_interface.hpp"

#include "../astap_client.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::client::astap {

/**
 * @brief ASTAP Solver Plugin
 *
 * Implements the ISolverPlugin interface for ASTAP plate solver.
 * - Registers ASTAP solver type
 * - Creates ASTAP solver instances
 * - Manages ASTAP binary detection
 */
class AstapSolverPlugin : public solver::SolverPluginBase {
public:
    /**
     * @brief Default plugin name
     */
    static constexpr const char* PLUGIN_NAME = "ASTAP";

    /**
     * @brief Plugin version
     */
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /**
     * @brief Solver type name
     */
    static constexpr const char* SOLVER_TYPE = "ASTAP";

    /**
     * @brief Construct ASTAP solver plugin
     */
    AstapSolverPlugin();

    /**
     * @brief Destructor
     */
    ~AstapSolverPlugin() override;

    // ==================== IPlugin Interface ====================

    /**
     * @brief Get plugin name
     */
    [[nodiscard]] auto getName() const -> std::string override {
        return PLUGIN_NAME;
    }

    /**
     * @brief Get plugin version
     */
    [[nodiscard]] auto getVersion() const -> std::string override {
        return PLUGIN_VERSION;
    }

    /**
     * @brief Initialize the plugin
     */
    auto initialize(const nlohmann::json& config) -> bool override;

    /**
     * @brief Shutdown the plugin
     */
    void shutdown() override;

    // ==================== ISolverPlugin Interface ====================

    /**
     * @brief Get solver types provided by this plugin
     */
    [[nodiscard]] auto getSolverTypes() const
        -> std::vector<solver::SolverTypeInfo> override;

    /**
     * @brief Register solver types with the registry
     */
    auto registerSolverTypes(solver::SolverTypeRegistry& registry)
        -> size_t override;

    /**
     * @brief Unregister solver types from the registry
     */
    auto unregisterSolverTypes(solver::SolverTypeRegistry& registry)
        -> size_t override;

    /**
     * @brief Register solver creators with the factory
     */
    void registerSolverCreators(solver::SolverFactory& factory) override;

    /**
     * @brief Unregister solver creators from the factory
     */
    void unregisterSolverCreators(solver::SolverFactory& factory) override;

    /**
     * @brief Create a solver instance
     */
    [[nodiscard]] auto createSolver(const std::string& solverId,
                                     const nlohmann::json& config)
        -> std::shared_ptr<SolverClient> override;

    /**
     * @brief Check if external binary is required
     */
    [[nodiscard]] auto hasExternalBinary() const -> bool override { return true; }

    /**
     * @brief Find ASTAP binary
     */
    [[nodiscard]] auto findBinary()
        -> std::optional<std::filesystem::path> override;

    /**
     * @brief Validate ASTAP binary
     */
    [[nodiscard]] auto validateBinary(const std::filesystem::path& path)
        -> bool override;

    /**
     * @brief Get ASTAP binary version
     */
    [[nodiscard]] auto getBinaryVersion() const -> std::string override;

    /**
     * @brief Set custom binary path
     */
    auto setBinaryPath(const std::filesystem::path& path) -> bool override;

    /**
     * @brief Get current binary path
     */
    [[nodiscard]] auto getBinaryPath() const
        -> std::optional<std::filesystem::path> override;

    /**
     * @brief Get default solver options
     */
    [[nodiscard]] auto getDefaultOptions() const -> nlohmann::json override;

    /**
     * @brief Validate solver options
     */
    [[nodiscard]] auto validateOptions(const nlohmann::json& options)
        -> solver::SolverResult<bool> override;

    // ==================== ASTAP-Specific Methods ====================

    /**
     * @brief Get available database paths
     * @return Vector of database paths
     */
    [[nodiscard]] auto getDatabasePaths() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Set preferred database path
     * @param dbPath Path to database directory
     */
    void setPreferredDatabase(const std::filesystem::path& dbPath);

    /**
     * @brief Check if database is available
     * @return true if at least one database is available
     */
    [[nodiscard]] auto isDatabaseAvailable() const -> bool;

private:
    /**
     * @brief Build solver type info
     */
    [[nodiscard]] auto buildTypeInfo() const -> solver::SolverTypeInfo;

    /**
     * @brief Build options JSON schema
     */
    [[nodiscard]] auto buildOptionsSchema() const -> nlohmann::json;

    /**
     * @brief Scan for ASTAP binary in common locations
     */
    [[nodiscard]] auto scanForBinary() const
        -> std::optional<std::filesystem::path>;

    /**
     * @brief Extract version from ASTAP binary
     */
    [[nodiscard]] auto extractVersion(const std::filesystem::path& binary) const
        -> std::string;

    // Database path
    std::optional<std::filesystem::path> databasePath_;

    // Statistics
    std::atomic<size_t> solveCount_{0};
    std::atomic<size_t> successCount_{0};
    mutable std::mutex mutex_;
};

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

/**
 * @brief Create plugin instance
 */
LITHIUM_EXPORT auto createSolverPlugin() -> solver::ISolverPlugin*;

/**
 * @brief Destroy plugin instance
 */
LITHIUM_EXPORT void destroySolverPlugin(solver::ISolverPlugin* plugin);

/**
 * @brief Get plugin API version
 */
LITHIUM_EXPORT auto getSolverPluginApiVersion() -> int;

/**
 * @brief Get plugin metadata
 */
LITHIUM_EXPORT auto getSolverPluginMetadata() -> solver::SolverPluginMetadata;

}  // extern "C"

}  // namespace lithium::client::astap

#endif  // LITHIUM_CLIENT_ASTAP_SOLVER_PLUGIN_HPP
