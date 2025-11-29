/*
 * array_view.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file array_view.hpp
 * @brief Zero-Copy NumPy Array View
 * @date 2024
 * @version 1.0.0
 *
 * This module provides a zero-copy view into NumPy arrays, allowing efficient
 * access to array data without copying:
 * - Direct pointer access to array data
 * - Shape and stride information
 * - Contiguity checking
 * - Iterator support for range-based loops
 * - 1D and 2D element access patterns
 */

#ifndef LITHIUM_SCRIPT_NUMPY_ARRAY_VIEW_HPP
#define LITHIUM_SCRIPT_NUMPY_ARRAY_VIEW_HPP

#include "types.hpp"

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

/**
 * @brief Zero-copy view into a NumPy array
 *
 * This class provides efficient access to NumPy array data without copying.
 * It maintains pointers to the original array data along with shape and stride
 * information, enabling high-performance array operations in C++.
 *
 * @tparam T The element type (must satisfy NumpyCompatible concept)
 *
 * @note The view does not own the data; it merely references it. The original
 *       NumPy array must remain alive for the duration of the view's use.
 */
template<NumpyCompatible T>
class NumpyArrayView {
public:
    /**
     * @brief Constructs a view from a NumPy array
     *
     * Extracts shape, stride, and data pointer information from the NumPy array
     * to create a zero-copy view.
     *
     * @param arr NumPy array to wrap
     *
     * @note The view holds a reference to the buffer info, which may become
     *       invalid if the NumPy array is resized or deallocated.
     */
    explicit NumpyArrayView(py::array_t<T>& arr)
        : info_(arr.request()),
          data_(static_cast<T*>(info_.ptr)) {
        // Extract shape
        for (py::ssize_t dim : info_.shape) {
            shape_.push_back(static_cast<size_t>(dim));
        }
        // Extract strides
        for (py::ssize_t stride : info_.strides) {
            strides_.push_back(static_cast<size_t>(stride));
        }
    }

    /**
     * @brief Get pointer to data
     *
     * @return Mutable pointer to the underlying array data
     */
    [[nodiscard]] T* data() noexcept { return data_; }

    /**
     * @brief Get const pointer to data
     *
     * @return Const pointer to the underlying array data
     */
    [[nodiscard]] const T* data() const noexcept { return data_; }

    /**
     * @brief Get total number of elements
     *
     * Calculates the product of all dimensions to determine the total
     * number of elements in the array.
     *
     * @return Total element count
     */
    [[nodiscard]] size_t size() const noexcept {
        size_t total = 1;
        for (size_t dim : shape_) {
            total *= dim;
        }
        return total;
    }

    /**
     * @brief Get array shape
     *
     * @return Reference to vector containing dimensions along each axis
     */
    [[nodiscard]] const std::vector<size_t>& shape() const noexcept {
        return shape_;
    }

    /**
     * @brief Get array strides
     *
     * Strides represent the number of bytes to advance when moving along
     * each dimension. This is crucial for non-contiguous or transposed arrays.
     *
     * @return Reference to vector containing stride values in bytes
     */
    [[nodiscard]] const std::vector<size_t>& strides() const noexcept {
        return strides_;
    }

    /**
     * @brief Get number of dimensions
     *
     * @return Number of dimensions (rank) of the array
     */
    [[nodiscard]] size_t ndim() const noexcept {
        return shape_.size();
    }

    /**
     * @brief Check if array is contiguous in memory
     *
     * A contiguous array has strides that match the expected C-order layout
     * (last dimension changes fastest). This is useful for optimizing
     * operations that assume contiguous memory.
     *
     * @return True if array is contiguous, false otherwise
     */
    [[nodiscard]] bool isContiguous() const noexcept {
        if (shape_.empty()) return true;

        size_t expectedStride = sizeof(T);
        for (ssize_t i = static_cast<ssize_t>(shape_.size()) - 1; i >= 0; --i) {
            if (strides_[i] != expectedStride) {
                return false;
            }
            expectedStride *= shape_[i];
        }
        return true;
    }

    /**
     * @brief Get element at linear index
     *
     * Accesses an element using a single linear index, suitable for
     * 1D arrays or flattened iteration.
     *
     * @param index Linear index into the array
     * @return Reference to the element at the given index
     */
    [[nodiscard]] T& operator[](size_t index) {
        return data_[index];
    }

    /**
     * @brief Get const element at linear index
     *
     * Const version of operator[] for read-only access.
     *
     * @param index Linear index into the array
     * @return Const reference to the element at the given index
     */
    [[nodiscard]] const T& operator[](size_t index) const {
        return data_[index];
    }

    /**
     * @brief Get element at 2D index
     *
     * Accesses an element using 2D indices (row, column), applying
     * the appropriate stride calculations.
     *
     * @param row Row index
     * @param col Column index
     * @return Reference to the element at the given 2D position
     *
     * @note This method assumes a 2D array. Behavior is undefined for
     *       arrays with other dimensionalities.
     */
    [[nodiscard]] T& at(size_t row, size_t col) {
        return *reinterpret_cast<T*>(
            reinterpret_cast<char*>(data_) + row * strides_[0] + col * strides_[1]);
    }

    /**
     * @brief Get const element at 2D index
     *
     * Const version of at() for read-only access.
     *
     * @param row Row index
     * @param col Column index
     * @return Const reference to the element at the given 2D position
     */
    [[nodiscard]] const T& at(size_t row, size_t col) const {
        return *reinterpret_cast<const T*>(
            reinterpret_cast<const char*>(data_) + row * strides_[0] + col * strides_[1]);
    }

    /**
     * @brief Get iterator to beginning of array
     *
     * @return Mutable iterator to the first element
     */
    [[nodiscard]] T* begin() noexcept { return data_; }

    /**
     * @brief Get iterator to end of array
     *
     * @return Mutable iterator to one past the last element
     */
    [[nodiscard]] T* end() noexcept { return data_ + size(); }

    /**
     * @brief Get const iterator to beginning of array
     *
     * @return Const iterator to the first element
     */
    [[nodiscard]] const T* begin() const noexcept { return data_; }

    /**
     * @brief Get const iterator to end of array
     *
     * @return Const iterator to one past the last element
     */
    [[nodiscard]] const T* end() const noexcept { return data_ + size(); }

private:
    py::buffer_info info_;           ///< Buffer information from NumPy
    T* data_;                        ///< Pointer to array data
    std::vector<size_t> shape_;      ///< Array dimensions
    std::vector<size_t> strides_;    ///< Byte strides for each dimension
};

}  // namespace lithium::numpy

#endif  // LITHIUM_SCRIPT_NUMPY_ARRAY_VIEW_HPP
