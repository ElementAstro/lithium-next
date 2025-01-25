#ifndef LITHIUM_CLIENT_STELLARSOLVER_STATISTIC_HPP
#define LITHIUM_CLIENT_STELLARSOLVER_STATISTIC_HPP

#include <pybind11/pybind11.h>

#include <libstellarsolver/stellarsolver.h>

namespace py = pybind11;

namespace lithium::client {
/**
 * @class FitsImageStatistic
 * @brief A class that encapsulates statistical data for FITS images.
 *
 * This class provides methods to get and set various statistical parameters
 * related to FITS images, such as minimum, maximum, mean, median values,
 * signal-to-noise ratio (SNR), data type, samples per channel, width,
 * height, and number of channels.
 */
class FitsImageStatistic {
public:
    /**
     * @brief Constructs a new Fits Image Statistic object.
     */
    FitsImageStatistic();

    /**
     * @brief Destroys the Fits Image Statistic object.
     */
    ~FitsImageStatistic() = default;

    /**
     * @brief Retrieves the minimum values as a Python list.
     *
     * @return py::list containing minimum values.
     */
    [[nodiscard]] auto getMin() const -> py::list;

    /**
     * @brief Sets the minimum values from a Python object.
     *
     * @param value A Python object containing minimum values.
     */
    void setMin(const py::object& value);

    /**
     * @brief Retrieves the maximum values as a Python list.
     *
     * @return py::list containing maximum values.
     */
    [[nodiscard]] auto getMax() const -> py::list;

    /**
     * @brief Sets the maximum values from a Python object.
     *
     * @param value A Python object containing maximum values.
     */
    void setMax(const py::object& value);

    /**
     * @brief Retrieves the mean values as a Python list.
     *
     * @return py::list containing mean values.
     */
    [[nodiscard]] auto getMean() const -> py::list;

    /**
     * @brief Sets the mean values from a Python object.
     *
     * @param value A Python object containing mean values.
     */
    void setMean(const py::object& value);

    /**
     * @brief Retrieves the median values as a Python list.
     *
     * @return py::list containing median values.
     */
    [[nodiscard]] auto getMedian() const -> py::list;

    /**
     * @brief Sets the median values from a Python object.
     *
     * @param value A Python object containing median values.
     */
    void setMedian(const py::object& value);

    /**
     * @brief Retrieves the Signal-to-Noise Ratio (SNR).
     *
     * @return double representing the SNR.
     */
    [[nodiscard]] auto getSnr() const -> double;

    /**
     * @brief Sets the Signal-to-Noise Ratio (SNR).
     *
     * @param value A double value representing the SNR.
     */
    void setSnr(double value);

    /**
     * @brief Retrieves the data type identifier.
     *
     * @return uint32_t representing the data type.
     */
    [[nodiscard]] auto getDataType() const -> uint32_t;

    /**
     * @brief Sets the data type identifier.
     *
     * @param value A uint32_t value representing the data type.
     */
    void setDataType(uint32_t value);

    /**
     * @brief Retrieves the number of samples per channel.
     *
     * @return uint32_t representing samples per channel.
     */
    [[nodiscard]] auto getSamplesPerChannel() const -> uint32_t;

    /**
     * @brief Sets the number of samples per channel.
     *
     * @param value A uint32_t value representing samples per channel.
     */
    void setSamplesPerChannel(uint32_t value);

    /**
     * @brief Retrieves the width of the FITS image.
     *
     * @return uint16_t representing the width.
     */
    [[nodiscard]] auto getWidth() const -> uint16_t;

    /**
     * @brief Sets the width of the FITS image.
     *
     * @param value A uint16_t value representing the width.
     */
    void setWidth(uint16_t value);

    /**
     * @brief Retrieves the height of the FITS image.
     *
     * @return uint16_t representing the height.
     */
    [[nodiscard]] auto getHeight() const -> uint16_t;

    /**
     * @brief Sets the height of the FITS image.
     *
     * @param value A uint16_t value representing the height.
     */
    void setHeight(uint16_t value);

    /**
     * @brief Retrieves the number of channels in the FITS image.
     *
     * @return uint8_t representing the number of channels.
     */
    [[nodiscard]] auto getChannels() const -> uint8_t;

    /**
     * @brief Sets the number of channels in the FITS image.
     *
     * @param value A uint8_t value representing the number of channels.
     */
    void setChannels(uint8_t value);

    /**
     * @brief Retrieves a reference to the underlying FITSImage::Statistic
     * object.
     *
     * @return FITSImage::Statistic& reference to the statistic data.
     */
    auto getStat() -> FITSImage::Statistic&;

private:
    /**
     * @brief Sets an array of double values from a Python object.
     *
     * @param array Pointer to the array where values will be set.
     * @param length Number of elements in the array.
     * @param value A Python object containing the values.
     */
    void setDoubleArray(double* array, unsigned int length,
                        const py::object& value);

    FITSImage::Statistic
        stat_; /**< @brief Internal statistic data structure. */
};
}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_STELLARSOLVER_STATISTIC_HPP
