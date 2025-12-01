/*
 * ascom_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Camera Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_CAMERA_HPP
#define LITHIUM_CLIENT_ASCOM_CAMERA_HPP

#include "ascom_device_base.hpp"

#include <condition_variable>

namespace lithium::client::ascom {

/**
 * @brief Camera state enumeration
 */
enum class CameraState : uint8_t {
    Idle,
    Waiting,
    Exposing,
    Reading,
    Download,
    Error
};

/**
 * @brief Sensor type enumeration
 */
enum class SensorType : uint8_t {
    Monochrome,
    Color,
    RGGB,
    CMYG,
    CMYG2,
    LRGB
};

/**
 * @brief Camera capabilities
 */
struct CameraCapabilities {
    bool canAbortExposure{false};
    bool canAsymmetricBin{false};
    bool canFastReadout{false};
    bool canGetCoolerPower{false};
    bool canPulseGuide{false};
    bool canSetCCDTemperature{false};
    bool canStopExposure{false};
    bool hasShutter{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"canAbortExposure", canAbortExposure},
                {"canAsymmetricBin", canAsymmetricBin},
                {"canFastReadout", canFastReadout},
                {"canGetCoolerPower", canGetCoolerPower},
                {"canPulseGuide", canPulseGuide},
                {"canSetCCDTemperature", canSetCCDTemperature},
                {"canStopExposure", canStopExposure},
                {"hasShutter", hasShutter}};
    }
};

/**
 * @brief Camera sensor information
 */
struct SensorInfo {
    int cameraXSize{0};
    int cameraYSize{0};
    double pixelSizeX{0.0};
    double pixelSizeY{0.0};
    int maxBinX{1};
    int maxBinY{1};
    int maxADU{65535};
    double electronsPerADU{1.0};
    double fullWellCapacity{0.0};
    SensorType sensorType{SensorType::Monochrome};
    std::string sensorName;
    int bayerOffsetX{0};
    int bayerOffsetY{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"cameraXSize", cameraXSize},
                {"cameraYSize", cameraYSize},
                {"pixelSizeX", pixelSizeX},
                {"pixelSizeY", pixelSizeY},
                {"maxBinX", maxBinX},
                {"maxBinY", maxBinY},
                {"maxADU", maxADU},
                {"electronsPerADU", electronsPerADU},
                {"fullWellCapacity", fullWellCapacity},
                {"sensorType", static_cast<int>(sensorType)},
                {"sensorName", sensorName},
                {"bayerOffsetX", bayerOffsetX},
                {"bayerOffsetY", bayerOffsetY}};
    }
};

/**
 * @brief Exposure settings
 */
struct ExposureSettings {
    double duration{1.0};
    bool light{true};
    int binX{1};
    int binY{1};
    int startX{0};
    int startY{0};
    int numX{0};
    int numY{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"duration", duration}, {"light", light},
                {"binX", binX},         {"binY", binY},
                {"startX", startX},     {"startY", startY},
                {"numX", numX},         {"numY", numY}};
    }
};

/**
 * @brief Temperature information
 */
struct TemperatureInfo {
    double ccdTemperature{0.0};
    double setPoint{0.0};
    double coolerPower{0.0};
    bool coolerOn{false};
    double heatSinkTemperature{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"ccdTemperature", ccdTemperature},
                {"setPoint", setPoint},
                {"coolerPower", coolerPower},
                {"coolerOn", coolerOn},
                {"heatSinkTemperature", heatSinkTemperature}};
    }
};

/**
 * @brief Gain/Offset settings
 */
struct GainSettings {
    int gain{0};
    int gainMin{0};
    int gainMax{100};
    std::vector<std::string> gains;
    int offset{0};
    int offsetMin{0};
    int offsetMax{100};
    std::vector<std::string> offsets;

    [[nodiscard]] auto toJson() const -> json {
        return {{"gain", gain},       {"gainMin", gainMin},
                {"gainMax", gainMax}, {"gains", gains},
                {"offset", offset},   {"offsetMin", offsetMin},
                {"offsetMax", offsetMax}, {"offsets", offsets}};
    }
};

/**
 * @brief ASCOM Camera Device
 *
 * Provides camera functionality including:
 * - Exposure control
 * - Temperature/cooling control
 * - Binning and subframe
 * - Gain/offset control
 * - Image download
 */
class ASCOMCamera : public ASCOMDeviceBase {
public:
    /**
     * @brief Construct a new ASCOM Camera
     * @param name Device name
     * @param deviceNumber Device number
     */
    explicit ASCOMCamera(std::string name, int deviceNumber = 0);

