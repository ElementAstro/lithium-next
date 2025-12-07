/*
 * solver_plugin_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver plugin interface for extending plate solver support

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_PLUGIN_INTERFACE_HPP
#define LITHIUM_CLIENT_SOLVER_PLUGIN_INTERFACE_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "server/plugin/plugin_interface.hpp"

#include "../common/solver_types.hpp"

// Forward declarations
namespace lithium::client {
class SolverClient;
}

namespace lithium::solver {

// Forward declarations
class SolverTypeRegistry;
class SolverFactory;

/**
 * @brief Solver plugin metadata extending server plugin metadata
 */
struct SolverPluginMetadata : public server::plugin::PluginMetadata {
    std::string solverType;              ///< Solver type identifier (e.g., "astap", "astrometry")
    std::string backendVersion;          ///< Version of external solver binary
    bool supportsBlindSolve{true};       ///< Supports solving without hints
    bool supportsAbort{true};            ///< Supports aborting solve operation
    bool requiresExternalBinary{true};   ///< Requires external executable
    std::vector<std::string> supportedFormats;  ///< Supported image formats

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> SolverPluginMetadata;
};

/**
 * @brief Base interface for solver plugins
 *
 * Solver plugins extend server plugins with solver-specific functionality:
 * - Solver type registration
 * - Solver factory registration
 * - External binary management
 * - Solve operation management
 */
class ISolverPlugin : public server::plugin::IPlugin {
public:
    ~ISolverPlugin() override = default;

    // ==================== Solver Type Registration ====================

    /**
     * @brief Get solver types provided by this plugin
     * @return Vector of solver type information
     */
    [[nodiscard]] virtual auto getSolverTypes() const
        -> std::vector<SolverTypeInfo> = 0;

    /**
     * @brief Register solver types with the type registry
     * @param registry Solver type registry
     * @return Number of types registered
     */
    virtual auto registerSolverTypes(SolverTypeRegistry& registry)
        -> size_t = 0;

    /**
     * @brief Unregister solver types from the type registry
     * @param registry Solver type registry
     * @return Number of types unregistered
     */
    virtual auto unregisterSolverTypes(SolverTypeRegistry& registry)
        -> size_t = 0;

    // ==================== Solver Factory Registration ====================

    /**
     * @brief Register solver creators with the factory
     * @param factory Solver factory
     */
    virtual void registerSolverCreators(SolverFactory& factory) = 0;

    /**
     * @brief Unregister solver creators from the factory
     * @param factory Solver factory
     */
    virtual void unregisterSolverCreators(SolverFactory& factory) = 0;

    // ==================== Solver Instance Management ====================

    /**
     * @brief Create a solver instance
     * @param solverId Unique identifier for the solver instance
     * @param config Configuration for the solver
     * @return Created solver or nullptr on failure
     */
    [[nodiscard]] virtual auto createSolver(
        const std::string& solverId,
        const nlohmann::json& config = {})
        -> std::shared_ptr<client::SolverClient> = 0;

    /**
     * @brief Get all active solver instances from this plugin
     * @return Vector of active solvers
     */
    [[nodiscard]] virtual auto getActiveSolvers() const
        -> std::vector<std::shared_ptr<client::SolverClient>> = 0;

    /**
     * @brief Destroy a solver instance
     * @param solverId Solver instance ID
     * @return true if destroyed successfully
     */
    virtual auto destroySolver(const std::string& solverId) -> bool = 0;

    // ==================== External Binary Management ====================

    /**
     * @brief Check if plugin requires external binary
     * @return true if external binary is required
     */
    [[nodiscard]] virtual auto hasExternalBinary() const -> bool = 0;

    /**
     * @brief Find the external solver binary
     * @return Path to binary if found
     */
    [[nodiscard]] virtual auto findBinary()
        -> std::optional<std::filesystem::path> = 0;

    /**
     * @brief Validate the external binary at given path
     * @param path Path to binary
     * @return true if binary is valid
     */
    [[nodiscard]] virtual auto validateBinary(
        const std::filesystem::path& path) -> bool = 0;

