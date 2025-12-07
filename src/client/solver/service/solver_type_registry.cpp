/*
 * solver_type_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <algorithm>

namespace lithium::solver {

auto SolverTypeRegistry::getInstance() -> SolverTypeRegistry& {
    static SolverTypeRegistry instance;
    return instance;
}

auto SolverTypeRegistry::registerType(const SolverTypeInfo& info) -> bool {
    std::unique_lock lock(mutex_);

    if (info.typeName.empty()) {
        LOG_WARN("Cannot register type with empty name");
        return false;
    }

    if (types_.contains(info.typeName)) {
        LOG_WARN("Solver type '{}' already registered", info.typeName);
        return false;
    }

    types_[info.typeName] = info;
    LOG_INFO("Registered solver type: {} (plugin: {})",
             info.typeName, info.pluginName);

    lock.unlock();
    notifySubscribers(info.typeName, true);

    return true;
}

auto SolverTypeRegistry::registerTypeFromPlugin(
    const SolverTypeInfo& info,
    const std::string& pluginName) -> bool {
    SolverTypeInfo infoWithPlugin = info;
    infoWithPlugin.pluginName = pluginName;
    return registerType(infoWithPlugin);
}

auto SolverTypeRegistry::unregisterType(const std::string& typeName) -> bool {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        LOG_WARN("Solver type '{}' not found for unregistration", typeName);
        return false;
    }

    types_.erase(it);
    LOG_INFO("Unregistered solver type: {}", typeName);

    lock.unlock();
    notifySubscribers(typeName, false);

    return true;
}

auto SolverTypeRegistry::unregisterPluginTypes(const std::string& pluginName)
    -> size_t {
    std::unique_lock lock(mutex_);

    std::vector<std::string> typesToRemove;
    for (const auto& [name, info] : types_) {
        if (info.pluginName == pluginName) {
            typesToRemove.push_back(name);
        }
    }

    for (const auto& name : typesToRemove) {
        types_.erase(name);
    }

    lock.unlock();

    for (const auto& name : typesToRemove) {
        notifySubscribers(name, false);
    }

    LOG_INFO("Unregistered {} types from plugin '{}'",
             typesToRemove.size(), pluginName);
    return typesToRemove.size();
}

auto SolverTypeRegistry::updateType(const std::string& typeName,
                                     const SolverTypeInfo& info) -> bool {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        LOG_WARN("Solver type '{}' not found for update", typeName);
        return false;
    }

    it->second = info;
    LOG_DEBUG("Updated solver type: {}", typeName);

    return true;
}

auto SolverTypeRegistry::hasType(const std::string& typeName) const -> bool {
    std::shared_lock lock(mutex_);
    return types_.contains(typeName);
}

auto SolverTypeRegistry::getTypeInfo(const std::string& typeName) const
    -> std::optional<SolverTypeInfo> {
    std::shared_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto SolverTypeRegistry::getAllTypes() const -> std::vector<SolverTypeInfo> {
    std::shared_lock lock(mutex_);

    std::vector<SolverTypeInfo> result;
    result.reserve(types_.size());
    for (const auto& [name, info] : types_) {
        result.push_back(info);
    }

    // Sort by priority (descending)
    std::sort(result.begin(), result.end(),
              [](const SolverTypeInfo& a, const SolverTypeInfo& b) {
                  return a.priority > b.priority;
              });

    return result;
}

auto SolverTypeRegistry::getEnabledTypes() const -> std::vector<SolverTypeInfo> {
    std::shared_lock lock(mutex_);

    std::vector<SolverTypeInfo> result;
    for (const auto& [name, info] : types_) {
        if (info.enabled) {
            result.push_back(info);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const SolverTypeInfo& a, const SolverTypeInfo& b) {
                  return a.priority > b.priority;
              });

    return result;
}

auto SolverTypeRegistry::getPluginTypes(const std::string& pluginName) const
    -> std::vector<SolverTypeInfo> {
    std::shared_lock lock(mutex_);

    std::vector<SolverTypeInfo> result;
    for (const auto& [name, info] : types_) {
        if (info.pluginName == pluginName) {
            result.push_back(info);
        }
    }

    return result;
}

auto SolverTypeRegistry::getTypesWithCapability(const std::string& capability) const
    -> std::vector<SolverTypeInfo> {
    std::shared_lock lock(mutex_);

    std::vector<SolverTypeInfo> result;
    for (const auto& [name, info] : types_) {
        bool hasCapability = false;

        // Check built-in capabilities
        if (capability == solver_capabilities::BLIND_SOLVE) {
            hasCapability = info.capabilities.canBlindSolve;
        } else if (capability == solver_capabilities::HINTED_SOLVE) {
            hasCapability = info.capabilities.canHintedSolve;
        } else if (capability == solver_capabilities::ABORT) {
            hasCapability = info.capabilities.canAbort;
        } else if (capability == solver_capabilities::ASYNC) {
            hasCapability = info.capabilities.supportsAsync;
        } else if (capability == solver_capabilities::DOWNSAMPLE) {
            hasCapability = info.capabilities.supportsDownsample;
        } else if (capability == solver_capabilities::SCALE_HINTS) {
            hasCapability = info.capabilities.supportsScale;
        } else if (capability == solver_capabilities::SIP_DISTORTION) {
            hasCapability = info.capabilities.supportsSIP;
        } else if (capability == solver_capabilities::WCS_OUTPUT) {
            hasCapability = info.capabilities.supportsWCSOutput;
        } else if (capability == solver_capabilities::ANNOTATION) {
            hasCapability = info.capabilities.supportsAnnotation;
        } else if (capability == solver_capabilities::STAR_EXTRACTION) {
            hasCapability = info.capabilities.supportsStarExtraction;
        }

        if (hasCapability) {
            result.push_back(info);
        }
    }

    return result;
}

auto SolverTypeRegistry::getBestType() const -> std::optional<SolverTypeInfo> {
    auto enabled = getEnabledTypes();
    if (enabled.empty()) {
        return std::nullopt;
    }
    return enabled.front();  // Already sorted by priority
}

auto SolverTypeRegistry::getTypeNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    std::vector<std::string> names;
    names.reserve(types_.size());
    for (const auto& [name, info] : types_) {
        names.push_back(name);
    }

    return names;
}

auto SolverTypeRegistry::getTypeCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return types_.size();
}

auto SolverTypeRegistry::enableType(const std::string& typeName) -> bool {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return false;
    }

    it->second.enabled = true;
    LOG_DEBUG("Enabled solver type: {}", typeName);
    return true;
}

auto SolverTypeRegistry::disableType(const std::string& typeName) -> bool {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return false;
    }

    it->second.enabled = false;
    LOG_DEBUG("Disabled solver type: {}", typeName);
    return true;
}

auto SolverTypeRegistry::isTypeEnabled(const std::string& typeName) const -> bool {
    std::shared_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return false;
    }

    return it->second.enabled;
}

auto SolverTypeRegistry::setTypePriority(const std::string& typeName,
                                          int priority) -> bool {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return false;
    }

    it->second.priority = priority;
    LOG_DEBUG("Set priority {} for solver type: {}", priority, typeName);
    return true;
}

auto SolverTypeRegistry::subscribe(TypeRegistrationCallback callback)
    -> uint64_t {
    std::unique_lock lock(subscriberMutex_);
    uint64_t id = nextSubscriberId_++;
    subscribers_[id] = std::move(callback);
    return id;
}

void SolverTypeRegistry::unsubscribe(uint64_t callbackId) {
    std::unique_lock lock(subscriberMutex_);
    subscribers_.erase(callbackId);
}

void SolverTypeRegistry::initializeBuiltInTypes() {
    // No built-in types - all types come from plugins
    LOG_DEBUG("Solver type registry initialized");
}

void SolverTypeRegistry::clear() {
    std::unique_lock lock(mutex_);

    std::vector<std::string> names;
    for (const auto& [name, info] : types_) {
        names.push_back(name);
    }

    types_.clear();

    lock.unlock();

    for (const auto& name : names) {
        notifySubscribers(name, false);
    }

    LOG_DEBUG("Solver type registry cleared");
}

auto SolverTypeRegistry::toJson() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json j = nlohmann::json::array();
    for (const auto& [name, info] : types_) {
        j.push_back(info.toJson());
    }

    return j;
}

auto SolverTypeRegistry::fromJson(const nlohmann::json& j) -> bool {
    if (!j.is_array()) {
        return false;
    }

    for (const auto& item : j) {
        try {
            auto info = SolverTypeInfo::fromJson(item);
            registerType(info);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to import solver type: {}", e.what());
        }
    }

    return true;
}

void SolverTypeRegistry::notifySubscribers(const std::string& typeName,
                                            bool registered) {
    std::shared_lock lock(subscriberMutex_);
    for (const auto& [id, callback] : subscribers_) {
        try {
            callback(typeName, registered);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in type registration callback {}: {}", id, e.what());
        }
    }
}

}  // namespace lithium::solver
