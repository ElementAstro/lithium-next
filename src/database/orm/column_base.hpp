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

#ifndef LITHIUM_DATABASE_ORM_COLUMN_BASE_HPP
#define LITHIUM_DATABASE_ORM_COLUMN_BASE_HPP

#include <string>

#include "database/core/statement.hpp"

namespace lithium::database {
// Bring Statement from core namespace into database namespace
using core::Statement;
}  // namespace lithium::database

namespace lithium::database::orm {

class ColumnBase {
public:
    /**
     * @brief Virtual destructor for ColumnBase.
     */
    virtual ~ColumnBase() = default;

    /**
     * @brief Gets the name of the column.
     *
     * @return The name of the column.
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Gets the type of the column.
     *
     * @return The type of the column.
     */
    virtual std::string getType() const = 0;

    /**
     * @brief Gets additional column constraints.
     *
     * @return String of constraints (e.g., "PRIMARY KEY", "NOT NULL").
     */
    virtual std::string getConstraints() const = 0;

    /**
     * @brief Converts the column value to an SQL string.
     *
     * @param model A pointer to the model containing the column value.
     * @return The column value as an SQL string.
     */
    virtual std::string toSQLValue(const void* model) const = 0;

    /**
     * @brief Sets the column value from an SQL string.
     *
     * @param model A pointer to the model to set the column value in.
     * @param value The column value as an SQL string.
     */
    virtual void fromSQLValue(void* model, const std::string& value) const = 0;

    /**
     * @brief Binds the column value to a statement.
     *
     * @param stmt The statement to bind to.
     * @param index The parameter index to bind at.
     * @param model A pointer to the model containing the column value.
     */
    virtual void bindToStatement(lithium::database::Statement& stmt, int index,
                                 const void* model) const = 0;

    /**
     * @brief Reads the column value from a statement.
     *
     * @param stmt The statement to read from.
     * @param index The column index to read from.
     * @param model A pointer to the model to set the column value in.
     */
    virtual void readFromStatement(const lithium::database::Statement& stmt,
                                   int index, void* model) const = 0;
};

}  // namespace lithium::database::orm

#endif  // LITHIUM_DATABASE_ORM_COLUMN_BASE_HPP
