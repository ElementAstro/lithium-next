/*
 * exceptions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Exceptions

**************************************************/

#pragma once

#include "atom/error/exception.hpp"

namespace lithium {

// ============================================================================
// Component Loading Exceptions
// ============================================================================

/**
 * @brief Exception thrown when a component fails to load.
 */
class FailToLoadComponent : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a component fails to unload.
 */
class FailToUnloadComponent : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a component is not found.
 */
class ComponentNotFound : public atom::error::Exception {
public:
    using Exception::Exception;
};

// ============================================================================
// Component State Exceptions
// ============================================================================

/**
 * @brief Exception thrown when a component state transition is invalid.
 */
class InvalidStateTransition : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a component operation times out.
 */
class ComponentTimeout : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a component is in an invalid state for an
 * operation.
 */
class InvalidComponentState : public atom::error::Exception {
public:
    using Exception::Exception;
};

// ============================================================================
// Dependency Exceptions
// ============================================================================

/**
 * @brief Exception thrown when a dependency is missing.
 */
class MissingDependency : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a circular dependency is detected.
 */
class CircularDependency : public atom::error::Exception {
public:
    using Exception::Exception;
};

/**
 * @brief Exception thrown when a dependency version conflict is detected.
 */
class DependencyVersionConflict : public atom::error::Exception {
public:
    using Exception::Exception;
};

// ============================================================================
// Configuration Exceptions
// ============================================================================

/**
 * @brief Exception thrown when component configuration is invalid.
 */
class InvalidConfiguration : public atom::error::Exception {
public:
    using Exception::Exception;
};

// ============================================================================
// Convenience Macros
// ============================================================================

#define THROW_FAIL_TO_LOAD_COMPONENT(...)                              \
    throw lithium::FailToLoadComponent(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_FAIL_TO_UNLOAD_COMPONENT(...)                              \
    throw lithium::FailToUnloadComponent(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_COMPONENT_NOT_FOUND(...)                               \
    throw lithium::ComponentNotFound(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_INVALID_STATE_TRANSITION(...)                               \
    throw lithium::InvalidStateTransition(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                          ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_COMPONENT_TIMEOUT(...)                                \
    throw lithium::ComponentTimeout(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_INVALID_COMPONENT_STATE(...)                               \
    throw lithium::InvalidComponentState(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_MISSING_DEPENDENCY(...)                                \
    throw lithium::MissingDependency(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                     ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_CIRCULAR_DEPENDENCY(...)                                \
    throw lithium::CircularDependency(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                      ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_DEPENDENCY_VERSION_CONFLICT(...)                               \
    throw lithium::DependencyVersionConflict(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                             ATOM_FUNC_NAME, __VA_ARGS__)

#define THROW_INVALID_CONFIGURATION(...)                                \
    throw lithium::InvalidConfiguration(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                        ATOM_FUNC_NAME, __VA_ARGS__)

}  // namespace lithium
