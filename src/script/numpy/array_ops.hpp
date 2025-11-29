/*
 * array_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file array_ops.hpp
 * @brief NumPy Array Operations
 * @date 2024
 * @version 1.0.0
 *
 * This module provides utilities for NumPy array manipulation:
 * - Array creation and transformation
 * - Shape and dtype operations
 * - Array stacking and concatenation
 */

#ifndef LITHIUM_SCRIPT_ARRAY_OPS_HPP
#define LITHIUM_SCRIPT_ARRAY_OPS_HPP

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <concepts>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

// Forward declarations
enum class NumpyError;
template<typename T>
using NumpyResult = std::expected<T, NumpyError>;

/**
 * @brief NumPy dtype type trait
 */
template<typename T>
struct NumpyDtype {
    static constexpr const char* format = nullptr;
    static constexpr const char* name = nullptr;
};

// Specializations for common types
template<> struct NumpyDtype<float> {
    static constexpr const char* format = "f";
    static constexpr const char* name = "float32";
};

template<> struct NumpyDtype<double> {
    static constexpr const char* format = "d";
    static constexpr const char* name = "float64";
};

template<> struct NumpyDtype<int8_t> {
    static constexpr const char* format = "b";
    static constexpr const char* name = "int8";
};

template<> struct NumpyDtype<uint8_t> {
    static constexpr const char* format = "B";
    static constexpr const char* name = "uint8";
};

template<> struct NumpyDtype<int16_t> {
    static constexpr const char* format = "h";
    static constexpr const char* name = "int16";
};

template<> struct NumpyDtype<uint16_t> {
    static constexpr const char* format = "H";
    static constexpr const char* name = "uint16";
};

template<> struct NumpyDtype<int32_t> {
    static constexpr const char* format = "i";
    static constexpr const char* name = "int32";
};

template<> struct NumpyDtype<uint32_t> {
    static constexpr const char* format = "I";
    static constexpr const char* name = "uint32";
};

template<> struct NumpyDtype<int64_t> {
    static constexpr const char* format = "q";
    static constexpr const char* name = "int64";
};

template<> struct NumpyDtype<uint64_t> {
    static constexpr const char* format = "Q";
    static constexpr const char* name = "uint64";
};

template<> struct NumpyDtype<bool> {
    static constexpr const char* format = "?";
    static constexpr const char* name = "bool";
};

/**
 * @brief Concept for NumPy-compatible types
 */
template<typename T>
concept NumpyCompatible = requires {
    { NumpyDtype<T>::format } -> std::convertible_to<const char*>;
    requires std::is_trivially_copyable_v<T>;
};

/**
 * @brief NumPy array operations
 */
class ArrayOps {
public:
    // =========================================================================
    // Array Creation
    // =========================================================================

    /**
     * @brief Create a NumPy array from a vector
     * @tparam T NumPy-compatible data type
     * @param data Vector of values
     * @return NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(const std::vector<T>& data);

    /**
     * @brief Create a NumPy array from raw data with shape
     * @tparam T NumPy-compatible data type
     * @param data Pointer to data
     * @param shape Array dimensions
     * @param copy Whether to copy data (true) or create a view (false)
     * @return NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(
        T* data, const std::vector<size_t>& shape, bool copy = true);

    /**
     * @brief Create a NumPy array from a span
     * @tparam T NumPy-compatible data type
     * @param data Span of values
     * @return NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(std::span<T> data);

    /**
     * @brief Create a 2D NumPy array from nested vectors
     * @tparam T NumPy-compatible data type
     * @param data Nested vector of values
     * @return 2D NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray2D(
        const std::vector<std::vector<T>>& data);

    /**
     * @brief Create a zeros array
     * @tparam T NumPy-compatible data type
     * @param shape Array dimensions
     * @return NumPy array filled with zeros
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> zeros(const std::vector<size_t>& shape);

    /**
     * @brief Create an empty array
     * @tparam T NumPy-compatible data type
     * @param shape Array dimensions
     * @return Uninitialized NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> empty(const std::vector<size_t>& shape);

    /**
     * @brief Create an array filled with a value
     * @tparam T NumPy-compatible data type
     * @param shape Array dimensions
     * @param value Fill value
     * @return NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> full(const std::vector<size_t>& shape, T value);

    // =========================================================================
    // Array Conversion
    // =========================================================================

    /**
     * @brief Convert NumPy array to vector
     * @tparam T NumPy-compatible data type
     * @param arr NumPy array
     * @return Vector copy of array data
     */
    template<NumpyCompatible T>
    [[nodiscard]] static std::vector<T> toVector(const py::array_t<T>& arr);

    /**
     * @brief Convert NumPy array to 2D nested vector
     * @tparam T NumPy-compatible data type
     * @param arr 2D NumPy array
     * @return Nested vector of array data
     */
    template<NumpyCompatible T>
    [[nodiscard]] static std::vector<std::vector<T>> toVector2D(
        const py::array_t<T>& arr);

    /**
     * @brief Copy array data to a buffer
     * @tparam T NumPy-compatible data type
     * @param arr NumPy array
     * @param buffer Destination buffer
     * @param bufferSize Size of buffer in elements
     */
    template<NumpyCompatible T>
    static void copyToBuffer(const py::array_t<T>& arr, T* buffer, size_t bufferSize);

    /**
     * @brief Get array shape as vector
     * @param arr Any NumPy array
     * @return Vector of dimension sizes
     */
    [[nodiscard]] static std::vector<size_t> getShape(const py::array& arr);

    /**
     * @brief Get array dtype name
     * @param arr Any NumPy array
     * @return String name of dtype (e.g., "float32", "int64")
     */
    [[nodiscard]] static std::string getDtypeName(const py::array& arr);

    // =========================================================================
    // Array Operations
    // =========================================================================

    /**
     * @brief Reshape array
     * @param arr Input array
     * @param newShape New shape
     * @return Reshaped array on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> reshape(
        const py::array& arr, const std::vector<size_t>& newShape);

    /**
     * @brief Transpose array
     * @param arr Input array
     * @return Transposed array on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> transpose(const py::array& arr);

    /**
     * @brief Stack arrays along a new axis
     * @param arrays Input arrays
     * @param axis New axis position (default: 0)
     * @return Stacked array on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> stack(
        const std::vector<py::array>& arrays, int axis = 0);

    /**
     * @brief Concatenate arrays along existing axis
     * @param arrays Input arrays
     * @param axis Axis to concatenate along (default: 0)
     * @return Concatenated array on success, error on failure
     */
    [[nodiscard]] static NumpyResult<py::array> concatenate(
        const std::vector<py::array>& arrays, int axis = 0);
};

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

#endif  // LITHIUM_SCRIPT_ARRAY_OPS_HPP
