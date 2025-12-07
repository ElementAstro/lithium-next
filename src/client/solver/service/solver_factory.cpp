/*
 * solver_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "solver_factory.hpp"
#include "solver_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <sstream>

namespace lithium::solver {

auto SolverFactory::getInstance() -> SolverFactory& {
    static SolverFactory instance;
    return instance;
}

void SolverFactory::registerCreator(const std::string& typeName,
                                     Creator creator) {
    std::unique_lock lock(mutex_);

    if (typeName.empty()) {
        LOG_WARN("Cannot register creator with empty type name");
        return;
    }

    if (!creator) {
        LOG_WARN("Cannot register null creator for type '{}'", typeName);
        return;
    }

    creators_[typeName] = std::move(creator);
    LOG_INFO("Registered solver creator for type: {}", typeName);
}

void SolverFactory::unregisterCreator(const std::string& typeName) {
    std::unique_lock lock(mutex_);

    auto it = creators_.find(typeName);
    if (it == creators_.end()) {
        LOG_WARN("Solver creator for type '{}' not found", typeName);
        return;
    }

    creators_.erase(it);
    LOG_INFO("Unregistered solver creator for type: {}", typeName);
}

auto SolverFactory::hasCreator(const std::string& typeName) const -> bool {
    std::shared_lock lock(mutex_);
    return creators_.contains(typeName);
}

auto SolverFactory::getRegisteredTypes() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    std::vector<std::string> types;
    types.reserve(creators_.size());
    for (const auto& [name, creator] : creators_) {
        types.push_back(name);
    }

    return types;
}

auto SolverFactory::createSolver(const std::string& typeName,
                                  const std::string& id,
                                  const nlohmann::json& config)
    -> std::shared_ptr<client::SolverClient> {
    std::shared_lock lock(mutex_);

    auto it = creators_.find(typeName);
    if (it == creators_.end()) {
        LOG_ERROR("No creator registered for solver type: {}", typeName);
        return nullptr;
    }

    try {
        auto solver = it->second(id, config);
        if (solver) {
            LOG_DEBUG("Created solver instance '{}' of type '{}'", id, typeName);
        } else {
            LOG_ERROR("Creator returned null for solver type: {}", typeName);
        }
        return solver;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception creating solver '{}' of type '{}': {}",
                  id, typeName, e.what());
        return nullptr;
    }
}

auto SolverFactory::createSolver(const std::string& typeName,
                                  const nlohmann::json& config)
    -> std::shared_ptr<client::SolverClient> {
    return createSolver(typeName, generateId(typeName), config);
}

auto SolverFactory::createBestSolver(const std::string& id,
                                      const nlohmann::json& config)
    -> std::shared_ptr<client::SolverClient> {
    auto& registry = SolverTypeRegistry::getInstance();
    auto bestType = registry.getBestType();

    if (!bestType) {
        LOG_WARN("No enabled solver types available");
        return nullptr;
    }

    return createSolver(bestType->typeName, id, config);
}

void SolverFactory::clear() {
    std::unique_lock lock(mutex_);
    creators_.clear();
    idCounters_.clear();
    LOG_DEBUG("Solver factory cleared");
}

auto SolverFactory::getCreatorCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return creators_.size();
}

auto SolverFactory::generateId(const std::string& typeName) -> std::string {
    std::unique_lock lock(mutex_);

    uint64_t& counter = idCounters_[typeName];
    counter++;

    std::ostringstream oss;
    oss << typeName << "_" << counter;
    return oss.str();
}

}  // namespace lithium::solver
