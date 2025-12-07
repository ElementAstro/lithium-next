/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Types and Enums

**************************************************/

#pragma once

#include <string>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium {

/**
 * @brief Enumerates the types of events that can occur for a component.
 *
 * Used to signal various stages and changes in a component's lifecycle.
 */
enum class ComponentEvent {
    PreLoad,       /**< Event triggered before a component is loaded. */
    PostLoad,      /**< Event triggered after a component is loaded. */
    PreUnload,     /**< Event triggered before a component is unloaded. */
    PostUnload,    /**< Event triggered after a component is unloaded. */
    ConfigChanged, /**< Event triggered when a component's configuration
                      changes. */
    StateChanged,  /**< Event triggered when a component's state changes. */
    Error, /**< Event triggered when an error occurs in the component. */
    DependencyResolved, /**< Event triggered when dependencies are resolved. */
    HealthCheck         /**< Event triggered during health check. */
};

/**
 * @brief Enumerates the possible lifecycle states of a component.
 *
 * Represents the current operational state of a component.
 */
enum class ComponentState {
    Created,     /**< The component has been created but not yet initialized. */
    Initialized, /**< The component has been initialized and is ready to run. */
    Running,     /**< The component is currently running. */
    Paused,      /**< The component is paused. */
    Stopped,     /**< The component has been stopped. */
    Error,       /**< The component is in an error state. */
    Unloading,   /**< The component is being unloaded. */
    Disabled     /**< The component is disabled. */
};

/**
 * @brief Configuration options for a component.
 *
 * Defines startup behavior, loading strategy, priority, grouping, and custom
 * configuration.
 */
struct ComponentOptions {
    bool autoStart{
        true};         /**< Whether the component should start automatically. */
    bool lazy{false};  /**< Whether the component should be loaded lazily. */
    int priority{0};   /**< The loading priority of the component (higher means
                          higher priority). */
    std::string group; /**< The group to which the component belongs. */
    json config; /**< Custom configuration for the component, in JSON format. */
    int timeout{
        30000}; /**< Timeout in milliseconds for component operations. */
    bool restartOnError{false}; /**< Whether to restart on error. */
    int maxRetries{3}; /**< Maximum retry attempts for failed operations. */
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Convert ComponentEvent to string representation.
 * @param event The ComponentEvent to convert.
 * @return String name of the event.
 */
[[nodiscard]] inline std::string componentEventToString(ComponentEvent event) {
    switch (event) {
        case ComponentEvent::PreLoad:
            return "PreLoad";
        case ComponentEvent::PostLoad:
            return "PostLoad";
        case ComponentEvent::PreUnload:
            return "PreUnload";
        case ComponentEvent::PostUnload:
            return "PostUnload";
        case ComponentEvent::ConfigChanged:
            return "ConfigChanged";
        case ComponentEvent::StateChanged:
            return "StateChanged";
        case ComponentEvent::Error:
            return "Error";
        case ComponentEvent::DependencyResolved:
            return "DependencyResolved";
        case ComponentEvent::HealthCheck:
            return "HealthCheck";
    }
    return "Unknown";
}

/**
 * @brief Convert ComponentState to string representation.
 * @param state The ComponentState to convert.
 * @return String name of the state.
 */
[[nodiscard]] inline std::string componentStateToString(ComponentState state) {
    switch (state) {
        case ComponentState::Created:
            return "Created";
        case ComponentState::Initialized:
            return "Initialized";
        case ComponentState::Running:
            return "Running";
        case ComponentState::Paused:
            return "Paused";
        case ComponentState::Stopped:
            return "Stopped";
        case ComponentState::Error:
            return "Error";
        case ComponentState::Unloading:
            return "Unloading";
        case ComponentState::Disabled:
            return "Disabled";
    }
    return "Unknown";
}

/**
 * @brief Check if a state transition is valid.
 * @param from Current state.
 * @param to Target state.
 * @return True if the transition is valid.
 */
[[nodiscard]] inline bool isValidStateTransition(ComponentState from,
                                                 ComponentState to) {
    // Error and Disabled can transition from any state
    if (to == ComponentState::Error || to == ComponentState::Disabled) {
        return true;
    }

    switch (from) {
        case ComponentState::Created:
            return to == ComponentState::Initialized ||
                   to == ComponentState::Stopped;
        case ComponentState::Initialized:
            return to == ComponentState::Running ||
                   to == ComponentState::Stopped;
        case ComponentState::Running:
            return to == ComponentState::Paused ||
                   to == ComponentState::Stopped ||
                   to == ComponentState::Unloading;
        case ComponentState::Paused:
            return to == ComponentState::Running ||
                   to == ComponentState::Stopped;
        case ComponentState::Stopped:
            return to == ComponentState::Initialized ||
                   to == ComponentState::Unloading;
        case ComponentState::Error:
            return to == ComponentState::Stopped ||
                   to == ComponentState::Initialized;
        case ComponentState::Unloading:
            return false;  // Terminal state
        case ComponentState::Disabled:
            return to == ComponentState::Created;
    }
    return false;
}

/**
 * @brief Check if a component state is active (running or paused).
 * @param state The state to check.
 * @return True if the component is active.
 */
[[nodiscard]] inline bool isActiveState(ComponentState state) {
    return state == ComponentState::Running || state == ComponentState::Paused;
}

/**
 * @brief Check if a component state is terminal (cannot transition further).
 * @param state The state to check.
 * @return True if the state is terminal.
 */
[[nodiscard]] inline bool isTerminalState(ComponentState state) {
    return state == ComponentState::Unloading;
}

}  // namespace lithium
