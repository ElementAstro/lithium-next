/*
 * dataframe_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file dataframe_ops.hpp
 * @brief Pandas DataFrame Operations
 * @date 2024
 * @version 1.0.0
 *
 * This module provides utilities for creating, manipulating, and converting
 * Pandas DataFrames. It includes methods for:
 * - DataFrame creation from various data structures
 * - JSON conversion
 * - Column and row access
 * - Metadata retrieval
 */

#ifndef LITHIUM_SCRIPT_DATAFRAME_OPS_HPP
#define LITHIUM_SCRIPT_DATAFRAME_OPS_HPP

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

// Forward declarations
enum class NumpyError;
template <typename T>
using NumpyResult = std::expected<T, NumpyError>;

/**
 * @brief DataFrame operations using Pandas
 */
class DataFrameOps {
public:
    /**
     * @brief Check if Pandas is available
     * @return True if Pandas module can be imported, false otherwise
     */
    [[nodiscard]] static bool isPandasAvailable();

    // =========================================================================
    // DataFrame Creation
    // =========================================================================

    /**
     * @brief Create a Pandas DataFrame from JSON-like structure
     * @param data Map of column names to vectors of Python objects
     * @return DataFrame object on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::object> createDataFrame(
        const std::unordered_map<std::string, std::vector<py::object>>& data);

    /**
     * @brief Create a DataFrame from column names and rows
     * @param columns Vector of column names
     * @param rows Vector of rows, where each row is a vector of Python objects
     * @return DataFrame object on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::object> createDataFrame(
        const std::vector<std::string>& columns,
        const std::vector<std::vector<py::object>>& rows);

    // =========================================================================
    // DataFrame Conversion
    // =========================================================================

    /**
     * @brief Convert DataFrame to JSON string
     * @param df Pandas DataFrame object
     * @param orient JSON orientation ("records", "index", "columns", "values",
     * etc.)
     * @return JSON string on success, error on failure
     */
    [[nodiscard]] static NumpyResult<std::string> dataFrameToJson(
        const py::object& df, const std::string& orient = "records");

    // =========================================================================
    // DataFrame Access
    // =========================================================================

    /**
     * @brief Get DataFrame column as NumPy array
     * @param df Pandas DataFrame object
     * @param columnName Name of the column to retrieve
     * @return NumPy array containing column values on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> getDataFrameColumn(
        const py::object& df, const std::string& columnName);

    /**
     * @brief Get number of rows in DataFrame
     * @param df Pandas DataFrame object
     * @return Row count on success, error on failure
     */
    [[nodiscard]] static NumpyResult<size_t> getDataFrameRowCount(
        const py::object& df);

    /**
     * @brief Get all column names from DataFrame
     * @param df Pandas DataFrame object
     * @return Vector of column names on success, error on failure
     */
    [[nodiscard]] static NumpyResult<std::vector<std::string>>
    getDataFrameColumns(const py::object& df);

private:
    static inline bool pandas_available_ = false;
};

}  // namespace lithium::numpy

#endif  // LITHIUM_SCRIPT_DATAFRAME_OPS_HPP
