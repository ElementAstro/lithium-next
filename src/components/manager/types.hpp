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
    Error /**< Event triggered when an error occurs in the component. */
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
    Error        /**< The component is in an error state. */
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
};

}  // namespace lithium
