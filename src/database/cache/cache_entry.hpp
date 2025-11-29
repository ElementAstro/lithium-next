// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Lithium-Next - A modern astrophotography terminal
 * Copyright (C) 2024 Max Qian
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LITHIUM_DATABASE_CACHE_CACHE_ENTRY_HPP
#define LITHIUM_DATABASE_CACHE_CACHE_ENTRY_HPP

#include <chrono>
#include <string>

namespace lithium::database::cache {

/**
 * @brief Represents a single cache entry with a value and expiration time.
 */
struct CacheEntry {
    std::string value;  ///< The cached value.
    std::chrono::steady_clock::time_point
        expiry;  ///< The expiration time.
};

}  // namespace lithium::database::cache

#endif  // LITHIUM_DATABASE_CACHE_CACHE_ENTRY_HPP
