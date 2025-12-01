// ===========================================================================
// Lithium-Target Service Module Header
// ===========================================================================
// This project is licensed under the terms of the GPL3 license.
//
// Module: Target Service
// Description: Public interface for target service subsystem
// ===========================================================================

#pragma once

#include "celestial_service.hpp"

// Aggregate namespace for service module
namespace lithium::target::service {
// Re-export service classes and functions

// Core service class
using CelestialServiceClass = CelestialService;

// Configuration types
using ServiceConfigType = ServiceConfig;
using ServiceStatsType = ServiceStats;

}  // namespace lithium::target::service
