/*
 * indi_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Camera Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_CAMERA_HPP
#define LITHIUM_CLIENT_INDI_CAMERA_HPP

#include "indi_device_base.hpp"

#include <memory>

namespace lithium::client::indi {

/**
 * @brief Image format enumeration
 */
enum class ImageFormat : uint8_t { FITS, Native, XISF, Unknown };

/**
 * @brief Camera state enumeration
 */
enum class CameraState : uint8_t {
    Idle,
    Exposing,
    Downloading,
    Aborted,
    Error,
    Unknown
};

/**
 * @brief Frame type enumeration
 */
enum class FrameType : uint8_t { Light, Bias, Dark, Flat };

/**
 * @brief Upload mode enumeration
 */
enum class UploadMode : uint8_t { Client, Local, Both };

/**
 * @brief Camera frame information
 */
struct CameraFrame {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    int binX{1};
    int binY{1};
    int bitDepth{16};
    double pixelSizeX{0.0};
    double pixelSizeY{0.0};
    FrameType frameType{FrameType::Light};

    [[nodiscard]] auto toJson() const -> json {
        return {{"x", x},
                {"y", y},
                {"width", width},
                {"height", height},
                {"binX", binX},
                {"binY", binY},
                {"bitDepth", bitDepth},
                {"pixelSizeX", pixelSizeX},
                {"pixelSizeY", pixelSizeY},
                {"frameType", static_cast<int>(frameType)}};
    }
};

/**
 * @brief Camera sensor information
 */
struct SensorInfo {
    int maxWidth{0};
    int maxHeight{0};
    double pixelSizeX{0.0};
    double pixelSizeY{0.0};
    int bitDepth{16};
    int maxBinX{1};
    int maxBinY{1};
    bool isColor{false};
    std::string bayerPattern;

    [[nodiscard]] auto toJson() const -> json {
        return {{"maxWidth", maxWidth},
                {"maxHeight", maxHeight},
                {"pixelSizeX", pixelSizeX},
                {"pixelSizeY", pixelSizeY},
                {"bitDepth", bitDepth},
                {"maxBinX", maxBinX},
                {"maxBinY", maxBinY},
                {"isColor", isColor},
                {"bayerPattern", bayerPattern}};
    }
};

/**
 * @brief Temperature control information
 */
struct CoolerInfo {
    bool hasCooler{false};
    bool coolerOn{false};
    double currentTemp{0.0};
    double targetTemp{0.0};
    double coolerPower{0.0};
    double minTemp{-40.0};
    double maxTemp{40.0};
    // Temperature ramp settings
    double tempRampSlope{0.0};
    double tempRampThreshold{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"hasCooler", hasCooler},
                {"coolerOn", coolerOn},
                {"currentTemp", currentTemp},
                {"targetTemp", targetTemp},
                {"coolerPower", coolerPower},
                {"minTemp", minTemp},
                {"maxTemp", maxTemp},
                {"tempRampSlope", tempRampSlope},
                {"tempRampThreshold", tempRampThreshold}};
    }
};

/**
 * @brief Gain/Offset control information
 */
struct GainOffsetInfo {
    int gain{0};
    int minGain{0};
    int maxGain{100};
    int offset{0};
    int minOffset{0};
    int maxOffset{100};

    [[nodiscard]] auto toJson() const -> json {
        return {{"gain", gain},       {"minGain", minGain},
                {"maxGain", maxGain}, {"offset", offset},
                {"minOffset", minOffset}, {"maxOffset", maxOffset}};
    }
};

/**
 * @brief Exposure result data
 */
struct ExposureResult {
    bool success{false};
    std::string filename;
    std::string format;
    std::vector<uint8_t> data;
    size_t size{0};
    double duration{0.0};
    std::chrono::system_clock::time_point timestamp;
    CameraFrame frame;

    [[nodiscard]] auto toJson() const -> json {
        return {{"success", success},
                {"filename", filename},
                {"format", format},
                {"size", size},
                {"duration", duration},
                {"frame", frame.toJson()}};
    }
};

/**
 * @brief INDI Camera Device
 *
 * Provides camera-specific functionality including:
 * - Exposure control
 * - Temperature control
 * - Gain/Offset control
 * - Frame settings
 * - Video streaming
 */
