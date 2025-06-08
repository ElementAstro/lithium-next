/*
 * manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager (the core of the plugin system)

**************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/memory/memory.hpp"
#include "atom/memory/object.hpp"
#include "atom/type/json_fwd.hpp"

#include "types.hpp"

using json = nlohmann::json;

namespace lithium {

// Forward declaration
class ComponentManagerImpl;

/**
 * @class ComponentManager
 * @brief Manages the lifecycle and dependencies of components in the system.
 *
 * The ComponentManager is responsible for loading, unloading, and managing
 * components. It also handles the dependency graph of components and ensures
 * that components are loaded and unloaded in the correct order.
 */
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
     * This method should be called before using the manager to ensure all
     * internal structures are properly set up.
     *
     * @return true if initialization is successful, false otherwise.
     */
    auto initialize() -> bool;

    /**
     * @brief Destroys the ComponentManager, unloading all components.
     *
     * This method will unload all loaded components and release resources.
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
     * This function searches the given directory for new or updated components.
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
     *
     * This function outputs the current dependency relationships between all
     * loaded components.
     */
    void printDependencyTree();

    /**
     * @brief Creates and registers a new component of type T.
     *
     * @tparam T The component type to create.
     * @param name The name to assign to the component.
     * @param options Optional configuration options for the component.
     * @return A shared pointer to the created component.
     */
    template <typename T>
    auto createComponent(const std::string& name,
                         const ComponentOptions& options = {})
        -> std::shared_ptr<T>;

    /**
     * @brief Starts a component by its name.
     *
     * @param name The name of the component.
     * @return true if the component is started successfully, false otherwise.
     */
    auto startComponent(const std::string& name) -> bool;

    /**
     * @brief Stops a component by its name.
     *
     * @param name The name of the component.
     * @return true if the component is stopped successfully, false otherwise.
     */
    auto stopComponent(const std::string& name) -> bool;

    /**
     * @brief Pauses a component by its name.
     *
     * @param name The name of the component.
     * @return true if the component is paused successfully, false otherwise.
     */
    auto pauseComponent(const std::string& name) -> bool;

    /**
     * @brief Resumes a paused component by its name.
     *
     * @param name The name of the component.
     * @return true if the component is resumed successfully, false otherwise.
     */
    auto resumeComponent(const std::string& name) -> bool;

    /**
     * @brief Registers an event listener for a specific component event.
     *
     * @param event The event type to listen for.
     * @param callback The callback function to invoke when the event occurs.
     */
    using EventCallback =
        std::function<void(const std::string&, ComponentEvent, const json&)>;
    void addEventListener(ComponentEvent event, EventCallback callback);

    /**
     * @brief Removes all event listeners for a specific event type.
     *
     * @param event The event type whose listeners should be removed.
     */
    void removeEventListener(ComponentEvent event);

    /**
     * @brief Loads multiple components in a batch operation.
     *
     * @param components A vector of component names to load.
     * @return true if all components are loaded successfully, false otherwise.
     */
    auto batchLoad(const std::vector<std::string>& components) -> bool;

    /**
     * @brief Unloads multiple components in a batch operation.
     *
     * @param components A vector of component names to unload.
     * @return true if all components are unloaded successfully, false
     * otherwise.
     */
    auto batchUnload(const std::vector<std::string>& components) -> bool;

    /**
     * @brief Retrieves the current state of a component.
     *
     * @param name The name of the component.
     * @return The current state of the component.
     */
    auto getComponentState(const std::string& name) -> ComponentState;

    /**
     * @brief Updates the configuration of a component.
     *
     * @param name The name of the component.
     * @param config The new configuration in JSON format.
     */
    void updateConfig(const std::string& name, const json& config);

    /**
     * @brief Retrieves the configuration of a component.
     *
     * @param name The name of the component.
     * @return The configuration of the component in JSON format.
     */
    auto getConfig(const std::string& name) -> json;

    /**
     * @brief Adds a component to a group.
     *
     * @param name The name of the component.
     * @param group The group to add the component to.
     */
    void addToGroup(const std::string& name, const std::string& group);

    /**
     * @brief Retrieves all components belonging to a specific group.
     *
     * @param group The name of the group.
     * @return A vector of component names in the group.
     */
    auto getGroupComponents(const std::string& group)
        -> std::vector<std::string>;

    /**
     * @brief Retrieves performance metrics for all components.
     *
     * @return A JSON object containing performance metrics.
     */
    auto getPerformanceMetrics() -> json;

    /**
     * @brief Enables or disables performance monitoring.
     *
     * @param enable True to enable, false to disable.
     */
    void enablePerformanceMonitoring(bool enable);

    /**
     * @brief Retrieves the last error message encountered by the manager.
     *
     * @return A string describing the last error.
     */
    auto getLastError() -> std::string;

    /**
     * @brief Clears all stored error messages.
     */
    void clearErrors();

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

    std::unique_ptr<ComponentManagerImpl>
        impl_; /**< Pointer to the implementation (PIMPL idiom). */

    // Event listeners for each event type.
    std::unordered_map<ComponentEvent, std::vector<EventCallback>>
        eventListeners_;

    // Current state of each component.
    std::unordered_map<std::string, ComponentState> componentStates_;

    // Options/configuration for each component.
    std::unordered_map<std::string, ComponentOptions> componentOptions_;

    // Group membership for components.
    std::unordered_map<std::string, std::vector<std::string>> componentGroups_;

    std::string lastError_; /**< Last error message. */
    bool performanceMonitoringEnabled_{
        false}; /**< Performance monitoring flag. */

    // Memory pool for components.
    std::shared_ptr<atom::memory::ObjectPool<std::shared_ptr<Component>>>
        component_pool_;

    // General-purpose memory pool.
    std::unique_ptr<MemoryPool<char, 4096>> memory_pool_;

    /**
     * @brief Notifies all registered listeners of a component event.
     *
     * @param component The name of the component.
     * @param event The event type.
     * @param data Additional event data in JSON format (optional).
     */
    void notifyListeners(const std::string& component, ComponentEvent event,
                         const json& data = {});

    /**
     * @brief Validates whether an operation can be performed on a component.
     *
     * @param name The name of the component.
     * @return true if the operation is valid, false otherwise.
     */
    auto validateComponentOperation(const std::string& name) -> bool;

    /**
     * @brief Updates the state of a component.
     *
     * @param name The name of the component.
     * @param state The new state to set.
     */
    void updateComponentState(const std::string& name, ComponentState state);

    /**
     * @brief Initializes a component by its name.
     *
     * @param name The name of the component.
     * @return true if initialization is successful, false otherwise.
     */
    auto initializeComponent(const std::string& name) -> bool;
};

}  // namespace lithium
