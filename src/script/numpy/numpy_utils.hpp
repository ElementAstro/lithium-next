/*
 * numpy_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file numpy_utils.hpp
 * @brief NumPy Integration Facade and Unified Interface
 * @date 2024
 * @version 1.0.0
 *
 * This facade header provides a unified interface for all NumPy and Pandas
 * integration functionality:
 * - Type definitions and error handling (types.hpp)
 * - Zero-copy array views (array_view.hpp)
 * - Array operations (array_ops.hpp)
 * - DataFrame operations (dataframe_ops.hpp)
 * - Astronomical data processing (astro_ops.hpp)
 *
 * The NumpyUtils class offers convenient shortcuts for common operations,
 * while individual operation classes (ArrayOps, DataFrameOps, AstroOps)
 * provide more specialized functionality.
 *
 * Usage Example:
 * \code
 * // Initialize NumPy
 * auto result = NumpyUtils::initialize();
 * if (!result) {
 *     // Handle error
 * }
 *
 * // Create and manipulate arrays
 * std::vector<float> data = {1.0f, 2.0f, 3.0f};
 * auto arr = NumpyUtils::createArray(data);
 *
 * // Access with zero-copy view
 * NumpyArrayView<float> view(arr);
 * for (auto val : view) {
 *     // Process values
 * }
 *
 * // DataFrame operations
 * if (NumpyUtils::isPandasAvailable()) {
 *     auto df = NumpyUtils::createDataFrame(columns, rows);
 * }
 * \endcode
 */

#ifndef LITHIUM_SCRIPT_NUMPY_NUMPY_UTILS_HPP
#define LITHIUM_SCRIPT_NUMPY_NUMPY_UTILS_HPP

#include "array_ops.hpp"
#include "array_view.hpp"
#include "astro_ops.hpp"
#include "dataframe_ops.hpp"
#include "types.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace py = pybind11;

namespace lithium::numpy {

/**
 * @brief Unified facade for NumPy and Pandas operations
 *
 * This class provides a simplified, unified interface to all NumPy/Pandas
 * functionality. It acts as a facade to the specialized operation classes
 * while maintaining backward compatibility and providing convenience methods.
 *
 * Thread-safe for read-only initialization and operations after initialization
 * is complete.
 *
 * @note All operations require the Python GIL to be held by pybind11.
 */
class NumpyUtils {
public:
    /**
     * @brief Initialize NumPy module
     *
     * Must be called once before using other NumPy operations. Should be
     * called with the Python GIL held (automatically managed by pybind11).
     *
     * @return NumpyResult<void> Success or NumpyError code
     *
     * @note This is idempotent - calling multiple times is safe
     */
    [[nodiscard]] static NumpyResult<void> initialize();

    /**
     * @brief Check if NumPy module is available
     *
     * @return True if NumPy can be imported, false otherwise
     */
    [[nodiscard]] static bool isNumpyAvailable();

    /**
     * @brief Check if Pandas module is available
     *
     * @return True if Pandas can be imported, false otherwise
     */
    [[nodiscard]] static bool isPandasAvailable();

    /**
     * @brief Check if Astropy module is available (for FITS and WCS)
     *
     * @return True if Astropy can be imported, false otherwise
     */
    [[nodiscard]] static bool isAstropryAvailable();

    // =========================================================================
    // Array Creation and Conversion (via ArrayOps)
    // =========================================================================

