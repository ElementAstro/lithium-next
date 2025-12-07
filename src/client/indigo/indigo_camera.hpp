/*
 * indigo_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Camera Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_CAMERA_HPP
#define LITHIUM_CLIENT_INDIGO_CAMERA_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Camera frame type
 */
enum class FrameType : uint8_t {
    Light,
    Bias,
    Dark,
    Flat
};

/**
 * @brief Camera binning mode
 */
struct BinningMode {
    int horizontal{1};
    int vertical{1};
};

/**
 * @brief Camera region of interest (ROI)
 */
struct CameraROI {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

/**
 * @brief Camera sensor information
 */
struct SensorInfo {
    int width{0};             // Sensor width in pixels
    int height{0};            // Sensor height in pixels
    double pixelSizeX{0.0};   // Pixel size X in microns
    double pixelSizeY{0.0};   // Pixel size Y in microns
    int bitsPerPixel{16};     // Bit depth
    std::vector<BinningMode> supportedBinning;
};

/**
 * @brief Camera temperature information
 */
struct TemperatureInfo {
    double currentTemp{0.0};
    double targetTemp{0.0};
    double coolerPower{0.0};
    bool coolerOn{false};
};

/**
 * @brief Exposure status
 */
struct ExposureStatus {
    bool exposing{false};
    double duration{0.0};       // Total exposure duration
    double elapsed{0.0};        // Elapsed time
    double remaining{0.0};      // Remaining time
    PropertyState state{PropertyState::Idle};
};

/**
 * @brief Captured image data
 */
struct ImageData {
    std::vector<uint8_t> data;
    std::string format;         // e.g., ".fits", ".raw"
    int width{0};
    int height{0};
    int bitDepth{16};
    std::string url;            // BLOB URL if using URL mode
    json metadata;
};

/**
 * @brief Image received callback
 */
using ImageCallback = std::function<void(const ImageData&)>;

/**
 * @brief Exposure progress callback
 */
using ExposureProgressCallback = std::function<void(double elapsed, double total)>;

/**
 * @brief INDIGO Camera Device
 *
 * Provides camera control functionality for INDIGO-connected cameras:
 * - Exposure control (start, abort, progress)
 * - Temperature/cooling control
 * - Binning and ROI settings
 * - Frame type selection
 * - Gain/offset control
 */
class INDIGOCamera : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOCamera(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOCamera() override;

    // ==================== Exposure Control ====================

    /**
     * @brief Start an exposure
     * @param duration Exposure duration in seconds
     * @param frameType Frame type (Light, Dark, etc.)
     * @return Success or error
     */
    auto startExposure(double duration,
                       FrameType frameType = FrameType::Light) -> DeviceResult<bool>;

    /**
     * @brief Abort current exposure
     * @return Success or error
     */
    auto abortExposure() -> DeviceResult<bool>;

    /**
     * @brief Check if currently exposing
     */
    [[nodiscard]] auto isExposing() const -> bool;

    /**
     * @brief Get exposure status
     */
    [[nodiscard]] auto getExposureStatus() const -> ExposureStatus;

    /**
     * @brief Set image callback for receiving captured images
     */
    void setImageCallback(ImageCallback callback);

    /**
     * @brief Set exposure progress callback
     */
    void setExposureProgressCallback(ExposureProgressCallback callback);

    // ==================== Temperature Control ====================

    /**
     * @brief Check if camera has cooling capability
     */
    [[nodiscard]] auto hasCooler() const -> bool;

    /**
     * @brief Set cooler on/off
     * @param on Enable or disable cooler
     */
    auto setCoolerOn(bool on) -> DeviceResult<bool>;

    /**
     * @brief Check if cooler is on
     */
    [[nodiscard]] auto isCoolerOn() const -> bool;

    /**
     * @brief Set target temperature
     * @param celsius Target temperature in Celsius
     */
    auto setTargetTemperature(double celsius) -> DeviceResult<bool>;

    /**
     * @brief Get current temperature
     */
    [[nodiscard]] auto getCurrentTemperature() const -> double;

    /**
     * @brief Get target temperature
     */
    [[nodiscard]] auto getTargetTemperature() const -> double;

    /**
     * @brief Get cooler power percentage
     */
    [[nodiscard]] auto getCoolerPower() const -> double;

    /**
     * @brief Get full temperature info
     */
    [[nodiscard]] auto getTemperatureInfo() const -> TemperatureInfo;

    // ==================== Sensor Information ====================

    /**
     * @brief Get sensor information
     */
    [[nodiscard]] auto getSensorInfo() const -> SensorInfo;

    /**
     * @brief Get sensor width in pixels
     */
    [[nodiscard]] auto getSensorWidth() const -> int;

    /**
     * @brief Get sensor height in pixels
     */
    [[nodiscard]] auto getSensorHeight() const -> int;

    /**
     * @brief Get pixel size in microns
     */
    [[nodiscard]] auto getPixelSize() const -> std::pair<double, double>;

    /**
     * @brief Get bit depth
     */
    [[nodiscard]] auto getBitDepth() const -> int;

    // ==================== Binning Control ====================

    /**
     * @brief Set binning mode
     * @param horizontal Horizontal binning factor
     * @param vertical Vertical binning factor
     */
    auto setBinning(int horizontal, int vertical) -> DeviceResult<bool>;

    /**
     * @brief Get current binning
     */
    [[nodiscard]] auto getBinning() const -> BinningMode;

    /**
     * @brief Get supported binning modes
     */
    [[nodiscard]] auto getSupportedBinning() const -> std::vector<BinningMode>;

    // ==================== ROI Control ====================

    /**
     * @brief Set region of interest
     * @param roi Region of interest settings
     */
    auto setROI(const CameraROI& roi) -> DeviceResult<bool>;

    /**
     * @brief Get current ROI
     */
    [[nodiscard]] auto getROI() const -> CameraROI;

    /**
     * @brief Reset ROI to full frame
     */
    auto resetROI() -> DeviceResult<bool>;

    // ==================== Gain/Offset Control ====================

    /**
     * @brief Check if camera supports gain control
     */
    [[nodiscard]] auto hasGainControl() const -> bool;

    /**
     * @brief Set gain value
     * @param gain Gain value
     */
    auto setGain(double gain) -> DeviceResult<bool>;

    /**
     * @brief Get current gain
     */
    [[nodiscard]] auto getGain() const -> double;

    /**
     * @brief Get gain range
     * @return pair of (min, max)
     */
    [[nodiscard]] auto getGainRange() const -> std::pair<double, double>;

    /**
     * @brief Check if camera supports offset control
     */
    [[nodiscard]] auto hasOffsetControl() const -> bool;

    /**
     * @brief Set offset value
     * @param offset Offset value
     */
    auto setOffset(double offset) -> DeviceResult<bool>;

    /**
     * @brief Get current offset
     */
    [[nodiscard]] auto getOffset() const -> double;

    /**
     * @brief Get offset range
     * @return pair of (min, max)
     */
    [[nodiscard]] auto getOffsetRange() const -> std::pair<double, double>;

    // ==================== Frame Type ====================

    /**
     * @brief Set frame type
     * @param type Frame type
     */
    auto setFrameType(FrameType type) -> DeviceResult<bool>;

    /**
     * @brief Get current frame type
     */
    [[nodiscard]] auto getFrameType() const -> FrameType;

    // ==================== Image Format ====================

    /**
     * @brief Set image format
     * @param format Format string (e.g., "FITS", "RAW", "JPEG")
     */
    auto setImageFormat(const std::string& format) -> DeviceResult<bool>;

    /**
     * @brief Get current image format
     */
    [[nodiscard]] auto getImageFormat() const -> std::string;

    /**
     * @brief Get supported image formats
     */
    [[nodiscard]] auto getSupportedFormats() const -> std::vector<std::string>;

    // ==================== Utility ====================

    /**
     * @brief Get camera capabilities as JSON
     */
    [[nodiscard]] auto getCapabilities() const -> json;

    /**
     * @brief Get current camera status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> cameraImpl_;
};

// ============================================================================
// Frame type conversion
// ============================================================================

[[nodiscard]] constexpr auto frameTypeToString(FrameType type) noexcept
    -> std::string_view {
    switch (type) {
        case FrameType::Light: return "Light";
        case FrameType::Bias: return "Bias";
        case FrameType::Dark: return "Dark";
        case FrameType::Flat: return "Flat";
        default: return "Light";
    }
}

[[nodiscard]] auto frameTypeFromString(std::string_view str) -> FrameType;

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_CAMERA_HPP