    /**
     * @brief Get the version of the external binary
     * @return Version string
     */
    [[nodiscard]] virtual auto getBinaryVersion() const -> std::string = 0;

    /**
     * @brief Set custom binary path
     * @param path Path to binary
     * @return true if path is valid and set
     */
    virtual auto setBinaryPath(const std::filesystem::path& path) -> bool = 0;

    /**
     * @brief Get current binary path
     * @return Current binary path
     */
    [[nodiscard]] virtual auto getBinaryPath() const
        -> std::optional<std::filesystem::path> = 0;

    // ==================== Plugin Metadata ====================

    /**
     * @brief Get solver plugin specific metadata
     */
    [[nodiscard]] virtual auto getSolverMetadata() const
        -> SolverPluginMetadata = 0;

    /**
     * @brief Get current solver plugin state
     */
    [[nodiscard]] virtual auto getSolverPluginState() const
        -> SolverPluginState = 0;

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to plugin events
     * @param callback Event callback
     * @return Subscription ID
     */
    virtual auto subscribeEvents(SolverPluginEventCallback callback)
        -> uint64_t = 0;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    virtual void unsubscribeEvents(uint64_t subscriptionId) = 0;

    // ==================== Configuration ====================

    /**
     * @brief Get default solver options
     * @return Default options as JSON
     */
    [[nodiscard]] virtual auto getDefaultOptions() const -> nlohmann::json = 0;

    /**
     * @brief Validate solver options
     * @param options Options to validate
     * @return Validation result with errors if any
     */
    [[nodiscard]] virtual auto validateOptions(const nlohmann::json& options)
        -> SolverResult<bool> = 0;
};

/**
 * @brief Base implementation of ISolverPlugin with common functionality
 *
 * Provides default implementations for common solver plugin operations.
 * Derived classes should override solver-specific methods.
 */
class SolverPluginBase : public ISolverPlugin {
public:
    explicit SolverPluginBase(SolverPluginMetadata metadata);
    ~SolverPluginBase() override = default;

    // ==================== IPlugin Interface ====================

    [[nodiscard]] auto getMetadata() const
        -> const server::plugin::PluginMetadata& override;

    auto initialize(const nlohmann::json& config) -> bool override;
    void shutdown() override;

    [[nodiscard]] auto getState() const -> server::plugin::PluginState override;
    [[nodiscard]] auto getLastError() const -> std::string override;
    [[nodiscard]] auto isHealthy() const -> bool override;

    auto pause() -> bool override;
    auto resume() -> bool override;

    [[nodiscard]] auto getStatistics() const
        -> server::plugin::PluginStatistics override;

    auto updateConfig(const nlohmann::json& config) -> bool override;
    [[nodiscard]] auto getConfig() const -> nlohmann::json override;

    // ==================== ISolverPlugin Interface ====================

    [[nodiscard]] auto getSolverMetadata() const
        -> SolverPluginMetadata override;

    [[nodiscard]] auto getSolverPluginState() const
        -> SolverPluginState override;

    auto subscribeEvents(SolverPluginEventCallback callback)
        -> uint64_t override;
    void unsubscribeEvents(uint64_t subscriptionId) override;

    // Default implementations
    [[nodiscard]] auto getActiveSolvers() const
        -> std::vector<std::shared_ptr<client::SolverClient>> override;

    auto destroySolver(const std::string& solverId) -> bool override;

    [[nodiscard]] auto getDefaultOptions() const -> nlohmann::json override;

    [[nodiscard]] auto validateOptions(const nlohmann::json& options)
        -> SolverResult<bool> override;

protected:
    /**
     * @brief Set the plugin state
     */
    void setState(SolverPluginState state);

    /**
     * @brief Set the last error message
     */
    void setLastError(const std::string& error);

    /**
     * @brief Emit a plugin event
     */
    void emitEvent(const SolverPluginEvent& event);

