/*
 * solver_type_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver type registry for runtime type management

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_TYPE_REGISTRY_HPP
#define LITHIUM_CLIENT_SOLVER_TYPE_REGISTRY_HPP

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../common/solver_types.hpp"

namespace lithium::solver {

/**
 * @brief Registry for solver types
 *
 * Manages registration and discovery of solver types at runtime.
 * Thread-safe singleton implementation.
 */
class SolverTypeRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> SolverTypeRegistry&;

    // Disable copy and move
    SolverTypeRegistry(const SolverTypeRegistry&) = delete;
    SolverTypeRegistry& operator=(const SolverTypeRegistry&) = delete;
    SolverTypeRegistry(SolverTypeRegistry&&) = delete;
    SolverTypeRegistry& operator=(SolverTypeRegistry&&) = delete;

    // ==================== Type Registration ====================

    /**
     * @brief Register a solver type
     * @param info Type information
     * @return true if registered successfully
     */
    auto registerType(const SolverTypeInfo& info) -> bool;

    /**
     * @brief Register a solver type from a plugin
     * @param info Type information
     * @param pluginName Name of the plugin providing this type
     * @return true if registered successfully
     */
    auto registerTypeFromPlugin(const SolverTypeInfo& info,
                                const std::string& pluginName) -> bool;

    /**
     * @brief Unregister a solver type
     * @param typeName Type name to unregister
     * @return true if unregistered successfully
     */
    auto unregisterType(const std::string& typeName) -> bool;

    /**
     * @brief Unregister all types from a plugin
     * @param pluginName Plugin name
     * @return Number of types unregistered
     */
    auto unregisterPluginTypes(const std::string& pluginName) -> size_t;

    /**
     * @brief Update an existing type
     * @param typeName Type name to update
     * @param info New type information
     * @return true if updated successfully
     */
    auto updateType(const std::string& typeName, const SolverTypeInfo& info)
        -> bool;

    // ==================== Type Query ====================

    /**
     * @brief Check if a type is registered
     * @param typeName Type name
     * @return true if registered
     */
    [[nodiscard]] auto hasType(const std::string& typeName) const -> bool;

    /**
     * @brief Get type information
     * @param typeName Type name
     * @return Type info if found
     */
    [[nodiscard]] auto getTypeInfo(const std::string& typeName) const
        -> std::optional<SolverTypeInfo>;

    /**
     * @brief Get all registered types
     * @return Vector of all type information
     */
    [[nodiscard]] auto getAllTypes() const -> std::vector<SolverTypeInfo>;

    /**
     * @brief Get all enabled types
     * @return Vector of enabled type information
     */
    [[nodiscard]] auto getEnabledTypes() const -> std::vector<SolverTypeInfo>;

    /**
     * @brief Get types from a specific plugin
     * @param pluginName Plugin name
     * @return Vector of type information from the plugin
     */
    [[nodiscard]] auto getPluginTypes(const std::string& pluginName) const
        -> std::vector<SolverTypeInfo>;

    /**
     * @brief Get types with specific capability
     * @param capability Capability name (from solver_capabilities namespace)
     * @return Vector of matching types
     */
    [[nodiscard]] auto getTypesWithCapability(const std::string& capability) const
        -> std::vector<SolverTypeInfo>;

    /**
     * @brief Get the best available solver type (highest priority enabled)
     * @return Best type if any available
     */
    [[nodiscard]] auto getBestType() const -> std::optional<SolverTypeInfo>;

    /**
     * @brief Get type names
     * @return Vector of type names
     */
    [[nodiscard]] auto getTypeNames() const -> std::vector<std::string>;

    /**
     * @brief Get total number of registered types
     * @return Count of types
     */
    [[nodiscard]] auto getTypeCount() const -> size_t;

    // ==================== Type State ====================

    /**
     * @brief Enable a solver type
     * @param typeName Type name
     * @return true if enabled successfully
     */
    auto enableType(const std::string& typeName) -> bool;

    /**
     * @brief Disable a solver type
     * @param typeName Type name
     * @return true if disabled successfully
     */
    auto disableType(const std::string& typeName) -> bool;

    /**
     * @brief Check if a type is enabled
     * @param typeName Type name
     * @return true if enabled
     */
    [[nodiscard]] auto isTypeEnabled(const std::string& typeName) const -> bool;

    /**
     * @brief Set type priority
     * @param typeName Type name
     * @param priority New priority value (higher = preferred)
     * @return true if priority set successfully
     */
    auto setTypePriority(const std::string& typeName, int priority) -> bool;

    // ==================== Event System ====================

    /**
     * @brief Subscribe to type registration changes
     * @param callback Callback function
     * @return Subscription ID
     */
    auto subscribe(TypeRegistrationCallback callback) -> uint64_t;

    /**
     * @brief Unsubscribe from type registration changes
     * @param callbackId Subscription ID
     */
    void unsubscribe(uint64_t callbackId);

    // ==================== Initialization ====================

    /**
     * @brief Initialize with built-in types (if any)
     */
    void initializeBuiltInTypes();

    /**
     * @brief Clear all registered types
     */
    void clear();

    /**
     * @brief Export registry to JSON
     * @return JSON representation
     */
    [[nodiscard]] auto toJson() const -> nlohmann::json;

    /**
     * @brief Import registry from JSON
     * @param j JSON data
     * @return true if import successful
     */
    auto fromJson(const nlohmann::json& j) -> bool;

private:
    SolverTypeRegistry() = default;
    ~SolverTypeRegistry() = default;

    /**
     * @brief Notify subscribers of type change
     */
    void notifySubscribers(const std::string& typeName, bool registered);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, SolverTypeInfo> types_;

    mutable std::shared_mutex subscriberMutex_;
    std::unordered_map<uint64_t, TypeRegistrationCallback> subscribers_;
    uint64_t nextSubscriberId_{1};
};

}  // namespace lithium::solver

#endif  // LITHIUM_CLIENT_SOLVER_TYPE_REGISTRY_HPP