class INDICamera : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Camera
     * @param name Device name
     */
    explicit INDICamera(std::string name);

    /**
     * @brief Destructor
     */
    ~INDICamera() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Camera";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Exposure Control ====================

    /**
     * @brief Start an exposure
     * @param duration Exposure duration in seconds
     * @return true if exposure started
     */
    auto startExposure(double duration) -> bool;

    /**
     * @brief Abort current exposure
     * @return true if aborted
     */
    auto abortExposure() -> bool;

    /**
     * @brief Check if camera is exposing
     * @return true if exposing
     */
    [[nodiscard]] auto isExposing() const -> bool;

    /**
     * @brief Get exposure progress
     * @return Remaining time in seconds, or nullopt if not exposing
     */
    [[nodiscard]] auto getExposureProgress() const -> std::optional<double>;

    /**
     * @brief Get last exposure result
     * @return Exposure result
     */
    [[nodiscard]] auto getExposureResult() const -> ExposureResult;

    /**
     * @brief Wait for exposure to complete
     * @param timeout Timeout in milliseconds
     * @return true if exposure completed
     */
    auto waitForExposure(std::chrono::milliseconds timeout =
                             std::chrono::minutes(30)) -> bool;

    // ==================== Temperature Control ====================

    /**
     * @brief Start cooling to target temperature
     * @param targetTemp Target temperature in Celsius
     * @return true if cooling started
     */
    auto startCooling(double targetTemp) -> bool;

    /**
     * @brief Stop cooling
     * @return true if cooling stopped
     */
    auto stopCooling() -> bool;

    /**
     * @brief Check if cooler is on
     * @return true if cooler is on
     */
    [[nodiscard]] auto isCoolerOn() const -> bool;

    /**
     * @brief Get current temperature
     * @return Temperature in Celsius, or nullopt if not available
     */
    [[nodiscard]] auto getTemperature() const -> std::optional<double>;

    /**
     * @brief Get cooler power percentage
     * @return Power percentage, or nullopt if not available
     */
    [[nodiscard]] auto getCoolerPower() const -> std::optional<double>;

    /**
     * @brief Check if camera has cooler
     * @return true if has cooler
     */
    [[nodiscard]] auto hasCooler() const -> bool;

    /**
     * @brief Get cooler information
     * @return Cooler info
     */
    [[nodiscard]] auto getCoolerInfo() const -> CoolerInfo;

    // ==================== Gain/Offset Control ====================

    /**
     * @brief Set gain value
     * @param gain Gain value
     * @return true if set successfully
     */
    auto setGain(int gain) -> bool;

    /**
     * @brief Get current gain
     * @return Gain value, or nullopt if not available
     */
    [[nodiscard]] auto getGain() const -> std::optional<int>;

    /**
     * @brief Set offset value
     * @param offset Offset value
     * @return true if set successfully
     */
    auto setOffset(int offset) -> bool;

    /**
     * @brief Get current offset
     * @return Offset value, or nullopt if not available
     */
    [[nodiscard]] auto getOffset() const -> std::optional<int>;

    /**
     * @brief Get gain/offset information
     * @return Gain/offset info
     */
    [[nodiscard]] auto getGainOffsetInfo() const -> GainOffsetInfo;

    // ==================== Frame Settings ====================

    /**
     * @brief Set frame region
     * @param x X offset
     * @param y Y offset
     * @param width Width
     * @param height Height
     * @return true if set successfully
     */
    auto setFrame(int x, int y, int width, int height) -> bool;

    /**
     * @brief Reset frame to full sensor
     * @return true if reset successfully
     */
    auto resetFrame() -> bool;

    /**
     * @brief Get current frame settings
     * @return Frame info
     */
    [[nodiscard]] auto getFrame() const -> CameraFrame;

    /**
     * @brief Set binning
     * @param binX Horizontal binning
     * @param binY Vertical binning
     * @return true if set successfully
     */
    auto setBinning(int binX, int binY) -> bool;

    /**
     * @brief Get current binning
     * @return Pair of (binX, binY)
     */
    [[nodiscard]] auto getBinning() const -> std::pair<int, int>;

    /**
     * @brief Set frame type
     * @param type Frame type
     * @return true if set successfully
     */
    auto setFrameType(FrameType type) -> bool;

    /**
     * @brief Get current frame type
     * @return Frame type
     */
    [[nodiscard]] auto getFrameType() const -> FrameType;

    /**
     * @brief Set upload mode
     * @param mode Upload mode
     * @return true if set successfully
     */
    auto setUploadMode(UploadMode mode) -> bool;

    /**
     * @brief Set upload directory
     * @param directory Directory path
     * @return true if set successfully
     */
    auto setUploadDirectory(const std::string& directory) -> bool;

    /**
     * @brief Set upload prefix
     * @param prefix Filename prefix
     * @return true if set successfully
     */
    auto setUploadPrefix(const std::string& prefix) -> bool;

    // ==================== Sensor Information ====================

    /**
     * @brief Get sensor information
     * @return Sensor info
     */
    [[nodiscard]] auto getSensorInfo() const -> SensorInfo;

    /**
     * @brief Check if camera is color
     * @return true if color camera
     */
    [[nodiscard]] auto isColor() const -> bool;

    // ==================== Video Streaming ====================

    /**
     * @brief Start video streaming
     * @return true if started
     */
    auto startVideo() -> bool;

    /**
     * @brief Stop video streaming
     * @return true if stopped
     */
    auto stopVideo() -> bool;

    /**
     * @brief Check if video is running
     * @return true if running
     */
    [[nodiscard]] auto isVideoRunning() const -> bool;

    /**
     * @brief Set video frame rate
     * @param fps Frames per second
     * @return true if set successfully
     */
    auto setVideoFrameRate(double fps) -> bool;

    // ==================== Image Format ====================

    /**
     * @brief Set image format
     * @param format Image format
     * @return true if set successfully
     */
    auto setImageFormat(ImageFormat format) -> bool;

    /**
     * @brief Get current image format
     * @return Image format
     */
    [[nodiscard]] auto getImageFormat() const -> ImageFormat;

    // ==================== Status ====================

    /**
     * @brief Get camera state
     * @return Camera state
     */
    [[nodiscard]] auto getCameraState() const -> CameraState;

    /**
     * @brief Get camera status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;
    void onBlobReceived(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleExposureProperty(const INDIProperty& property);
    void handleTemperatureProperty(const INDIProperty& property);
    void handleCoolerProperty(const INDIProperty& property);
    void handleGainProperty(const INDIProperty& property);
    void handleOffsetProperty(const INDIProperty& property);
    void handleFrameProperty(const INDIProperty& property);
    void handleBinningProperty(const INDIProperty& property);
    void handleCCDInfoProperty(const INDIProperty& property);
    void handleBlobProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // Camera state
    std::atomic<CameraState> cameraState_{CameraState::Idle};
    std::atomic<bool> isExposing_{false};
    std::atomic<bool> isVideoRunning_{false};

    // Exposure
    std::atomic<double> currentExposure_{0.0};
    std::atomic<double> exposureRemaining_{0.0};
    ExposureResult lastExposureResult_;
    mutable std::mutex exposureMutex_;
    std::condition_variable exposureCondition_;

    // Temperature control
    CoolerInfo coolerInfo_;
    mutable std::mutex coolerMutex_;

    // Gain/Offset
    GainOffsetInfo gainOffsetInfo_;
    mutable std::mutex gainOffsetMutex_;

    // Frame settings
    CameraFrame currentFrame_;
    SensorInfo sensorInfo_;
    mutable std::mutex frameMutex_;

    // Image format
    std::atomic<ImageFormat> imageFormat_{ImageFormat::FITS};
    std::atomic<UploadMode> uploadMode_{UploadMode::Client};
    std::string uploadDirectory_;
    std::string uploadPrefix_{"IMAGE_XXX"};

    // Associated devices
    std::string associatedTelescope_;
    std::string associatedFocuser_;
    std::string associatedRotator_;
    std::string associatedFilterWheel_;
    mutable std::mutex associatedMutex_;
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_CAMERA_HPP
