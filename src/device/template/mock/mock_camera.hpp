/*
 * mock_camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Camera Implementation for testing

*************************************************/

#pragma once

#include "../template/camera.hpp"

#include <random>

class MockCamera : public AtomCamera {
public:
    explicit MockCamera(const std::string& name = "MockCamera");
    ~MockCamera() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;

    // Exposure control
    auto startExposure(double duration) -> bool override;
    auto abortExposure() -> bool override;
    auto isExposing() const -> bool override;
    auto getExposureProgress() const -> double override;
    auto getExposureRemaining() const -> double override;
    auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> override;
    auto saveImage(const std::string& path) -> bool override;

    // Exposure history
    auto getLastExposureDuration() const -> double override;
    auto getExposureCount() const -> uint32_t override;
    auto resetExposureCount() -> bool override;

    // Video control
    auto startVideo() -> bool override;
    auto stopVideo() -> bool override;
    auto isVideoRunning() const -> bool override;
    auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> override;
    auto setVideoFormat(const std::string& format) -> bool override;
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

private:
    // Mock configuration
    static constexpr int MOCK_WIDTH = 1920;
    static constexpr int MOCK_HEIGHT = 1080;
    static constexpr double MOCK_PIXEL_SIZE = 3.75;  // micrometers
    static constexpr int MOCK_BIT_DEPTH = 16;

    // State variables
    bool is_exposing_{false};
    bool is_video_running_{false};
    bool shutter_open_{true};
    int fan_speed_{50};

    // Camera parameters
    int current_gain_{0};
    int current_offset_{10};
    int current_iso_{100};
    FrameType current_frame_type_{FrameType::FITS};
    UploadMode current_upload_mode_{UploadMode::LOCAL};

    // Temperature control
    bool cooler_on_{false};
    double target_temperature_{0.0};
    double current_temperature_{20.0};
    double cooling_power_{0.0};

    // Resolution and binning
    AtomCameraFrame::Resolution current_resolution_{MOCK_WIDTH, MOCK_HEIGHT,
                                                    MOCK_WIDTH, MOCK_HEIGHT};
    AtomCameraFrame::Binning current_binning_{1, 1};

    // Exposure tracking
    std::chrono::system_clock::time_point exposure_start_;
    double exposure_duration_{0.0};

    // Random number generation for simulation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;

    // Helper methods
    void simulateExposure();
    void simulateTemperatureControl();
    std::shared_ptr<AtomCameraFrame> generateMockFrame();
    std::vector<uint16_t> generateMockImageData();
};
