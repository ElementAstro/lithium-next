/*
 * camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Camera Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
// clang-format on
#endif

#include "device/template/camera.hpp"

// ASCOM-specific types and constants
enum class ASCOMCameraState {
    IDLE = 0,
    WAITING = 1,
    EXPOSING = 2,
    READING = 3,
    DOWNLOAD = 4,
    ERROR = 5
};

enum class ASCOMSensorType {
    MONOCHROME = 0,
    COLOR = 1,
    RGGB = 2,
    CMYG = 3,
    CMYG2 = 4,
    LRGB = 5
};

class ASCOMCamera : public AtomCamera {
public:
    explicit ASCOMCamera(std::string name);
    ~ASCOMCamera() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry)
        -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Exposure control
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string &path) -> bool override;

    // Exposure history and statistics
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // Video/streaming control
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string &format) -> bool override;
    auto getVideoFormats() -> std::vector<std::string> override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool override;
    auto stopCooling() -> bool override;
    auto isCoolerOn() const -> bool override;
    auto getTemperature() const -> std::optional<double> override;
    auto getTemperatureInfo() const -> TemperatureInfo override;
    auto getCoolingPower() const -> std::optional<double> override;
    auto hasCooler() const -> bool override;
    auto setTemperature(double temperature) -> bool override;

    // Color information
    auto isColor() const -> bool override;
    auto getBayerPattern() const -> BayerPattern override;
    auto setBayerPattern(BayerPattern pattern) -> bool override;

    // Parameter control
    auto setGain(int gain) -> bool override;
    auto getGain() -> std::optional<int> override;
    auto getGainRange() -> std::pair<int, int> override;
    auto setOffset(int offset) -> bool override;
    auto getOffset() -> std::optional<int> override;
    auto getOffsetRange() -> std::pair<int, int> override;
    auto setISO(int iso) -> bool override;
    auto getISO() -> std::optional<int> override;
    auto getISOList() -> std::vector<int> override;

    // Frame settings
    auto getResolution() -> std::optional<AtomCameraFrame::Resolution> override;
    auto setResolution(int x, int y, int width, int height) -> bool override;
    auto getMaxResolution() -> AtomCameraFrame::Resolution override;
    auto getBinning() -> std::optional<AtomCameraFrame::Binning> override;
    auto setBinning(int horizontal, int vertical) -> bool override;
    auto getMaxBinning() -> AtomCameraFrame::Binning override;
    auto setFrameType(FrameType type) -> bool override;
    auto getFrameType() -> FrameType override;
    auto setUploadMode(UploadMode mode) -> bool override;
    auto getUploadMode() -> UploadMode override;
    auto getFrameInfo() const -> std::shared_ptr<AtomCameraFrame> override;

    // Pixel information
    auto getPixelSize() -> double override;
    auto getPixelSizeX() -> double override;
    auto getPixelSizeY() -> double override;
    auto getBitDepth() -> int override;

    // Shutter control
    auto hasShutter() -> bool override;
    auto setShutter(bool open) -> bool override;
    auto getShutterStatus() -> bool override;

    // Fan control
    auto hasFan() -> bool override;
    auto setFanSpeed(int speed) -> bool override;
    auto getFanSpeed() -> int override;

    // Advanced video features
    auto startVideoRecording(const std::string &filename) -> bool override;
    auto stopVideoRecording() -> bool override;
    auto isVideoRecording() const -> bool override;
    auto setVideoExposure(double exposure) -> bool override;
    auto getVideoExposure() const -> double override;
    auto setVideoGain(int gain) -> bool override;
    auto getVideoGain() const -> int override;

    // Image sequence capabilities
    auto startSequence(int count, double exposure, double interval)
        -> bool override;
    auto stopSequence() -> bool override;
    auto isSequenceRunning() const -> bool override;
    auto getSequenceProgress() const -> std::pair<int, int> override;

    // Advanced image processing
    auto setImageFormat(const std::string &format) -> bool override;
    auto getImageFormat() const -> std::string override;
    auto enableImageCompression(bool enable) -> bool override;
    auto isImageCompressionEnabled() const -> bool override;
    auto getSupportedImageFormats() const -> std::vector<std::string> override;

    // Image quality and statistics
    auto getFrameStatistics() const -> std::map<std::string, double> override;
    auto getTotalFramesReceived() const -> uint64_t override;
    auto getDroppedFrames() const -> uint64_t override;
    auto getAverageFrameRate() const -> double override;
    auto getLastImageQuality() const -> std::map<std::string, double> override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // ASCOM Camera-specific properties
    auto canAbortExposure() -> bool;
    auto canAsymmetricBin() -> bool;
    auto canFastReadout() -> bool;
    auto canStopExposure() -> bool;
    auto canSubFrame() -> bool;
    auto getCameraState() -> ASCOMCameraState;
    auto getSensorType() -> ASCOMSensorType;
    auto getElectronsPerADU() -> double;
    auto getFullWellCapacity() -> double;
    auto getMaxADU() -> int;

    // Alpaca discovery and connection
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto connectToAlpacaDevice(const std::string &host, int port,
                               int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;

    // ASCOM COM object connection (Windows only)
#ifdef _WIN32
    auto connectToCOMDriver(const std::string &progId) -> bool;
    auto disconnectFromCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

protected:
    // Connection management
    enum class ConnectionType {
        COM_DRIVER,
        ALPACA_REST
    } connection_type_{ConnectionType::ALPACA_REST};

    // Device state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_exposing_{false};
    std::atomic<bool> is_streaming_{false};
    std::atomic<bool> is_cooling_{false};

    // ASCOM device information
    std::string device_name_;
    std::string driver_info_;
    std::string driver_version_;
    std::string client_id_{"Lithium-Next"};
    int interface_version_{3};

    // Alpaca connection details
    std::string alpaca_host_{"localhost"};
    int alpaca_port_{11111};
    int alpaca_device_number_{0};

#ifdef _WIN32
    // COM object for Windows ASCOM drivers
    IDispatch *com_camera_{nullptr};
    std::string com_prog_id_;
#endif

    // Camera properties cache
    struct ASCOMCameraInfo {
        int camera_x_size{0};
        int camera_y_size{0};
        double pixel_size_x{0.0};
        double pixel_size_y{0.0};
        int max_bin_x{1};
        int max_bin_y{1};
        int bayer_offset_x{0};
        int bayer_offset_y{0};
        bool can_abort_exposure{false};
        bool can_asymmetric_bin{false};
        bool can_fast_readout{false};
        bool can_stop_exposure{false};
        bool can_sub_frame{false};
        bool has_shutter{false};
        ASCOMSensorType sensor_type{ASCOMSensorType::MONOCHROME};
        double electrons_per_adu{1.0};
        double full_well_capacity{0.0};
        int max_adu{65535};
    } ascom_camera_info_;

    // Current settings
    struct CameraSettings {
        int bin_x{1};
        int bin_y{1};
        int start_x{0};
        int start_y{0};
        int num_x{0};
        int num_y{0};
        double exposure_duration{1.0};
        FrameType frame_type{FrameType::FITS};
        int gain{0};
        int offset{0};
        double target_temperature{-10.0};
        bool cooler_on{false};
    } current_settings_;

    // Statistics
    mutable std::atomic<uint32_t> exposure_count_{0};
    mutable std::atomic<double> last_exposure_duration_{0.0};

    // Threading for monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method,
                           const std::string &endpoint,
                           const std::string &params = "") const
        -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response)
        -> std::optional<std::string>;
    auto updateCameraInfo() -> bool;
    auto startMonitoring() -> void;
    auto stopMonitoring() -> void;
    auto monitoringLoop() -> void;

#ifdef _WIN32
    auto invokeCOMMethod(const std::string &method, VARIANT *params = nullptr,
                         int param_count = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string &property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string &property, const VARIANT &value)
        -> bool;
    auto getImageArray() -> std::optional<std::vector<uint16_t>>;
#endif
};
