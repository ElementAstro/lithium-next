/*
 * stellarsolver_plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: StellarSolver Plugin - Implements ISolverPlugin for StellarSolver
             library (Qt-based plate solver and star extractor)

**************************************************/

#ifndef LITHIUM_CLIENT_STELLARSOLVER_PLUGIN_HPP
#define LITHIUM_CLIENT_STELLARSOLVER_PLUGIN_HPP

#include "client/solver/plugin/solver_plugin_interface.hpp"

#include "../stellarsolver_client.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::client::stellarsolver {

/**
 * @brief StellarSolver Plugin
 *
 * Implements the ISolverPlugin interface for StellarSolver library.
 * Unlike ASTAP and Astrometry.net, StellarSolver is a library that
 * doesn't require external binaries.
 *
 * Features:
 * - Built-in star extraction (no external SExtractor needed)
 * - HFR calculation for focusing
 * - Multiple solving profiles
 * - Qt-based (requires Qt5/Qt6)
 */
class StellarSolverPlugin : public solver::SolverPluginBase {
public:
    /**
     * @brief Default plugin name
     */
    static constexpr const char* PLUGIN_NAME = "StellarSolver";

    /**
     * @brief Plugin version
     */
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /**
     * @brief Solver type name
     */
    static constexpr const char* SOLVER_TYPE = "StellarSolver";

    /**
     * @brief Extractor type name (for star extraction only)
     */
    static constexpr const char* EXTRACTOR_TYPE = "StellarSolver-Extractor";

    /**
     * @brief Construct StellarSolver plugin
     */
    StellarSolverPlugin();

    /**
     * @brief Destructor
     */
    ~StellarSolverPlugin() override;

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
     * @note StellarSolver is a library, no external binary needed
     */
    [[nodiscard]] auto hasExternalBinary() const -> bool override {
        return false;
    }

    /**
     * @brief Find binary (N/A for StellarSolver)
     */
    [[nodiscard]] auto findBinary()
        -> std::optional<std::filesystem::path> override {
        return std::nullopt;  // Not applicable
    }

    /**
     * @brief Validate binary (N/A for StellarSolver)
     */
    [[nodiscard]] auto validateBinary(const std::filesystem::path& /*path*/)
        -> bool override {
        return false;  // Not applicable
    }

    /**
     * @brief Get library version
     */
    [[nodiscard]] auto getBinaryVersion() const -> std::string override;

    /**
     * @brief Set binary path (N/A for StellarSolver)
     */
    auto setBinaryPath(const std::filesystem::path& /*path*/) -> bool override {
        return false;  // Not applicable
    }

    /**
     * @brief Get binary path (N/A for StellarSolver)
     */
    [[nodiscard]] auto getBinaryPath() const
        -> std::optional<std::filesystem::path> override {
        return std::nullopt;  // Not applicable
    }

    /**
     * @brief Get default solver options
     */
    [[nodiscard]] auto getDefaultOptions() const -> nlohmann::json override;

    /**
     * @brief Validate solver options
     */
    [[nodiscard]] auto validateOptions(const nlohmann::json& options)
        -> solver::SolverResult<bool> override;

    // ==================== StellarSolver-Specific Methods ====================

    /**
     * @brief Check if StellarSolver library is available
     * @return true if library is linked and functional
     */
    [[nodiscard]] auto isLibraryAvailable() const -> bool;

    /**
     * @brief Get available parameter profiles
     * @return Vector of profile names
     */
    [[nodiscard]] auto getAvailableProfiles() const
        -> std::vector<std::string>;

    /**
     * @brief Get profile parameters
     * @param profileName Profile name
     * @return JSON with profile parameters
     */
    [[nodiscard]] auto getProfileParameters(const std::string& profileName) const
        -> nlohmann::json;

    /**
     * @brief Get default index folder paths
     * @return Vector of default index paths
     */
    [[nodiscard]] auto getDefaultIndexPaths() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Add custom index folder
     * @param folder Path to index folder
     */
    void addIndexFolder(const std::filesystem::path& folder);

    /**
     * @brief Get current index folders
     */
    [[nodiscard]] auto getIndexFolders() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Check if Qt is properly initialized
     */
    [[nodiscard]] auto isQtInitialized() const -> bool;

    /**
     * @brief Create extractor-only instance (for star extraction without solving)
     */
    [[nodiscard]] auto createExtractor(const std::string& extractorId,
                                        const nlohmann::json& config)
        -> std::shared_ptr<StellarSolverClient>;

private:
    /**
     * @brief Build solver type info
     */
    [[nodiscard]] auto buildSolverTypeInfo() const -> solver::SolverTypeInfo;

    /**
     * @brief Build extractor type info
     */
    [[nodiscard]] auto buildExtractorTypeInfo() const -> solver::SolverTypeInfo;

    /**
     * @brief Build options JSON schema
     */
    [[nodiscard]] auto buildOptionsSchema() const -> nlohmann::json;

    /**
     * @brief Get library version string
     */
    [[nodiscard]] auto detectLibraryVersion() const -> std::string;

    /**
     * @brief Initialize Qt application context if needed
     */
    auto initializeQt() -> bool;

    // Index folders
    std::vector<std::filesystem::path> indexFolders_;

    // Library version
    std::string libraryVersion_;

    // Qt availability
    bool qtAvailable_{false};

    // Statistics
    std::atomic<size_t> solveCount_{0};
    std::atomic<size_t> extractCount_{0};
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

}  // namespace lithium::client::stellarsolver

#endif  // LITHIUM_CLIENT_STELLARSOLVER_PLUGIN_HPP
