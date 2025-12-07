// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file search.hpp
 * @brief Aggregated header for target search module.
 *
 * This is the primary include file for the target search components.
 * Include this file to get access to search engine, coordinate searcher,
 * and filter evaluator.
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#pragma once

// ============================================================================
// Search Components
// ============================================================================

#include "coordinate_searcher.hpp"
#include "engine.hpp"
#include "filter_evaluator.hpp"
#include "search_engine.hpp"

namespace lithium::target::search {

/**
 * @brief Search module version.
 */
inline constexpr const char* SEARCH_MODULE_VERSION = "1.0.0";

/**
 * @brief Get search module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getSearchModuleVersion() noexcept {
    return SEARCH_MODULE_VERSION;
}

}  // namespace lithium::target::search
