/*
 * solver_plugin_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "solver_plugin_interface.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::solver {

// ============================================================================
// SolverPluginMetadata
// ============================================================================

auto SolverPluginMetadata::toJson() const -> nlohmann::json {
    auto j = nlohmann::json{
        {"name", name},
        {"version", version},
        {"description", description},
        {"author", author},
        {"license", license},
        {"solverType", solverType},
        {"backendVersion", backendVersion},
        {"supportsBlindSolve", supportsBlindSolve},
        {"supportsAbort", supportsAbort},
        {"requiresExternalBinary", requiresExternalBinary},
        {"supportedFormats", supportedFormats}
    };
    if (!tags.empty()) {
        j["tags"] = tags;
    }
    if (!capabilities.empty()) {
        j["capabilities"] = capabilities;
    }
    if (!dependencies.empty()) {
        j["dependencies"] = dependencies;
    }
    return j;
}

auto SolverPluginMetadata::fromJson(const nlohmann::json& j) -> SolverPluginMetadata {
    SolverPluginMetadata meta;
    if (j.contains("name")) meta.name = j["name"];
    if (j.contains("version")) meta.version = j["version"];
    if (j.contains("description")) meta.description = j["description"];
    if (j.contains("author")) meta.author = j["author"];
    if (j.contains("license")) meta.license = j["license"];
    if (j.contains("solverType")) meta.solverType = j["solverType"];
    if (j.contains("backendVersion")) meta.backendVersion = j["backendVersion"];
    if (j.contains("supportsBlindSolve")) meta.supportsBlindSolve = j["supportsBlindSolve"];
    if (j.contains("supportsAbort")) meta.supportsAbort = j["supportsAbort"];
    if (j.contains("requiresExternalBinary")) meta.requiresExternalBinary = j["requiresExternalBinary"];
    if (j.contains("supportedFormats")) {
        meta.supportedFormats = j["supportedFormats"].get<std::vector<std::string>>();
    }
    if (j.contains("tags")) {
        meta.tags = j["tags"].get<std::vector<std::string>>();
    }
    if (j.contains("capabilities")) {
        meta.capabilities = j["capabilities"].get<std::vector<std::string>>();
    }
    if (j.contains("dependencies")) {
        meta.dependencies = j["dependencies"].get<std::vector<std::string>>();
    }
    return meta;
}

// ============================================================================
// SolverPluginBase
// ============================================================================

SolverPluginBase::SolverPluginBase(SolverPluginMetadata metadata)
    : metadata_(std::move(metadata)) {
    // Initialize statistics
    statistics_.loadTime = std::chrono::system_clock::now();
}

auto SolverPluginBase::getMetadata() const
    -> const server::plugin::PluginMetadata& {
    return metadata_;
}

auto SolverPluginBase::initialize(const nlohmann::json& config) -> bool {
    setState(SolverPluginState::Initializing);
    config_ = config;

    try {
        // Try to find external binary if required
        if (hasExternalBinary()) {
            auto binaryPath = findBinary();
            if (binaryPath) {
                binaryPath_ = *binaryPath;
                if (validateBinary(*binaryPath)) {
                    LOG_INFO("Found valid {} binary at: {}",
                             metadata_.name, binaryPath->string());
                    emitEvent(createEvent(SolverPluginEventType::BinaryFound,
                        "Binary found at " + binaryPath->string()));
                } else {
                    LOG_WARN("{} binary at {} is not valid",
                             metadata_.name, binaryPath->string());
                    setLastError("Binary validation failed");
                    setState(SolverPluginState::Error);
                    return false;
                }
            } else {
                LOG_WARN("{} binary not found on system", metadata_.name);
                emitEvent(createEvent(SolverPluginEventType::BinaryNotFound,
                    "Binary not found"));
                // Don't fail - binary might be configured later
            }
        }

        setState(SolverPluginState::Ready);
        pluginState_ = server::plugin::PluginState::Running;

        LOG_INFO("{} solver plugin initialized successfully", metadata_.name);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("{} solver plugin initialization failed: {}",
                  metadata_.name, e.what());
        setLastError(e.what());
        setState(SolverPluginState::Error);
        return false;
    }
}

void SolverPluginBase::shutdown() {
    setState(SolverPluginState::Stopping);

    // Destroy all active solvers
    {
        std::unique_lock lock(solversMutex_);
        for (auto& [id, solver] : activeSolvers_) {
            if (solver) {
                solver->abort();
            }
        }
        activeSolvers_.clear();
    }

    // Clear event subscribers
    {
        std::unique_lock lock(eventMutex_);
        eventSubscribers_.clear();
    }

    setState(SolverPluginState::Unloaded);
    pluginState_ = server::plugin::PluginState::Stopped;

    LOG_INFO("{} solver plugin shut down", metadata_.name);
}

auto SolverPluginBase::getState() const -> server::plugin::PluginState {
    return pluginState_;
}

auto SolverPluginBase::getLastError() const -> std::string {
    return lastError_;
}

auto SolverPluginBase::isHealthy() const -> bool {
    if (state_ == SolverPluginState::Error ||
        state_ == SolverPluginState::Disabled) {
        return false;
    }

    // Check if binary is available (if required)
    if (hasExternalBinary() && !binaryPath_.has_value()) {
        return false;
    }

    return true;
}

auto SolverPluginBase::pause() -> bool {
    if (state_ != SolverPluginState::Ready &&
        state_ != SolverPluginState::Solving) {
        return false;
    }
    setState(SolverPluginState::Paused);
    pluginState_ = server::plugin::PluginState::Paused;
    return true;
}

auto SolverPluginBase::resume() -> bool {
    if (state_ != SolverPluginState::Paused) {
        return false;
    }
    setState(SolverPluginState::Ready);
    pluginState_ = server::plugin::PluginState::Running;
    return true;
}

auto SolverPluginBase::getStatistics() const
    -> server::plugin::PluginStatistics {
    return statistics_;
}

auto SolverPluginBase::updateConfig(const nlohmann::json& config) -> bool {
    config_.merge_patch(config);
    emitEvent(createEvent(SolverPluginEventType::ConfigurationChanged,
        "Configuration updated"));
    return true;
}

auto SolverPluginBase::getConfig() const -> nlohmann::json {
    return config_;
}

auto SolverPluginBase::getSolverMetadata() const -> SolverPluginMetadata {
    return metadata_;
}

auto SolverPluginBase::getSolverPluginState() const -> SolverPluginState {
    return state_;
}

auto SolverPluginBase::subscribeEvents(SolverPluginEventCallback callback)
    -> uint64_t {
    std::unique_lock lock(eventMutex_);
    uint64_t id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void SolverPluginBase::unsubscribeEvents(uint64_t subscriptionId) {
    std::unique_lock lock(eventMutex_);
    eventSubscribers_.erase(subscriptionId);
}

auto SolverPluginBase::getActiveSolvers() const
    -> std::vector<std::shared_ptr<client::SolverClient>> {
    std::shared_lock lock(solversMutex_);
    std::vector<std::shared_ptr<client::SolverClient>> solvers;
    solvers.reserve(activeSolvers_.size());
    for (const auto& [id, solver] : activeSolvers_) {
        solvers.push_back(solver);
    }
    return solvers;
}

auto SolverPluginBase::destroySolver(const std::string& solverId) -> bool {
    std::unique_lock lock(solversMutex_);
    auto it = activeSolvers_.find(solverId);
    if (it == activeSolvers_.end()) {
        return false;
    }

    if (it->second) {
        it->second->abort();
    }
    activeSolvers_.erase(it);
    return true;
}

auto SolverPluginBase::getDefaultOptions() const -> nlohmann::json {
    return nlohmann::json::object();
}

auto SolverPluginBase::validateOptions(const nlohmann::json& /*options*/)
    -> SolverResult<bool> {
    // Default implementation accepts all options
    return makeSuccess(true);
}

void SolverPluginBase::setState(SolverPluginState state) {
    state_ = state;
}

void SolverPluginBase::setLastError(const std::string& error) {
    lastError_ = error;
}

void SolverPluginBase::emitEvent(const SolverPluginEvent& event) {
    std::shared_lock lock(eventMutex_);
    for (const auto& [id, callback] : eventSubscribers_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in event callback {}: {}", id, e.what());
        }
    }
}

auto SolverPluginBase::createEvent(SolverPluginEventType type,
                                    const std::string& message) const
    -> SolverPluginEvent {
    SolverPluginEvent event;
    event.type = type;
    event.pluginName = metadata_.name;
    event.message = message;
    event.timestamp = std::chrono::system_clock::now();
    return event;
}

void SolverPluginBase::registerActiveSolver(
    const std::string& id,
    std::shared_ptr<client::SolverClient> solver) {
    std::unique_lock lock(solversMutex_);
    activeSolvers_[id] = std::move(solver);
}

void SolverPluginBase::unregisterActiveSolver(const std::string& id) {
    std::unique_lock lock(solversMutex_);
    activeSolvers_.erase(id);
}

}  // namespace lithium::solver