    /**
     * @brief Create NumPy array from vector
     *
     * @tparam T NumPy-compatible element type
     * @param data Vector of data to convert
     * @return py::array_t<T> NumPy array (zero-copy when possible)
     *
     * @note The returned array holds a reference to the vector data.
     *       The input vector must outlive the array if no copy is made.
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(const std::vector<T>& data);

    /**
     * @brief Create NumPy array from raw pointer with shape
     *
     * @tparam T NumPy-compatible element type
     * @param data Pointer to array data
     * @param shape Vector of dimensions
     * @param copy If true, copies data; if false, creates view (risky)
     * @return py::array_t<T> NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(
        T* data, const std::vector<size_t>& shape, bool copy = true);

    /**
     * @brief Create NumPy array from span
     *
     * @tparam T NumPy-compatible element type
     * @param data Span of data elements
     * @return py::array_t<T> NumPy array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray(std::span<T> data);

    /**
     * @brief Create 2D NumPy array from nested vectors
     *
     * @tparam T NumPy-compatible element type
     * @param data Vector of vectors (rows)
     * @return py::array_t<T> 2D NumPy array
     *
     * @note All rows should have the same size; incomplete rows are
     *       padded with zeros.
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> createArray2D(
        const std::vector<std::vector<T>>& data);

    /**
     * @brief Create array filled with zeros
     *
     * @tparam T NumPy-compatible element type
     * @param shape Vector of dimensions
     * @return py::array_t<T> Zero-filled array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> zeros(const std::vector<size_t>& shape);

    /**
     * @brief Create uninitialized array
     *
     * @tparam T NumPy-compatible element type
     * @param shape Vector of dimensions
     * @return py::array_t<T> Uninitialized array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> empty(const std::vector<size_t>& shape);

    /**
     * @brief Create array filled with a constant value
     *
     * @tparam T NumPy-compatible element type
     * @param shape Vector of dimensions
     * @param value Fill value
     * @return py::array_t<T> Filled array
     */
    template<NumpyCompatible T>
    [[nodiscard]] static py::array_t<T> full(
        const std::vector<size_t>& shape, T value);

    /**
     * @brief Convert NumPy array to vector (copies data)
     *
     * @tparam T NumPy-compatible element type
     * @param arr NumPy array to convert
     * @return std::vector<T> Vector copy of array data
     */
    template<NumpyCompatible T>
    [[nodiscard]] static std::vector<T> toVector(const py::array_t<T>& arr);

    /**
     * @brief Convert NumPy 2D array to nested vectors
     *
     * @tparam T NumPy-compatible element type
     * @param arr 2D NumPy array
     * @return std::vector<std::vector<T>> Nested vector copy
     *
     * @throw std::runtime_error if array is not 2D
     */
    template<NumpyCompatible T>
    [[nodiscard]] static std::vector<std::vector<T>> toVector2D(
        const py::array_t<T>& arr);

    /**
     * @brief Copy array data to a buffer
     *
     * @tparam T NumPy-compatible element type
     * @param arr NumPy array
     * @param buffer Output buffer
     * @param bufferSize Size of buffer in elements
     *
     * @note Copies min(arr.size(), bufferSize) elements
     */
    template<NumpyCompatible T>
    static void copyToBuffer(
        const py::array_t<T>& arr, T* buffer, size_t bufferSize);

    /**
     * @brief Get array shape as vector
     *
     * @param arr NumPy array
     * @return std::vector<size_t> Shape (dimensions)
     */
    [[nodiscard]] static std::vector<size_t> getShape(const py::array& arr);

    /**
     * @brief Get array dtype name
     *
     * @param arr NumPy array
     * @return std::string Dtype name (e.g., "float32", "int64")
     */
    [[nodiscard]] static std::string getDtypeName(const py::array& arr);

    /**
     * @brief Get array total element count
     *
     * @param arr NumPy array
     * @return size_t Product of all dimensions
     */
    [[nodiscard]] static size_t getArraySize(const py::array& arr);

    // =========================================================================
    // Array Operations
    // =========================================================================

    /**
     * @brief Reshape array to new shape
     *
     * @param arr Source array
     * @param newShape New dimensions
     * @return NumpyResult<py::array> Reshaped array or error
     */
    [[nodiscard]] static NumpyResult<py::array> reshape(
        const py::array& arr, const std::vector<size_t>& newShape);

    /**
     * @brief Transpose array (swap dimensions)
     *
     * @param arr Source array
     * @return NumpyResult<py::array> Transposed array or error
     */
    [[nodiscard]] static NumpyResult<py::array> transpose(const py::array& arr);

    /**
     * @brief Stack arrays along new axis
     *
     * @param arrays Vector of arrays to stack
     * @param axis New axis position (default: 0)
     * @return NumpyResult<py::array> Stacked array or error
     */
    [[nodiscard]] static NumpyResult<py::array> stack(
        const std::vector<py::array>& arrays, int axis = 0);

