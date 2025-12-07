/**
 * @file manager_module.hpp
 * @brief Aggregated header for component manager module.
 *
 * Include this file to get access to all component manager functionality
 * including lifecycle management, event handling, and configuration.
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_COMPONENTS_MANAGER_MODULE_HPP
#define LITHIUM_COMPONENTS_MANAGER_MODULE_HPP

#include <memory>

// ============================================================================
// Manager Module Components
// ============================================================================

#include "exceptions.hpp"
#include "manager.hpp"
#include "types.hpp"

namespace lithium {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Manager module version.
 */
inline constexpr const char* MANAGER_MODULE_VERSION = "1.1.0";

/**
 * @brief Get manager module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getManagerModuleVersion() noexcept {
    return MANAGER_MODULE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to ComponentManager
using ComponentManagerPtr = std::shared_ptr<ComponentManager>;

/// Weak pointer to ComponentManager
using ComponentManagerWeakPtr = std::weak_ptr<ComponentManager>;

/// Event callback type
using ComponentEventCallback = ComponentManager::EventCallback;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new ComponentManager instance.
 * @return Shared pointer to the new ComponentManager.
 */
[[nodiscard]] inline ComponentManagerPtr createComponentManager() {
    return ComponentManager::createShared();
}

/**
 * @brief Create and initialize a new ComponentManager instance.
 * @return Shared pointer to the initialized ComponentManager, or nullptr on
 * failure.
 */
[[nodiscard]] inline ComponentManagerPtr createAndInitializeComponentManager() {
    auto manager = ComponentManager::createShared();
    if (manager && manager->initialize()) {
        return manager;
    }
    return nullptr;
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Get the string representation of a component event.
 * @param event The event to convert.
 * @return String name of the event.
 */
[[nodiscard]] inline std::string getEventName(ComponentEvent event) {
    return componentEventToString(event);
}

/**
 * @brief Get the string representation of a component state.
 * @param state The state to convert.
 * @return String name of the state.
 */
[[nodiscard]] inline std::string getStateName(ComponentState state) {
    return componentStateToString(state);
}

/**
 * @brief Check if a component state is operational.
 * @param state The state to check.
 * @return True if the component can perform operations.
 */
[[nodiscard]] inline bool isOperationalState(ComponentState state) {
    return state == ComponentState::Running ||
           state == ComponentState::Initialized ||
           state == ComponentState::Paused;
}

/**
 * @brief Check if a component state indicates an error condition.
 * @param state The state to check.
 * @return True if the component is in an error state.
 */
[[nodiscard]] inline bool isErrorState(ComponentState state) {
    return state == ComponentState::Error;
}

/**
 * @brief Create default component options.
 * @return Default ComponentOptions structure.
 */
[[nodiscard]] inline ComponentOptions createDefaultOptions() {
    return ComponentOptions{};
}

/**
 * @brief Create component options with specific priority.
 * @param priority The loading priority.
 * @return ComponentOptions with the specified priority.
 */
[[nodiscard]] inline ComponentOptions createOptionsWithPriority(int priority) {
    ComponentOptions options;
    options.priority = priority;
    return options;
}

/**
 * @brief Create component options for lazy loading.
 * @return ComponentOptions configured for lazy loading.
 */
[[nodiscard]] inline ComponentOptions createLazyLoadOptions() {
    ComponentOptions options;
    options.lazy = true;
    options.autoStart = false;
    return options;
}

}  // namespace lithium

#endif  // LITHIUM_COMPONENTS_MANAGER_MODULE_HPP
