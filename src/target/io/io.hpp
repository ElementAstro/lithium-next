// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef LITHIUM_TARGET_IO_MODULE_HPP
#define LITHIUM_TARGET_IO_MODULE_HPP

// Include all IO-related headers
#include "csv_handler.hpp"
#include "json_handler.hpp"

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
 * auto [objects, stats] = csvHandler.importCelestialObjects("targets.csv").value();
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
inline auto createCsvHandler() -> CsvHandler {
    return CsvHandler();
}

/**
 * @brief Create a JSON handler instance
 * @return JsonHandler instance
 */
inline auto createJsonHandler() -> JsonHandler {
    return JsonHandler();
}

}  // namespace lithium::target::io

#endif  // LITHIUM_TARGET_IO_MODULE_HPP
