/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief NumPy Type Definitions and Utilities
 * @date 2024
 * @version 1.0.0
 *
 * This module provides core type definitions for NumPy integration:
 * - Error codes and error handling
 * - NumPy-compatible type traits and concepts
 * - Astronomical data structures
 * - Image statistics structures
 */

#ifndef LITHIUM_SCRIPT_NUMPY_TYPES_HPP
#define LITHIUM_SCRIPT_NUMPY_TYPES_HPP

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <concepts>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

/**
 * @brief Error codes for NumPy operations
 */
enum class NumpyError {
    Success = 0,
    ModuleNotFound,
    ArrayCreationFailed,
    TypeConversionFailed,
    ShapeMismatch,
    InvalidBuffer,
    DataFrameError,
    FitsError,
    UnsupportedDtype,
    MemoryAllocationFailed,
    UnknownError
};

/**
 * @brief Get string representation of NumpyError
 */
[[nodiscard]] constexpr std::string_view numpyErrorToString(NumpyError error) noexcept {
    switch (error) {
        case NumpyError::Success: return "Success";
        case NumpyError::ModuleNotFound: return "NumPy/Pandas module not found";
        case NumpyError::ArrayCreationFailed: return "Array creation failed";
        case NumpyError::TypeConversionFailed: return "Type conversion failed";
        case NumpyError::ShapeMismatch: return "Shape mismatch";
        case NumpyError::InvalidBuffer: return "Invalid buffer";
        case NumpyError::DataFrameError: return "DataFrame error";
        case NumpyError::FitsError: return "FITS file error";
        case NumpyError::UnsupportedDtype: return "Unsupported dtype";
        case NumpyError::MemoryAllocationFailed: return "Memory allocation failed";
        case NumpyError::UnknownError: return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for NumPy operations
 */
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
 * @brief Star data structure for astronomical catalogs
 */
struct StarData {
    double ra;           ///< Right Ascension (degrees)
    double dec;          ///< Declination (degrees)
    float magnitude;     ///< Visual magnitude
    float bv_color;      ///< B-V color index
    char name[32];       ///< Star name/identifier
    uint32_t hip_id;     ///< Hipparcos ID
    float proper_motion_ra;   ///< Proper motion in RA (mas/yr)
    float proper_motion_dec;  ///< Proper motion in Dec (mas/yr)
    float parallax;      ///< Parallax (mas)
};

/**
 * @brief Image statistics
 */
struct ImageStats {
    double min{0};
    double max{0};
    double mean{0};
    double median{0};
    double stddev{0};
    double sum{0};
    size_t validPixels{0};
};

}  // namespace lithium::numpy

#endif  // LITHIUM_SCRIPT_NUMPY_TYPES_HPP
