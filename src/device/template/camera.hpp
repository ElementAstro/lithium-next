/*
 * camera.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomCamera Simulator and Basic Definition

*************************************************/

#pragma once

#include "camera_frame.hpp"
#include "device.hpp"

#include <optional>

class AtomCamera : public AtomDriver {
public:
    explicit AtomCamera(const std::string &name);

    // 曝光控制
    virtual auto startExposure(double duration) -> bool = 0;
    virtual auto abortExposure() -> bool = 0;
    [[nodiscard]] virtual auto isExposing() const -> bool = 0;
    virtual auto getExposureResult() -> std::shared_ptr<AtomCameraFrame> = 0;
    virtual auto saveImage(const std::string &path) -> bool = 0;

    // 视频控制
    virtual auto startVideo() -> bool = 0;
    virtual auto stopVideo() -> bool = 0;
    [[nodiscard]] virtual auto isVideoRunning() const -> bool = 0;
    virtual auto getVideoFrame() -> std::shared_ptr<AtomCameraFrame> = 0;

    // 温度控制
    virtual auto startCooling(double targetTemp) -> bool = 0;
    virtual auto stopCooling() -> bool = 0;
    [[nodiscard]] virtual auto isCoolerOn() const -> bool = 0;
    [[nodiscard]] virtual auto getTemperature() const
        -> std::optional<double> = 0;
    [[nodiscard]] virtual auto getCoolingPower() const
        -> std::optional<double> = 0;
    [[nodiscard]] virtual auto hasCooler() const -> bool = 0;

    // 参数控制
    virtual auto setGain(int gain) -> bool = 0;
    [[nodiscard]] virtual auto getGain() -> std::optional<int> = 0;
    virtual auto setOffset(int offset) -> bool = 0;
    [[nodiscard]] virtual auto getOffset() -> std::optional<int> = 0;
    virtual auto setISO(int iso) -> bool = 0;
    [[nodiscard]] virtual auto getISO() -> std::optional<int> = 0;

    // 帧设置
    virtual auto getResolution() -> std::optional<AtomCameraFrame::Resolution> = 0;
    virtual auto setResolution(int x, int y, int width, int height) -> bool = 0;
    virtual auto getBinning() -> std::optional<AtomCameraFrame::Binning> = 0;
    virtual auto setBinning(int horizontal, int vertical) -> bool = 0;
    virtual auto setFrameType(FrameType type) -> bool = 0;
    virtual auto setUploadMode(UploadMode mode) -> bool = 0;
    [[nodiscard]] virtual auto getFrameInfo() const -> AtomCameraFrame = 0;

protected:
    std::shared_ptr<AtomCameraFrame> current_frame_;
};
