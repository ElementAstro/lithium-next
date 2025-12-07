/*
 * solver_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Common types for solver plugin system

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_TYPES_HPP
#define LITHIUM_CLIENT_SOLVER_TYPES_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::solver {

/**
 * @brief Solver capabilities flags
 */
struct SolverCapabilities {
    bool canBlindSolve{true};       ///< Can solve without position hints
    bool canHintedSolve{true};      ///< Can use position hints
    bool canAbort{true};            ///< Can abort solve operation
    bool supportsDownsample{true};  ///< Supports image downsampling
    bool supportsScale{true};       ///< Supports scale hints
    bool supportsDepth{true};       ///< Supports depth parameter
    bool supportsSIP{false};        ///< Supports SIP distortion
    bool supportsWCSOutput{true};   ///< Can output WCS data
    bool supportsAnnotation{false}; ///< Can annotate solved image
    bool supportsStarExtraction{false}; ///< Can extract stars separately
    bool supportsAsync{true};       ///< Supports async solving
    bool requiresQt{false};         ///< Requires Qt runtime

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        return nlohmann::json{
            {"canBlindSolve", canBlindSolve},
            {"canHintedSolve", canHintedSolve},
            {"canAbort", canAbort},
            {"supportsDownsample", supportsDownsample},
            {"supportsScale", supportsScale},
            {"supportsDepth", supportsDepth},
            {"supportsSIP", supportsSIP},
            {"supportsWCSOutput", supportsWCSOutput},
            {"supportsAnnotation", supportsAnnotation},
            {"supportsStarExtraction", supportsStarExtraction},
            {"supportsAsync", supportsAsync},
            {"requiresQt", requiresQt}
        };
    }

    static auto fromJson(const nlohmann::json& j) -> SolverCapabilities {
        SolverCapabilities caps;
        if (j.contains("canBlindSolve")) caps.canBlindSolve = j["canBlindSolve"];
        if (j.contains("canHintedSolve")) caps.canHintedSolve = j["canHintedSolve"];
        if (j.contains("canAbort")) caps.canAbort = j["canAbort"];
        if (j.contains("supportsDownsample")) caps.supportsDownsample = j["supportsDownsample"];
        if (j.contains("supportsScale")) caps.supportsScale = j["supportsScale"];
        if (j.contains("supportsDepth")) caps.supportsDepth = j["supportsDepth"];
        if (j.contains("supportsSIP")) caps.supportsSIP = j["supportsSIP"];
        if (j.contains("supportsWCSOutput")) caps.supportsWCSOutput = j["supportsWCSOutput"];
        if (j.contains("supportsAnnotation")) caps.supportsAnnotation = j["supportsAnnotation"];
        if (j.contains("supportsStarExtraction")) caps.supportsStarExtraction = j["supportsStarExtraction"];
        if (j.contains("supportsAsync")) caps.supportsAsync = j["supportsAsync"];
        if (j.contains("requiresQt")) caps.requiresQt = j["requiresQt"];
        return caps;
    }
};

/**
 * @brief Solver type information for registry
 */
struct SolverTypeInfo {
    std::string typeName;        ///< Internal type name ("ASTAP", "Astrometry", "StellarSolver")
    std::string displayName;     ///< Human-readable display name
    std::string description;     ///< Description of the solver
    std::string pluginName;      ///< Name of the plugin providing this type
    std::string version;         ///< Solver version string
    SolverCapabilities capabilities; ///< Solver capabilities
    nlohmann::json optionSchema; ///< JSON Schema for solver options
    bool enabled{true};          ///< Whether this solver type is enabled
    int priority{0};             ///< Priority for auto-selection (higher = preferred)

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        return nlohmann::json{
            {"typeName", typeName},
            {"displayName", displayName},
            {"description", description},
            {"pluginName", pluginName},
            {"version", version},
            {"capabilities", capabilities.toJson()},
            {"optionSchema", optionSchema},
            {"enabled", enabled},
            {"priority", priority}
        };
    }

    static auto fromJson(const nlohmann::json& j) -> SolverTypeInfo {
        SolverTypeInfo info;
        if (j.contains("typeName")) info.typeName = j["typeName"];
        if (j.contains("displayName")) info.displayName = j["displayName"];
        if (j.contains("description")) info.description = j["description"];
        if (j.contains("pluginName")) info.pluginName = j["pluginName"];
        if (j.contains("version")) info.version = j["version"];
        if (j.contains("capabilities")) info.capabilities = SolverCapabilities::fromJson(j["capabilities"]);
        if (j.contains("optionSchema")) info.optionSchema = j["optionSchema"];
        if (j.contains("enabled")) info.enabled = j["enabled"];
        if (j.contains("priority")) info.priority = j["priority"];
        return info;
    }
};

