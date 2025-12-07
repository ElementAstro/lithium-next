/*
 * solver.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Solver plugin system aggregator header

**************************************************/

#ifndef LITHIUM_CLIENT_SOLVER_HPP
#define LITHIUM_CLIENT_SOLVER_HPP

// Common types
#include "common/solver_types.hpp"

// Plugin system
#include "plugin/solver_plugin_interface.hpp"
#include "plugin/solver_plugin_loader.hpp"

// Services
#include "service/solver_factory.hpp"
#include "service/solver_manager.hpp"
#include "service/solver_type_registry.hpp"

#endif  // LITHIUM_CLIENT_SOLVER_HPP
