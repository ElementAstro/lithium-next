/*
 * dataframe_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "dataframe_ops.hpp"

#include <spdlog/spdlog.h>

#include "numpy_utils.hpp"

namespace lithium::numpy {

bool DataFrameOps::isPandasAvailable() {
    if (!pandas_available_) {
        try {
            py::module::import("pandas");
            pandas_available_ = true;
        } catch (...) {
            return false;
        }
    }
    return pandas_available_;
}

NumpyResult<py::object> DataFrameOps::createDataFrame(
    const std::unordered_map<std::string, std::vector<py::object>>& data) {

    if (!isPandasAvailable()) {
        return std::unexpected(NumpyError::ModuleNotFound);
    }

    try {
        py::module pd = py::module::import("pandas");
        py::dict pyData;

        for (const auto& [key, values] : data) {
            py::list pyList;
            for (const auto& val : values) {
                pyList.append(val);
            }
            pyData[py::cast(key)] = pyList;
        }

        return pd.attr("DataFrame")(pyData);

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to create DataFrame: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<py::object> DataFrameOps::createDataFrame(
    const std::vector<std::string>& columns,
    const std::vector<std::vector<py::object>>& rows) {

    if (!isPandasAvailable()) {
        return std::unexpected(NumpyError::ModuleNotFound);
    }

    try {
        py::module pd = py::module::import("pandas");

        py::list pyRows;
        for (const auto& row : rows) {
            py::list pyRow;
            for (const auto& val : row) {
                pyRow.append(val);
            }
            pyRows.append(pyRow);
        }

        py::list pyColumns;
        for (const auto& col : columns) {
            pyColumns.append(col);
        }

        return pd.attr("DataFrame")(pyRows, py::arg("columns") = pyColumns);

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to create DataFrame: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<std::string> DataFrameOps::dataFrameToJson(
    const py::object& df, const std::string& orient) {

    try {
        py::object json = df.attr("to_json")(py::arg("orient") = orient);
        return json.cast<std::string>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to convert DataFrame to JSON: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<py::array> DataFrameOps::getDataFrameColumn(
    const py::object& df, const std::string& columnName) {

    try {
        py::object column = df[py::cast(columnName)];
        return column.attr("values").cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to get DataFrame column: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<size_t> DataFrameOps::getDataFrameRowCount(const py::object& df) {
    try {
        return py::len(df);
    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to get DataFrame row count: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

NumpyResult<std::vector<std::string>> DataFrameOps::getDataFrameColumns(
    const py::object& df) {

    try {
        py::object columns = df.attr("columns");
        std::vector<std::string> result;

        for (auto col : columns) {
            result.push_back(py::str(col).cast<std::string>());
        }

        return result;

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to get DataFrame columns: {}", e.what());
        return std::unexpected(NumpyError::DataFrameError);
    }
}

}  // namespace lithium::numpy