/**
 * @brief Solver plugin state
 */
enum class SolverPluginState {
    Unloaded,       ///< Plugin not loaded
    Loading,        ///< Plugin currently loading
    Loaded,         ///< Plugin loaded but not initialized
    Initializing,   ///< Plugin initializing
    Ready,          ///< Plugin ready, solver binary found
    Solving,        ///< Currently solving an image
    Paused,         ///< Plugin paused
    Stopping,       ///< Plugin shutting down
    Error,          ///< Plugin in error state
    Disabled        ///< Plugin disabled by user
};

/**
 * @brief Convert solver plugin state to string
 */
[[nodiscard]] inline auto solverPluginStateToString(SolverPluginState state) -> std::string {
    switch (state) {
        case SolverPluginState::Unloaded: return "Unloaded";
        case SolverPluginState::Loading: return "Loading";
        case SolverPluginState::Loaded: return "Loaded";
        case SolverPluginState::Initializing: return "Initializing";
        case SolverPluginState::Ready: return "Ready";
        case SolverPluginState::Solving: return "Solving";
        case SolverPluginState::Paused: return "Paused";
        case SolverPluginState::Stopping: return "Stopping";
        case SolverPluginState::Error: return "Error";
        case SolverPluginState::Disabled: return "Disabled";
        default: return "Unknown";
    }
}

/**
 * @brief Solver plugin event types
 */
enum class SolverPluginEventType {
    TypeRegistered,        ///< New solver type registered
    TypeUnregistered,      ///< Solver type unregistered
    BinaryFound,           ///< External binary found
    BinaryNotFound,        ///< External binary not found
    SolveStarted,          ///< Solve operation started
    SolveCompleted,        ///< Solve operation completed
    SolveFailed,           ///< Solve operation failed
    SolveAborted,          ///< Solve operation aborted
    ConfigurationChanged,  ///< Configuration changed
    Error                  ///< Error occurred
};

/**
 * @brief Solver plugin event
 */
struct SolverPluginEvent {
    SolverPluginEventType type;
    std::string pluginName;
    std::string typeName;           ///< For type events
    std::string solverId;           ///< For solve events
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json data;            ///< Additional event data

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        return nlohmann::json{
            {"type", static_cast<int>(type)},
            {"pluginName", pluginName},
            {"typeName", typeName},
            {"solverId", solverId},
            {"message", message},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count()},
            {"data", data}
        };
    }
};

/**
 * @brief Solver plugin event callback
 */
using SolverPluginEventCallback = std::function<void(const SolverPluginEvent&)>;

/**
 * @brief Type registration change callback
 */
using TypeRegistrationCallback = std::function<void(const std::string& typeName, bool registered)>;

/**
 * @brief Result wrapper for solver operations
 */
template <typename T>
class SolverResult {
public:
    SolverResult() = default;

    explicit SolverResult(T value) : value_(std::move(value)), success_(true) {}

    explicit SolverResult(std::string error)
        : error_(std::move(error)), success_(false) {}

    [[nodiscard]] auto isSuccess() const -> bool { return success_; }
    [[nodiscard]] auto isError() const -> bool { return !success_; }

    [[nodiscard]] auto value() const -> const T& {
        if (!success_) {
            throw std::runtime_error("Accessing value of failed result: " + error_);
        }
        return value_;
    }

    [[nodiscard]] auto value() -> T& {
        if (!success_) {
            throw std::runtime_error("Accessing value of failed result: " + error_);
        }
        return value_;
    }

    [[nodiscard]] auto error() const -> const std::string& { return error_; }

    [[nodiscard]] auto valueOr(const T& defaultValue) const -> T {
        return success_ ? value_ : defaultValue;
    }

    explicit operator bool() const { return success_; }

private:
    T value_{};
    std::string error_;
    bool success_{false};
};

/**
 * @brief Helper function to create success result
 */
template <typename T>
auto makeSuccess(T value) -> SolverResult<T> {
    return SolverResult<T>(std::move(value));
}

/**
 * @brief Helper function to create error result
 */
template <typename T>
auto makeError(std::string error) -> SolverResult<T> {
    return SolverResult<T>(std::move(error));
}

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_TYPES_HPP
