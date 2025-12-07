/*
 * astrometry_solver_plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Astrometry.net Solver Plugin - Implements ISolverPlugin for
             local solve-field and remote nova.astrometry.net API

**************************************************/

#ifndef LITHIUM_CLIENT_ASTROMETRY_SOLVER_PLUGIN_HPP
#define LITHIUM_CLIENT_ASTROMETRY_SOLVER_PLUGIN_HPP

#include "client/solver/plugin/solver_plugin_interface.hpp"

#include "../astrometry_client.hpp"
#include "../remote/client.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::client::astrometry {

/**
 * @brief Solving mode for Astrometry.net
 */
enum class SolveMode {
    Local,   // Use local solve-field binary
    Remote,  // Use nova.astrometry.net API
    Auto     // Try local first, fallback to remote
};

/**
 * @brief Astrometry.net Solver Plugin
 *
 * Implements the ISolverPlugin interface for Astrometry.net plate solver.
 * Supports both local solve-field and remote nova.astrometry.net API.
 *
 * Features:
 * - Local binary detection and version checking
 * - Remote API authentication and job management
 * - Index file discovery and management
 * - Multiple solver type registration (local/remote)
 */
class AstrometrySolverPlugin : public solver::SolverPluginBase {
public:
    /**
     * @brief Default plugin name
     */
    static constexpr const char* PLUGIN_NAME = "Astrometry";

    /**
     * @brief Plugin version
     */
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /**
     * @brief Local solver type name
     */
    static constexpr const char* SOLVER_TYPE_LOCAL = "Astrometry-Local";

    /**
     * @brief Remote solver type name
     */
    static constexpr const char* SOLVER_TYPE_REMOTE = "Astrometry-Remote";

    /**
     * @brief Construct Astrometry solver plugin
     */
    AstrometrySolverPlugin();

    /**
     * @brief Destructor
     */
    ~AstrometrySolverPlugin() override;

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
    [[nodiscard]] auto hasExternalBinary() const -> bool override {
        return true;  // Local mode requires solve-field
    }

    /**
     * @brief Find solve-field binary
     */
    [[nodiscard]] auto findBinary()
        -> std::optional<std::filesystem::path> override;

    /**
     * @brief Validate solve-field binary
     */
    [[nodiscard]] auto validateBinary(const std::filesystem::path& path)
        -> bool override;

    /**
     * @brief Get solve-field version
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

    // ==================== Astrometry-Specific Methods ====================

    /**
     * @brief Get available index file directories
     * @return Vector of index directory paths
     */
    [[nodiscard]] auto getIndexDirectories() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Add index file directory
     * @param directory Path to index directory
     */
    void addIndexDirectory(const std::filesystem::path& directory);

    /**
     * @brief Scan for index files in configured directories
     * @return Vector of found index file paths
     */
    [[nodiscard]] auto scanIndexFiles() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Get index file coverage information
     * @return JSON with index coverage details
     */
    [[nodiscard]] auto getIndexCoverage() const -> nlohmann::json;

    /**
     * @brief Check if remote API is available
     * @return true if API is reachable
     */
    [[nodiscard]] auto isRemoteAvailable() const -> bool;

    /**
     * @brief Set remote API configuration
     * @param apiKey API key for nova.astrometry.net
     * @param apiUrl Optional custom API URL
     */
    void setRemoteConfig(const std::string& apiKey,
                         const std::string& apiUrl = "");

    /**
     * @brief Get current solve mode
     */
    [[nodiscard]] auto getSolveMode() const -> SolveMode;

    /**
     * @brief Set solve mode
     */
    void setSolveMode(SolveMode mode);

    /**
     * @brief Create local solver instance
     */
    [[nodiscard]] auto createLocalSolver(const std::string& solverId,
                                          const nlohmann::json& config)
        -> std::shared_ptr<SolverClient>;

    /**
     * @brief Create remote solver wrapper
     */
    [[nodiscard]] auto createRemoteSolver(const std::string& solverId,
                                           const nlohmann::json& config)
        -> std::shared_ptr<SolverClient>;

private:
    /**
     * @brief Build local solver type info
     */
    [[nodiscard]] auto buildLocalTypeInfo() const -> solver::SolverTypeInfo;

    /**
     * @brief Build remote solver type info
     */
    [[nodiscard]] auto buildRemoteTypeInfo() const -> solver::SolverTypeInfo;

    /**
     * @brief Build local options JSON schema
     */
    [[nodiscard]] auto buildLocalOptionsSchema() const -> nlohmann::json;

    /**
     * @brief Build remote options JSON schema
     */
    [[nodiscard]] auto buildRemoteOptionsSchema() const -> nlohmann::json;

    /**
     * @brief Scan for solve-field binary in common locations
     */
    [[nodiscard]] auto scanForBinary() const
        -> std::optional<std::filesystem::path>;

    /**
     * @brief Extract version from solve-field binary
     */
    [[nodiscard]] auto extractVersion(const std::filesystem::path& binary) const
        -> std::string;

    /**
     * @brief Get default index directories based on platform
     */
    [[nodiscard]] auto getDefaultIndexDirectories() const
        -> std::vector<std::filesystem::path>;

    // Index directories
    std::vector<std::filesystem::path> indexDirectories_;

    // Remote API configuration
    std::string apiKey_;
    std::string apiUrl_{"http://nova.astrometry.net/api"};
    std::unique_ptr<::astrometry::AstrometryClient> remoteClient_;

    // Solve mode
    SolveMode solveMode_{SolveMode::Auto};

    // Statistics
    std::atomic<size_t> localSolveCount_{0};
    std::atomic<size_t> remoteSolveCount_{0};
    std::atomic<size_t> localSuccessCount_{0};
    std::atomic<size_t> remoteSuccessCount_{0};
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

}  // namespace lithium::client::astrometry

#endif  // LITHIUM_CLIENT_ASTROMETRY_SOLVER_PLUGIN_HPP
