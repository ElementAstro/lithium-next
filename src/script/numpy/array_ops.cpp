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

// =========================================================================
// Template Implementations
// =========================================================================

template<NumpyCompatible T>
py::array_t<T> ArrayOps::createArray(const std::vector<T>& data) {
    return py::array_t<T>(data.size(), data.data());
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::createArray(T* data, const std::vector<size_t>& shape, bool copy) {
    std::vector<py::ssize_t> pyShape(shape.begin(), shape.end());

    // Calculate strides (C-order)
    std::vector<py::ssize_t> strides(shape.size());
    py::ssize_t stride = sizeof(T);
    for (ssize_t i = static_cast<ssize_t>(shape.size()) - 1; i >= 0; --i) {
        strides[i] = stride;
        stride *= static_cast<py::ssize_t>(shape[i]);
    }

    if (copy) {
        return py::array_t<T>(pyShape, strides, data);
    } else {
        // Return a view (caller must ensure data lifetime)
        return py::array_t<T>(
            pyShape, strides, data,
            py::capsule(data, [](void*) {}));  // No-op capsule for non-owning view
    }
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::createArray(std::span<T> data) {
    return py::array_t<T>(data.size(), data.data());
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::createArray2D(const std::vector<std::vector<T>>& data) {
    if (data.empty()) {
        return py::array_t<T>({0, 0});
    }

    size_t rows = data.size();
    size_t cols = data[0].size();

    auto arr = py::array_t<T>({rows, cols});
    auto ptr = arr.mutable_data();

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < std::min(cols, data[i].size()); ++j) {
            ptr[i * cols + j] = data[i][j];
        }
    }

    return arr;
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::zeros(const std::vector<size_t>& shape) {
    py::module np = py::module::import("numpy");
    std::vector<py::ssize_t> pyShape(shape.begin(), shape.end());
    return np.attr("zeros")(pyShape, py::dtype::of<T>()).cast<py::array_t<T>>();
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::empty(const std::vector<size_t>& shape) {
    py::module np = py::module::import("numpy");
    std::vector<py::ssize_t> pyShape(shape.begin(), shape.end());
    return np.attr("empty")(pyShape, py::dtype::of<T>()).cast<py::array_t<T>>();
}

template<NumpyCompatible T>
py::array_t<T> ArrayOps::full(const std::vector<size_t>& shape, T value) {
    py::module np = py::module::import("numpy");
    std::vector<py::ssize_t> pyShape(shape.begin(), shape.end());
    return np.attr("full")(pyShape, value, py::dtype::of<T>()).cast<py::array_t<T>>();
}

template<NumpyCompatible T>
std::vector<T> ArrayOps::toVector(const py::array_t<T>& arr) {
    auto info = arr.request();
    T* data = static_cast<T*>(info.ptr);
    return std::vector<T>(data, data + arr.size());
}

template<NumpyCompatible T>
std::vector<std::vector<T>> ArrayOps::toVector2D(const py::array_t<T>& arr) {
    auto info = arr.request();
    if (info.ndim != 2) {
        throw std::runtime_error("Expected 2D array");
    }

    size_t rows = info.shape[0];
    size_t cols = info.shape[1];
    T* data = static_cast<T*>(info.ptr);

    std::vector<std::vector<T>> result(rows);
    for (size_t i = 0; i < rows; ++i) {
        result[i].resize(cols);
        for (size_t j = 0; j < cols; ++j) {
            result[i][j] = data[i * cols + j];
        }
    }

    return result;
}

template<NumpyCompatible T>
void ArrayOps::copyToBuffer(const py::array_t<T>& arr, T* buffer, size_t bufferSize) {
    auto info = arr.request();
    size_t copySize = std::min(static_cast<size_t>(arr.size()), bufferSize);
    std::memcpy(buffer, info.ptr, copySize * sizeof(T));
}

}  // namespace lithium::numpy
