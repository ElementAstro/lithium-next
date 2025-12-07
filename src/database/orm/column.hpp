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

#ifndef LITHIUM_DATABASE_ORM_COLUMN_HPP
#define LITHIUM_DATABASE_ORM_COLUMN_HPP

#include <string>

#include "column_base.hpp"
#include "column_value.hpp"

// Statement type alias is defined in column_base.hpp

namespace lithium::database::orm {

template <typename T, typename Model>
class Column : public ColumnBase {
public:
    using MemberPtr = T Model::*;

    /**
     * @brief Constructs a Column instance.
     *
     * @param name The name of the column.
     * @param ptr A pointer to the member variable representing the column.
     * @param type The type of the column (optional).
     * @param constraints Additional SQL constraints for the column (optional).
     */
    Column(const std::string& name, MemberPtr ptr, const std::string& type = "",
           const std::string& constraints = "");

    /**
     * @brief Gets the name of the column.
     *
     * @return The name of the column.
     */
    std::string getName() const override;

    /**
     * @brief Gets the type of the column.
     *
     * @return The type of the column.
     */
    std::string getType() const override;

    /**
     * @brief Gets additional column constraints.
     *
     * @return String of constraints (e.g., "PRIMARY KEY", "NOT NULL").
     */
    std::string getConstraints() const override;

    /**
     * @brief Converts the column value to an SQL string.
     *
     * @param model A pointer to the model containing the column value.
     * @return The column value as an SQL string.
     */
    std::string toSQLValue(const void* model) const override;

    /**
     * @brief Sets the column value from an SQL string.
     *
     * @param model A pointer to the model to set the column value in.
     * @param value The column value as an SQL string.
     */
    void fromSQLValue(void* model, const std::string& value) const override;

    /**
     * @brief Binds the column value to a statement.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param model A pointer to the model containing the column value.
     */
    void bindToStatement(Statement& stmt, int index,
                         const void* model) const override;

    /**
     * @brief Reads the column value from a statement.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @param model A pointer to the model to set the column value in.
     */
    void readFromStatement(const Statement& stmt, int index,
                           void* model) const override;

private:
    std::string name;  ///< The name of the column.
    MemberPtr
        ptr;  ///< A pointer to the member variable representing the column.
    std::string customType;   ///< The custom type of the column (optional).
    std::string constraints;  ///< Additional SQL constraints for the column.
};

// Column implementation
template <typename T, typename Model>
Column<T, Model>::Column(const std::string& name, MemberPtr ptr,
                         const std::string& type,
                         const std::string& constraints)
    : name(name), ptr(ptr), customType(type), constraints(constraints) {}

template <typename T, typename Model>
std::string Column<T, Model>::getName() const {
    return name;
}

template <typename T, typename Model>
std::string Column<T, Model>::getType() const {
    if (!customType.empty()) {
        return customType;
    }
    if constexpr (std::is_same_v<T, int>)
        return "INTEGER";
    if constexpr (std::is_same_v<T, std::string>)
        return "TEXT";
    if constexpr (std::is_same_v<T, bool>)
        return "BOOLEAN";
    if constexpr (std::is_floating_point_v<T>)
        return "REAL";
    return "TEXT";
}

template <typename T, typename Model>
std::string Column<T, Model>::getConstraints() const {
    return constraints;
}

template <typename T, typename Model>
std::string Column<T, Model>::toSQLValue(const void* model) const {
    const Model* m = static_cast<const Model*>(model);
    return ColumnValue<T>::toSQLValue(m->*ptr);
}

template <typename T, typename Model>
void Column<T, Model>::fromSQLValue(void* model,
                                    const std::string& value) const {
    Model* m = static_cast<Model*>(model);
    m->*ptr = ColumnValue<T>::fromSQLValue(value);
}

template <typename T, typename Model>
void Column<T, Model>::bindToStatement(Statement& stmt, int index,
                                       const void* model) const {
    const Model* m = static_cast<const Model*>(model);
    ColumnValue<T>::bindToStatement(stmt, index, m->*ptr);
}

template <typename T, typename Model>
void Column<T, Model>::readFromStatement(const Statement& stmt, int index,
                                         void* model) const {
    Model* m = static_cast<Model*>(model);
    m->*ptr = ColumnValue<T>::readFromStatement(stmt, index);
}

}  // namespace lithium::database::orm

#endif  // LITHIUM_DATABASE_ORM_COLUMN_HPP