    /**
     * @brief Concatenate arrays along existing axis
     *
     * @param arrays Vector of arrays to concatenate
     * @param axis Axis to concatenate along (default: 0)
     * @return NumpyResult<py::array> Concatenated array or error
     */
    [[nodiscard]] static NumpyResult<py::array> concatenate(
        const std::vector<py::array>& arrays, int axis = 0);

    // =========================================================================
    // DataFrame Operations (via DataFrameOps)
    // =========================================================================

    /**
     * @brief Create DataFrame from column name to data map
     *
     * @param data Map of column_name -> vector of Python objects
     * @return NumpyResult<py::object> DataFrame or error
     *
     * Example:
     * \code
     * std::unordered_map<std::string, std::vector<py::object>> data;
     * data["name"] = {py::cast("Alice"), py::cast("Bob")};
     * data["age"] = {py::cast(30), py::cast(25)};
     * auto df = NumpyUtils::createDataFrame(data);
     * \endcode
     */
    [[nodiscard]] static NumpyResult<py::object> createDataFrame(
        const std::unordered_map<std::string, std::vector<py::object>>& data);

    /**
     * @brief Create DataFrame from columns and rows
     *
     * @param columns Vector of column names
     * @param rows Vector of rows (each row is vector of Python objects)
     * @return NumpyResult<py::object> DataFrame or error
     */
    [[nodiscard]] static NumpyResult<py::object> createDataFrame(
        const std::vector<std::string>& columns,
        const std::vector<std::vector<py::object>>& rows);

    /**
     * @brief Convert DataFrame to JSON string
     *
     * @param df Pandas DataFrame
     * @param orient JSON orientation ("records", "index", "columns", "values")
     * @return NumpyResult<std::string> JSON string or error
     */
    [[nodiscard]] static NumpyResult<std::string> dataFrameToJson(
        const py::object& df, const std::string& orient = "records");

    /**
     * @brief Get DataFrame column as NumPy array
     *
     * @param df Pandas DataFrame
     * @param columnName Column name
     * @return NumpyResult<py::array> Column array or error
     */
    [[nodiscard]] static NumpyResult<py::array> getDataFrameColumn(
        const py::object& df, const std::string& columnName);

    /**
     * @brief Get DataFrame row count
     *
     * @param df Pandas DataFrame
     * @return NumpyResult<size_t> Row count or error
     */
    [[nodiscard]] static NumpyResult<size_t> getDataFrameRowCount(
        const py::object& df);

    /**
     * @brief Get DataFrame column names
     *
     * @param df Pandas DataFrame
     * @return NumpyResult<std::vector<std::string>> Column names or error
     */
    [[nodiscard]] static NumpyResult<std::vector<std::string>>
    getDataFrameColumns(const py::object& df);

    // =========================================================================
    // Astronomical Data Operations (via AstroOps)
    // =========================================================================

    /**
     * @brief Create star catalog DataFrame
     *
     * Creates a Pandas DataFrame from star data with columns:
     * ra, dec, magnitude, bv_color, name, hip_id, pm_ra, pm_dec, parallax
     *
     * @param stars Vector of StarData structures
     * @return NumpyResult<py::object> DataFrame or error
     */
    [[nodiscard]] static NumpyResult<py::object> createStarCatalog(
        const std::vector<StarData>& stars);

    /**
     * @brief Parse star catalog DataFrame
     *
     * Extracts star data from a DataFrame back to StarData structures.
     *
     * @param df Pandas DataFrame with star data
     * @return NumpyResult<std::vector<StarData>> Star data or error
     */
    [[nodiscard]] static NumpyResult<std::vector<StarData>> parseStarCatalog(
        const py::object& df);

    /**
     * @brief Load FITS image file
     *
     * @param fitsPath Path to FITS file
     * @param hdu Header-Data Unit index (0 for primary)
     * @return NumpyResult<py::array> Image data array or error
     */
    [[nodiscard]] static NumpyResult<py::array> loadFitsImage(
        const std::filesystem::path& fitsPath, int hdu = 0);