    /**
     * @brief Destructor
     */
    ~ASCOMCamera() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Camera";
    }

    // ==================== Connection ====================

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;
    auto disconnect() -> bool override;

    // ==================== Capabilities ====================

    /**
     * @brief Get camera capabilities
     */
    [[nodiscard]] auto getCapabilities() -> CameraCapabilities;

    /**
     * @brief Get sensor information
     */
    [[nodiscard]] auto getSensorInfo() -> SensorInfo;

    // ==================== Exposure Control ====================

    /**
     * @brief Start an exposure
     * @param duration Exposure duration in seconds
     * @param light True for light frame, false for dark
     * @return true if exposure started
     */
    auto startExposure(double duration, bool light = true) -> bool;

    /**
     * @brief Abort current exposure
     * @return true if aborted
     */
    auto abortExposure() -> bool;

    /**
     * @brief Stop current exposure (graceful)
     * @return true if stopped
     */
    auto stopExposure() -> bool;

    /**
     * @brief Check if exposure is in progress
     */
    [[nodiscard]] auto isExposing() const -> bool;

    /**
     * @brief Check if image is ready
     */
    [[nodiscard]] auto isImageReady() -> bool;

    /**
     * @brief Wait for exposure to complete
     * @param timeout Maximum wait time
     * @return true if exposure completed
     */
    auto waitForExposure(std::chrono::milliseconds timeout = std::chrono::minutes(30))
        -> bool;

    /**
     * @brief Get exposure progress (0-100%)
     */
    [[nodiscard]] auto getExposureProgress() -> int;

    /**
     * @brief Get last exposure duration
     */
    [[nodiscard]] auto getLastExposureDuration() -> double;

    /**
     * @brief Get last exposure start time
     */
    [[nodiscard]] auto getLastExposureStartTime() -> std::string;

    // ==================== Image Download ====================

    /**
     * @brief Get image data as array
     * @return Image data or empty on failure
     */
    [[nodiscard]] auto getImageArray() -> std::vector<int32_t>;

    /**
     * @brief Get image data as 2D array
     * @return 2D image data
     */
    [[nodiscard]] auto getImageArray2D() -> std::vector<std::vector<int32_t>>;

    // ==================== Binning ====================

    /**
     * @brief Set binning
     * @param binX X binning factor
     * @param binY Y binning factor
     * @return true if set successfully
     */
    auto setBinning(int binX, int binY) -> bool;

    /**
     * @brief Get current binning
     * @return Pair of (binX, binY)
     */
    [[nodiscard]] auto getBinning() -> std::pair<int, int>;

    // ==================== Subframe ====================

    /**
     * @brief Set subframe
     * @param startX Start X coordinate
     * @param startY Start Y coordinate
     * @param numX Width
     * @param numY Height
     * @return true if set successfully
     */
    auto setSubframe(int startX, int startY, int numX, int numY) -> bool;

    /**
     * @brief Get current subframe
     * @return Tuple of (startX, startY, numX, numY)
     */
    [[nodiscard]] auto getSubframe() -> std::tuple<int, int, int, int>;

    /**
     * @brief Reset to full frame
     */
    auto resetSubframe() -> bool;

    // ==================== Temperature Control ====================

    /**
     * @brief Get temperature information
     */
    [[nodiscard]] auto getTemperatureInfo() -> TemperatureInfo;

    /**
     * @brief Get CCD temperature
     */
    [[nodiscard]] auto getCCDTemperature() -> std::optional<double>;

    /**
     * @brief Set target temperature
     * @param temperature Target temperature in Celsius
     * @return true if set successfully
     */
    auto setTargetTemperature(double temperature) -> bool;

    /**
     * @brief Get target temperature
     */
    [[nodiscard]] auto getTargetTemperature() -> std::optional<double>;

    /**
     * @brief Enable/disable cooler
     * @param enable True to enable
     * @return true if set successfully
     */
    auto setCoolerOn(bool enable) -> bool;

    /**
     * @brief Check if cooler is on
     */
    [[nodiscard]] auto isCoolerOn() -> bool;

    /**
     * @brief Get cooler power (0-100%)
     */
    [[nodiscard]] auto getCoolerPower() -> std::optional<double>;

    // ==================== Gain/Offset ====================

    /**
     * @brief Get gain settings
     */
    [[nodiscard]] auto getGainSettings() -> GainSettings;

    /**
     * @brief Set gain
     * @param gain Gain value
     * @return true if set successfully
     */
    auto setGain(int gain) -> bool;

    /**
     * @brief Get current gain
     */
    [[nodiscard]] auto getGain() -> std::optional<int>;

    /**
     * @brief Set offset
     * @param offset Offset value
     * @return true if set successfully
     */
    auto setOffset(int offset) -> bool;

    /**
     * @brief Get current offset
     */
    [[nodiscard]] auto getOffset() -> std::optional<int>;

    // ==================== Readout Mode ====================

    /**
     * @brief Get available readout modes
     */
    [[nodiscard]] auto getReadoutModes() -> std::vector<std::string>;

    /**
     * @brief Set readout mode
     * @param mode Mode index
     * @return true if set successfully
     */
    auto setReadoutMode(int mode) -> bool;

    /**
     * @brief Get current readout mode
     */
    [[nodiscard]] auto getReadoutMode() -> std::optional<int>;

    /**
     * @brief Set fast readout mode
     * @param fast True for fast readout
     * @return true if set successfully
     */
    auto setFastReadout(bool fast) -> bool;

    /**
     * @brief Check if fast readout is enabled
     */
    [[nodiscard]] auto isFastReadout() -> bool;

    // ==================== Pulse Guiding ====================

    /**
     * @brief Start pulse guide
     * @param direction 0=N, 1=S, 2=E, 3=W
     * @param duration Duration in milliseconds
     * @return true if started
     */
    auto pulseGuide(int direction, int duration) -> bool;

    /**
     * @brief Check if pulse guiding
     */
    [[nodiscard]] auto isPulseGuiding() -> bool;

    // ==================== Status ====================

    /**
     * @brief Get camera state
     */
    [[nodiscard]] auto getCameraState() -> CameraState;

    /**
     * @brief Get camera status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

private:
    // ==================== Internal Methods ====================

    void refreshCapabilities();
    void refreshSensorInfo();

    // ==================== Member Variables ====================

    std::atomic<CameraState> cameraState_{CameraState::Idle};
    std::atomic<bool> exposing_{false};

    CameraCapabilities capabilities_;
    SensorInfo sensorInfo_;
    ExposureSettings exposureSettings_;
    TemperatureInfo temperatureInfo_;
    GainSettings gainSettings_;

    mutable std::mutex exposureMutex_;
    std::condition_variable exposureCV_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_CAMERA_HPP
