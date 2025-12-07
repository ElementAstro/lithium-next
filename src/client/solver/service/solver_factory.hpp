/*
 * solver_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver factory for creating solver instances

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_FACTORY_HPP
#define LITHIUM_CLIENT_SOLVER_FACTORY_HPP

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

#include "../common/solver_types.hpp"

// Forward declarations
namespace lithium::client {
class SolverClient;
}

namespace lithium::solver {

/**
 * @brief Factory for creating solver instances
 *
 * Singleton factory that manages solver creators and creates solver instances.
 */
class SolverFactory {
public:
    /**
     * @brief Creator function type
     */
    using Creator = std::function<std::shared_ptr<client::SolverClient>(
        const std::string& id, const nlohmann::json& config)>;

    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> SolverFactory&;

    // Disable copy and move
    SolverFactory(const SolverFactory&) = delete;
    SolverFactory& operator=(const SolverFactory&) = delete;
    SolverFactory(SolverFactory&&) = delete;
    SolverFactory& operator=(SolverFactory&&) = delete;

    // ==================== Creator Registration ====================

    /**
     * @brief Register a solver creator
     * @param typeName Type name to register
     * @param creator Creator function
     */
    void registerCreator(const std::string& typeName, Creator creator);

    /**
     * @brief Unregister a solver creator
     * @param typeName Type name to unregister
     */
    void unregisterCreator(const std::string& typeName);

    /**
     * @brief Check if a creator is registered
     * @param typeName Type name
     * @return true if registered
     */
    [[nodiscard]] auto hasCreator(const std::string& typeName) const -> bool;

    /**
     * @brief Get registered type names
     * @return Vector of type names
     */
    [[nodiscard]] auto getRegisteredTypes() const -> std::vector<std::string>;

    // ==================== Solver Creation ====================

    /**
     * @brief Create a solver instance
     * @param typeName Type of solver to create
     * @param id Unique ID for the solver instance
     * @param config Configuration for the solver
     * @return Created solver or nullptr on failure
     */
    [[nodiscard]] auto createSolver(const std::string& typeName,
                                     const std::string& id,
                                     const nlohmann::json& config = {})
        -> std::shared_ptr<client::SolverClient>;

    /**
     * @brief Create solver with default ID
     * @param typeName Type of solver to create
     * @param config Configuration for the solver
     * @return Created solver or nullptr on failure
     */
    [[nodiscard]] auto createSolver(const std::string& typeName,
                                     const nlohmann::json& config = {})
        -> std::shared_ptr<client::SolverClient>;

    /**
     * @brief Create the best available solver
     * @param id Unique ID for the solver instance
     * @param config Configuration for the solver
     * @return Created solver or nullptr if no solver available
     */
    [[nodiscard]] auto createBestSolver(const std::string& id,
                                         const nlohmann::json& config = {})
        -> std::shared_ptr<client::SolverClient>;

    // ==================== Utility ====================

    /**
     * @brief Clear all registered creators
     */
    void clear();

    /**
     * @brief Get count of registered creators
     * @return Number of registered creators
     */
    [[nodiscard]] auto getCreatorCount() const -> size_t;

private:
    SolverFactory() = default;
    ~SolverFactory() = default;

    /**
     * @brief Generate unique solver ID
     */
    auto generateId(const std::string& typeName) -> std::string;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Creator> creators_;
    std::unordered_map<std::string, uint64_t> idCounters_;
};

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_FACTORY_HPP
