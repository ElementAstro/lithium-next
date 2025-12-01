// ===========================================================================
// Lithium-Target Module Aggregate Header
// ===========================================================================
// This project is licensed under the terms of the GPL3 license.
//
// Project Name: Lithium-Target
// Description: Complete target management system with modular architecture
// Author: Max Qian
// License: GPL3
//
// Module Architecture:
//   - model/        : Core data models for celestial targets
//   - index/        : Indexing and caching mechanisms
//   - repository/   : Data access and persistence layer
//   - search/       : Search engine and query mechanisms
//   - recommendation/: Preference management and recommendations
//   - observability/: Metrics, monitoring, and logging
//   - io/           : Input/Output operations and file handling
//   - service/      : High-level service layer
// ===========================================================================

#pragma once

// Core model types
#include "model/model.hpp"
#include "model/celestial_model.hpp"

// Index and caching
#include "index/index.hpp"

// Data persistence
#include "repository/repository.hpp"
#include "repository/celestial_repository.hpp"

// Search functionality
#include "search/search.hpp"
#include "engine.hpp"
#include "reader.hpp"

// Recommendations and preferences
#include "recommendation/recommendation.hpp"
#include "recommendation/preference.hpp"

// Observability
#include "observability/observability.hpp"

// IO operations
#include "io/io.hpp"

// Service layer
#include "service/service.hpp"

// ===========================================================================
// Convenience Namespace Aggregation
// ===========================================================================

namespace lithium::target {

// Re-export all submodule namespaces for convenient access
using namespace lithium::target::model;
using namespace lithium::target::index;
using namespace lithium::target::repository;
using namespace lithium::target::search;
using namespace lithium::target::recommendation;
using namespace lithium::target::observability;
using namespace lithium::target::io;
using namespace lithium::target::service;

}  // namespace lithium::target
