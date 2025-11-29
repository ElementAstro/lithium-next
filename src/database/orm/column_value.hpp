/*
 * This file is part of Lithium-Next.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * See LICENSE file for more details.
 *
 * Lithium-Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Lithium-Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Lithium-Next. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LITHIUM_DATABASE_ORM_COLUMN_VALUE_HPP
#define LITHIUM_DATABASE_ORM_COLUMN_VALUE_HPP

#include <sstream>
#include <string>
#include <type_traits>

namespace lithium::database {

// Forward declarations
class Statement;

}  // namespace lithium::database

namespace lithium::database::orm {

// Using std::false_type is a modern C++ idiom for static_assert failure
template <typename T>
struct always_false : std::false_type {};

// SQL string escaping helper
namespace impl {
inline std::string escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() +
                   10);  // Pre-allocate to avoid multiple reallocations

    for (char c : str) {
        if (c == '\'') {
            result += "''";
        } else {
            result += c;
        }
    }
    return result;
}
}  // namespace impl

template <typename T>
struct ColumnValue {
    /**
     * @brief Converts a value to an SQL string.
     *
     * @param value The value to convert.
     * @return The value as an SQL string.
     */
    static std::string toSQLValue(const T& value);

    /**
     * @brief Converts an SQL string to a value.
     *
     * @param value The SQL string to convert.
     * @return The converted value.
     */
    static T fromSQLValue(const std::string& value);

    /**
     * @brief Binds a value to a statement parameter.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param value The value to bind.
     */
    static void bindToStatement(lithium::database::Statement& stmt, int index,
                                const T& value);

    /**
     * @brief Reads a value from a statement column.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @return The read value.
     */
    static T readFromStatement(const lithium::database::Statement& stmt,
                               int index);
};

// ColumnValue implementation
template <typename T>
std::string ColumnValue<T>::toSQLValue(const T& value) {
    std::stringstream ss;
    if constexpr (std::is_integral_v<T>) {
        ss << value;
    } else if constexpr (std::is_floating_point_v<T>) {
        ss << value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        ss << "'" << impl::escapeString(value) << "'";
    } else if constexpr (std::is_same_v<T, bool>) {
        ss << (value ? 1 : 0);
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
    return ss.str();
}

template <typename T>
T ColumnValue<T>::fromSQLValue(const std::string& value) {
    std::stringstream ss(value);
    T result;
    if constexpr (std::is_same_v<T, std::string>) {
        result = value;
    } else {
        ss >> result;
    }
    return result;
}

template <typename T>
void ColumnValue<T>::bindToStatement(lithium::database::Statement& stmt,
                                     int index, const T& value) {
    if constexpr (std::is_integral_v<T>) {
        stmt.bind(index, static_cast<int64_t>(value));
    } else if constexpr (std::is_floating_point_v<T>) {
        stmt.bind(index, static_cast<double>(value));
    } else if constexpr (std::is_same_v<T, std::string>) {
        stmt.bind(index, value);
    } else if constexpr (std::is_same_v<T, bool>) {
        stmt.bind(index, value ? 1 : 0);
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
}

template <typename T>
T ColumnValue<T>::readFromStatement(const lithium::database::Statement& stmt,
                                    int index) {
    if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(stmt.getInt64(index));
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(stmt.getDouble(index));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return stmt.getText(index);
    } else if constexpr (std::is_same_v<T, bool>) {
        return stmt.getInt(index) != 0;
    } else {
        static_assert(always_false<T>::value, "Unsupported type");
    }
}

}  // namespace lithium::database::orm

#endif  // LITHIUM_DATABASE_ORM_COLUMN_VALUE_HPP
