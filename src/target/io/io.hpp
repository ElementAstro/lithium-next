// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file io.hpp
 * @brief Aggregated header for target IO module.
 *
 * This is the primary include file for the target IO components.
 * Include this file to get access to CSV and JSON handlers,
 * as well as the CSV reader utility.
 *
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2024 Max Qian
 */

#ifndef LITHIUM_TARGET_IO_MODULE_HPP
#define LITHIUM_TARGET_IO_MODULE_HPP

// ============================================================================
// IO Components
// ============================================================================

#include "csv_handler.hpp"
#include "json_handler.hpp"
#include "reader.hpp"

/**
 * @brief Target IO Module
 *
 * Provides comprehensive I/O functionality for the target module,
 * including CSV and JSON import/export handlers for celestial objects.
 *
 * Usage:
 * @code
 * using namespace lithium::target::io;
 *
 * // CSV import
 * CsvHandler csvHandler;
 * auto [objects, stats] =
 * csvHandler.importCelestialObjects("targets.csv").value();
 *
 * // JSON export
 * JsonHandler jsonHandler;
 * jsonHandler.exportCelestialObjects("targets.json", objects, true, 2);
 * @endcode
 */

namespace lithium::target::io {

/**
 * @brief Create a CSV handler instance
 * @return CsvHandler instance
 */
inline auto createCsvHandler() -> CsvHandler { return CsvHandler(); }

/**
 * @brief Create a JSON handler instance
 * @return JsonHandler instance
 */
inline auto createJsonHandler() -> JsonHandler { return JsonHandler(); }

}  // namespace lithium::target::io

#endif  // LITHIUM_TARGET_IO_MODULE_HPP
