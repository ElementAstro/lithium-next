/*
 * array_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "array_ops.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace lithium::numpy {

std::vector<size_t> ArrayOps::getShape(const py::array& arr) {
    std::vector<size_t> shape;
    auto info = arr.request();
    for (auto dim : info.shape) {
        shape.push_back(static_cast<size_t>(dim));
    }
    return shape;
}

std::string ArrayOps::getDtypeName(const py::array& arr) {
    try {
        return py::str(arr.dtype()).cast<std::string>();
    } catch (...) {
        return "unknown";
    }
}

NumpyResult<py::array> ArrayOps::reshape(
    const py::array& arr, const std::vector<size_t>& newShape) {

    try {
        py::module np = py::module::import("numpy");
        std::vector<py::ssize_t> pyShape(newShape.begin(), newShape.end());
        return np.attr("reshape")(arr, pyShape).cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to reshape array: {}", e.what());
        return std::unexpected(NumpyError::ShapeMismatch);
    }
}

NumpyResult<py::array> ArrayOps::transpose(const py::array& arr) {
    try {
        py::module np = py::module::import("numpy");
        return np.attr("transpose")(arr).cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to transpose array: {}", e.what());
        return std::unexpected(NumpyError::ArrayCreationFailed);
    }
}

NumpyResult<py::array> ArrayOps::stack(
    const std::vector<py::array>& arrays, int axis) {

    try {
        py::module np = py::module::import("numpy");
        py::list pyArrays;
        for (const auto& arr : arrays) {
            pyArrays.append(arr);
        }
        return np.attr("stack")(pyArrays, py::arg("axis") = axis).cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to stack arrays: {}", e.what());
        return std::unexpected(NumpyError::ArrayCreationFailed);
    }
}

NumpyResult<py::array> ArrayOps::concatenate(
    const std::vector<py::array>& arrays, int axis) {

    try {
        py::module np = py::module::import("numpy");
        py::list pyArrays;
        for (const auto& arr : arrays) {
            pyArrays.append(arr);
        }
        return np.attr("concatenate")(pyArrays, py::arg("axis") = axis).cast<py::array>();

    } catch (const py::error_already_set& e) {
        spdlog::error("Failed to concatenate arrays: {}", e.what());
        return std::unexpected(NumpyError::ArrayCreationFailed);
    }
}

// Template implementations are in the header file (array_ops.hpp)

}  // namespace lithium::numpy