    /**
     * @brief Create a plugin event
     */
    [[nodiscard]] auto createEvent(SolverPluginEventType type,
                                    const std::string& message = {}) const
        -> SolverPluginEvent;

    /**
     * @brief Register an active solver instance
     */
    void registerActiveSolver(const std::string& id,
                              std::shared_ptr<client::SolverClient> solver);

    /**
     * @brief Unregister an active solver instance
     */
    void unregisterActiveSolver(const std::string& id);

    // Protected members
    SolverPluginMetadata metadata_;
    SolverPluginState state_{SolverPluginState::Unloaded};
    server::plugin::PluginState pluginState_{
        server::plugin::PluginState::Unloaded};
    std::string lastError_;
    nlohmann::json config_;
    server::plugin::PluginStatistics statistics_;

    // Binary path
    std::optional<std::filesystem::path> binaryPath_;
    std::string binaryVersion_;

    // Active solvers
    mutable std::shared_mutex solversMutex_;
    std::unordered_map<std::string, std::shared_ptr<client::SolverClient>> activeSolvers_;

    // Event subscribers
    mutable std::shared_mutex eventMutex_;
    std::unordered_map<uint64_t, SolverPluginEventCallback> eventSubscribers_;
    uint64_t nextSubscriberId_{1};
};

// ============================================================================
// Plugin Entry Points
// ============================================================================

/**
 * @brief Solver plugin factory function type
 */
using SolverPluginFactory = std::function<std::shared_ptr<ISolverPlugin>()>;

/**
 * @brief Solver plugin entry point function signature
 *
 * Dynamic libraries must export this function to be loadable as solver plugins.
 * The function name must be "createSolverPlugin".
 */
extern "C" using CreateSolverPluginFunc = ISolverPlugin* (*)();

/**
 * @brief Solver plugin destruction function signature
 *
 * Optional function for custom cleanup. Function name: "destroySolverPlugin"
 */
extern "C" using DestroySolverPluginFunc = void (*)(ISolverPlugin*);

/**
 * @brief Get solver plugin API version function signature
 *
 * Returns the API version the plugin was built against.
 * Function name: "getSolverPluginApiVersion"
 */
extern "C" using GetSolverPluginApiVersionFunc = int (*)();

/**
 * @brief Get solver plugin metadata function signature
 *
 * Returns plugin metadata without instantiating the plugin.
 * Function name: "getSolverPluginMetadata"
 */
extern "C" using GetSolverPluginMetadataFunc = SolverPluginMetadata (*)();

/// Current solver plugin API version
constexpr int SOLVER_PLUGIN_API_VERSION = 1;

// ============================================================================
// Plugin Capability Constants
// ============================================================================

namespace solver_capabilities {
    constexpr const char* BLIND_SOLVE = "solver_blind_solve";
    constexpr const char* HINTED_SOLVE = "solver_hinted_solve";
    constexpr const char* ABORT = "solver_abort";
    constexpr const char* ASYNC = "solver_async";
    constexpr const char* DOWNSAMPLE = "solver_downsample";
    constexpr const char* SCALE_HINTS = "solver_scale_hints";
    constexpr const char* SIP_DISTORTION = "solver_sip";
    constexpr const char* WCS_OUTPUT = "solver_wcs";
    constexpr const char* ANNOTATION = "solver_annotate";
    constexpr const char* STAR_EXTRACTION = "solver_stars";
}  // namespace solver_capabilities

// ============================================================================
// Plugin Tag Constants
// ============================================================================

namespace solver_tags {
    constexpr const char* SOLVER_PLUGIN = "solver";
    constexpr const char* ASTAP = "astap";
    constexpr const char* ASTROMETRY = "astrometry";
    constexpr const char* STELLARSOLVER = "stellarsolver";
    constexpr const char* LOCAL = "local";
    constexpr const char* REMOTE = "remote";
    constexpr const char* FAST = "fast";
    constexpr const char* ACCURATE = "accurate";
}  // namespace solver_tags

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_PLUGIN_INTERFACE_HPP