    /**
     * @brief Save array as FITS image file
     *
     * @tparam T NumPy-compatible element type
     * @param arr Image data array
     * @param fitsPath Output file path
     * @param header Optional FITS header keywords
     * @return NumpyResult<void> Success or error
     */
    template<NumpyCompatible T>
    [[nodiscard]] static NumpyResult<void> saveFitsImage(
        const py::array_t<T>& arr,
        const std::filesystem::path& fitsPath,
        const std::unordered_map<std::string, std::string>& header = {});

    /**
     * @brief Calculate image statistics
     *
     * Computes min, max, mean, median, stddev, sum, and valid pixel count.
     *
     * @tparam T NumPy-compatible element type
     * @param arr Image data array
     * @return ImageStats Statistics structure
     */
    template<NumpyCompatible T>
    [[nodiscard]] static ImageStats calculateImageStats(const py::array_t<T>& arr);

    /**
     * @brief Convert pixel coordinates to world coordinates
     *
     * @param wcs WCS object (from astropy.wcs)
     * @param x Pixel X coordinate
     * @param y Pixel Y coordinate
     * @return NumpyResult<std::pair<double, double>> (RA, Dec) or error
     */
    [[nodiscard]] static NumpyResult<std::pair<double, double>> pixelToWorld(
        const py::object& wcs, double x, double y);

    /**
     * @brief Convert world coordinates to pixel coordinates
     *
     * @param wcs WCS object (from astropy.wcs)
     * @param ra Right Ascension (degrees)
     * @param dec Declination (degrees)
     * @return NumpyResult<std::pair<double, double>> (pixel_x, pixel_y) or error
     */
    [[nodiscard]] static NumpyResult<std::pair<double, double>> worldToPixel(
        const py::object& wcs, double ra, double dec);

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Get detailed error message
     *
     * @param error NumpyError code
     * @return std::string_view Human-readable error description
     */
    [[nodiscard]] static std::string_view getErrorMessage(NumpyError error) {
        return numpyErrorToString(error);
    }

private:
    // Internal state management
    static inline bool initialized_ = false;
    static inline bool numpy_available_ = false;
    static inline bool pandas_available_ = false;
    static inline bool astropy_available_ = false;
};

// =========================================================================
// Template Implementations (Forwarded to ArrayOps)
// =========================================================================

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::createArray(const std::vector<T>& data) {
    return ArrayOps::createArray(data);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::createArray(
    T* data, const std::vector<size_t>& shape, bool copy) {
    return ArrayOps::createArray(data, shape, copy);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::createArray(std::span<T> data) {
    return ArrayOps::createArray(data);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::createArray2D(
    const std::vector<std::vector<T>>& data) {
    return ArrayOps::createArray2D(data);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::zeros(const std::vector<size_t>& shape) {
    return ArrayOps::zeros<T>(shape);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::empty(const std::vector<size_t>& shape) {
    return ArrayOps::empty<T>(shape);
}

template<NumpyCompatible T>
py::array_t<T> NumpyUtils::full(
    const std::vector<size_t>& shape, T value) {
    return ArrayOps::full(shape, value);
}

template<NumpyCompatible T>
std::vector<T> NumpyUtils::toVector(const py::array_t<T>& arr) {
    return ArrayOps::toVector(arr);
}

template<NumpyCompatible T>
std::vector<std::vector<T>> NumpyUtils::toVector2D(const py::array_t<T>& arr) {
    return ArrayOps::toVector2D(arr);
}

template<NumpyCompatible T>
void NumpyUtils::copyToBuffer(
    const py::array_t<T>& arr, T* buffer, size_t bufferSize) {
    return ArrayOps::copyToBuffer(arr, buffer, bufferSize);
}

template<NumpyCompatible T>
NumpyResult<void> NumpyUtils::saveFitsImage(
    const py::array_t<T>& arr,
    const std::filesystem::path& fitsPath,
    const std::unordered_map<std::string, std::string>& header) {
    return AstroOps::saveFitsImage(arr, fitsPath, header);
}

template<NumpyCompatible T>
ImageStats NumpyUtils::calculateImageStats(const py::array_t<T>& arr) {
    return AstroOps::calculateImageStats(arr);
}

}  // namespace lithium::numpy

#endif  // LITHIUM_SCRIPT_NUMPY_NUMPY_UTILS_HPP
