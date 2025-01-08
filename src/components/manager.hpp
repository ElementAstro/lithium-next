/*
 * component_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager (the core of the plugin system)

**************************************************/

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/type/json_fwd.hpp"

using json = nlohmann::json;

namespace lithium {

/**
 * @class ComponentManager
 * @brief Manages the lifecycle and dependencies of components in the system.
 *
 * The ComponentManager is responsible for loading, unloading, and managing
 * components. It also handles the dependency graph of components and ensures
 * that components are loaded and unloaded in the correct order.
 */
class ComponentManagerImpl;

class ComponentManager {
public:
    /**
     * @brief Constructs a new ComponentManager object.
     */
    explicit ComponentManager();

    /**
     * @brief Destroys the ComponentManager object.
     */
    ~ComponentManager();

    /**
     * @brief Initializes the ComponentManager.
     *
     * @return true if initialization is successful, false otherwise.
     */
    auto initialize() -> bool;

    /**
     * @brief Destroys the ComponentManager, unloading all components.
     *
     * @return true if destruction is successful, false otherwise.
     */
    auto destroy() -> bool;

    /**
     * @brief Creates a shared pointer to a new ComponentManager.
     *
     * @return A shared pointer to a new ComponentManager.
     */
    static auto createShared() -> std::shared_ptr<ComponentManager>;

    /**
     * @brief Loads a component based on the provided parameters.
     *
     * @param params JSON object containing the parameters for the component.
     * @return true if the component is loaded successfully, false otherwise.
     */
    auto loadComponent(const json& params) -> bool;

    /**
     * @brief Unloads a component based on the provided parameters.
     *
     * @param params JSON object containing the parameters for the component.
     * @return true if the component is unloaded successfully, false otherwise.
     */
    auto unloadComponent(const json& params) -> bool;

    /**
     * @brief Scans the specified path for components.
     *
     * @param path The path to scan for components.
     * @return A vector of strings representing the new or modified components
     * found.
     */
    auto scanComponents(const std::string& path) -> std::vector<std::string>;

    /**
     * @brief Retrieves a component by its name.
     *
     * @param component_name The name of the component.
     * @return An optional weak pointer to the component if found, std::nullopt
     * otherwise.
     */
    auto getComponent(const std::string& component_name)
        -> std::optional<std::weak_ptr<Component>>;

    /**
     * @brief Retrieves information about a component by its name.
     *
     * @param component_name The name of the component.
     * @return An optional JSON object containing the component information if
     * found, std::nullopt otherwise.
     */
    auto getComponentInfo(const std::string& component_name)
        -> std::optional<json>;

    /**
     * @brief Retrieves a list of all components.
     *
     * @return A vector of strings representing the names of all components.
     */
    auto getComponentList() -> std::vector<std::string>;

    /**
     * @brief Retrieves the documentation of a component by its name.
     *
     * @param component_name The name of the component.
     * @return A string containing the documentation of the component.
     */
    auto getComponentDoc(const std::string& component_name) -> std::string;

    /**
     * @brief Checks if a component exists by its name.
     *
     * @param component_name The name of the component.
     * @return true if the component exists, false otherwise.
     */
    auto hasComponent(const std::string& component_name) -> bool;

    /**
     * @brief Prints the dependency tree of all components.
     */
    void printDependencyTree();

private:
    /**
     * @brief Updates the dependency graph for a component.
     *
     * @param component_name The name of the component.
     * @param version The version of the component.
     * @param dependencies A vector of strings representing the dependencies of
     * the component.
     * @param dependencies_version A vector of strings representing the versions
     * of the dependencies.
     */
    void updateDependencyGraph(
        const std::string& component_name, const std::string& version,
        const std::vector<std::string>& dependencies,
        const std::vector<std::string>& dependencies_version);

    std::unique_ptr<ComponentManagerImpl> impl_;
};

}  // namespace lithium